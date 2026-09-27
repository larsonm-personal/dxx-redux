#!/usr/bin/env bash
# Exercise the real installer against small local archives, without network access
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"
TEST_ROOT="$(create_temp_dir jdk-install "$REPO_ROOT/android/temp")"
trap 'rm -rf "$TEST_ROOT"' EXIT
FIXTURE="$TEST_ROOT/repository with spaces"
HELPERS="$FIXTURE/android/get_deps/helpers"
export FIXTURE_ARCHIVE="$TEST_ROOT/archive.tar.gz"
export FIXTURE_URL_LOG="$TEST_ROOT/url.txt"
export TMPDIR="$TEST_ROOT/downloads"
mkdir -p "$HELPERS" "$TMPDIR" "$TEST_ROOT/local/jdk-21/bin" "$TEST_ROOT/archive/jdk-21.2.3+11/bin"
printf '%s\n' "$TEST_ROOT/local" >"$FIXTURE/dependency_base.txt"
for helper in get_jdk.sh platform.sh resolve_dep_base.sh verify_sha256.sh; do
    cp "$REPO_ROOT/android/get_deps/helpers/$helper" "$HELPERS/"
done
cat >>"$HELPERS/platform.sh" <<'MOCK'
get_host_os() { echo linux; }
df() { printf 'Filesystem 1024-blocks Used Available Capacity Mounted on\nfixture 67108864 0 67108864 0%% /\n'; }
download_file() {
    printf '%s\n' "$2" > "$FIXTURE_URL_LOG"
    cp "$FIXTURE_ARCHIVE" "$1"
    [ "${FIXTURE_DOWNLOAD_FAIL:-0}" = 0 ]
}
MOCK
printf 'JAVA_VERSION="21.0.1"\n' >"$TEST_ROOT/local/jdk-21/release"
printf '#!/bin/sh\necho old-jdk\n' >"$TEST_ROOT/local/jdk-21/bin/java"
chmod +x "$TEST_ROOT/local/jdk-21/bin/java"
printf 'JAVA_VERSION="21.2.3"\nJAVA_RUNTIME_VERSION="21.2.3+11-LTS"\n' >"$TEST_ROOT/archive/jdk-21.2.3+11/release"
printf '#!/bin/sh\necho fixture-jdk\n' >"$TEST_ROOT/archive/jdk-21.2.3+11/bin/java"
chmod +x "$TEST_ROOT/archive/jdk-21.2.3+11/bin/java"
tar -czf "$FIXTURE_ARCHIVE" -C "$TEST_ROOT/archive" jdk-21.2.3+11
HASH="$(sha256sum "$FIXTURE_ARCHIVE" | cut -d ' ' -f1)"
write_conf() {
    cat >"$FIXTURE/android/get_deps/tool_versions.conf" <<CONF
JDK_MAJOR=21
JDK_VERSION=21.2.3
JDK_BUILD=11
JDK_LINUX_SHA256=$1
JDK_URL=unused
CONF
}
assert_clean() {
    [ -z "$(find "$TEST_ROOT/local" -maxdepth 1 -type f -print)" ]
    [ -z "$(ls -A "$TMPDIR")" ]
    [ -z "$(find "$TEST_ROOT/local" -maxdepth 1 -name '.jdk-*' -print)" ]
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
bash "$HELPERS/get_jdk.sh"
assert_clean
[ "$("$TEST_ROOT/local/jdk-21/bin/java")" = fixture-jdk ]
[ "$(cat "$FIXTURE_URL_LOG")" = 'https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.2.3%2B11/OpenJDK21U-jdk_x64_linux_hotspot_21.2.3_11.tar.gz' ]
rm "$FIXTURE_URL_LOG"
bash "$HELPERS/get_jdk.sh"
[ ! -f "$FIXTURE_URL_LOG" ]
assert_clean
echo 'PASS: pinned JDK install, verification, retry, idempotence, and failure cleanup'
