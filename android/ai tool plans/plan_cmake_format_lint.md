# cmake-format and cmake-lint integration

Add cmake_format (cheshirekow/cmakelang) formatting and linting in the same style as
clang-format / ktlint / shfmt. Apply only to cmake files added on the cmake branch
(android/, cmake/, tools/etc2tool/), never to files in d1/ or d2/.

## In-scope cmake files
- android/app/src/main/cpp/CMakeLists.txt
- android/app/src/main/cpp/extract/CMakeLists.txt
- cmake/input-demo-build-metadata.cmake
- cmake/input-demo-codec-deps.cmake
- cmake/ndk-auto.cmake
- cmake/vcpkg-auto.cmake
- tools/etc2tool/CMakeLists.txt

## Phases
1. [x] Plan file
2. [x] tool_versions.conf entries (CMAKELANG_VERSION, PYTHON_EMBED_VERSION)
3. [x] get_cmake_format.sh: download embeddable Python on Windows; pip install cmakelang into a per-version DEST
4. [x] .cmake-format.yaml at repo root with line_width=100, tab_size=4, dangle_parens=False, autosort=False
5. [x] run-cmake-format.ps1 (format/check) scoped to in-scope files
6. [x] run-cmake-lint.ps1 (lint only) scoped to in-scope files
7. [x] Hook into run-code-quality.ps1 (cmake-format then cmake-lint stages)
8. [x] Add to get_all.sh; register cmakelang in check-updates.ps1
9. [x] Run get_cmake_format.sh; run formatter; review diff
10. [x] Run cmake-lint; review issues
11. [x] copilot-instructions: no edit needed (wrapper is unchanged), add helper note in run-code-quality.ps1 docstring
