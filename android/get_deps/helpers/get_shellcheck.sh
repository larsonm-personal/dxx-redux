#!/bin/bash
# get_shellcheck.sh -- Download a pre-built shellcheck binary if not present.
# Reads version from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/shellcheck-$SHELLCHECK_VERSION"
begin_dependency_install "$DEST"

DEST_NAME="$(get_platform_executable_name shellcheck "$DEST")"
if [ -f "$DEST/$DEST_NAME" ] && verify_dependency_tool_version "${SHELLCHECK_VERSION}" exact "$DEST/$DEST_NAME" --version >/dev/null 2>&1; then
    echo "Verified shellcheck ${SHELLCHECK_VERSION} already installed at $DEST"
    exit 0
fi

prepare_dependency_workspace "$DEST"
DEST_NAME="$(get_platform_executable_name shellcheck "$DEST")"
if is_windows_target_path "$DEST"; then
    URL="https://github.com/koalaman/shellcheck/releases/download/v${SHELLCHECK_VERSION}/shellcheck-v${SHELLCHECK_VERSION}.zip"
    download_file "$DEPENDENCY_ARCHIVE" "$URL"
    unzip -q "$DEPENDENCY_ARCHIVE" -d "$DEPENDENCY_STAGE_DIR"
else
    OS_TOKEN="$(get_host_os)"
    if [ "$OS_TOKEN" = macos ]; then OS_TOKEN=darwin; fi
    case "$OS_TOKEN" in
    linux | darwin) ;;
    *)
        echo "Unsupported shellcheck host: $OS_TOKEN" >&2
        exit 1
        ;;
    esac
    URL="https://github.com/koalaman/shellcheck/releases/download/v${SHELLCHECK_VERSION}/shellcheck-v${SHELLCHECK_VERSION}.$OS_TOKEN.x86_64.tar.xz"
    download_file "$DEPENDENCY_ARCHIVE" "$URL"
    tar -xJf "$DEPENDENCY_ARCHIVE" -C "$DEPENDENCY_STAGE_DIR" --strip-components=1 "shellcheck-v${SHELLCHECK_VERSION}/shellcheck"
fi
chmod +x "$DEPENDENCY_STAGE_DIR/$DEST_NAME"
verify_dependency_tool_version "${SHELLCHECK_VERSION}" exact "$DEPENDENCY_STAGE_DIR/$DEST_NAME" --version
publish_dependency_directory "$DEPENDENCY_STAGE_DIR" "$DEST"
echo "shellcheck $SHELLCHECK_VERSION installed at $DEST"
