"""Asset-free controls for the actual earned-phase CMake receipt consumer."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(cmake):
    owner = Path(__file__).with_name("pumo_player_phase.cmake").resolve()
    with tempfile.TemporaryDirectory(prefix="pumo-receipt-") as temporary:
        root = Path(temporary)
        output = root / "output"
        output.mkdir()
        (output / "preview-user").mkdir()
        engine = root / "engine"
        engine.write_text("public receipt fixture executable identity\n")
        plan = root / "plan.json"
        plan.write_text("{}\n")
        save = output / "preview-user" / "checkpoint.sav"
        save.write_bytes(b"public derived checkpoint\n")
        expected = {
            "receipt_version": 2,
            "result": "PASS",
            "steps": 3,
            "initial_level_setup": 7,
            "final_level": 8,
            "resumed_earned_checkpoint": True,
            "process_id": 123,
            "run_id": "new-process-nonce",
            "plan_sha256": sha(plan),
            "executable_sha256": sha(engine),
            "resume_sha256": sha(save),
            "transitions": [{"from": 7, "to": 8}],
            "checkpoints": {"8": {"file": save.name, "sha256": sha(save)}},
            "checkpoint_history": [{"step": 1, "file": save.name, "sha256": sha(save), "state": {}}],
        }
        state = {"current_level_num": 8, "player_dead": False, "cheats_enabled": False}
        script = root / "validate.cmake"
        script.write_text(
            f'set(OUTPUT "{output.as_posix()}")\n'
            f'set(ENGINE "{engine.as_posix()}")\n'
            f'set(executed_plan "{plan.as_posix()}")\n'
            'set(expected_run_id "new-process-nonce")\n'
            f'set(expected_plan_sha256 "{sha(plan)}")\n'
            f'set(expected_executable_sha256 "{sha(engine)}")\n'
            f'set(expected_resume_sha256 "{sha(save)}")\n'
            f'include("{owner.as_posix()}")\n'
            "validate_pumo_player_receipt(control 7 8 3 1)\n"
        )

        def check(label, receipt=None, final_state=None, missing=None, raw=None, passes=False):
            (output / "player-result.json").write_text(raw if raw is not None else json.dumps(receipt or expected))
            (output / "player-state.json").write_text(json.dumps(final_state or state))
            (output / "player-trace.jsonl").write_text('{"event":"public-derived"}\n')
            if missing:
                (output / missing).unlink()
            result = subprocess.run([cmake, "-P", str(script)], capture_output=True, text=True, timeout=10)
            if (result.returncode == 0) != passes:
                raise AssertionError(f"{label}: {result.stdout}{result.stderr}")

        check("valid receipt", passes=True)
        for missing in ("player-result.json", "player-state.json", "player-trace.jsonl"):
            check(f"missing {missing}", missing=missing)
        check("truncated receipt", raw='{"result":')
        for field, value in (
            ("receipt_version", 1),
            ("result", "FAIL"),
            ("steps", 2),
            ("initial_level_setup", 6),
            ("final_level", 9),
            ("resumed_earned_checkpoint", False),
            ("process_id", 0),
            ("run_id", "old-process-nonce"),
            ("plan_sha256", "0" * 64),
            ("executable_sha256", "0" * 64),
            ("resume_sha256", "0" * 64),
            ("transitions", []),
            ("transitions", [{"from": 6, "to": 8}]),
            ("checkpoints", {"8": {"file": save.name, "sha256": "0" * 64}}),
            ("checkpoints", {"8": {"file": "../checkpoint.sav", "sha256": sha(save)}}),
        ):
            check(f"wrong {field}: {value}", receipt={**expected, field: value})
        for field in ("receipt_version", "steps", "initial_level_setup", "final_level", "process_id"):
            check(f"string instead of number {field}", receipt={**expected, field: str(expected[field])})
        check("string instead of boolean", receipt={**expected, "resumed_earned_checkpoint": "true"})
        check("string final dead flag", final_state={**state, "player_dead": "false"})
        check("wrong transition field type", receipt={**expected, "transitions": [{"from": "7", "to": 8}]})
        check(
            "duplicate checkpoint history",
            receipt={**expected, "checkpoint_history": expected["checkpoint_history"] * 2},
        )
        check(
            "wrong history hash",
            receipt={**expected, "checkpoint_history": [{**expected["checkpoint_history"][0], "sha256": "0" * 64}]},
        )
        for field in expected:
            incomplete = dict(expected)
            del incomplete[field]
            check(f"missing receipt {field}", receipt=incomplete)
        check("dead state", final_state={**state, "player_dead": True})
        check("wrong state destination", final_state={**state, "current_level_num": 9})
        check("cheat state", final_state={**state, "cheats_enabled": True})
        engine.write_text("changed executable\n")
        check("executable changed during run")
        if os.name != "nt":
            # Exercise the actual launcher: a zero exit with only stale files is failure
            engine.write_text("#!/bin/sh\nexit 0\n")
            engine.chmod(0o700)
            (output / "phase-plan.json").write_text(
                json.dumps({"resume_checkpoint": save.name, "resume_sha256": sha(save)})
            )
            script.write_text(
                f'set(OUTPUT "{output.as_posix()}")\n'
                f'set(ENGINE "{engine.as_posix()}")\n'
                'set(DATA "unused")\nset(RESOURCES "unused")\n'
                f'include("{owner.as_posix()}")\n'
                "run_pumo_player_phase(phase 7 8 3 1)\n"
            )
            (output / "player-result.json").write_text(json.dumps(expected))
            (output / "player-state.json").write_text(json.dumps(state))
            (output / "player-trace.jsonl").write_text("{}\n")
            launched = subprocess.run([cmake, "-P", str(script)], capture_output=True, text=True, timeout=10)
            if launched.returncode == 0 or "missing newly generated" not in launched.stderr:
                raise AssertionError(f"stale launcher receipt accepted: {launched.stderr}")
            for member in ("result.json", "state.json", "trace.jsonl"):
                if (output / f"player-{member}").exists():
                    raise AssertionError(f"stale launcher output survived: {member}")
            engine.write_text("#!/bin/sh\nexit 7\n")
            launched = subprocess.run([cmake, "-P", str(script)], capture_output=True, text=True, timeout=10)
            if launched.returncode == 0 or "failed (7)" not in launched.stderr:
                raise AssertionError(f"process failure misclassified: {launched.stderr}")
    print("Pumo earned-phase receipt controls passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake")
    run(parser.parse_args().cmake)
