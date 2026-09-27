#!/usr/bin/env bash
# Exercise the real installer against small local archives, without network access
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"
TEST_ROOT="$(create_temp_dir jdk-install "$REPO_ROOT/android/temp")"
trap 'rm -rf "$TEST_ROOT"' EXIT
FIXTURE="$TEST_ROOT/repository with spaces"
HELPERS="$FIXTURE/android/get_deps/helpers"
HOST_OS="$(get_host_os)"
export FIXTURE_ARCHIVE="$TEST_ROOT/archive.tar.gz"
if [ "$HOST_OS" = windows ]; then FIXTURE_ARCHIVE="$TEST_ROOT/archive.zip"; fi
export FIXTURE_URL_LOG="$TEST_ROOT/url.txt"
export TMPDIR="$TEST_ROOT/downloads"
mkdir -p "$HELPERS" "$TMPDIR" "$TEST_ROOT/local/jdk-21/bin" "$TEST_ROOT/archive/jdk-21.2.3+11/bin"
printf '%s\n' "$TEST_ROOT/local" >"$FIXTURE/dependency_base.txt"
for helper in get_jdk.sh platform.sh resolve_dep_base.sh verify_sha256.sh assert_install_not_in_use.ps1; do
    cp "$REPO_ROOT/android/get_deps/helpers/$helper" "$HELPERS/"
done
cat >>"$HELPERS/platform.sh" <<'MOCK'
df() { printf 'Filesystem 1024-blocks Used Available Capacity Mounted on\nfixture 67108864 0 67108864 0%% /\n'; }
download_file() {
    printf '%s\n' "$2" > "$FIXTURE_URL_LOG"
    cp "$FIXTURE_ARCHIVE" "$1"
    [ "${FIXTURE_DOWNLOAD_FAIL:-0}" = 0 ]
}
mv() {
    case "${FIXTURE_MOVE_FAIL:-}:$1" in
        backup:*/local/jdk-21 | publish:*/install/jdk-21.2.3+11) return 1 ;;
    esac
    command mv "$@"
}
MOCK
printf 'JAVA_VERSION="21.0.1"\n' >"$TEST_ROOT/local/jdk-21/release"
printf '#!/bin/sh\necho old-jdk\n' >"$TEST_ROOT/local/jdk-21/bin/java"
chmod +x "$TEST_ROOT/local/jdk-21/bin/java"
printf 'JAVA_VERSION="21.2.3"\nJAVA_RUNTIME_VERSION="21.2.3+11-LTS"\n' >"$TEST_ROOT/archive/jdk-21.2.3+11/release"
printf '#!/bin/sh\necho fixture-jdk\n' >"$TEST_ROOT/archive/jdk-21.2.3+11/bin/java"
chmod +x "$TEST_ROOT/archive/jdk-21.2.3+11/bin/java"
if [ "$HOST_OS" = windows ]; then
    (cd "$TEST_ROOT/archive" && python -m zipfile -c "$FIXTURE_ARCHIVE" jdk-21.2.3+11)
else
    tar -czf "$FIXTURE_ARCHIVE" -C "$TEST_ROOT/archive" jdk-21.2.3+11
fi
HASH="$(sha256sum "$FIXTURE_ARCHIVE" | cut -d ' ' -f1)"
write_conf() {
    cat >"$FIXTURE/android/get_deps/tool_versions.conf" <<CONF
JDK_MAJOR=21
JDK_VERSION=21.2.3
JDK_BUILD=11
JDK_LINUX_SHA256=$1
JDK_WINDOWS_SHA256=$1
JDK_MAC_SHA256=$1
JDK_URL=unused
CONF
}
assert_clean() {
    local entry
    for entry in "$TEST_ROOT/local"/*; do [ ! -f "$entry" ]; done
    [ -z "$(ls -A "$TMPDIR")" ]
    for entry in "$TEST_ROOT/local"/.jdk-* "$TEST_ROOT/local"/.dxx-tool-stage.* "$TEST_ROOT/local"/.dxx-install-backup.*; do
        [ ! -e "$entry" ]
    done
}
write_conf "$HASH"
export GET_ALL_RUNNING=1
if FIXTURE_DOWNLOAD_FAIL=1 bash "$HELPERS/get_jdk.sh" >"$TEST_ROOT/result.log" 2>&1; then
    echo 'Failed download was accepted' >&2
    exit 1
fi
assert_clean
[ "$("$TEST_ROOT/local/jdk-21/bin/java")" = old-jdk ]
write_conf "$(printf '%064d' 0)"
if bash "$HELPERS/get_jdk.sh" >"$TEST_ROOT/result.log" 2>&1; then
    echo 'Wrong checksum was accepted' >&2
    exit 1
fi
assert_clean
[ "$("$TEST_ROOT/local/jdk-21/bin/java")" = old-jdk ]
write_conf "$HASH"
for failure in backup publish; do
    if FIXTURE_MOVE_FAIL="$failure" bash "$HELPERS/get_jdk.sh" >"$TEST_ROOT/result.log" 2>&1; then
        echo "Failed $failure move was accepted" >&2
        exit 1
    fi
    if [ "$failure" = backup ]; then
        grep -q 'close applications using it' "$TEST_ROOT/result.log"
    fi
    assert_clean
    [ "$("$TEST_ROOT/local/jdk-21/bin/java")" = old-jdk ]
done
bash "$HELPERS/get_jdk.sh"
assert_clean
[ "$("$TEST_ROOT/local/jdk-21/bin/java")" = fixture-jdk ]
[ "$(cat "$FIXTURE_URL_LOG")" = "$(get_jdk_download_url 21 21.2.3 11)" ]
rm "$FIXTURE_URL_LOG"
bash "$HELPERS/get_jdk.sh"
[ ! -f "$FIXTURE_URL_LOG" ]
assert_clean
echo "PASS ($HOST_OS): pinned JDK install, verification, rollback, retry, idempotence, and failure cleanup"
