#!/usr/bin/env bash
# Integration checks for staged native tool and SDK installation without downloads
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"
TEST_ROOT="$(create_temp_dir archive-install "$REPO_ROOT/android/temp")"
trap 'rm -rf "$TEST_ROOT"' EXIT
FIXTURE="$TEST_ROOT/repository with spaces"
HELPERS="$FIXTURE/android/get_deps/helpers"
export FIXTURE_ARCHIVE="$TEST_ROOT/archive.zip"
export TMPDIR="$TEST_ROOT/downloads"
export GET_ALL_RUNNING=1
mkdir -p "$HELPERS" "$TMPDIR" "$TEST_ROOT/local" "$TEST_ROOT/payload"
printf '%s\n' "$TEST_ROOT/local" >"$FIXTURE/dependency_base.txt"
for helper in get_unar.sh get_dosbox.sh get_soundfont.sh verify_sha256.sh get_ndk.sh get_sdk.sh get_cmake.sh get_shfmt.sh get_shellcheck.sh get_clang_format.sh get_ktlint.sh platform.sh resolve_dep_base.sh; do
    cp "$REPO_ROOT/android/get_deps/helpers/$helper" "$HELPERS/"
done
cat >>"$HELPERS/platform.sh" <<'MOCK'
get_host_os() { echo linux; }
df() {
    if [ "${FIXTURE_LOW_SPACE:-0}" = 1 ]; then
        printf 'Filesystem 1024-blocks Used Available Capacity Mounted on\nfixture 100 99 1 99%% /\n'
    else
        printf 'Filesystem 1024-blocks Used Available Capacity Mounted on\nfixture 67108864 0 67108864 0%% /\n'
    fi
}
download_file() {
    if [ "${FIXTURE_ORPHAN_DOWNLOAD:-0}" = 1 ]; then
        sleep 2 &
        printf '%s\n' "$!" > "$TMPDIR/download-child.pending"
        command mv "$TMPDIR/download-child.pending" "$TMPDIR/download-child"
        wait "$!"
    fi
    if [ "${FIXTURE_HOLD_DOWNLOAD:-0}" = 1 ]; then
        touch "$TMPDIR/download-ready"
        for ((attempt = 0; attempt < 100; attempt++)); do
            [ ! -e "$TMPDIR/download-release" ] || break
            sleep 0.05
        done
    fi
    if [ "${FIXTURE_BAD_ARCHIVE:-0}" = 1 ]; then printf broken > "$1"; return 0; fi
    cp "$FIXTURE_ARCHIVE" "$1"
    if [ "${FIXTURE_CRASH_DOWNLOAD:-0}" = 1 ]; then kill -KILL "$$"; fi
    if [ "${FIXTURE_INTERRUPT:-0}" = 1 ]; then kill -TERM "$$"; fi
    [ "${FIXTURE_DOWNLOAD_FAIL:-0}" = 0 ]
}
mv() {
    if [ "${FIXTURE_PUBLISH_FAIL:-0}" = 1 ]; then
        case "$1" in
            */.ndk-stage.*/*|*/.sdk-extract.*/*|*/.cmake-stage.*/*|*/.dxx-tool-stage.*/install | */.dxx-tool-stage.*/install/* | */.dxx-install-state/work/install | */.dxx-install-state/work/install/* | */.dxx-soundfont.*) return 23 ;;
        esac
    fi
    command mv "$@" || return
    case "${FIXTURE_CRASH_PUBLICATION:-}:$2" in
        backup:*/.dxx-install-state/work/previous | publish:*/local/shfmt-99.0.0) kill -KILL "$$" ;;
    esac
    return 0
}
MOCK
cat >"$FIXTURE/android/get_deps/tool_versions.conf" <<'CONF'
NDK_VERSION=r99
NDK_FULL_VERSION=99.0.0
NDK_URL=unused
SDK_CMDLINE_TOOLS_URL=https://dl.google.com/android/repository/commandlinetools-win-999_latest.zip
CMAKE_VERSION=99.0.0
CMAKE_URL=unused
SHFMT_VERSION=99.0.0
SHELLCHECK_VERSION=99.0.0
CLANG_FORMAT_VERSION=99.0.0
CLANG_FORMAT_RELEASE_TAG=fixture
KTLINT_VERSION=99.0.0
KTLINT_URL=unused
JDK_MAJOR=99
CONF
mkdir -p "$TEST_ROOT/local/jdk-99/bin"
printf '#!/bin/sh\nshift\nexec sh "$@"\n' >"$TEST_ROOT/local/jdk-99/bin/java"
chmod +x "$TEST_ROOT/local/jdk-99/bin/java"
export JAVA_HOME="$TEST_ROOT/local/jdk-99"
assert_clean() {
    [ -z "$(find "$TEST_ROOT/local" -maxdepth 1 -type f -print)" ]
    [ -z "$(ls -A "$TMPDIR")" ]
    [ -z "$(find "$TEST_ROOT/local" -name '.ndk-stage.*' -o -name '.sdk-extract.*' -o -name '.cmake-stage.*' -o -name '.dxx-install-backup.*' -o -name '.dxx-tool-stage.*')" ]
    [ ! -e "$TEST_ROOT/local/.dxx-install-state/work" ]
}
for tool in ndk sdk cmake shfmt shellcheck clang_format ktlint; do
    rm -rf "$TEST_ROOT/payload"
    mkdir -p "$TEST_ROOT/payload"
    case "$tool" in
    ndk)
        payload="$TEST_ROOT/payload/android-ndk-r99"
        destination="$TEST_ROOT/local/android-ndk-r99"
        mkdir -p "$payload/build/cmake"
        printf 'toolchain\n' >"$payload/build/cmake/android.toolchain.cmake"
        printf 'Pkg.Revision = 99.0.0\n' >"$payload/source.properties"
        executable=build/cmake/android.toolchain.cmake
        ;;
    sdk)
        payload="$TEST_ROOT/payload/cmdline-tools"
        destination="$TEST_ROOT/local/android-sdk/cmdline-tools/latest"
        mkdir -p "$payload/bin"
        printf '#!/bin/sh\nexit 0\n' >"$payload/bin/sdkmanager"
        chmod +x "$payload/bin/sdkmanager"
        executable=bin/sdkmanager
        ;;
    cmake)
        payload="$TEST_ROOT/payload/cmake-99.0.0-linux-x86_64"
        destination="$TEST_ROOT/local/cmake-99.0.0"
        mkdir -p "$payload/bin"
        printf '#!/bin/sh\necho "cmake version 99.0.0"\n' >"$payload/bin/cmake"
        chmod +x "$payload/bin/cmake"
        executable=bin/cmake
        ;;
    shellcheck)
        payload="$TEST_ROOT/payload/shellcheck-v99.0.0"
        destination="$TEST_ROOT/local/shellcheck-99.0.0"
        mkdir -p "$payload"
        printf '#!/bin/sh\necho "ShellCheck version 99.0.0"\n' >"$payload/shellcheck"
        chmod +x "$payload/shellcheck"
        executable=shellcheck
        ;;
    shfmt | clang_format | ktlint)
        tool_name="${tool//_/-}"
        destination="$TEST_ROOT/local/$tool_name-99.0.0"
        executable="$tool_name"
        if [ "$tool" = ktlint ]; then executable=ktlint.jar; fi
        printf '#!/bin/sh\necho "fixture version 99.0.0"\n' >"$TEST_ROOT/payload/$executable"
        ;;
    esac
    rm -f "$FIXTURE_ARCHIVE"
    case "$tool" in
    cmake) tar -czf "$FIXTURE_ARCHIVE" -C "$TEST_ROOT/payload" . ;;
    shellcheck) tar -cJf "$FIXTURE_ARCHIVE" -C "$TEST_ROOT/payload" shellcheck-v99.0.0 ;;
    shfmt | clang_format | ktlint) cp "$TEST_ROOT/payload/$executable" "$FIXTURE_ARCHIVE" ;;
    *) (cd "$TEST_ROOT/payload" && zip -qr "$FIXTURE_ARCHIVE" .) ;;
    esac
    mkdir -p "$destination"
    printf 'previous install\n' >"$destination/sentinel"
    for failure in space download archive publish interrupt; do
        export FIXTURE_DOWNLOAD_FAIL=0 FIXTURE_PUBLISH_FAIL=0 FIXTURE_BAD_ARCHIVE=0 FIXTURE_LOW_SPACE=0 FIXTURE_INTERRUPT=0
        case "$failure" in
        space) export FIXTURE_LOW_SPACE=1 ;;
        download) export FIXTURE_DOWNLOAD_FAIL=1 ;;
        archive) export FIXTURE_BAD_ARCHIVE=1 ;;
        publish) export FIXTURE_PUBLISH_FAIL=1 ;;
        interrupt) export FIXTURE_INTERRUPT=1 ;;
        esac
        if bash "$HELPERS/get_$tool.sh" >"$TEST_ROOT/result.log" 2>&1; then
            echo "$tool accepted $failure failure" >&2
            exit 1
        fi
        [ -f "$destination/sentinel" ]
        assert_clean
    done
    export FIXTURE_DOWNLOAD_FAIL=0 FIXTURE_PUBLISH_FAIL=0 FIXTURE_BAD_ARCHIVE=0 FIXTURE_LOW_SPACE=0 FIXTURE_INTERRUPT=0
    bash "$HELPERS/get_$tool.sh"
    [ -f "$destination/$executable" ]
    [ ! -f "$destination/sentinel" ]
    assert_clean
    # A second call must reuse the validated install without needing a download
    FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_$tool.sh"
    assert_clean
    case "$tool" in
    shfmt | shellcheck | clang_format | ktlint)
        # Existing files must not bypass admission merely because their folder is pinned
        printf '#!/bin/sh\necho "version 98.0.0"\n' >"$destination/$executable"
        chmod +x "$destination/$executable"
        if FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_$tool.sh" >"$TEST_ROOT/wrong-version.log" 2>&1; then
            echo "$tool reused a mismatched cached version" >&2
            exit 1
        fi
        [ -f "$destination/$executable" ]
        assert_clean
        cp "$FIXTURE_ARCHIVE" "$TEST_ROOT/good-archive"
        if [ "$tool" = shellcheck ]; then
            printf '#!/bin/sh\necho "version 99.0.0-preview"\n' >"$payload/shellcheck"
            tar -cJf "$FIXTURE_ARCHIVE" -C "$TEST_ROOT/payload" shellcheck-v99.0.0
        else
            printf '#!/bin/sh\necho "version 99.0.0-preview"\n' >"$FIXTURE_ARCHIVE"
        fi
        previous_hash="$(sha256sum "$destination/$executable")"
        if bash "$HELPERS/get_$tool.sh" >"$TEST_ROOT/wrong-download-version.log" 2>&1; then
            echo "$tool published a mismatched downloaded version" >&2
            exit 1
        fi
        [ "$(sha256sum "$destination/$executable")" = "$previous_hash" ]
        assert_clean
        mv "$TEST_ROOT/good-archive" "$FIXTURE_ARCHIVE"
        bash "$HELPERS/get_$tool.sh"
        assert_clean
        ;;
    esac
