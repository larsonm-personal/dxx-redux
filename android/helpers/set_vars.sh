#!/bin/bash
# set_vars.sh - Source this to set JAVA_HOME, ANDROID_HOME, ANDROID_NDK_ROOT.
# Reads the dependency base directory from dependency_base.txt.

# Resolve LOCAL_DIR from dependency_base.txt
_SET_VARS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_ANDROID_DIR="$(cd "$_SET_VARS_DIR/.." && pwd)"
source "$_ANDROID_DIR/get_deps/helpers/resolve_dep_base.sh"

# Find newest folder matching a prefix.
# Uses a bash glob (expands in ascending order) instead of ls|sort -rV,
# because Windows sort.exe shadows /usr/bin/sort in some Git Bash setups.
_find_newest() {
    local prefix="$1" result=""
    for _d in "${LOCAL_DIR}/${prefix}"*; do
        [ -d "$_d" ] && result="$_d"
    done
    echo "$result"
}

# --- JDK ---
if [ -z "${JAVA_HOME:-}" ]; then
    _jdk=$(_find_newest "jdk-")
    if [ -n "$_jdk" ]; then
        export JAVA_HOME="$_jdk"
    else
        echo "WARNING: No jdk-* folder found in $LOCAL_DIR" >&2
    fi
fi

# --- Android SDK ---
if [ -z "${ANDROID_HOME:-}" ]; then
    _sdk=$(_find_newest "android-sdk")
    if [ -n "$_sdk" ]; then
        export ANDROID_HOME="$_sdk"
        export ANDROID_SDK_ROOT="$_sdk"
    else
        echo "WARNING: No android-sdk* folder found in $LOCAL_DIR" >&2
    fi
fi

# --- Android NDK ---
if [ -z "${ANDROID_NDK_ROOT:-}" ]; then
    _ndk=$(_find_newest "android-ndk-")
    if [ -n "$_ndk" ]; then
        export ANDROID_NDK_ROOT="$_ndk"
    else
        echo "WARNING: No android-ndk-* folder found in $LOCAL_DIR" >&2
    fi
fi

echo "JAVA_HOME=${JAVA_HOME:-}"
echo "ANDROID_HOME=${ANDROID_HOME:-}"
echo "ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-}"
