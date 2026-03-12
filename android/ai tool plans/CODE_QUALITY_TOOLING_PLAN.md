# Plan: Code Quality Tooling Scripts

Add install scripts, format runners, and warning gatherers for C/C++ (clang-format)
and Kotlin (ktlint), scoped to the android/ folder. Follow existing get_deps/ conventions.

## Phase 1: clang-format for C/C++ (android/ only)

### 1.1 Pin version in tool_versions.conf
- Add CLANG_FORMAT_VERSION=19.1.7
- Add CLANG_FORMAT_URL pointing to the pre-built llvm-project GitHub release
  (clang+llvm-19.1.7-x86_64-pc-windows-msvc.tar.xz from llvm/llvm-project releases
   is ~700MB; instead use the standalone clang-format from muttleyxd/clang-tools-extra-binaries
   or extract just clang-format.exe from the official LLVM installer)
- Final decision: use muttleyxd/clang-tools-static-binaries GitHub releases -- single ~3MB binary

### 1.2 Create android/get_deps/get_clang_format.sh
- Follows existing pattern: source tool_versions.conf + resolve_dep_base.sh
- Downloads clang-format binary to $LOCAL_DIR/clang-format-$VERSION/
- Idempotent: skips if clang-format.exe already present
- Wait-for-key at end (unless GET_ALL_RUNNING)

### 1.3 Create .clang-format at repo root
Matches observed android/ C code style:
- BasedOnStyle: LLVM (closest match)
- IndentWidth: 4 (tabs in d1/d2 but 4-space in android/)
- TabWidth: 4, UseTab: ForIndentation in d1/d2 but Never in android/
- Brace style: Allman-ish (function opening brace on new line in some places, same line in others -- use Attach/Linux style as most android/ code uses it)
- ColumnLimit: 100
- AllowShortFunctionsOnASingleLine: Empty
- PointerAlignment: Right (int *p style)
- SpaceBeforeParens: ControlStatements
- AlignConsecutiveMacros: true

### 1.4 Create android/run-clang-format.ps1
- Finds clang-format.exe from $DEP_BASE/clang-format-$VERSION/ or PATH
- Targets: android/app/src/main/cpp/**/*.{c,cpp,h}
- EXCLUDES SDL patches: SDL_androidaudio.c, SDL_androidaudio.h, SDL_config_android.h
- Default mode: format in-place
- --check mode: dry-run, exit 1 if diffs exist (for CI)
- Reports file count and changes

## Phase 2: Kotlin formatting (ktlint)

### 2.1 Pin ktlint version in tool_versions.conf
- Add KTLINT_VERSION=1.5.0
- Add KTLINT_URL for the GitHub release jar

### 2.2 Create android/get_deps/get_ktlint.sh
- Downloads ktlint jar to $LOCAL_DIR/ktlint-$VERSION/
- Uses JDK from $LOCAL_DIR/jdk-$JDK_MAJOR for java command
- Idempotent

### 2.3 Create .editorconfig at repo root
- Standard Kotlin settings: indent_size=4, max_line_length=120
- ktlint reads .editorconfig natively

### 2.4 Create android/run-ktlint.ps1
- Finds ktlint jar + java from $DEP_BASE
- Runs on android/app/src/main/java/**/*.kt
- --check mode (default): report issues, exit 1 if any
- --format mode: auto-fix

## Phase 3: Compiler warning gathering

### 3.1 Create android/gather-warnings.ps1
- Runs gradlew assembleDebug, captures all output
- Filters for ": warning:" lines (C/C++/clang) and Kotlin warnings
- Writes to temp/warnings-YYYY-MM-DD.log
- Gathers ALL warnings (including d1/d2 sources compiled via NDK)
- Instructions to fixer: do not touch d1/d2 code
- Supports --native-only, --kotlin-only flags

### 3.2 Create android/gather-warnings-msvc.ps1
- Runs cmake --build for d1 and d2 Windows builds
- Captures warnings to temp/warnings-msvc-YYYY-MM-DD.log
- Uses /m for parallel and captures warning lines
- Includes all warnings (d1/d2 -- the fixer instructions say don't modify d1/d2)

## Phase 4: Unified runner

### 4.1 Create android/run-code-quality.ps1
- Runs clang-format check + ktlint check
- Reports pass/fail for each
- --fix: auto-format both

## Files to create
- android/get_deps/get_clang_format.sh
- android/get_deps/get_ktlint.sh
- android/run-clang-format.ps1
- android/run-ktlint.ps1
- android/gather-warnings.ps1
- android/gather-warnings-msvc.ps1
- android/run-code-quality.ps1
- .clang-format
- .editorconfig

## Files to modify
- android/get_deps/tool_versions.conf -- add version pins
- android/get_deps/get_all.sh -- add clang-format + ktlint steps

## Verification
- Run get_clang_format.sh -- binary appears in $DEP_BASE
- Run run-clang-format.ps1 --check -- scans android/ C files
- Run get_ktlint.sh -- jar appears in $DEP_BASE
- Run run-ktlint.ps1 --check -- scans all .kt files
- Run gather-warnings.ps1 -- log file in temp/
