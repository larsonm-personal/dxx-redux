"""Generate three bounded input-order candidates without changing any old oracle.

This is equivalent-behavior conversion, not reconstruction of lost event order.
A candidate is usable only after its replay passes the original complete oracle.
Failures (including unknown input-time RNG) require a fresh live capture instead.
"""

import argparse
import copy
import json
import hashlib
from pathlib import Path

import compact_public_replay as compact
import d1_replay_parity as parity
import ordered_public_replay as ordered

SCHEDULES = ("sample-only", "reset-before-sample", "prior-held-then-current")
# Native item helpers consume these counts; primary count survives until simulation
CONSUMED_PULSES = frozenset(("f2", "fl", "db", "cp", "cs", "sw", "tb", "hl"))


def _terminal_rng(state_trace, rng_trace, last_frame):
    terminals = [
        row
        for row in parity.records(state_trace)
        if row.get("type") == "object_boundary" and row.get("phase") == "terminal"
    ]
    if len(terminals) != 1:
        raise parity.EvidenceError("Conversion needs one verified terminal RNG boundary")
    streams = terminals[0].get("rng", [])
    if not streams or streams[0].get("available") != 1:
        raise parity.EvidenceError("Conversion needs available terminal SIM RNG")
    result = {"s": streams[0]["state"], "c": streams[0]["calls"]}
    events = list(parity.rng_events(rng_trace))
    last = events[-1] if events else None
    expected = {"s": last["state_after"], "c": last["call_count"]} if last else last_frame["rng"]
    if result != expected:
        raise parity.EvidenceError("Terminal RNG disagrees with the unchanged verified SIM stream")
    return result


def candidate_rows(rows, schedule, terminal_rng):
    """Apply one schedule to the whole recording; never search per-frame choices."""
    if schedule not in SCHEDULES or rows[0]["version"] not in (2, 3, 4):
        raise parity.EvidenceError("Expected one known schedule and a legacy public recording")
    header = copy.deepcopy(rows[0])
    header["version"] = 5
    state, previous_state, frames = {}, {}, []
    frame_time = None
    input_time = rows[1].get("ft")
    for old in rows[1:-1]:
        frame_time = old.get("ft", frame_time)
        state.update(old["input"].get("s", {}))
        state = {key: value for key, value in state.items() if value}
        pulse = old["input"].get("p", {})
        if any(state.get(key) or pulse.get(key) for key in ("rvs", "ams", "rv", "am")):
            raise parity.EvidenceError("Wall-clock rear view and modal automap require fresh qualified capture")
        ops = []
        if schedule == "prior-held-then-current" and previous_state.get("f2s"):
            ops.append({"k": "sample", "ft": input_time, "s": dict(previous_state)})
        if schedule != "sample-only":
            ops.append({"k": "reset", "ft": input_time})
        ops.append({"k": "sample", "ft": input_time, "s": dict(state), "p": dict(pulse)})
        frame = copy.deepcopy(old)
        frame["input"] = {"s": dict(old["input"].get("s", {}))}
        residual = {key: value for key, value in pulse.items() if value and key not in CONSUMED_PULSES}
        if residual:
            frame["input"]["p"] = residual
        # This explicit candidate assumes no input SIM draw. Native post-input
        # validation and the unchanged RNG oracle reject a wrong assumption
        frame["dispatch"] = {"pre_rng": dict(old["rng"]), "ops": ops}
        frames.append(frame)
        previous_state = dict(state)
        input_time = frame_time
    tail_ops = []
    if schedule == "prior-held-then-current" and state.get("f2s"):
        tail_ops.append({"k": "sample", "ft": frame_time, "s": dict(state)})
    tail = {
        "type": "input_tail",
        "f": header["frame_count"],
        "input": {"s": dict(state)},
        "rng": dict(terminal_rng),
        "dispatch": {"pre_rng": dict(terminal_rng), "ops": tail_ops},
    }
    frames, tail = ordered.compact(header, frames, tail)
    return [header, *frames, tail, copy.deepcopy(rows[-1])]


def convert(demo, oracle, state_trace, rng_trace, result, output, source_revision):
    """Verify the baseline first, then write new candidate inputs and provenance only."""
    demo, oracle, output = Path(demo), Path(oracle), Path(output)
    if not compact._digest(source_revision, 40):
        raise parity.EvidenceError("Conversion requires the exact original corpus commit")
    header, _ = compact.validate(demo, oracle)
    if header["version"] == 5:
        raise parity.EvidenceError("Recording already preserves ordered input")
    compact.verify(header["game"], demo, oracle, state_trace, rng_trace, result)
    _, rows = compact._records(demo, full=False)
    terminal_rng = _terminal_rng(state_trace, rng_trace, rows[-2])
    candidates, ineligible = [], []
    for schedule in SCHEDULES:
        path = output / schedule / demo.name
        sidecar = path.with_suffix(".migration.json")
        if path.exists() or sidecar.exists() or path.resolve() in (demo.resolve(), oracle.resolve()):
            raise parity.EvidenceError("Conversion requires separate new output files")
        converted = candidate_rows(rows, schedule, terminal_rng)
        raw = b"".join(compact.canonical(row) + b"\n" for row in converted)
        if len(raw) > compact.MAX_BYTES:
            ineligible.append(
                {"schedule": schedule, "bytes": len(raw), "status": "size-ineligible", "semantic_verdict": "not-run"}
            )
            continue
        metadata = {
            "schema": 1,
            "kind": "ordered-input-candidate",
            "schedule": schedule,
            "original_corpus_revision": source_revision,
            "source_demo_sha256": parity.digest(demo),
            "source_oracle_sha256": parity.digest(oracle),
            "converted_demo_sha256": hashlib.sha256(raw).hexdigest(),
            "baseline_sha256": {
                "state": parity.digest(state_trace),
                "rng": parity.digest(rng_trace),
                "result": parity.digest(result),
            },
        }
        candidates.append((path, sidecar, raw, metadata))
    for path, sidecar, raw, metadata in candidates:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            stream.write(raw)
        with sidecar.open("x", encoding="utf-8") as stream:
            stream.write(json.dumps(metadata, indent=2, sort_keys=True) + "\n")
    return {
        "source_demo": str(demo),
        "source_oracle_sha256": parity.digest(oracle),
        "candidates": [
            {"schedule": value[3]["schedule"], "demo": str(value[0]), "bytes": len(value[2])} for value in candidates
        ],
        "size_ineligible": ineligible,
        "status": "unverified-candidates",
        "historical_event_order_recovered": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("demo", "oracle", "baseline-state", "baseline-rng", "baseline-result", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--source-revision", required=True)
    args = parser.parse_args()
    report = convert(
        args.demo,
        args.oracle,
        args.baseline_state,
        args.baseline_rng,
        args.baseline_result,
        args.output,
        args.source_revision,
    )
    print(json.dumps(report, sort_keys=True, indent=2))


if __name__ == "__main__":
    main()
