#!/usr/bin/env bash
# Exercise SDK provisioning through actual entry points without downloads or devices
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"
TEST_ROOT="$REPO_ROOT/android/temp/sdk_provisioning/run_$(pwsh -NoProfile -Command '[guid]::NewGuid().ToString("N")')"
pwsh -NoProfile -File "$REPO_ROOT/android/helpers/retain-recent-artifacts.ps1" -Artifacts "$TEST_ROOT" -DirectoryPrefix run_
mkdir -p "$TEST_ROOT"
exec {FIXTURE_LOCK_FD}>>"$TEST_ROOT/producer.lock"
flock -n "$FIXTURE_LOCK_FD"
cleanup() {
    local status=$?
    if [ "$status" != 0 ] && [ -f "$TEST_ROOT/result.log" ]; then cat "$TEST_ROOT/result.log" >&2; fi
    exec {FIXTURE_LOCK_FD}>&-
    rm -rf "$TEST_ROOT"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
export FIXTURE_ROOT="$TEST_ROOT"
HELPERS="$TEST_ROOT/repository with spaces/android/get_deps/helpers"
mkdir -p "$HELPERS" "$TEST_ROOT/shared tools/android-sdk/cmdline-tools/latest/bin"
printf '%s\n' "$TEST_ROOT/shared tools" >"$HELPERS/../../../dependency_base.txt"
cat >"$HELPERS/../tool_versions.conf" <<'CONF'
COMPILE_SDK=36
BUILD_TOOLS_VERSION=36.0.0
CMAKE_VERSION=3.31.6
EMULATOR_API_LEVEL=34
CONF
for helper in platform.sh resolve_dep_base.sh sdk_tools.sh finalize.sh get_emulator.sh; do
    cp "$REPO_ROOT/android/get_deps/helpers/$helper" "$HELPERS/"
done
cat >>"$HELPERS/platform.sh" <<'MOCK'
get_host_os() { echo "${FIXTURE_HOST:-linux}"; }
df() {
    if [ "${FIXTURE_LOW_SPACE:-0}" = 1 ]; then
        printf 'Filesystem 1024-blocks Used Available Capacity Mounted on\nfixture 100 99 1 99%% /\n'
    else
        printf 'Filesystem 1024-blocks Used Available Capacity Mounted on\nfixture 67108864 0 67108864 0%% /\n'
    fi
}
MOCK
SDK="$TEST_ROOT/shared tools/android-sdk"
export JAVA_HOME="$TEST_ROOT/java"
export GET_ALL_RUNNING=1
cat >"$SDK/cmdline-tools/latest/bin/sdkmanager" <<'MANAGER'
#!/usr/bin/env bash
set -euo pipefail
[ "$ANDROID_HOME" = "$FIXTURE_ROOT/shared tools/android-sdk" ]
[ "$ANDROID_SDK_ROOT" = "$ANDROID_HOME" ]
printf '%s\n' "$@" >>"$FIXTURE_ROOT/calls"
if [ "${FIXTURE_FAIL:-}" = "$1" ]; then exit 23; fi
case "$1" in
--licenses) read -r _answer; exit 0 ;;
--list) printf '  platforms;android-36.0 | fixture\n  platforms;android-34 | fixture\n'; exit 0 ;;
esac
[ "${FIXTURE_NO_PAYLOAD:-0}" != 1 ] || exit 0
for package in "$@"; do
    directory="$ANDROID_HOME/${package//;/\/}"
    mkdir -p "$directory"
    printf metadata >"$directory/package.xml"
    printf metadata >"$directory/source.properties"
    case "$package" in
    emulator)
        printf '#!/bin/sh\nexit 0\n' >"$directory/emulator"
        chmod +x "$directory/emulator"
        ;;
    system-images*) printf image >"$directory/system.img" ;;
    esac
done
MANAGER
chmod +x "$SDK/cmdline-tools/latest/bin/sdkmanager"
run() { bash "$HELPERS/$1" >"$TEST_ROOT/result.log" 2>&1; }
run finalize.sh
printf '%s\n' --licenses --list 'platforms;android-36.0' 'build-tools;36.0.0' platform-tools 'cmake;3.31.6' 'platforms;android-34' >"$TEST_ROOT/expected"
cmp "$TEST_ROOT/expected" "$TEST_ROOT/calls"
for fail in --licenses --list 'platforms;android-36.0'; do
    : >"$TEST_ROOT/calls"
    if FIXTURE_FAIL="$fail" run finalize.sh; then
        echo "Accepted SDK failure $fail" >&2
        exit 1
    fi
    [ "$(tail -1 "$TEST_ROOT/calls")" = "$fail" ] || [ "$fail" = 'platforms;android-36.0' ]
done
: >"$TEST_ROOT/calls"
if FIXTURE_LOW_SPACE=1 run finalize.sh; then
    echo 'Accepted low space for SDK packages' >&2
    exit 1
fi
[ "$(tail -1 "$TEST_ROOT/calls")" = --list ]
echo 'PASS: spaced SDK paths, package argument boundaries, API alias, license/list/install failures and disk reserve'
IMAGE="$SDK/system-images/android-34/google_apis/x86_64"
mkdir -p "$IMAGE"
: >"$TEST_ROOT/calls"
run get_emulator.sh
printf '%s\n' emulator 'system-images;android-34;google_apis;x86_64' >"$TEST_ROOT/expected"
cmp "$TEST_ROOT/expected" "$TEST_ROOT/calls"
: >"$TEST_ROOT/calls"
FIXTURE_LOW_SPACE=1 run get_emulator.sh
[ ! -s "$TEST_ROOT/calls" ]
for missing in package.xml source.properties system.img; do
    rm "$IMAGE/$missing"
    : >"$TEST_ROOT/calls"
    if FIXTURE_LOW_SPACE=1 run get_emulator.sh; then
        echo 'Accepted partial image with low space' >&2
        exit 1
    fi
    [ ! -s "$TEST_ROOT/calls" ]
    run get_emulator.sh
    [ "$(cat "$TEST_ROOT/calls")" = 'system-images;android-34;google_apis;x86_64' ]
