"""Explicit known-account endorsement of a same-repository candidate SHA."""

import json
import os
import re
import subprocess
from urllib.parse import quote

import nightly_retail as retail

# Verified using the authenticated GitHub profile, not commit metadata
KNOWN_ACTORS = {265118235: "larsonm-personal"}
REPOSITORY_ID = 1171374860
CAMPAIGNS = ("xl-fast", "xl-originals")


def api(path):
    result = subprocess.run(
        ["gh", "api", "repos/" + retail.REPOSITORY + "/" + path],
        text=True,
        capture_output=True,
        timeout=30,
        check=True,
    )
    return json.loads(result.stdout)


def verify(target_sha=None, env=None, fetch=api):
    """Return the explicitly endorsed SHA, or None for the unchanged normal path."""
    env = os.environ if env is None else env
    number, sha = env.get("CANDIDATE_PR", ""), env.get("CANDIDATE_SHA", "")
    if not number and not sha:
        return None
    try:
        if not (
            retail.trusted_context(env)
            and env.get("GITHUB_EVENT_NAME") == "workflow_dispatch"
            and env.get("CANDIDATE_CAMPAIGN") in CAMPAIGNS
            and re.fullmatch(r"[1-9][0-9]*", number)
            and re.fullmatch(r"[0-9a-f]{40}", sha)
            and (target_sha is None or target_sha == sha)
        ):
            raise ValueError
        run_id, attempt = env["GITHUB_RUN_ID"], env["GITHUB_RUN_ATTEMPT"]
        if not run_id.isdecimal() or not attempt.isdecimal():
            raise ValueError
        run = fetch("actions/runs/" + run_id)
        actor = run["actor"]
        trigger = run["triggering_actor"]
        if not (
            actor["id"] in KNOWN_ACTORS
            and actor["login"] == KNOWN_ACTORS[actor["id"]]
            and trigger["id"] == actor["id"]
            and trigger["login"] == actor["login"]
            and env["GITHUB_ACTOR_ID"] == str(actor["id"])
            and env["GITHUB_ACTOR"] == actor["login"]
            and env["GITHUB_TRIGGERING_ACTOR"] == trigger["login"]
            and run["id"] == int(run_id)
            and run["run_attempt"] == int(attempt)
            and run["event"] == "workflow_dispatch"
            and run["head_branch"] == "cmake"
            and run["head_sha"] == env["GITHUB_SHA"]
            and run["path"] == ".github/workflows/nightly-tooling-sweep.yml"
            and run["repository"]["id"] == REPOSITORY_ID
        ):
            raise ValueError
        pr = fetch("pulls/" + number)
        head = pr["head"]
        if not (
            pr["number"] == int(number)
            and pr["state"] == "open"
            and pr["user"]["id"] == actor["id"]
            and pr["base"]["repo"]["id"] == REPOSITORY_ID
            and head["repo"]["id"] == REPOSITORY_ID
            and head["repo"]["full_name"] == retail.REPOSITORY
            and head["repo"]["fork"] is False
            and head["sha"] == sha
            and fetch("git/ref/heads/" + quote(head["ref"], safe=""))["object"]["sha"] == sha
        ):
            raise ValueError
    except (KeyError, TypeError, ValueError, subprocess.SubprocessError) as error:
        raise ValueError("Candidate qualification provenance was not established") from error
    return sha


if __name__ == "__main__":
    print(verify() or "")
