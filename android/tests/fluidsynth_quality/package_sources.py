"""Package the modified LGPL library, GCEM, notices and offline rebuild inputs.

Run after configuring a build that includes cmake/fluidsynth-music.cmake.
Publish the output beside APK/AAB releases containing libfluidsynth.so.
"""
import argparse
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fluid-source', type=Path, required=True)
    parser.add_argument('--gcem-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    pins = dict(line.split('=', 1) for line in (
        root / 'android/get_deps/tool_versions.conf').read_text().splitlines()
        if line.startswith(('FLUIDSYNTH_', 'GCEM_')))
    manifest = ''.join(f'{key}={value}\n' for key, value in pins.items())
    cmake = '''cmake_minimum_required(VERSION 3.24)
project(dxx-fluidsynth-source C CXX)
set(FETCHCONTENT_SOURCE_DIR_FLUID "${CMAKE_CURRENT_LIST_DIR}/fluidsynth")
set(FETCHCONTENT_SOURCE_DIR_GCEM "${CMAKE_CURRENT_LIST_DIR}/gcem")
include(${CMAKE_CURRENT_LIST_DIR}/cmake/fluidsynth-music.cmake)
add_custom_target(dxx_fluidsynth ALL DEPENDS libfluidsynth)
'''
    readme = f'''Corresponding source: FluidSynth {pins['FLUIDSYNTH_VERSION']}, as used by DXX-Redux

Includes the actual modified library source and Apache-2.0 GCEM headers.
See fluidsynth/LICENSE and gcem/LICENSE. Preserve these notices.
The application dynamically links the separate library. Its source and build
files permit rebuilding the APK with a modified library; no game ROMs needed.

Desktop (CMake 3.24+, C/C++ compiler):
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --config Release --target dxx_fluidsynth

Android (NDK r30, CMake 3.31.6, Ninja; replace the NDK path):
  cmake -S . -B build-android -G Ninja -DCMAKE_TOOLCHAIN_FILE=/ndk/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release
  cmake --build build-android --target dxx_fluidsynth
Use armeabi-v7a or x86_64 for the other supported ABIs.

No network downloads are required. The CMake file documents every build option
and the numeric cast patch (already applied in this source tree).
To rebuild the app with modified sources, pass FETCHCONTENT_SOURCE_DIR_FLUID
and FETCHCONTENT_SOURCE_DIR_GCEM to its Android CMake build, build assembleDebug
with JDK 21, and install the resulting APK. See the app repository's Android
build scripts for SDK/tool setup. A rebuilt APK can be signed with your own key.
'''
    assert (args.fluid_source / 'LICENSE').is_file()
    assert (args.gcem_source / 'LICENSE').is_file()
    assert 'static_cast<float>(std::numeric_limits<T>::max())' in (
        args.fluid_source / 'src/drivers/fluid_audio_convert.h').read_text()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as archive:
        def add(name, data):
            entry = zipfile.ZipInfo(name)
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
        for directory, prefix in ((args.fluid_source, 'fluidsynth'), (args.gcem_source, 'gcem')):
            for path in sorted(directory.rglob('*')):
                if path.is_file() and '.git' not in path.relative_to(directory).parts:
                    add(prefix + '/' + path.relative_to(directory).as_posix(), path.read_bytes())
        add('CMakeLists.txt', cmake.encode())
        add('README.txt', readme.encode())
        for name in ('fluidsynth-music.cmake', 'dxx-verified-dependencies.cmake'):
            add('cmake/' + name, (root / 'cmake' / name).read_bytes())
        add('android/get_deps/tool_versions.conf', manifest.encode())
    print(args.output)


if __name__ == '__main__':
    main()
