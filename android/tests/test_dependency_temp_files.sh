#!/bin/bash
# Verify that dependency bootstrap temporary paths work with BSD and GNU mktemp
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck disable=SC1091
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"

TEST_ROOT="$REPO_ROOT/android/temp/dependency temp test.$$"
TMPDIR="$TEST_ROOT/default temp"
CUSTOM_PARENT="$TEST_ROOT/custom stage"
mkdir -p "$TMPDIR" "$CUSTOM_PARENT"
trap 'rm -rf "$TEST_ROOT"' EXIT

first="$(create_temp_file repeated)"
second="$(create_temp_file repeated)"
directory="$(create_temp_dir extracted)"
[ "$first" != "$second" ]
[ -f "$first" ]
[ -f "$second" ]
[ -d "$directory" ]

# Model stock macOS mktemp: no GNU -p and the X run must end the template
mktemp() {
    local make_directory=0
    local template
    local output

    if [ "${1:-}" = "-d" ]; then
        make_directory=1
        shift
    fi
    if [ "$#" -ne 1 ]; then
        echo "BSD mktemp fixture received unsupported arguments: $*" >&2
        return 64
    fi
    template="$1"
    case "$template" in
    *XXXXXX) ;;
    *)
        echo "BSD mktemp fixture requires a final XXXXXX run: $template" >&2
        return 64
        ;;
    esac
    output="${template%XXXXXX}ABC123"
    if [ "$make_directory" -eq 1 ]; then
        (umask 077 && mkdir "$output")
    else
        (umask 077 && : >"$output")
    fi
    printf '%s\n' "$output"
}

mac_file="$(create_temp_file sdk.zip)"
mac_directory="$(create_temp_dir sdk-extract)"
staged_directory="$(create_temp_dir .jdk-21-stage "$CUSTOM_PARENT")"
[ "$mac_file" = "$TMPDIR/sdk.zip.ABC123" ]
[ "$mac_directory" = "$TMPDIR/sdk-extract.ABC123" ]
[ "$staged_directory" = "$CUSTOM_PARENT/.jdk-21-stage.ABC123" ]
[ -f "$mac_file" ]
[ -d "$mac_directory" ]
[ -d "$staged_directory" ]