done
echo 'PASS: SDK, NDK, CMake, and formatter staging, failure rollback, cleanup, and idempotence'

# Single-file assets must preserve their prior contents through failed replacement
asset="$FIXTURE/android/app/src/main/assets/gm.sf2"
mkdir -p "$(dirname "$asset")"
printf 'verified soundfont fixture' >"$FIXTURE_ARCHIVE"
asset_hash="$(sha256sum "$FIXTURE_ARCHIVE" | awk '{print $1}')"
printf '\nSOUNDFONT_VERSION=fixture\nSOUNDFONT_URL=unused\nSOUNDFONT_SHA256=%s\n' "$asset_hash" >>"$FIXTURE/android/get_deps/tool_versions.conf"
printf 'previous soundfont' >"$asset"
for failure in FIXTURE_LOW_SPACE FIXTURE_DOWNLOAD_FAIL FIXTURE_BAD_ARCHIVE FIXTURE_PUBLISH_FAIL FIXTURE_INTERRUPT; do
    if env "$failure=1" bash "$HELPERS/get_soundfont.sh" >"$TEST_ROOT/soundfont.log" 2>&1; then
        echo "Soundfont accepted $failure" >&2
        exit 1
    fi
    [ "$(cat "$asset")" = 'previous soundfont' ]
    if compgen -G "$(dirname "$asset")/.dxx-soundfont.*" >/dev/null; then
        echo 'Soundfont download leaked' >&2
        exit 1
    fi
