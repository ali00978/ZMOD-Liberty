// Synthetic hot-path test based on the user's ~3657 indexed draws/frame.
// Measures only host CPU rebasing; it does NOT predict GPU or GTA IV FPS.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_sparse_index_batch.h"

using namespace rex::graphics::gta4_native;

int main() {
  constexpr size_t kDraws = 32;
  constexpr size_t kIndicesEach = 36;
  constexpr int kIterations = 600;
  std::vector<uint16_t> source(kDraws * (kIndicesEach + 8), 0);
  std::array<NativeSparseIndexRange, kDraws> ranges{};
  for (size_t d = 0; d < kDraws; ++d) {
    ranges[d] = {uint32_t(d * (kIndicesEach + 8)), uint32_t(kIndicesEach),
                 int32_t(d) - 8};
    for (size_t i = 0; i < kIndicesEach; ++i)
      source[ranges[d].first + i] = uint16_t((i * 13 + d * 7) % 1000 + 16);
  }
  std::vector<uint8_t> sourceBytes(source.size() * sizeof(uint16_t));
  std::memcpy(sourceBytes.data(), source.data(), sourceBytes.size());
  std::vector<uint8_t> destination(kDraws * kIndicesEach * sizeof(uint32_t));
  int32_t outputBase = 0;
  uint64_t checksum = 0;
  auto started = std::chrono::steady_clock::now();
  for (int run = 0; run < kIterations; ++run) {
    if (!WriteNativeRebasedSparseTriangleIndices(
            sourceBytes, ranges, false, destination, &outputBase)) return 1;
    if (outputBase != -8) return 2;
    for (size_t d = 0; d < kDraws; ++d) {
      for (size_t i = 0; i < kIndicesEach; ++i) {
        uint32_t out;
        std::memcpy(&out, destination.data() +
                (d * kIndicesEach + i) * sizeof(uint32_t), sizeof(out));
        int64_t actual = int64_t(out) + outputBase;
        int64_t expected = int64_t(source[ranges[d].first + i]) +
                ranges[d].base_vertex;
        if (actual != expected) return 3;
        checksum += out;
      }
    }
  }
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - started).count();
  if (checksum == 0) return 4;
  std::cout << "{\"test\":\"host_index_rebase\",\"draws_per_batch\":" << kDraws
            << ",\"iterations\":" << kIterations << ",\"microseconds\":" << micros
            << ",\"checksum\":" << checksum
            << ",\"warning\":\"Host CPU only; not device FPS\"}\n";
}
