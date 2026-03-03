#!/bin/bash
# set_vars.sh — Source this to set JAVA_HOME, ANDROID_HOME, ANDROID_NDK_ROOT.
# Scans C:/local (or /c/local in MSYS/Git Bash) for the newest matching folders.

# Determine the base directory (works in Git Bash / MSYS2 and WSL)
if [ -d "/c/local" ]; then
    LOCAL_DIR="/c/local"
elif [ -d "/mnt/c/local" ]; then
    LOCAL_DIR="/mnt/c/local"
elif [ -d "C:/local" ]; then
    LOCAL_DIR="C:/local"
else
    echo "ERROR: C:\\local not found" >&2
    return 1 2>/dev/null || exit 1
fi

# Find newest folder matching a prefix (sorted descending, first match wins)
_find_newest() {
    local prefix="$1"
    # ls -d sorts alphabetically; reverse to get newest version first
    ls -d "${LOCAL_DIR}/${prefix}"* 2>/dev/null | sort -rV | head -n1
}

# --- JDK ---
if [ -z "$JAVA_HOME" ]; then
    _jdk=$(_find_newest "jdk-")
    if [ -n "$_jdk" ]; then
        export JAVA_HOME="$_jdk"
    else
        echo "WARNING: No jdk-* folder found in $LOCAL_DIR" >&2
    fi
fi

# --- Android SDK ---
if [ -z "$ANDROID_HOME" ]; then
    _sdk=$(_find_newest "android-sdk")
    if [ -n "$_sdk" ]; then
        export ANDROID_HOME="$_sdk"
        export ANDROID_SDK_ROOT="$_sdk"
    else
        echo "WARNING: No android-sdk* folder found in $LOCAL_DIR" >&2
    fi
fi

# --- Android NDK ---
if [ -z "$ANDROID_NDK_ROOT" ]; then
    _ndk=$(_find_newest "android-ndk-")
    if [ -n "$_ndk" ]; then
        export ANDROID_NDK_ROOT="$_ndk"
    else
        echo "WARNING: No android-ndk-* folder found in $LOCAL_DIR" >&2
    fi
fi

echo "JAVA_HOME=$JAVA_HOME"
echo "ANDROID_HOME=$ANDROID_HOME"
echo "ANDROID_NDK_ROOT=$ANDROID_NDK_ROOT"
