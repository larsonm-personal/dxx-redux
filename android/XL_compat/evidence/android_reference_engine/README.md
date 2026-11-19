# Original XL game loop on Android

This experiment builds the pinned Arbruijn D2X-XL engine outside the Redux runtime. It selects an authentic mission through the original mission browser, enters the requested level and runs 120 game frames. The SDL 1.2 adapter supplies an Android EGL pbuffer. GL4ES translates the desktop OpenGL calls. Immediately before each swap the optional observer reads the GLES default framebuffer and restores its binding. On completion it saves the last presented image, unwinds the game loop, saves the test profile, closes the original graphics owners and releases EGL.

## Build

Use the same NDK and GL4ES build as the [trace replay](../android_reference_replay/README.md). Supply a local Git checkout containing Arbruijn commit `2576a98b5de310e3ac7c9777df2677d1627b8bae`. The builder creates a fresh checkout, adds the shared campaign observer and applies the Android port changes. It uses the upstream engine source lists. The Android target omits the native BSD IPX backend. The experiment uses a WAV mixer and an offline curl build. The original eight-character highest-level table skips mission names that cannot fit.

Supply the following clean source directories under `--deps`. The builder checks each Git revision. GLEW 2.2.0 and libpng 1.6.50 use the extracted release trees specified by `--glew` and `--png`. Their full tree digests are verified before compilation. Use the SDL 1.2 source and shared library produced by the same ABI's Redux Gradle build.

| Directory     | Upstream             | Commit                                   |
| ------------- | -------------------- | ---------------------------------------- |
| dep-curl      | curl/curl            | fdb8a789d2b446b77bd7cdd2eff95f6cbc814cf4 |
| dep-glu       | freedesktop Mesa GLU | a2b96c7bba8db8fec3e02fb4227a7f7b02cabad1 |
| dep-SDL_image | libsdl-org/SDL_image | 2ffb2e3e1eba037897164e3ac6c67570d8bccd79 |
| dep-SDL_mixer | libsdl-org/SDL_mixer | 1a14d94ed4271e45435ecb5512d61792e1a42932 |
| dep-SDL_net   | libsdl-org/SDL_net   | 7aa8ffae32fda6455792c5538a8b64a655dd0721 |

Run from this directory with Python 3.11 or later. In this example `$repo` is the Redux checkout, `$gl` is the ABI-specific trace-replay build directory and `$build` is a new output directory. Use the same ABI for SDL and GL4ES. The output contains the executable, both shared libraries and `build-receipt.json`.

```powershell
python build.py --abi x86_64 --work $build --redux $repo --original $upstream --deps $deps --sdl-source $sdlSource --sdl-library $sdlLibrary --gl-source "$gl/gl4es" --gl-library "$gl/libGL4ES.so" --png $png --glew $glew --ndk $ndk --cmake $cmake --ninja $ninja
```

## Run

Use a disposable Android test profile. The provisioned device directory contains the original engine's `game`, `user` and `cache` directories. Place the retail inputs in the original engine's data layout. Standoff uses `game/missions/darkflamewolf/vignette-xl.{mn2,hog}`. The Pumo hub uses `game/missions/single/pmines.{mn2,hog}` and the author's `game/mods/pmines` tree. Preserve the full released asset tree when provisioning Pumo. Private inputs and captured images stay outside the source checkout.

The runner requires adb and Python Pillow. It creates a separate on-device executable directory for every measurement, verifies the uploaded files, and saves the engine log, tab-separated game receipt, PPM image and result JSON. A passing run must identify the requested mission and level, reach 120 frames alive, produce an 800 by 600 image with multiple colors, and exit successfully. Running times include the readback instrument. They do not describe interactive frame rates.

```powershell
python run.py --adb $adb --serial $serial --device-root /data/local/tmp/xl-original --engine "$build/xl_original" --sdl "$build/libSDL12.so" --gl "$build/libGL.so.1" --scene standoff --output "$results/standoff"
python run.py --adb $adb --serial $serial --device-root /data/local/tmp/xl-original --engine "$build/xl_original" --sdl "$build/libSDL12.so" --gl "$build/libGL.so.1" --scene pumo --output "$results/pumo"
```

The experiment uses offscreen graphics, muted audio and scripted mission selection. Touch input, audible playback, activity pause and resume, physical arm64 gameplay, save continuation and network play require their own tests. The image tracks the most recent graphics swap visible to the observer. Game frames and graphics swaps have distinct counters.

On November 19 the native x86_64 engine passed both scenes on an API 34 emulator. Standoff finished in 6.157 seconds and presented 18,064 colors. The Pumo hub finished in 13.766 seconds and presented 58,923 colors. The 2,667 Pumo mod files and 15 base data files matched the desktop input trees byte for byte. The arm64 executable built from the same recipe. Under the x86_64 emulator's native bridge it exited with signal 11 before opening a mission receipt. These images came from independent live sessions. The separate desktop-command-stream replay provides the matched pixel comparison. [Build and session receipts](results-20261119.json)
