#!/usr/bin/env bash
# Exercise final-path Python installation rollback without network downloads
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"
TEST_ROOT="$(create_temp_dir cmakelang-install "$REPO_ROOT/android/temp")"
trap 'rm -rf "$TEST_ROOT"' EXIT
FIXTURE="$TEST_ROOT/repository with spaces"
HELPERS="$FIXTURE/android/get_deps/helpers"
mkdir -p "$HELPERS" "$TEST_ROOT/local"
printf '%s\n' "$TEST_ROOT/local" >"$FIXTURE/dependency_base.txt"
cp "$REPO_ROOT/android/get_deps/tool_versions.conf" "$FIXTURE/android/get_deps/"
for helper in get_cmake_format.sh platform.sh resolve_dep_base.sh; do
    cp "$REPO_ROOT/android/get_deps/helpers/$helper" "$HELPERS/"
done
source "$FIXTURE/android/get_deps/tool_versions.conf"
DEST="$TEST_ROOT/local/cmakelang-$CMAKELANG_VERSION"
export FIXTURE_DEST="$DEST"
export FIXTURE_FORMAT_VERSION="$CMAKELANG_VERSION"
export FIXTURE_YAML_VERSION="$CMAKELANG_PYYAML_VERSION"
export FIXTURE_SIX_VERSION="$CMAKELANG_SIX_VERSION"
cat >>"$HELPERS/platform.sh" <<'MOCK'
mv() {
    command mv "$@" || return $?
    case "${FIXTURE_CRASH:-}:$2" in
        backup:*/.dxx-install-state/work/previous) kill -KILL "$$" ;;
        restore:"$FIXTURE_DEST") kill -KILL "$$" ;;
    esac
}
touch() {
    command touch "$@" || return $?
    case "${FIXTURE_CRASH:-}:$1" in
        committed:*/.dxx-install-state/work/committed) kill -KILL "$$" ;;
    esac
}
python3() {
    if [ "$1" = -c ]; then return 0; fi
    [ "$1" = -m ] && [ "$2" = venv ] || return 98
    mkdir -p "$3/bin"
    cat > "$3/bin/python" <<'PYTHON'
#!/usr/bin/env bash
set -e
if [ "$1" = -m ] && [ "$2" = pip ]; then
    [ "${FIXTURE_CRASH:-}" != pip ] || { kill -KILL "$PPID"; exit 23; }
    [ "${FIXTURE_INSTALL_FAIL:-0}" != 1 ] || exit 23
    [ "${FIXTURE_INTERRUPT:-0}" != 1 ] || { kill -TERM "$PPID"; exit 23; }
    [ "$4" = --no-cache-dir ] || exit 24
    for expected in --no-deps "cmakelang==$FIXTURE_FORMAT_VERSION" "pyyaml==$FIXTURE_YAML_VERSION" "six==$FIXTURE_SIX_VERSION"; do
        found=0
        for arg in "$@"; do [ "$arg" != "$expected" ] || found=1; done
        [ "$found" = 1 ] || exit 25
    done
    cat > "$(dirname "$0")/cmake-format" <<'FORMAT'
#!/usr/bin/env bash
echo "$FIXTURE_FORMAT_VERSION"
FORMAT
    chmod +x "$(dirname "$0")/cmake-format"
    exit 0
fi
if [ "$1" = -c ]; then
    [ "$3" = "$FIXTURE_FORMAT_VERSION" ] && [ "$4" = "$FIXTURE_YAML_VERSION" ] && [ "$5" = "$FIXTURE_SIX_VERSION" ] || exit 26
else
    [ "$1" = -m ] && [ "$2" = cmakelang.lint ] && [ "$3" = --version ]
fi
[ "${FIXTURE_VALIDATE_FAIL:-0}" != 1 ]
PYTHON
    chmod +x "$3/bin/python"
}
MOCK
assert_clean() {
    if [ -e "$TEST_ROOT/local/.dxx-install-state/work" ] || compgen -G "$TEST_ROOT/local/.dxx-cmakelang-stage.*" >/dev/null; then
        echo 'Cmakelang workspace leaked' >&2
        exit 1
    fi
    [ "$(cat "$TEST_ROOT/local/.dxx-install-state/format")" = dxx-install-v1 ]
}
export GET_ALL_RUNNING=1
mkdir -p "$DEST/venv/bin"
printf 'last good installation\n' >"$DEST/keep.txt"
printf '#!/bin/sh\nexit 1\n' >"$DEST/venv/bin/cmake-format"
chmod +x "$DEST/venv/bin/cmake-format"
for failure in FIXTURE_INSTALL_FAIL FIXTURE_VALIDATE_FAIL FIXTURE_INTERRUPT; do
    if env "$failure=1" bash "$HELPERS/get_cmake_format.sh" >"$TEST_ROOT/failure.log" 2>&1; then
        echo "Accepted $failure" >&2
        exit 1
    fi
    [ "$(cat "$DEST/keep.txt")" = 'last good installation' ]
    assert_clean
done
recover() {
    bash -c 'source "$1"; begin_dependency_install "$2"' _ "$HELPERS/platform.sh" "$TEST_ROOT/local/cached-sibling"
}
for point in backup pip; do
    if FIXTURE_CRASH="$point" bash "$HELPERS/get_cmake_format.sh" >"$TEST_ROOT/crash.log" 2>&1; then exit 1; fi
    [ "$(cat "$TEST_ROOT/local/.dxx-install-state/format")" = dxx-install-v2 ]
    [ -d "$TEST_ROOT/local/.dxx-install-state/work/previous" ]
    recover
    [ "$(cat "$DEST/keep.txt")" = 'last good installation' ]
    assert_clean
done
# Recovery itself can be killed after restoring the previous installation
if FIXTURE_CRASH=pip bash "$HELPERS/get_cmake_format.sh" >"$TEST_ROOT/crash.log" 2>&1; then exit 1; fi
if FIXTURE_CRASH=restore recover >"$TEST_ROOT/recovery-crash.log" 2>&1; then exit 1; fi
[ "$(cat "$DEST/keep.txt")" = 'last good installation' ]
recover
[ "$(cat "$DEST/keep.txt")" = 'last good installation' ]
assert_clean

# A committed final-path install survives a crash before backup cleanup
if FIXTURE_CRASH=committed bash "$HELPERS/get_cmake_format.sh" >"$TEST_ROOT/crash.log" 2>&1; then exit 1; fi
[ -d "$TEST_ROOT/local/.dxx-install-state/work/previous" ]
bash "$HELPERS/get_cmake_format.sh"
[ ! -e "$DEST/keep.txt" ]
[ "$("$DEST/venv/bin/cmake-format" --version)" = "$CMAKELANG_VERSION" ]
assert_clean
FIXTURE_INSTALL_FAIL=1 bash "$HELPERS/get_cmake_format.sh"
assert_clean
rm -rf "$DEST"
if FIXTURE_INSTALL_FAIL=1 bash "$HELPERS/get_cmake_format.sh" >"$TEST_ROOT/fresh-failure.log" 2>&1; then exit 1; fi
[ ! -e "$DEST" ]
assert_clean
if FIXTURE_CRASH=pip bash "$HELPERS/get_cmake_format.sh" >"$TEST_ROOT/fresh-crash.log" 2>&1; then exit 1; fi
recover
[ ! -e "$DEST" ]
assert_clean
echo 'PASS: cmakelang final-path install, rollback, validation, termination, SIGKILL and interrupted recovery, cleanup, and repeat run'
