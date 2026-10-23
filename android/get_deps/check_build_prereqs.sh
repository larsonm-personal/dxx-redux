#!/usr/bin/env bash
# Report Android build prerequisites without provisioning or modifying paths
set -eo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export DXX_DEPENDENCY_READ_ONLY=1
source "$SCRIPT_DIR/../helpers/set_vars.sh"
printf 'Dependency base: %s\n' "$LOCAL_DIR"
missing=0
require_file() {
    if [ ! -s "$1" ]; then
        printf 'MISSING: %s (run %s)\n' "$1" "$2" >&2
        missing=1
    fi
}
java_home="${JAVA_HOME:-$LOCAL_DIR/jdk-$JDK_MAJOR}"
sdk="${ANDROID_HOME:-$LOCAL_DIR/android-sdk}"
ndk="${ANDROID_NDK_ROOT:-$LOCAL_DIR/android-ndk-$NDK_VERSION}"
require_file "$java_home/bin/$(get_platform_executable_name java)" get_jdk.sh
require_file "$sdk/cmdline-tools/latest/bin/$(get_platform_batch_name sdkmanager)" get_sdk.sh
build_id="$(get_sdk_cmdline_tools_build_id "$SDK_CMDLINE_TOOLS_URL")"
marker="$sdk/cmdline-tools/latest/.dxx-cmdline-tools-build-id"
if [ ! -f "$marker" ] || [ "$(cat "$marker")" != "$build_id" ]; then
    echo "MISSING: command-line tools build $build_id marker (run get_sdk.sh)" >&2
    missing=1
fi
platform="platforms/android-$COMPILE_SDK"
if [ ! -d "$sdk/$platform" ] && [ -d "$sdk/$platform.0" ]; then platform="$platform.0"; fi
for component in "$platform" "build-tools/$BUILD_TOOLS_VERSION" "cmake/$CMAKE_VERSION" platform-tools; do
    require_file "$sdk/$component/package.xml" finalize.sh
    require_file "$sdk/$component/source.properties" finalize.sh
done
require_file "$ndk/source.properties" get_ndk.sh
require_file "$SCRIPT_DIR/../gradle/wrapper/gradle-wrapper.jar" get_gradle_wrapper.sh
if [ "$missing" -ne 0 ]; then
    echo "Build prerequisites incomplete; run the named helpers in android/get_deps/helpers, then repeat this check" >&2
    exit 1
fi
echo "Android build prerequisite files present; Gradle remains the build/version authority"
