# Original XL rendering on Android

An isolated Android executable replayed original desktop D2X-XL rendering for Standoff and the Pumo hub. At each selected swap it read the default framebuffer from GLES, compared all RGB pixels with a desktop replay of the same trace, and exited. The inputs were recorded after 120 original game iterations. Both runs used an API 34 x86_64 emulator with SwiftShader. The same build script produced arm64-v8a binaries. Arm64 execution and physical-device performance remain unmeasured.

| Original scene | WGL swap call | Identical RGB pixels / 5,184,000 | Pixels within 8 in every channel | Mean absolute error R / G / B (0-255) |
| -------------- | ------------: | -------------------------------: | -------------------------------: | ------------------------------------- |
| Standoff       |        856103 |                        4,918,498 |                        5,183,831 | 0.0287 / 0.0228 / 0.0215              |
| Pumo hub       |       1316127 |                        4,940,045 |                        5,182,880 | 0.0305 / 0.0168 / 0.0226              |

The first Standoff run returned a black image. A separate translated desktop triangle drew the expected red foreground and blue background. Loading GL4ES during that test changed the current EGL context. The final implementation restores the binding after GL4ES initializes and supplies the current drawable dimensions through its framebuffer-size callback. Standoff ends by clearing the screen and blitting an offscreen color buffer into it. A separate shader whitespace experiment produced the same final RGB bytes in both scenes as unmodified GL4ES. The published builder uses unmodified GL4ES. Shader compilation and depth/stencil conversion diagnostics remain in the private logs.

The capture stops at the requested swap. These results establish color output for two specific frames. Running original XL game code, controlling the ship, audio, background/resume, saves, network play and teardown require separate Android tests. The Redux launcher and its engine selection are outside this experiment.

## Reproduce

Use a private desktop WGL trace, its matching desktop PNG, Python 3.11 with Pillow, git, CMake, Ninja and the Android NDK. Supply a scratch directory for the two pinned third-party checkouts. Use a dedicated Android test device with enough free space for the trace. The runner writes its inputs and frame into a new directory under the device shell's temporary directory. A previously staged trace can be reused by passing its device path with `--device-trace`. The runner verifies its digest against the host trace.

```powershell
python build.py --work C:/scratch/xl-gl --abi x86_64 --ndk C:/local/android-ndk-r30 --cmake C:/local/android-sdk/cmake/3.31.6/bin/cmake.exe --ninja C:/local/android-sdk/cmake/3.31.6/bin/ninja.exe
python replay.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial <test-device> --build C:/scratch/xl-gl/x86_64 --trace <private-standoff.trace> --desktop <desktop-frame.png> --call 856103 --output C:/scratch/standoff-comparison
```

The comparison receipt includes binary and input identities, RGB digests, and pixel differences. The known draw must pass before the trace runs. Keep traces, private images and full logs in the private test workspace. [Measured results](results-20261119.json)
