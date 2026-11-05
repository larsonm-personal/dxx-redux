"""Read current native stack membership; uncertainty must never suppress tests."""

import fnmatch
import json
import os
import re
from pathlib import Path
import urllib.request


EXTENDED_PATHS = (
    ".github/workflows/*nightly*",
    ".github/workflows/stack-ci.yml",
    ".github/scripts/*",
    "android/tests/*nightly*",
    "android/tests/*host_test_suites*",
    "android/run_all_tests.ps1",
    "android/helpers/nightly_public_fixture_policy.ps1",
    "d[12]/main/net*",
    "d[12]/main/multi*",
    "android/app/src/main/cpp/shared/net/*",
    "android/app/src/main/cpp/shared/coop/*",
    "android/app/src/main/cpp/shared/android_network_profile.*",
    "android/app/src/main/java/com/dxxredux/app/lobby/*",
    "android/app/src/main/java/com/dxxredux/app/multiplayer/*",
    "android/tests/test_lan*",
)


def eligible(pr):
    """Only a complete, valid native non-tip record is grounds to skip."""
    stack = pr.get("stack")
    if not isinstance(stack, dict):
        return True
    position, size, number = (stack.get(key) for key in ("position", "size", "number"))
    if not all(type(value) is int for value in (position, size, number)):
        return True
    if number <= 0 or not 1 <= position <= size:
        return True
    return position == size


def refresh(number, repository, fetch=None, profile="", event_pr=None):
    # Public metadata needs no token. Never pass a privileged controller token
    # to this helper or broaden workflow permissions for an optimization
    def get(url):
        request = urllib.request.Request(
            url,
            headers={
                "Accept": "application/vnd.github+json",
                "X-GitHub-Api-Version": "2026-03-10",
            },
        )
        with urllib.request.urlopen(request, timeout=15) as response:
            return json.load(response)

    try:
        pr = (fetch or get)(f"https://api.github.com/repos/{repository}/pulls/{number}")
        if not isinstance(pr, dict) or pr.get("number") != number or pr.get("state") != "open":
            return True
        if not eligible(pr):
            return False
        # Native tips receive the complete sample, so lower-layer changes cannot
        # disappear behind an immediate-parent path filter
        if profile != "extended" or pr.get("stack") or (event_pr or {}).get("stack"):
            return True
        if not event_pr or pr.get("head", {}).get("sha") != event_pr.get("head", {}).get("sha"):
            return True
        base, head = event_pr["base"]["sha"], event_pr["head"]["sha"]
        if not all(isinstance(sha, str) and re.fullmatch("[0-9a-f]{40}", sha) for sha in (base, head)):
            return True
        comparison = (fetch or get)(f"https://api.github.com/repos/{repository}/compare/{base}...{head}")
        files = comparison.get("files")
        # GitHub caps compare files at 300; a possibly truncated response runs
        if not isinstance(files, list) or len(files) >= 300:
            return True
        for file in files:
            if not isinstance(file, dict) or not isinstance(file.get("filename"), str):
                return True
            for path in (file["filename"], file.get("previous_filename", "")):
                if any(fnmatch.fnmatchcase(path, pattern) for pattern in EXTENDED_PATHS):
                    return True
        return False
    except (OSError, ValueError, TypeError, KeyError, AttributeError):
        print("Stack metadata unavailable; retaining expensive coverage")
        return True


def main():
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    run = True
    if os.environ.get("GITHUB_EVENT_NAME") == "pull_request":
        number = event.get("pull_request", {}).get("number")
        if type(number) is int and number > 0:
            # Always refresh, including runs originally opened as independent PRs
            run = refresh(
                number,
                os.environ["GITHUB_REPOSITORY"],
                profile=os.environ.get("STACK_CI_PROFILE", ""),
                event_pr=event["pull_request"],
            )
    reason = (
        "Run expensive coverage (independent/tip or uncertain metadata)"
        if run
        else "Skip expensive coverage: native non-tip or unrelated standalone changes"
    )
    print(reason)
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        output.write(f"run={str(run).lower()}\n")
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as summary:
        summary.write(reason + "\n")


if __name__ == "__main__":
    main()
