#!/bin/bash
# get_jdk.sh - Download and install OpenJDK if missing or out of date.
# Reads version/URL from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

JDK_DIR_NAME="jdk-$JDK_MAJOR"
INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/$JDK_DIR_NAME"
STAGE_DIR=""
BACKUP_DIR=""
TMPFILE=""

cleanup() {
    if [ -n "$TMPFILE" ]; then
        rm -f "$TMPFILE"
    fi
    if [ -n "$STAGE_DIR" ] && [ -d "$STAGE_DIR" ]; then
        rm -rf "$STAGE_DIR"
    fi
}
trap cleanup EXIT

get_installed_jdk_version() {
    local release_file="$1/release"

    if [ ! -f "$release_file" ]; then
        return 1
    fi

    sed -n 's/^JAVA_VERSION="\(.*\)"$/\1/p' "$release_file" | tr -d '\r' | head -n1
}

recover_matching_incomplete_install() {
    local existing_file relative_path staged_file

    shopt -s globstar nullglob
    for existing_file in "$DEST"/**/*; do
        if [ ! -f "$existing_file" ]; then
            continue
        fi
        relative_path="${existing_file#"$DEST"/}"
        staged_file="$NEW_JDK_DIR/$relative_path"
        if [ ! -f "$staged_file" ] || ! cmp -s "$existing_file" "$staged_file"; then
            return 1
        fi
    done

    echo "Recovering the incomplete JDK without replacing matching files that are in use..."
    cp -a -n "$NEW_JDK_DIR"/. "$DEST"/
    [ "$(get_installed_jdk_version "$DEST" || true)" = "$JDK_VERSION" ] \
        && { [ -x "$DEST/bin/java" ] || [ -x "$DEST/bin/java.exe" ]; }
}

INSTALLED_VERSION=""
if [ -d "$DEST" ]; then
    INSTALLED_VERSION="$(get_installed_jdk_version "$DEST" || true)"
    if { [ -x "$DEST/bin/java" ] || [ -x "$DEST/bin/java.exe" ]; } && [ "$INSTALLED_VERSION" = "$JDK_VERSION" ]; then
        echo "JDK $JDK_MAJOR already installed at $DEST ($INSTALLED_VERSION)"
        exit 0
    fi

    if [ -n "$INSTALLED_VERSION" ]; then
        echo "JDK $JDK_MAJOR at $DEST is $INSTALLED_VERSION, expected $JDK_VERSION; reinstalling"
    else
        echo "JDK $JDK_MAJOR at $DEST is incomplete or has unknown version, expected $JDK_VERSION; reinstalling"
    fi
fi

URL="$JDK_URL"
ARCHIVE_KIND="zip"
if DERIVED_URL="$(get_jdk_download_url "$JDK_MAJOR" 2>/dev/null)"; then
    URL="$DERIVED_URL"
fi
case "$(get_host_os)" in
linux | macos) ARCHIVE_KIND="tar.gz" ;;
esac
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" jdk-XXXXXX)"
STAGE_DIR="$(mktemp -d "$INSTALL_DIR/.jdk-$JDK_MAJOR-stage-XXXXXX")"

echo "Downloading OpenJDK $JDK_VERSION..."
download_file "$TMPFILE" "$URL"

echo "Extracting OpenJDK $JDK_VERSION to a staging directory..."
if [ "$ARCHIVE_KIND" = "zip" ]; then
    unzip -q -o "$TMPFILE" -d "$STAGE_DIR"
else
    tar -xzf "$TMPFILE" -C "$STAGE_DIR"
fi

# The extracted folder may include a build number such as +7
# Use a bash glob instead of find(1) because Windows find.exe searches text
NEW_JDK_DIR=""
for _d in "$STAGE_DIR"/jdk-"${JDK_VERSION}"*; do
    if [ -d "$_d" ]; then
        NEW_JDK_DIR="$_d"
        break
    fi
done
if [ -z "$NEW_JDK_DIR" ]; then
    echo "OpenJDK archive did not contain the expected JDK $JDK_VERSION directory" >&2
    exit 1
fi

STAGED_VERSION="$(get_installed_jdk_version "$NEW_JDK_DIR" || true)"
if { [ ! -x "$NEW_JDK_DIR/bin/java" ] && [ ! -x "$NEW_JDK_DIR/bin/java.exe" ]; } || [ "$STAGED_VERSION" != "$JDK_VERSION" ]; then
    echo "Staged JDK is incomplete or version $STAGED_VERSION, expected $JDK_VERSION" >&2
    exit 1
fi

if [ -d "$DEST" ] && [ -z "$INSTALLED_VERSION" ] && recover_matching_incomplete_install; then
    echo "JDK $JDK_MAJOR installed at $DEST"
    "$DEST/bin/java" -version 2>&1 | head -1
    exit 0
fi

if [ -d "$DEST" ]; then
    BACKUP_DIR="$INSTALL_DIR/.jdk-$JDK_MAJOR-backup-$$"
    echo "Replacing JDK $JDK_MAJOR at $DEST..."
    if ! mv "$DEST" "$BACKUP_DIR"; then
        echo "Unable to move the current JDK because a file is in use" >&2
        echo "Close Gradle daemons and other processes using $DEST, then retry" >&2
        exit 1
    fi
fi

if ! mv "$NEW_JDK_DIR" "$DEST"; then
    echo "Unable to move the staged JDK into $DEST" >&2
    if [ -n "$BACKUP_DIR" ] && [ -d "$BACKUP_DIR" ]; then
        mv "$BACKUP_DIR" "$DEST" || true
        echo "Restored the previous JDK directory" >&2
    fi
    exit 1
fi

if [ -n "$BACKUP_DIR" ] && [ -d "$BACKUP_DIR" ] && ! rm -rf "$BACKUP_DIR"; then
    echo "WARNING: The old JDK remains at $BACKUP_DIR because a file is still in use" >&2
fi

echo "JDK $JDK_MAJOR installed at $DEST"
"$DEST/bin/java" -version 2>&1 | head -1
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