for helper in "$REPO_ROOT"/android/get_deps/helpers/*.sh; do
    if [ "$helper" != "$REPO_ROOT/android/get_deps/helpers/platform.sh" ] && grep -q 'mktemp' "$helper"; then
        echo "Dependency helper bypasses portable temporary creation: $helper" >&2
        exit 1
    fi
done

echo "PASS: dependency temporary files use portable final-X templates"

# A caller may inspect failure in an if statement, disabling Bash errexit
# The shared download helpers must still propagate the transport exit status
curl() { return 23; }
if download_file "$TEST_ROOT/failed-download" https://fixture.invalid/archive; then
    echo 'Download helper swallowed curl failure' >&2
    exit 1
fi
if download_text https://fixture.invalid/metadata; then
    echo 'Metadata download helper swallowed curl failure' >&2
    exit 1
fi
unset -f curl
echo 'PASS: transport errors propagate through conditional callers'

# Large archives get a caller-specific bounded deadline, not a five-minute restart loop
curl() {
    local arguments=" $* "
    [[ "$arguments" == *" --max-time 1800 "* ]] || return 99
    [[ "$arguments" == *" --retry-max-time 3610 "* ]] || return 99
    return 23
}
if download_file "$TEST_ROOT/large-download" https://fixture.invalid/large 1800; then
    echo 'Large download helper swallowed transport failure' >&2
    exit 1
else
    [ "$?" -eq 23 ]
fi
for deadline in 0 -1 invalid 1801; do
    if download_file "$TEST_ROOT/invalid-deadline" https://fixture.invalid/archive "$deadline"; then
        echo 'Download helper accepted an invalid deadline' >&2
        exit 1
    else
        [ "$?" -eq 2 ]
    fi
done
unset -f curl
echo 'PASS: large-archive deadlines stay bounded and preserve transport errors'

# Persisted entry points: no implicit installation, child-only environment, exact failures
fixture="$TEST_ROOT/tool wrapper repo"
deps="$fixture/android/get_deps"
mkdir -p "$deps/helpers" "$fixture/android/helpers"
cp "$REPO_ROOT/android/get_deps/"{build_tools.sh,check_build_prereqs.sh,tool_versions.conf} "$deps/"
cp "$REPO_ROOT/android/get_deps/helpers/"{platform.sh,resolve_dep_base.sh} "$deps/helpers/"
cp "$REPO_ROOT/android/helpers/"{set_vars.sh,run_android_native_smoke.sh} "$fixture/android/helpers/"
export DXX_DEPENDENCY_BASE="$TEST_ROOT/managed tools"
if bash "$deps/build_tools.sh" >"$TEST_ROOT/check.log" 2>&1; then exit 1; fi
[ ! -e "$DXX_DEPENDENCY_BASE" ] && [ ! -e "$fixture/dependency_base.txt" ]
# Installers are disposable shims; the actual resolver and checker remain in use
for installer in get_jdk get_sdk get_ndk get_cmake get_gradle_wrapper; do
    printf '#!/bin/bash\nexit 23\n' >"$deps/helpers/$installer.sh"
done
if bash "$deps/build_tools.sh" --install >"$TEST_ROOT/install.log" 2>&1; then exit 1; else [ "$?" -eq 23 ]; fi
printf '#!/bin/bash\nsleep 20\n' >"$deps/helpers/get_jdk.sh"
if DXX_INSTALL_TIMEOUT_SECONDS=1 bash "$deps/build_tools.sh" --install >"$TEST_ROOT/timeout.log" 2>&1; then exit 1; else [ "$?" -eq 124 ]; fi
source "$deps/tool_versions.conf"
managed="$DXX_DEPENDENCY_BASE"
# Versions are loaded from the copied manifest above
# shellcheck disable=SC2153
mkdir -p "$managed/jdk-$JDK_MAJOR/bin" "$managed/android-sdk/cmdline-tools/latest/bin" \
    "$managed/android-ndk-$NDK_VERSION" "$managed/cmake-$CMAKE_VERSION/bin" "$fixture/android/gradle/wrapper"
printf java >"$managed/jdk-$JDK_MAJOR/bin/java"
printf 'JAVA_VERSION="%s"\n' "$JDK_VERSION" >"$managed/jdk-$JDK_MAJOR/release"
printf sdkmanager >"$managed/android-sdk/cmdline-tools/latest/bin/sdkmanager"
get_sdk_cmdline_tools_build_id "$SDK_CMDLINE_TOOLS_URL" >"$managed/android-sdk/cmdline-tools/latest/.dxx-cmdline-tools-build-id"
printf 'Pkg.Revision = %s\n' "$NDK_FULL_VERSION" >"$managed/android-ndk-$NDK_VERSION/source.properties"
printf wrapper >"$fixture/android/gradle/wrapper/gradle-wrapper.jar"
cat >"$managed/cmake-$CMAKE_VERSION/bin/cmake" <<'CMAKE'
#!/bin/bash
if [ "$1" = --version ]; then echo "cmake version $FIXTURE_CMAKE_VERSION"; exit 0; fi
[ -z "${CPATH:-}${LIBRARY_PATH:-}${CMAKE_PREFIX_PATH:-}${CC:-}${CXX:-}${LDFLAGS:-}" ] || exit 99
printf '%s\n' "$1" >>"$FIXTURE_CMAKE_CALLS"
if [ "${FIXTURE_CMAKE_FAIL:-}" = "$1" ]; then exit 23; fi
CMAKE
chmod +x "$managed/cmake-$CMAKE_VERSION/bin/cmake"
export FIXTURE_CMAKE_VERSION="$CMAKE_VERSION" FIXTURE_CMAKE_CALLS="$TEST_ROOT/cmake.calls"
bash "$deps/build_tools.sh" >"$TEST_ROOT/check.log"
printf '%s\n' "$managed" >"$fixture/dependency_base.txt"
if DXX_DEPENDENCY_BASE="$TEST_ROOT/conflicting" bash "$deps/build_tools.sh" >"$TEST_ROOT/conflict.log" 2>&1; then exit 1; else [ "$?" -eq 2 ]; fi
[ "$(cat "$fixture/dependency_base.txt")" = "$managed" ]
if FIXTURE_CMAKE_VERSION=wrong bash "$deps/build_tools.sh" >"$TEST_ROOT/version.log" 2>&1; then exit 1; fi
if JAVA_HOME=/wrong bash "$deps/build_tools.sh" --run -- bash -c '[ "$JAVA_HOME" = "$DXX_DEPENDENCY_BASE/jdk-'"$JDK_MAJOR"'" ] || exit 99; exit 37' >"$TEST_ROOT/run.log"; then exit 1; else [ "$?" -eq 37 ]; fi
if bash "$deps/check_build_prereqs.sh" >"$TEST_ROOT/apk.log" 2>&1; then exit 1; fi
export CPATH=host LIBRARY_PATH=host CMAKE_PREFIX_PATH=host CC=host CXX=host LDFLAGS=host
bash "$fixture/android/helpers/run_android_native_smoke.sh" --build-dir "$TEST_ROOT/smoke" >"$TEST_ROOT/smoke.log"
printf '%s\n' -S --build >"$TEST_ROOT/expected.calls"
cmp "$TEST_ROOT/expected.calls" "$FIXTURE_CMAKE_CALLS"
: >"$FIXTURE_CMAKE_CALLS"
if FIXTURE_CMAKE_FAIL=-S bash "$fixture/android/helpers/run_android_native_smoke.sh" --build-dir "$TEST_ROOT/smoke" >"$TEST_ROOT/smoke.log"; then exit 1; else [ "$?" -eq 23 ]; fi
[ "$(cat "$FIXTURE_CMAKE_CALLS")" = -S ]
echo 'PASS: tool wrapper stays read-only by default, bounds installation, and preserves child failures'
