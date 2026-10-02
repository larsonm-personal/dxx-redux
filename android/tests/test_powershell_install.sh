#!/usr/bin/env bash
# Exercise bootstrap recovery without changing the host PowerShell or package database
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"
TEST_ROOT="$(create_temp_dir powershell-install "$REPO_ROOT/android/temp")"
trap 'rm -rf "$TEST_ROOT"' EXIT
export FIXTURE_ROOT="$TEST_ROOT"
HELPERS="$TEST_ROOT/repository with spaces/android/get_deps/helpers"
mkdir -p "$HELPERS" "$TEST_ROOT/local" "$TEST_ROOT/payload"
printf '%s\n' "$TEST_ROOT/local" >"$HELPERS/../../../dependency_base.txt"
printf 'POWERSHELL_VERSION=9.0.0\n' >"$HELPERS/../tool_versions.conf"
for helper in platform.sh resolve_dep_base.sh get_powershell.sh; do
    cp "$REPO_ROOT/android/get_deps/helpers/$helper" "$HELPERS/"
done
cat >>"$HELPERS/platform.sh" <<'MOCK'
source() {
    if [ "$1" = /etc/os-release ]; then
        ID="${FIXTURE_DISTRO:-ubuntu}"
        ID_LIKE=''
    else
        builtin source "$@"
    fi
}
uname() { if [ "${1:-}" = -m ]; then echo "${FIXTURE_ARCH:-x86_64}"; else command uname "$@"; fi; }
pwsh() {
    if [ -L "$FIXTURE_ROOT/pwsh" ]; then "$FIXTURE_ROOT/pwsh" "$@"
    elif [ -f "$FIXTURE_ROOT/package-installed" ]; then cat "$FIXTURE_ROOT/package-installed"
    else echo 8.0.0; fi
}
sudo() { if [ "${1:-}" = -n ]; then shift; fi; "$@"; }
apt() {
    if [ "$1" = update ]; then return 0; fi
    [ "$1" = install ] && [ "$2" = -y ] && [ -f "$3" ] || return 98
    [ "${FIXTURE_PACKAGE_FAIL:-0}" != 1 ] || return 23
    echo "${FIXTURE_PACKAGE_VERSION:-9.0.0}" >"$FIXTURE_ROOT/package-installed"
    if [ "${FIXTURE_CRASH:-}" = package ]; then kill -KILL "$$"; fi
}
dnf() { apt "$@"; }
ln() {
    [ "$1" = -sf ] && [ "$3" = /usr/local/bin/pwsh ] || return 97
    command ln -sf "$2" "$FIXTURE_ROOT/pwsh"
}
download_text() {
    printf '%s\n' '{"browser_download_url":"https://fixture/powershell_9.0.0.deb_amd64.deb"}' \
        '{"browser_download_url":"https://fixture/powershell-9.0.0.rh.x86_64.rpm"}' \
        '{"browser_download_url":"https://fixture/powershell-9.0.0-linux-x64.tar.gz"}'
}
download_file() {
    cp "$FIXTURE_ROOT/archive" "$1"
    if [ "${FIXTURE_CRASH:-}" = download ]; then kill -KILL "$$"; fi
    [ "${FIXTURE_DOWNLOAD_FAIL:-0}" != 1 ]
}
mv() {
    if [ "${FIXTURE_PUBLISH_FAIL:-0}" = 1 ] && [[ "$1" == */work/install ]]; then return 23; fi
    command mv "$@" || return $?
    case "${FIXTURE_CRASH:-}:$2" in
        backup:*/work/previous | publish:*/powershell-9.0.0) kill -KILL "$$" ;;
    esac
}
MOCK
DEST="$TEST_ROOT/local/powershell-9.0.0"
STATE="$TEST_ROOT/local/.dxx-install-state"
make_archive() {
    printf '#!/bin/sh\necho %s\n' "$1" >"$TEST_ROOT/payload/pwsh"
    tar -czf "$TEST_ROOT/archive" -C "$TEST_ROOT/payload" .
}
assert_clean() { [ ! -e "$STATE/work" ]; }
run() { bash "$HELPERS/get_powershell.sh" >"$TEST_ROOT/result.log" 2>&1; }
make_archive 9.0.0
for distro in ubuntu fedora; do
    export FIXTURE_DISTRO="$distro"
    rm -f "$TEST_ROOT/package-installed"
    for failure in FIXTURE_DOWNLOAD_FAIL FIXTURE_PACKAGE_FAIL; do
        if (
            export "$failure=1"
            run
        ); then
            echo "Accepted $failure" >&2
            exit 1
        fi
        assert_clean
        [ ! -e "$TEST_ROOT/package-installed" ]
    done
    if FIXTURE_PACKAGE_VERSION=8.0.0 run; then
        echo 'Accepted wrong installed version' >&2
        exit 1
    fi
    assert_clean
    rm "$TEST_ROOT/package-installed"
    for point in download package; do
        if FIXTURE_CRASH="$point" run; then
            echo "Did not crash at $point" >&2
            exit 1
        fi
        [ -d "$STATE/work" ]
        if [ "$point" = package ]; then
            FIXTURE_DOWNLOAD_FAIL=1 run
        else
            run
        fi
        assert_clean
        [ "$(cat "$TEST_ROOT/package-installed")" = 9.0.0 ]
        rm "$TEST_ROOT/package-installed"
    done
    run
    FIXTURE_DOWNLOAD_FAIL=1 run
    assert_clean
    rm "$TEST_ROOT/package-installed"
done
echo 'PASS: deb/rpm download and package failure cleanup, version checks, SIGKILL and cached recovery'
export FIXTURE_DISTRO=other
mkdir "$DEST"
printf preserve >"$DEST/sentinel"
make_archive 8.0.0
if run; then
    echo 'Accepted wrong tarball version' >&2
    exit 1
fi
[ "$(cat "$DEST/sentinel")" = preserve ]
assert_clean
make_archive 9.0.0
if FIXTURE_PUBLISH_FAIL=1 run; then
    echo 'Accepted failed publication' >&2
    exit 1
fi
[ "$(cat "$DEST/sentinel")" = preserve ]
assert_clean
for point in download backup publish; do
    if FIXTURE_CRASH="$point" run; then
        echo "Did not crash at $point" >&2
        exit 1
    fi
    [ -d "$STATE/work" ]
    # Admit an existing system package to verify recovery happens before cached return
    echo 9.0.0 >"$TEST_ROOT/package-installed"
    FIXTURE_DOWNLOAD_FAIL=1 run
    assert_clean
    rm "$TEST_ROOT/package-installed"
    if [ "$point" != publish ]; then [ "$(cat "$DEST/sentinel")" = preserve ]; fi
done
# A committed tree is reusable even if termination preceded command-link creation
FIXTURE_DOWNLOAD_FAIL=1 run
[ ! -e "$DEST/sentinel" ]
[ "$("$DEST/pwsh")" = 9.0.0 ]
FIXTURE_DOWNLOAD_FAIL=1 run
assert_clean
rm "$TEST_ROOT/pwsh"
if FIXTURE_ARCH=aarch64 run; then
    echo 'Accepted unsupported bootstrap architecture' >&2
    exit 1
fi
assert_clean
echo 'PASS: tarball staged validation, rollback, SIGKILL publication recovery, cached reuse and architecture policy'
