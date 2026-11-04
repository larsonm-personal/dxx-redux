#!/usr/bin/env bash
# Host storage only: preserve managed SDKs, caches, builds and test fixtures
set -euo pipefail

mode=${1:-}
minimum_gib=${2:-}
case "$mode" in prepare | retire-hosted-sdk | check | report) ;; *)
    echo 'Expected prepare, retire-hosted-sdk, check or report' >&2
    exit 2
    ;;
esac
[[ "$minimum_gib" =~ ^[0-9]+$ ]] || {
    echo 'Expected minimum free GiB' >&2
    exit 2
}
: "${GITHUB_WORKSPACE:?}" "${RUNNER_TEMP:?}"

# Fixed image tools unused by the Android/JVM lanes, including their native peers
unused=(/opt/ghc /usr/local/.ghcup /opt/hostedtoolcache/CodeQL /usr/share/swift)

report() {
    df -h "$GITHUB_WORKSPACE" "$RUNNER_TEMP"
    df -i "$GITHUB_WORKSPACE" "$RUNNER_TEMP"
    # Aggregate sizes only: no listing of private asset names or contents
    for path in "${unused[@]}" /usr/local/lib/android "$RUNNER_TEMP/dxx-nightly-deps" \
        "$HOME/.gradle" "$HOME/.android" "$GITHUB_WORKSPACE/target"; do
        if [[ -d "$path" ]]; then du -sh "$path" 2>/dev/null || true; fi
    done
}

report
if [[ "$mode" == prepare || "$mode" == retire-hosted-sdk ]]; then
    # Refuse destructive preparation on developer machines and self-hosted runners
    [[ "${GITHUB_ACTIONS:-}" == true && "${RUNNER_ENVIRONMENT:-}" == github-hosted &&
        "${RUNNER_OS:-}" == Linux && "${ImageOS:-}" == ubuntu24 ]] || {
        echo '::error::Storage cleanup requires a GitHub-hosted Ubuntu 24.04 runner'
        exit 1
    }
    if [[ "$mode" == retire-hosted-sdk ]]; then
        # Explicit opt-in only after the pinned SDK has replaced the image SDK
        : "${DXX_NIGHTLY_SDK_CACHE:?}" "${ANDROID_HOME:?}" "${ANDROID_SDK_ROOT:?}" "${ANDROID_NDK_ROOT:?}"
        sdk=$(realpath -e "$DXX_NIGHTLY_SDK_CACHE")
        [[ "$sdk" == "$(realpath -e "$ANDROID_HOME")" &&
        "$sdk" == "$(realpath -e "$ANDROID_SDK_ROOT")" &&
        -x "$sdk/platform-tools/adb" && -d "$ANDROID_NDK_ROOT" ]] || {
            echo '::error::Pinned SDK and NDK must be configured before retiring the hosted SDK'
            exit 1
        }
        unused=(/usr/local/lib/android)
    fi
    for path in "${unused[@]}"; do
        [[ -d "$path" && ! -L "$path" ]] || continue
        [[ "$(realpath -e "$path")" == "$path" ]] || {
            echo "::error::Refusing cleanup through a linked parent of $path"
            exit 1
        }
        # Do not remove a tool if a caller configured it as a job storage location
        for protected in "$GITHUB_WORKSPACE" "$RUNNER_TEMP" "${ANDROID_HOME:-}" \
            "${ANDROID_SDK_ROOT:-}" "${ANDROID_NDK_ROOT:-}" "${ANDROID_NDK_HOME:-}" \
            "${ANDROID_NDK_LATEST_HOME:-}" "${DXX_NIGHTLY_SDK_CACHE:-}" "${JAVA_HOME:-}"; do
            [[ -n "$protected" ]] || continue
            resolved=$(realpath -m "$protected")
            if [[ "$resolved" == "$path" || "$resolved" == "$path/"* ]]; then
                echo "::error::Refusing cleanup of protected location $path"
                exit 1
            fi
        done
        echo "Reclaiming unused hosted image tool: $path"
        sudo rm -rf -- "$path"
    done
    report
fi
[[ "$mode" != report ]] || exit 0

# These are conservative admission budgets, not claims about peak usage
for path in "$GITHUB_WORKSPACE" "$RUNNER_TEMP"; do
    available_kib=$(df -Pk "$path" | awk 'NR == 2 {print $4}')
    if ((available_kib < minimum_gib * 1024 * 1024)); then
        echo "::error::Host storage at $path has $available_kib KiB free; requires $minimum_gib GiB before the next phase"
        exit 1
    fi
done