done
bash "$HELPERS/get_soundfont.sh"
cmp "$asset" "$FIXTURE_ARCHIVE"
FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_soundfont.sh"
echo 'PASS: soundfont failure preservation, verified replacement, cleanup, and repeat run'

# Exercise the actual formatter installer across SIGKILL and publication boundaries
destination="$TEST_ROOT/local/shfmt-99.0.0"
printf '#!/bin/sh\necho "version 99.0.0"\n' >"$FIXTURE_ARCHIVE"
for point in download backup publish; do
    printf '#!/bin/sh\necho "version 98.0.0"\n' >"$destination/shfmt"
    if [ "$point" = download ]; then
        crash=FIXTURE_CRASH_DOWNLOAD=1
    else
        crash="FIXTURE_CRASH_PUBLICATION=$point"
    fi
    if env "$crash" bash "$HELPERS/get_shfmt.sh" >"$TEST_ROOT/crash.log" 2>&1; then
        echo "Installer did not stop at $point" >&2
        exit 1
    fi
    [ -d "$TEST_ROOT/local/.dxx-install-state/work" ]
    # Cached sibling tools must also retire abandoned work in their shared parent
    FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_shellcheck.sh"
    [ ! -e "$TEST_ROOT/local/.dxx-install-state/work" ]
    if [ "$point" = publish ]; then
        FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_shfmt.sh"
    else
        if FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_shfmt.sh" >"$TEST_ROOT/recovery.log" 2>&1; then
            echo 'Failed recovery download unexpectedly succeeded' >&2
            exit 1
        fi
        [ "$("$destination/shfmt")" = 'version 98.0.0' ]
        bash "$HELPERS/get_shfmt.sh"
    fi
    [ "$("$destination/shfmt")" = 'version 99.0.0' ]
    assert_clean