done
rm "$IMAGE/system.img"
if FIXTURE_NO_PAYLOAD=1 run get_emulator.sh; then
    echo 'Accepted incomplete successful installation' >&2
    exit 1
fi
run get_emulator.sh
# A foreign-host executable cannot satisfy the native emulator cache
mv "$SDK/emulator/emulator" "$SDK/emulator/emulator.exe"
: >"$TEST_ROOT/calls"
run get_emulator.sh
[ "$(cat "$TEST_ROOT/calls")" = emulator ]
# A concurrent SDK tool must honor the command-line installer lock
(
    exec 9>"$SDK/cmdline-tools/.dxx-install-state/lock"
    flock -n 9
    if DXX_DEPENDENCY_LOCK_TIMEOUT_SECONDS=0 run get_emulator.sh; then
        echo 'SDK tool bypassed installer lock' >&2
        exit 1
    fi
)
# Recover an interrupted command-line publication before locating sdkmanager
STATE="$SDK/cmdline-tools/.dxx-install-state"
mkdir "$STATE/work"
printf 'latest\n' >"$STATE/work/destination"
mv "$SDK/cmdline-tools/latest" "$STATE/work/previous"
run get_emulator.sh
[ -x "$SDK/cmdline-tools/latest/bin/sdkmanager" ]
[ ! -e "$STATE/work" ]
echo 'PASS: partial image repair, cached reuse without disk growth, installer locking and recovery'
# Standalone provisioning hands off to retention only after releasing its SDK lock
cat >"$HELPERS/../clean-sdk-packages.ps1" <<'CLEANER'
param([string]$RepoRoot, [switch]$RegisterCurrent, [switch]$Apply)
$ErrorActionPreference = 'Stop'
if (-not $RegisterCurrent -or -not $Apply -or -not (Test-Path -LiteralPath (Join-Path $RepoRoot 'dependency_base.txt'))) { throw 'Incorrect retention invocation' }
$base = (Get-Content -LiteralPath (Join-Path $RepoRoot 'dependency_base.txt') -Raw).Trim()
$lock = [IO.File]::Open((Join-Path $base 'android-sdk/cmdline-tools/.dxx-install-state/lock'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try { [IO.File]::AppendAllText((Join-Path $env:FIXTURE_ROOT 'retention-calls'), "retained`n") }
finally { $lock.Dispose() }
if ($env:FIXTURE_RETENTION_FAIL -eq '1') { exit 37 }
CLEANER
GET_ALL_RUNNING='' run finalize.sh
GET_ALL_RUNNING='' run get_emulator.sh
[ "$(wc -l <"$TEST_ROOT/retention-calls")" -eq 2 ]
if GET_ALL_RUNNING='' FIXTURE_RETENTION_FAIL=1 run get_emulator.sh; then
    echo 'Ignored retention failure' >&2
    exit 1
fi
[ "$(wc -l <"$TEST_ROOT/retention-calls")" -eq 3 ]
rm "$SDK/build-tools/36.0.0/source.properties"
if GET_ALL_RUNNING='' FIXTURE_NO_PAYLOAD=1 run finalize.sh; then
    echo 'Accepted missing package metadata' >&2
    exit 1
fi
[ "$(wc -l <"$TEST_ROOT/retention-calls")" -eq 3 ]
GET_ALL_RUNNING='' FIXTURE_FAIL=--licenses run finalize.sh && {
    echo 'Ignored license failure before retention' >&2
    exit 1
}
[ "$(wc -l <"$TEST_ROOT/retention-calls")" -eq 3 ]
run finalize.sh
[ "$(wc -l <"$TEST_ROOT/retention-calls")" -eq 3 ]
echo 'PASS: standalone retention handoff, shared lock release, failure propagation and full-bootstrap deferral'

# The Windows shell branch uses the real writer wrapper with this fixture checkout
export DXX_TEST_SDK_WRAPPER="$REPO_ROOT/android/get_deps/helpers/invoke_sdk_writer.ps1"
cat >"$HELPERS/invoke_sdk_writer.ps1" <<'WRAPPER'
param($RepoRoot, $ScriptName)
& $env:DXX_TEST_SDK_WRAPPER -RepoRoot $RepoRoot -ScriptName $ScriptName
exit $LASTEXITCODE
WRAPPER
# Both names may exist; the actual host must determine which wrapper runs
cp "$SDK/cmdline-tools/latest/bin/sdkmanager" "$SDK/cmdline-tools/latest/bin/sdkmanager.bat"
printf '#!/bin/sh\nexit 99\n' >"$SDK/cmdline-tools/latest/bin/sdkmanager"
mkdir "$STATE/work"
printf 'latest\n' >"$STATE/work/destination"
mv "$SDK/cmdline-tools/latest" "$STATE/work/previous"
FIXTURE_HOST=windows run finalize.sh
[ -f "$SDK/cmdline-tools/latest/bin/sdkmanager.bat" ]
[ ! -e "$STATE/work" ]
GET_ALL_RUNNING='' FIXTURE_HOST=windows run finalize.sh
[ "$(wc -l <"$TEST_ROOT/retention-calls")" -eq 4 ]
if FIXTURE_HOST=linux run finalize.sh; then
    echo 'Linux selected a Windows wrapper' >&2
    exit 1
fi
echo 'PASS: host-specific SDK executable selection with Windows wrapper fixture'
