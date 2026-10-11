DXX-Redux
=========

Descent 1&2 source port based on [DXX-Retro](https://github.com/CDarrow/DXX-Retro).

Building in Visual Studio
-------------------------

- [Install vcpkg](https://vcpkg.io/en/getting-started.html)

- Set the VCPKG_ROOT environment variable to the vcpkg folder with Start > search Environment >
  Environment Variables > New...

- Open the d1 or d2 folder in Visual Studio with 'Open a local folder' or Open > CMake...

- Wait until CMake generation finishes

- Select Startup Item main/d1x-redux.exe

- Optionally select Debug > Debug and Launch Settings for d1x-redux and add
  below the line starting with "name":
  `"args": [ "-hogdir", "c:\game-data-folder" ]`

Building in msys2 / Linux
-------------------------

- Install the required packages

  - msys2 (use the 'MSYS2 MINGW64' entry in the Start menu)

    `pacman -S git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
    mingw-w64-x86_64-physfs mingw-w64-x86_64-SDL mingw-w64-x86_64-SDL_mixer
    mingw-w64-x86_64-libpng mingw-w64-x86_64-glew`

  - Debian/Ubuntu

    `apt install build-essential git cmake libphysfs-dev libsdl1.2-dev libsdl-mixer1.2-dev libpng-dev
    libglew-dev`

  - Fedora

    `dnf install make gcc-c++ git cmake physfs-devel sdl12-compat-devel SDL_mixer-devel libpng-devel
    glew-devel`

  - Arch Linux

    `pacman -S base-devel git cmake physfs sdl12-compat sdl_mixer libpng glew`

- Get the source code from github

  `git clone https://github.com/dxx-redux/dxx-redux`

- Enter the dxx-redux directory

  `cd dxx-redux`

- Enter the the d1 or d2 directory

  `cd d1`

  Or stay at the repo root and use the helper script:

  `./run-linux-build.sh --target d1`

- Set the build options

  `cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`

  (see `cmake -B build -L` for more options)

- Build the code

  `cmake --build build`

  (add `-j4` to use 4 cores)

Linux native tests
------------------

Install the native development packages above and CMake 3.21 or newer. Ninja is
optional; Python 3 is required for the CLI regression suite and the PowerShell
native-test wrapper. The first configure downloads pinned source dependencies,
so it needs network access. PowerShell, the Android SDK and game media are not
required for this Bash/CTest workflow

From the repository root, build both games and run their asset-free native tests:

```sh
./run-linux-build.sh --test
./run-linux-build.sh --test --target d2 --jobs 2
```

Test mode builds all desktop, headless and test targets in `buildd1-test` and
`buildd2-test`, leaving the normal build trees and replay freshness stamps alone.
It enables OpenGL, SDL_mixer and UDP and uses Debug with reduced `-g1` debug
information. An explicit non-Debug `--build-type` is rejected so test assertions
stay active. Both single- and multi-configuration generators select Debug

Builds use four parallel jobs by default in test mode. CTest runs serially with
dummy SDL audio/video drivers, a 120-second per-test timeout and failure output.
Finding no tests is an error. Each tree contains `ctest-results.xml` and
`Testing/Temporary/` diagnostics. No retail game files or replay corpus are needed;
these component and integration tests do not replace full-game demo replays

Use `--sanitizer address --test` for separate `buildd1-test-asan` and
`buildd2-test-asan` trees. `--clean` deletes only the selected trees. `--cmake PATH`
selects CMake and prefers its sibling CTest; PATH is the fallback for CTest.
`--test --list-tools` prints the selected tools without building

Without `--test`, the existing defaults remain `RelWithDebInfo`, `buildd1` and
`buildd2`, with unbounded build parallelism unless `--jobs N` is supplied and no
CTest execution. See [.github/CI.md](.github/CI.md) for CI coverage and limits
