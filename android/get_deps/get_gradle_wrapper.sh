#!/bin/bash
# get_gradle_wrapper.sh - Generate gradle-wrapper.properties and download the
# wrapper jar.  Both are kept out of VCS (android/gradle/ is gitignored) and
# regenerated from tool_versions.conf so the version file is the single source
# of truth for the Gradle version.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"

WRAPPER_DIR="$SCRIPT_DIR/../gradle/wrapper"
WRAPPER_JAR="$WRAPPER_DIR/gradle-wrapper.jar"
WRAPPER_PROPS="$WRAPPER_DIR/gradle-wrapper.properties"

mkdir -p "$WRAPPER_DIR"

# Always regenerate properties so the version stays in sync with tool_versions.conf
cat > "$WRAPPER_PROPS" <<EOF
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip
networkTimeout=10000
validateDistributionUrl=true
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
EOF
echo "Generated $WRAPPER_PROPS (Gradle $GRADLE_VERSION)"

if [ -f "$WRAPPER_JAR" ]; then
    echo "gradle-wrapper.jar already present."
    if [ -z "$GET_ALL_RUNNING" ]; then
        echo ""
        echo "Press any key to exit..."
        read -r -n1 -s
    fi
    exit 0
fi

URL="https://raw.githubusercontent.com/gradle/gradle/v${GRADLE_VERSION}/gradle/wrapper/gradle-wrapper.jar"

echo "Downloading gradle-wrapper.jar for Gradle $GRADLE_VERSION..."
echo "  URL: $URL"
curl -fSL --progress-bar -o "$WRAPPER_JAR" "$URL"

echo "gradle-wrapper.jar installed at $WRAPPER_JAR"
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
