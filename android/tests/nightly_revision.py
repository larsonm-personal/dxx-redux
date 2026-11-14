"""Pin the latest reachable push by the authorized account using GitHub activity."""

import json
import os
from pathlib import Path
import re
import subprocess
from urllib.parse import quote

from nightly_candidate import KNOWN_ACTORS, REPOSITORY_ID, api
import nightly_retail as retail

SHA = re.compile(r"[0-9a-f]{40}")
KINDS = {"push", "force_push", "branch_creation", "pr_merge", "merge_queue_merge"}


def pages(path):
    # gh follows the server's Link cursors, including the repository activity API
    result = subprocess.run(
        ["gh", "api", "--paginate", "--jq", ".[] | @json", "repos/" + retail.REPOSITORY + "/" + path],
        check=True,
        capture_output=True,
        text=True,
        timeout=180,
    )
    values = [json.loads(line) for line in result.stdout.splitlines() if line.strip()]
    if any(not isinstance(row, dict) for row in values):
        raise ValueError("Invalid paginated provenance response")
    return values


def snapshot(fetch_pages=pages):
    branches = fetch_pages("branches?per_page=100")
    activity = fetch_pages("activity?per_page=100&direction=desc")
    refs = {"refs/heads/" + row["name"]: row["commit"]["sha"] for row in branches}
    if len(refs) != len(branches) or any(not SHA.fullmatch(sha) for sha in refs.values()):
        raise ValueError("Invalid branch snapshot")
    return refs, activity


def select(refs, activities, fetch=api):
    candidates = []
    for row in activities:
        actor = row.get("actor") or {}
        if actor.get("id") not in KNOWN_ACTORS or row.get("activity_type") not in KINDS or row.get("ref") not in refs:
            continue
        sha = row.get("after", "")
        if not SHA.fullmatch(sha) or sha == "0" * 40:
            continue
        # Timestamp is GitHub's push time, never a committer-controlled git date
        timestamp = row.get("timestamp", "")
        if not re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", timestamp) or type(row.get("id")) is not int:
            raise ValueError("Invalid activity identity")
        candidates.append(row)
    for row in sorted(candidates, key=lambda r: (r["timestamp"], r["id"], r["ref"], r["after"]), reverse=True):
        sha, tip = row["after"], refs[row["ref"]]
        if tip != sha:
            comparison = fetch("compare/" + sha + "..." + tip)
            if comparison.get("status") not in ("ahead", "identical"):
                continue
        return {
            "sha": sha,
            "ref": row["ref"],
            "activity_id": row["id"],
            "timestamp": row["timestamp"],
            "actor_id": row["actor"]["id"],
        }
    raise ValueError("No reachable revision with authorized push provenance")


def resolve(fetch=api, fetch_pages=pages):
    if fetch("")["id"] != REPOSITORY_ID:
        raise ValueError("Repository identity mismatch")
    # A concurrent push/deletion must not silently change the selection
    for _ in range(3):
        before = snapshot(fetch_pages)
        result = select(*before, fetch=fetch)
        after = snapshot(fetch_pages)
        if before == after:
            return result
    raise ValueError("Repository changed during revision selection; retry")


def verify(sha, env=None, fetch=api, fetch_pages=pages):
    env = os.environ if env is None else env
    if not retail.trusted_context(env) or env.get("GITHUB_EVENT_NAME") not in ("schedule", "workflow_dispatch"):
        raise ValueError("Nightly revision requires the trusted default-branch controller")
    if env.get("NIGHTLY_TARGET_SHA") != sha or not SHA.fullmatch(sha):
        raise ValueError("Nightly target mismatch")
    if fetch("")["id"] != REPOSITORY_ID:
        raise ValueError("Repository identity mismatch")
    ref = env["NIGHTLY_TARGET_REF"]
    if not ref.startswith("refs/heads/"):
        raise ValueError("Nightly target is not a branch")
    rows = fetch_pages("activity?per_page=100&ref=" + quote(ref, safe=""))
    matching = [r for r in rows if str(r.get("id")) == env["NIGHTLY_ACTIVITY_ID"]]
    tip = fetch("git/ref/heads/" + quote(ref[len("refs/heads/") :], safe=""))["object"]["sha"]
    if len(matching) != 1 or select({ref: tip}, matching, fetch)["sha"] != sha:
        raise ValueError("Pinned authorized revision is no longer reachable")
    return sha


if __name__ == "__main__":
    if not retail.trusted_context():
        raise ValueError("Nightly selection requires the trusted controller")
    selected = resolve()
    Path("plan").mkdir(exist_ok=True)
    Path("plan/revision.json").write_text(json.dumps(selected, indent=2) + "\n")
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        for key, value in selected.items():
            output.write(f"{key}={value}\n")
    with open(os.environ["GITHUB_ENV"], "a") as output:
        for key in ("sha", "ref", "activity_id"):
            name = "NIGHTLY_ACTIVITY_ID" if key == "activity_id" else "NIGHTLY_TARGET_" + key.upper()
            output.write(f"{name}={selected[key]}\n")
