#!/bin/bash
# get_jdk.sh - Download and install OpenJDK if missing or out of date.
# Reads version/URL from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/verify_sha256.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

JDK_DIR_NAME="jdk-$JDK_MAJOR"
INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/$JDK_DIR_NAME"
begin_dependency_install "$DEST"

get_installed_jdk_version() {
    local release_file="$1/release"

    if [ ! -f "$release_file" ]; then
        return 1
    fi

    sed -n 's/^JAVA_VERSION="\(.*\)"$/\1/p' "$release_file" | tr -d '\r' | head -n1
}

has_expected_jdk_runtime() {
    local runtime
    runtime="$(sed -n 's/^JAVA_RUNTIME_VERSION="\(.*\)"$/\1/p' "$1/release" | tr -d '\r' | head -n1)"
    [ "${runtime%%-*}" = "$JDK_VERSION+$JDK_BUILD" ]
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
    if { [ -x "$DEST/bin/java" ] || [ -x "$DEST/bin/java.exe" ]; } && [ "$INSTALLED_VERSION" = "$JDK_VERSION" ] && has_expected_jdk_runtime "$DEST"; then
        echo "JDK $JDK_MAJOR already installed at $DEST ($INSTALLED_VERSION)"
        exit 0
    fi

    if [ -n "$INSTALLED_VERSION" ]; then
        echo "JDK $JDK_MAJOR at $DEST is $INSTALLED_VERSION, expected $JDK_VERSION; reinstalling"
    else
        echo "JDK $JDK_MAJOR at $DEST is incomplete or has unknown version, expected $JDK_VERSION; reinstalling"
    fi
fi

# An incomplete install can still be recovered below without replacing open files
if [ -n "$INSTALLED_VERSION" ] && ! assert_dependency_not_in_use "$DEST"; then
    echo "For Gradle daemons, run the matching Gradle version with --stop; close IDE Java/Gradle sessions if they restart the daemon, then rerun helpers/get_jdk.sh" >&2
    exit 1
fi

URL="$JDK_URL"
ARCHIVE_KIND="zip"
if DERIVED_URL="$(get_jdk_download_url "$JDK_MAJOR" "$JDK_VERSION" "$JDK_BUILD" 2>/dev/null)"; then
    URL="$DERIVED_URL"
fi
case "$(get_host_os)" in
linux)
    ARCHIVE_KIND="tar.gz"
    ARCHIVE_SHA256="$JDK_LINUX_SHA256"
    ;;
macos)
    ARCHIVE_KIND="tar.gz"
    ARCHIVE_SHA256="$JDK_MAC_SHA256"
    ;;
windows) ARCHIVE_SHA256="$JDK_WINDOWS_SHA256" ;;
*)
    echo "Unsupported JDK host" >&2
    exit 1
    ;;
esac
prepare_dependency_workspace "$DEST" 2
TMPFILE="$DEPENDENCY_ARCHIVE"
STAGE_DIR="$DEPENDENCY_STAGE_DIR"

echo "Downloading OpenJDK $JDK_VERSION..."
download_file "$TMPFILE" "$URL"
verify_sha256 "$TMPFILE" "$ARCHIVE_SHA256" "OpenJDK $JDK_VERSION+$JDK_BUILD"

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
        if [ -d "$NEW_JDK_DIR/Contents/Home" ]; then NEW_JDK_DIR="$NEW_JDK_DIR/Contents/Home"; fi
        break
    fi
done
if [ -z "$NEW_JDK_DIR" ]; then
    echo "OpenJDK archive did not contain the expected JDK $JDK_VERSION directory" >&2
    exit 1
fi

STAGED_VERSION="$(get_installed_jdk_version "$NEW_JDK_DIR" || true)"
if { [ ! -x "$NEW_JDK_DIR/bin/java" ] && [ ! -x "$NEW_JDK_DIR/bin/java.exe" ]; } || [ "$STAGED_VERSION" != "$JDK_VERSION" ] || ! has_expected_jdk_runtime "$NEW_JDK_DIR"; then
    echo "Staged JDK is incomplete or version $STAGED_VERSION, expected $JDK_VERSION" >&2
    exit 1
fi

if [ -d "$DEST" ] && [ -z "$INSTALLED_VERSION" ] && recover_matching_incomplete_install; then
    echo "JDK $JDK_MAJOR installed at $DEST"
    "$DEST/bin/java" -version 2>&1 | head -1
    exit 0
fi

publish_dependency_directory "$NEW_JDK_DIR" "$DEST"

echo "JDK $JDK_MAJOR installed at $DEST"
"$DEST/bin/java" -version 2>&1 | head -1
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
