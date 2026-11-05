"""Default-branch controller for automatic owned-PR XL qualification."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import quote

from nightly_candidate import KNOWN_ACTORS, REPOSITORY_ID, api
import nightly_retail as retail

REQUEST_PATH = ".github/workflows/xl-asset-request.yml"
CONTROLLER_PATH = ".github/workflows/xl-pr-assets.yml"
STATUS_CONTEXT = "XL asset smoke (private assets)"


def verify(target_sha=None, env=None, fetch=api, event=None):
    env = os.environ if env is None else env
    try:
        if not (
            retail.trusted_context(env)
            and env.get("GITHUB_EVENT_NAME") == "workflow_run"
            and env.get("GITHUB_WORKFLOW_REF") == retail.PR_WORKFLOW
        ):
            raise ValueError
        event = json.loads(Path(env["GITHUB_EVENT_PATH"]).read_text()) if event is None else event
        source_id = env["XL_SOURCE_RUN_ID"]
        run_id = env["GITHUB_RUN_ID"]
        if not source_id.isdecimal() or not run_id.isdecimal():
            raise ValueError
        current, source = fetch("actions/runs/" + run_id), fetch("actions/runs/" + source_id)
        actor = current["actor"]
        if not (
            actor["id"] in KNOWN_ACTORS
            and actor["login"] == KNOWN_ACTORS[actor["id"]]
            and current["triggering_actor"]["id"] == actor["id"]
            and current["triggering_actor"]["login"] == actor["login"]
            and env["GITHUB_ACTOR_ID"] == str(actor["id"])
            and env["GITHUB_ACTOR"] == actor["login"]
            and env["GITHUB_TRIGGERING_ACTOR"] == actor["login"]
            and current["id"] == int(run_id)
            and current["run_attempt"] == int(env["GITHUB_RUN_ATTEMPT"])
            and current["event"] == "workflow_run"
            and current["path"] == CONTROLLER_PATH
            and current["head_branch"] == "cmake"
            and current["head_sha"] == env["GITHUB_SHA"]
            and current["repository"]["id"] == REPOSITORY_ID
            and event["action"] == "completed"
            and event["workflow_run"]["id"] == source["id"] == int(source_id)
            and event["workflow_run"]["run_attempt"] == source["run_attempt"]
            and event["workflow_run"]["head_sha"] == source["head_sha"]
            and source["repository"]["id"] == REPOSITORY_ID
            and source["head_repository"]["id"] == REPOSITORY_ID
            and source["event"] == "pull_request"
            and source["status"] == "completed"
            and source["conclusion"] == "success"
            and source["path"] == REQUEST_PATH
            and source["actor"]["id"] == source["triggering_actor"]["id"] == actor["id"]
            and source["actor"]["login"] == source["triggering_actor"]["login"] == actor["login"]
        ):
            raise ValueError
        sha = source["head_sha"]
        if not re.fullmatch("[0-9a-f]{40}", sha):
            raise ValueError
        # GitHub can omit pull_requests on the run; resolve the server-side commit association
        associations = fetch("commits/" + sha + "/pulls?per_page=100")
        if not isinstance(associations, list) or len(associations) >= 100:
            raise ValueError
        matching = [
            pr
            for pr in associations
            if pr["state"] == "open" and pr["head"]["sha"] == sha and pr["head"]["ref"] == source["head_branch"]
        ]
        if len(matching) != 1:
            raise ValueError
        number = matching[0]["number"]
        if type(number) is not int or number <= 0:
            raise ValueError
        pr = fetch("pulls/" + str(number))
        head = pr["head"]
        if not (
            pr["number"] == number
            and pr["state"] == "open"
            and pr["user"]["id"] == actor["id"]
            and pr["base"]["repo"]["id"] == REPOSITORY_ID
            and head["repo"]["id"] == REPOSITORY_ID
            and head["repo"]["full_name"] == retail.REPOSITORY
            and head["ref"] == source["head_branch"]
            and head["sha"] == sha
            and fetch("git/ref/heads/" + quote(head["ref"], safe=""))["object"]["sha"] == sha
            and (target_sha is None or target_sha == sha)
            and env.get("CANDIDATE_SHA", sha) == sha
            and env.get("CANDIDATE_PR", str(number)) == str(number)
        ):
            raise ValueError
    except (KeyError, TypeError, ValueError, OSError, subprocess.SubprocessError) as error:
        raise ValueError("Automatic XL candidate provenance was not established") from error
    return {"number": number, "sha": sha}


def positive_receipt(receipt, sha):
    from run_xl_originals import reviewed_fast_cohort

    if not isinstance(receipt, dict):
        raise ValueError("Invalid XL receipt object")
    rows = receipt.get("results", [])
    if receipt.get("target_sha") != sha or not isinstance(rows, list) or len(rows) != 1:
        raise ValueError("XL receipt does not match the exact single-owner target")
    row = rows[0]
    if not isinstance(row, dict) or not isinstance(row.get("xl_checks"), dict):
        raise ValueError("Invalid XL receipt checks")
    checks = row["xl_checks"]
    reviewed_fast_cohort(checks)
    if not (
        row.get("id") == "host-profile:retail-xl-fast"
        and row.get("status") == "PASS"
        and type(checks.get("format_witnesses")) is int
        and checks.get("format_witnesses") == 16
        and type(checks.get("modern_source_levels")) is int
        and checks.get("modern_source_levels") == 3
        and type(checks.get("navigation_smoke_passed")) is int
        and checks["navigation_smoke_passed"] == 1
        and type(checks.get("navigation_anchors_passed")) is int
        and checks.get("navigation_anchors_passed") == 4
        and type(checks.get("navigation_smoke_milliseconds")) is int
        and 0 <= checks["navigation_smoke_milliseconds"] <= 30000
    ):
        raise ValueError("XL receipt lacks a successful original load and navigation smoke")
    return checks["navigation_smoke_milliseconds"]


def status(sha, state, description):
    if not re.fullmatch("[0-9a-f]{40}", sha):
        raise ValueError("Invalid status target")
    payload = {
        "state": state,
        "context": STATUS_CONTEXT,
        "description": description,
        "target_url": "https://github.com/" + retail.REPOSITORY + "/actions/runs/" + os.environ["GITHUB_RUN_ID"],
    }
    subprocess.run(
        ["gh", "api", "--method", "POST", "repos/" + retail.REPOSITORY + "/statuses/" + sha, "--input", "-"],
        input=json.dumps(payload),
        text=True,
        capture_output=True,
        check=True,
        timeout=30,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("admit", "report"))
    parser.add_argument("--receipt", type=Path)
    args = parser.parse_args()
    if args.command == "admit":
        candidate = verify()
        sys.path.insert(0, str(Path(__file__).resolve().parents[2] / ".github" / "scripts"))
        from stack_ci import refresh

        run = refresh(candidate["number"], retail.REPOSITORY)
        with open(os.environ["GITHUB_OUTPUT"], "a") as output:
            output.write(f"eligible={str(run).lower()}\n")
        if not run:
            print("Skipping private XL qualification below the native stack tip")
            return 0
        status(candidate["sha"], "pending", "Running original XL load and assisted navigation")
        with open(os.environ["GITHUB_OUTPUT"], "a") as output:
            output.write(f"sha={candidate['sha']}\nnumber={candidate['number']}\n")
        return 0
    sha = os.environ["CANDIDATE_SHA"]
    try:
        verify(sha)
        if os.environ.get("XL_QUALIFICATION_RESULT") != "success":
            raise ValueError("XL owner did not finish successfully")
        if args.receipt.is_symlink() or args.receipt.stat().st_size > 1024 * 1024:
            raise ValueError("XL receipt exceeds the public receipt bound")
        elapsed = positive_receipt(json.loads(args.receipt.read_text()), sha)
    except (OSError, ValueError, TypeError) as error:
        status(sha, "failure", "XL load/navigation was not qualified; inspect the trusted run")
        raise ValueError("XL asset smoke was not qualified") from error
    status(sha, "success", f"Four assisted XL route anchors passed ({elapsed} ms)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
