# G925 Stock Stable (original Mali native libraries)

Based on the successful graphics-lab APK from run 41363883009 at commit
99d29c7ac0eb0d7d46964396153b5d8e10460046. Only Java launcher,
Gradle packaging, test and workflow files change from that successful baseline.
The fast build validates this ancestry and reuses the matching native APK
without recompiling the renderer.

Excluded native experiments: extra 96 MiB Vulkan image allocation pool and
longer texture/buffer cache lifetimes. Stock Mali default_args.txt is unchanged.

When an experimental G925 profile is recognized on first launch, graphics.txt
is backed up and restored to 30 FPS + run-ahead 0 with original image defaults;
valid touch-control preferences are preserved. Other user profiles are not
altered. The user may manually select either G925 STOCK 30 - SMOOTH or
G925 STOCK 60 - TEST, save, and restart. The 60 setting is a cap, not a 60 FPS
guarantee. Compare 60 seconds stationary plus 60 seconds driving on the same
save and with the phone at the same temperature. No game saves are changed.
