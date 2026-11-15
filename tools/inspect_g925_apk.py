#!/usr/bin/env python3
"""Inspect a delivered APK and optionally compare its resources with a baseline.

Requires cryptography. Supports RSA APK v2 signatures (not an Android installer).
Format: https://source.android.com/docs/security/features/apksigning/v2
Usage: python tools/inspect_g925_apk.py NEW.apk --baseline OLD.apk --version-code 37
"""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa


def require(condition, message):
    if not condition:
        raise ValueError(message)


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def take(self, size):
        require(0 <= size <= len(self.data) - self.offset, 'Invalid length-prefixed field')
        result = self.data[self.offset:self.offset + size]
        self.offset += size
        return result

    def u32(self):
        return struct.unpack('<I', self.take(4))[0]

    def lp(self):
        return self.take(self.u32())

    def done(self):
        require(self.offset == len(self.data), 'Unconsumed signature data')


def sequence(data):
    r = Reader(data)
    while r.offset < len(data):
        yield r.lp()
    r.done()


def records(data):
    result = []
    for item in sequence(data):
        r = Reader(item)
        algorithm = r.u32()
        result.append((algorithm, r.lp()))
        r.done()
    require(len({a for a, _ in result}) == len(result), 'Duplicate signature algorithm')
    return result


def verify_v2(data):
    data = bytes(data)
    eocd = data.rfind(b'PK\x05\x06', max(0, len(data) - 65557))
    require(eocd >= 0 and eocd + 22 <= len(data), 'Missing ZIP end record')
    require(eocd + 22 + struct.unpack_from('<H', data, eocd + 20)[0] == len(data), 'Trailing ZIP data')
    require(struct.unpack_from('<HH', data, eocd + 4) == (0, 0), 'Split ZIP is unsupported')
    cd_size, cd = struct.unpack_from('<II', data, eocd + 12)
    require(cd + cd_size == eocd and cd >= 24, 'Invalid central directory')
    require(data[cd - 16:cd] == b'APK Sig Block 42', 'Missing APK signing block')
    size = struct.unpack_from('<Q', data, cd - 24)[0]
    start = cd - size - 8
    require(size >= 24 and start >= 0, 'Invalid signing block size')
    require(struct.unpack_from('<Q', data, start)[0] == size, 'Signing block size mismatch')
    offset, v2 = start + 8, None
    while offset < cd - 24:
        require(offset + 8 <= cd - 24, 'Truncated signing pair')
        length = struct.unpack_from('<Q', data, offset)[0]
        offset += 8
        require(length >= 4 and offset + length <= cd - 24, 'Invalid signing pair')
        ident = struct.unpack_from('<I', data, offset)[0]
        if ident == 0x7109871A and v2 is None:
            v2 = data[offset + 4:offset + length]
        offset += length
    require(offset == cd - 24 and v2 is not None, 'Missing v2 signatures')
    outer = Reader(v2)
    signers = list(sequence(outer.lp()))
    outer.done()
    require(signers, 'No APK signers')
    adjusted = bytearray(data[eocd:])
    struct.pack_into('<I', adjusted, 16, start)
    content_digests = {}
    certificate_hashes = []
    for signer in signers:
        r = Reader(signer)
        signed, signatures, public_key = r.lp(), records(r.lp()), r.lp()
        r.done()
        supported = [a for a, _ in signatures if a in (0x101, 0x102, 0x103, 0x104)]
        require(supported, 'This verifier requires a supported RSA signature')
        algorithm = max(supported, key=lambda a: (a in (0x102, 0x104), a))
        sha512 = algorithm in (0x102, 0x104)
        hash_type = hashes.SHA512 if sha512 else hashes.SHA256
        hash_fn = hashlib.sha512 if sha512 else hashlib.sha256
        key = serialization.load_der_public_key(public_key)
        require(isinstance(key, rsa.RSAPublicKey), 'RSA algorithm has a non-RSA key')
        pad = (padding.PSS(mgf=padding.MGF1(hash_type()), salt_length=hash_type().digest_size)
               if algorithm in (0x101, 0x102) else padding.PKCS1v15())
        key.verify(dict(signatures)[algorithm], signed, pad, hash_type())
        s = Reader(signed)
        digests, certificates = records(s.lp()), list(sequence(s.lp()))
        s.lp()  # Additional attributes, already covered by the signature.
        # The verified project baseline includes one final empty field.
        # It too is signed; reject any nonempty or further extension.
        if s.offset < len(s.data):
            require(s.lp() == b'', 'Unexpected signed-data extension')
        s.done()
        require([a for a, _ in digests] == [a for a, _ in signatures], 'Signature algorithm list mismatch')
        require(certificates, 'Missing signer certificate')
        cert = x509.load_der_x509_certificate(certificates[0])
        require(cert.public_key().public_bytes(serialization.Encoding.DER,
                    serialization.PublicFormat.SubjectPublicKeyInfo) == public_key, 'Signer key mismatch')
        certificate_hashes.append(hashlib.sha256(certificates[0]).hexdigest())
        if sha512 not in content_digests:
            chunks = []
            for section in (memoryview(data)[:start], memoryview(data)[cd:eocd], adjusted):
                for at in range(0, len(section), 1024 * 1024):
                    chunk = section[at:at + 1024 * 1024]
                    digest = hash_fn(b'\xA5' + struct.pack('<I', len(chunk)))
                    digest.update(chunk)
                    chunks.append(digest.digest())
            content_digests[sha512] = hash_fn(b'\x5A' + struct.pack('<I', len(chunks)) + b''.join(chunks)).digest()
        require(dict(digests)[algorithm] == content_digests[sha512], 'APK content digest mismatch')
    return certificate_hashes


