# G925 Standard / 60 FPS

Experimental Mali APK: ordinary 720p native rendering, FXAA, console shadows, normal reflection range and 60 FPS frame-rate cap. The FPS is a ceiling, not a guarantee. CPU performance hint target 20 ms, run-ahead 0, no GPU driver changes.

On the first launch, existing graphics are backed up once to graphics.before_g925_standard60.txt. The standard 60 profile is applied, without touching installed game assets or save data. In the launcher select G925 STANDARD / 60 FPS or RESET TO DEFAULT, then Save changes and Play. RESTORE PRE-STANDARD GRAPHICS restores the backed-up file.

Test the real game in the same scene with the FPS/frametime overlay. The observed ~20 FPS at 49ms means simply increasing the cap will not guarantee 60 FPS. Further device profiling will be necessary if it remains below 30.