done

# A second installer must not remove an active download or publish over it
printf '#!/bin/sh\necho "version 98.0.0"\n' >"$destination/shfmt"
FIXTURE_HOLD_DOWNLOAD=1 bash "$HELPERS/get_shfmt.sh" >"$TEST_ROOT/first-installer.log" 2>&1 &
installer=$!
for ((attempt = 0; attempt < 100; attempt++)); do
    [ ! -e "$TMPDIR/download-ready" ] || break
    sleep 0.05
done
[ -e "$TMPDIR/download-ready" ]
if DXX_DEPENDENCY_LOCK_TIMEOUT_SECONDS=0 bash "$HELPERS/get_shfmt.sh" >"$TEST_ROOT/second-installer.log" 2>&1; then
    echo 'Concurrent installer bypassed the lock' >&2
    exit 1
fi
[ -d "$TEST_ROOT/local/.dxx-install-state/work" ]
touch "$TMPDIR/download-release"
wait "$installer"
rm "$TMPDIR/download-ready" "$TMPDIR/download-release"
FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_shfmt.sh"
assert_clean
echo 'PASS: installer serialization and SIGKILL recovery before and after publication'

# A surviving download child retains the kernel lock after its Bash owner dies
printf '#!/bin/sh\necho "version 98.0.0"\n' >"$destination/shfmt"
python3 - "$HELPERS/get_shfmt.sh" "$TMPDIR/download-child" <<'PY'
import ctypes
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

