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

get_installed_jdk_version() {
    local release_file="$1/release"

    if [ ! -f "$release_file" ]; then
        return 1
    fi

    sed -n 's/^JAVA_VERSION="\(.*\)"$/\1/p' "$release_file" | tr -d '\r' | head -n1
}

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
    rm -rf "$DEST"
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

echo "Downloading OpenJDK $JDK_VERSION..."
download_file "$TMPFILE" "$URL"

echo "Extracting to $DEST..."
if [ "$ARCHIVE_KIND" = "zip" ]; then
    unzip -q -o "$TMPFILE" -d "$INSTALL_DIR"
else
    tar -xzf "$TMPFILE" -C "$INSTALL_DIR"
fi
# Rename the extracted folder (may include build number like +7) to the canonical name.
# Use a bash glob instead of find(1) -- on Windows, find.exe is a text-search tool.
if [ ! -d "$DEST" ]; then
    for _d in "$INSTALL_DIR"/jdk-"${JDK_VERSION}"*; do
        if [ -d "$_d" ]; then
            mv "$_d" "$DEST"
            break
        fi
    done
fi

rm -f "$TMPFILE"
echo "JDK $JDK_MAJOR installed at $DEST"
"$DEST/bin/java" -version 2>&1 | head -1
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
