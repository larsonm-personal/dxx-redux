#!/bin/bash
# set_vars.sh - Source this to set JAVA_HOME, ANDROID_HOME, ANDROID_NDK_ROOT.
# Reads the dependency base directory from dependency_base.txt.

# Resolve LOCAL_DIR from dependency_base.txt
_SET_VARS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_ANDROID_DIR="$(cd "$_SET_VARS_DIR/.." && pwd)"
source "$_ANDROID_DIR/get_deps/helpers/resolve_dep_base.sh"

# Resolve configured versions rather than whichever directory sorts last
source "$_ANDROID_DIR/get_deps/tool_versions.conf"

# --- JDK ---
if [ -z "${JAVA_HOME:-}" ]; then
    _jdk="$LOCAL_DIR/jdk-$JDK_MAJOR"
    if [ -d "$_jdk" ]; then
        export JAVA_HOME="$_jdk"
    else
        echo "WARNING: Configured JDK $JDK_MAJOR not found in $LOCAL_DIR" >&2
    fi
fi

# --- Android SDK ---
if [ -z "${ANDROID_HOME:-}" ]; then
    _sdk="$LOCAL_DIR/android-sdk"
    if [ -d "$_sdk" ]; then
        export ANDROID_HOME="$_sdk"
        export ANDROID_SDK_ROOT="$_sdk"
    else
        echo "WARNING: No android-sdk* folder found in $LOCAL_DIR" >&2
    fi
fi

# --- Android NDK ---
if [ -z "${ANDROID_NDK_ROOT:-}" ]; then
    _ndk="$LOCAL_DIR/android-ndk-$NDK_VERSION"
    if [ -d "$_ndk" ]; then
        export ANDROID_NDK_ROOT="$_ndk"
    else
        echo "WARNING: Configured NDK $NDK_VERSION not found in $LOCAL_DIR" >&2
    fi
fi

if [ -n "${JAVA_HOME:-}" ]; then export PATH="$JAVA_HOME/bin:$PATH"; fi

echo "JAVA_HOME=${JAVA_HOME:-}"
echo "ANDROID_HOME=${ANDROID_HOME:-}"
echo "ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-}"