def manifest_values(data):
    strings, result = [], {}
    offset = struct.unpack_from('<H', data, 2)[0]
    while offset < len(data):
        kind, header_size, size = struct.unpack_from('<HHI', data, offset)
        require(size >= header_size >= 8 and offset + size <= len(data), 'Invalid XML chunk')
        if kind == 1:
            count, _, flags, string_start, _ = struct.unpack_from('<5I', data, offset + 8)
            positions = struct.unpack_from('<' + 'I' * count, data, offset + header_size)
            def length(at, utf8):
                value = data[at] if utf8 else struct.unpack_from('<H', data, at)[0]
                width, high = (1, 0x80) if utf8 else (2, 0x8000)
                if value & high:
                    rest = data[at + width] if utf8 else struct.unpack_from('<H', data, at + width)[0]
                    return ((value & (high - 1)) << (8 * width)) | rest, at + 2 * width
                return value, at + width
            for relative in positions:
                at = offset + string_start + relative
                if flags & 0x100:
                    _, at = length(at, True)
                    byte_count, at = length(at, True)
                    strings.append(data[at:at + byte_count].decode('utf-8'))
                else:
                    char_count, at = length(at, False)
                    strings.append(data[at:at + char_count * 2].decode('utf-16le'))
        elif kind == 0x102:
            name = strings[struct.unpack_from('<I', data, offset + 20)[0]]
            start, width, count = struct.unpack_from('<HHH', data, offset + 24)
            if name in ('manifest', 'uses-sdk'):
                attrs = {}
                for i in range(count):
                    at = offset + 16 + start + i * width
                    _, key, _ = struct.unpack_from('<III', data, at)
                    value_type, value = data[at + 15], struct.unpack_from('<I', data, at + 16)[0]
                    attrs[strings[key]] = strings[value] if value_type == 3 else value
                result[name] = attrs
        offset += size
    require('manifest' in result, 'Manifest root not found')
    return result


def inspect(path):
    data = Path(path).read_bytes()
    certs = verify_v2(data)
    with zipfile.ZipFile(path) as apk:
        names = apk.namelist()
        require(len(names) == len(set(names)), 'Duplicate APK entries')
        def digest(name):
            h = hashlib.sha256()
            with apk.open(name) as stream:
                while chunk := stream.read(1024 * 1024):
                    h.update(chunk)
            return h.hexdigest()
        defaults = [n for n in names if n.endswith('/default_args.txt')]
        require(len(defaults) == 1, 'Expected one default argument file')
        return dict(file=str(path), size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                    v2_signature_verified=True, certificate_sha256=certs,
                    manifest=manifest_values(apk.read('AndroidManifest.xml')),
                    default_args_sha256=digest(defaults[0]),
                    libraries_sha256={n: digest(n) for n in names if n.startswith('lib/') and n.endswith('.so')},
                    resources_sha256={n: digest(n) for n in names if n.startswith('assets/Resources/')})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk')
    parser.add_argument('--baseline')
    parser.add_argument('--version-code', type=int)
    parser.add_argument('--version-name')
    parser.add_argument('--native-marker', action='append', default=[])
    parser.add_argument('--dex-marker', action='append', default=[])
    parser.add_argument('--json-file')
    args = parser.parse_args()
    result = inspect(args.apk)
    manifest = result['manifest']['manifest']
    require(manifest['package'] == 'com.libertyrecomp', 'Wrong app package')
    if args.version_code:
        require(manifest['versionCode'] == args.version_code, 'Wrong app version')
    if args.version_name:
        require(manifest['versionName'] == args.version_name, 'Wrong app version name')
    if args.native_marker or args.dex_marker:
        with zipfile.ZipFile(args.apk) as apk:
            engine = apk.read('lib/arm64-v8a/librexgpu-gta4-native.so') if args.native_marker else b''
            dex = b''.join(apk.read(n) for n in apk.namelist() if n.endswith('.dex')) if args.dex_marker else b''
            for marker in args.native_marker:
                require(marker.encode() in engine, 'Native code marker missing: ' + marker)
            for marker in args.dex_marker:
                require(marker.encode() in dex, 'Report code marker missing: ' + marker)
        result['verified_requested_code_markers'] = True
    if args.baseline:
        base = inspect(args.baseline)
        require(base['certificate_sha256'] == result['certificate_sha256'], 'Update certificate changed')
        require(base['default_args_sha256'] == result['default_args_sha256'], 'Default settings changed')
        require(base['resources_sha256'] == result['resources_sha256'], 'Packaged resources changed')
        require(base['libraries_sha256'].keys() == result['libraries_sha256'].keys(), 'Native library set changed')
        result['changed_native_libraries'] = [n for n in result['libraries_sha256']
                                              if result['libraries_sha256'][n] != base['libraries_sha256'][n]]
        result['verified_update_identity_and_resources'] = True
    if args.json_file:
        Path(args.json_file).write_text(json.dumps(result, indent=2))
    summary = {k: result[k] for k in ('size', 'sha256', 'v2_signature_verified',
                                       'certificate_sha256', 'manifest')}
    summary.update(resource_count=len(result['resources_sha256']),
                   library_count=len(result['libraries_sha256']),
                   baseline_verified=result.get('verified_update_identity_and_resources', False))
    print(json.dumps(summary, indent=2))