assert ctypes.CDLL(None).prctl(36, 1, 0, 0, 0) == 0
script, ready = sys.argv[1], Path(sys.argv[2])
child = None
with subprocess.Popen(['bash', script], env={**os.environ, 'FIXTURE_ORPHAN_DOWNLOAD': '1'},
                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) as owner:
    try:
        deadline = time.monotonic() + 5
        while not ready.exists():
            assert owner.poll() is None and time.monotonic() < deadline, 'download did not start'
            time.sleep(0.01)
        child = int(ready.read_text())
        owner.kill()
        owner.wait(timeout=5)
        blocked = subprocess.run(['bash', script], env={**os.environ, 'DXX_DEPENDENCY_LOCK_TIMEOUT_SECONDS': '0'},
                                 capture_output=True, text=True, timeout=5)
        assert blocked.returncode != 0 and 'another dependency installer' in blocked.stderr, blocked.stderr
        os.waitpid(child, 0)
        child = None
    finally:
        if owner.poll() is None:
            owner.kill()
            owner.wait(timeout=5)
        if child is not None:
            try:
                os.kill(child, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(child, 0)
ready.unlink()
PY
bash "$HELPERS/get_shfmt.sh"
assert_clean
echo 'PASS: surviving download holds the installer lock until its own exit'

# Never follow a substituted transaction directory into unrelated user data
mkdir "$TEST_ROOT/unrelated"
printf 'preserve\n' >"$TEST_ROOT/unrelated/sentinel"
ln -s "$TEST_ROOT/unrelated" "$TEST_ROOT/local/.dxx-install-state/work"
if bash "$HELPERS/get_shfmt.sh" >"$TEST_ROOT/linked-state.log" 2>&1; then
    echo 'Installer accepted a linked transaction directory' >&2
    exit 1
fi
[ "$(cat "$TEST_ROOT/unrelated/sentinel")" = preserve ]
[ -L "$TEST_ROOT/local/.dxx-install-state/work" ]
rm "$TEST_ROOT/local/.dxx-install-state/work"
assert_clean
echo 'PASS: linked transaction state is preserved and rejected'

# Validate Windows archive publication without executing the packaged programs
printf '\nget_host_os() { echo windows; }\n' >>"$HELPERS/platform.sh"
for tool in unar dosbox; do
    rm -rf "$TEST_ROOT/payload"
    mkdir -p "$TEST_ROOT/payload"
    printf 'archive executable fixture' >"$TEST_ROOT/payload/tool.exe"
    exe_hash="$(sha256sum "$TEST_ROOT/payload/tool.exe" | awk '{print $1}')"
    if [ "$tool" = unar ]; then
        mv "$TEST_ROOT/payload/tool.exe" "$TEST_ROOT/payload/unar.exe"
        cp "$TEST_ROOT/payload/unar.exe" "$TEST_ROOT/payload/lsar.exe"
    else
        mkdir -p "$TEST_ROOT/payload/build"
        mv "$TEST_ROOT/payload/tool.exe" "$TEST_ROOT/payload/build/dosbox-x.exe"
    fi
    rm -f "$FIXTURE_ARCHIVE"
    (cd "$TEST_ROOT/payload" && zip -qr "$FIXTURE_ARCHIVE" .)
    archive_hash="$(sha256sum "$FIXTURE_ARCHIVE" | awk '{print $1}')"
    cat >>"$FIXTURE/android/get_deps/tool_versions.conf" <<CONF
UNAR_DIR_NAME=unar-fixture
UNAR_URL=unused
UNAR_ARCHIVE_SHA256=$archive_hash
UNAR_EXE_SHA256=$exe_hash
LSAR_EXE_SHA256=$exe_hash
DOSBOX_VERSION=fixture
DOSBOX_DIR_NAME=dosbox-fixture
DOSBOX_URL=unused
DOSBOX_ARCHIVE_SHA256=$archive_hash
DOSBOX_EXE_SHA256=$exe_hash
DOSBOX_EXE_RELATIVE_PATH=build/dosbox-x.exe
CONF
    destination="$TEST_ROOT/local/$tool-fixture"
    mkdir -p "$destination"
    printf 'previous archive installation' >"$destination/sentinel"
    for failure in FIXTURE_LOW_SPACE FIXTURE_DOWNLOAD_FAIL FIXTURE_BAD_ARCHIVE FIXTURE_PUBLISH_FAIL FIXTURE_INTERRUPT; do
        if env "$failure=1" bash "$HELPERS/get_$tool.sh" >"$TEST_ROOT/$tool.log" 2>&1; then
            echo "$tool accepted $failure" >&2
            exit 1
        fi
        [ "$(cat "$destination/sentinel")" = 'previous archive installation' ]
        assert_clean
    done
    bash "$HELPERS/get_$tool.sh"
    [ ! -e "$destination/sentinel" ]
    FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_$tool.sh"
    assert_clean
done
echo 'PASS: Windows unar and DOSBox archive staging, validation, rollback, and cleanup'
