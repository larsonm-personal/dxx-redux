"""Real Git tree histories for canonical review paths, including Win32-unrepresentable names."""

import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def run_fixture(workspace, helper):
    repo = workspace / "virtual-history"
    repo.mkdir()
    entries = {}

    def git(*args, data=None):
        result = subprocess.run(
            [
                "git",
                "-C",
                str(repo),
                "-c",
                "user.name=Fixture",
                "-c",
                "user.email=fixture@example.invalid",
                "-c",
                "commit.gpgsign=false",
                "-c",
                "core.autocrlf=false",
                "-c",
                "core.hooksPath=no-hooks",
                *args,
            ],
            input=data,
            capture_output=True,
            timeout=20,
        )
        if result.returncode:
            raise AssertionError(result.stderr.decode("utf-8"))
        return result.stdout

    def update(path, content, mode="100644"):
        blob = git("hash-object", "-w", "--stdin", data=content.encode("utf-8")).strip()
        entries[path] = (mode, blob)
        return blob.decode("ascii")

    def delete(path):
        del entries[path]

    def commit(message, parent=None):
        # Build tree objects directly: Git for Windows indexes reject control names.
        def tree(items):
            records = []
            directories = {}
            for path, (mode, blob) in items.items():
                name, separator, rest = path.partition("/")
                if separator:
                    directories.setdefault(name, {})[rest] = (mode, blob)
                else:
                    records.append(mode.encode("ascii") + b" blob " + blob + b"\t" + name.encode("utf-8") + b"\0")
            for name, children in directories.items():
                records.append(b"040000 tree " + tree(children) + b"\t" + name.encode("utf-8") + b"\0")
            return git("mktree", "-z", data=b"".join(records)).strip()

        args = ["commit-tree", tree(entries).decode("ascii"), "-m", message]
        if parent:
            args += ["-p", parent]
        tip = git(*args).decode("ascii").strip()
        git("update-ref", "refs/heads/fixture", tip)
        return tip

    def lines(name):
        return "".join(f"int {name}_{i} = {i};\n" for i in range(1, 31))

    git("init", "--quiet")
    git("symbolic-ref", "HEAD", "refs/heads/fixture")
    target = repo / "android/helpers/new_adversarial_review_ledger.ps1"
    target.parent.mkdir(parents=True)
    target.write_bytes(helper.read_bytes())
    entries["android/helpers/new_adversarial_review_ledger.ps1"] = (
        "100644",
        git("hash-object", "-w", "--stdin", data=helper.read_bytes()).strip(),
    )
    source = "android/app/src/main/cpp/shared/"
    originals = {
        "original.c": lines("original"),
        "edited-old.c": lines("edited"),
        "copy-source.c": lines("copy_source") + "\n\n",
        "modified.c": lines("modified"),
        "deleted.c": "int deleted = 1;\n",
        "space name.cpp": "int space_name = 1;\n",
        "caf\u00e9.c": "int unicode_name = 1;\n",
        "tab\tname.c": lines("tab_name"),
        "line\nname.c": lines("line_name"),
        "old space.cpp": lines("old_space"),
    }
    for name, content in originals.items():
        update(source + name, content)
    update("android/helpers/mode.sh", "#!/bin/sh\necho fixture\n")
    entries["assets/data.bin"] = ("100644", git("hash-object", "-w", "--stdin", data=b"\x00binary-before").strip())
    base = commit("before")
    delete(source + "original.c")
    update(source + "renamed.c", originals["original.c"])
    delete(source + "edited-old.c")
    update(source + "edited-new.c", originals["edited-old.c"].replace("= 15;", "= 99;"))
    update(source + "copied.c", originals["copy-source.c"])
    update(source + "modified.c", originals["modified.c"].replace("= 20;", "= 77;"))
    delete(source + "deleted.c")
    delete(source + "old space.cpp")
    update(source + "new space.cpp", originals["old space.cpp"])
    for name in ("space name.cpp", "caf\u00e9.c", "tab\tname.c", "line\nname.c"):
        update(source + name, originals[name].replace("= 1;", "= 42;"))
    update(source + "added.cpp", "int newly_added = 3;\n")
    update("android/helpers/mode.sh", "#!/bin/sh\necho fixture\n", "100755")
    entries["assets/data.bin"] = ("100644", git("hash-object", "-w", "--stdin", data=b"\x00binary-after").strip())
    head = commit("after", base)

    output = repo / "review.md"
    command = [
        "pwsh",
        "-NoProfile",
        "-File",
        str(target),
        "-BaseRef",
        base,
        "-HeadRef",
        head,
        "-CampaignId",
        "PATHS",
        "-OutputPath",
        str(output),
    ]
    result = subprocess.run(command, capture_output=True, timeout=60)
    assert result.returncode == 0, result.stdout.decode("utf-8") + result.stderr.decode("utf-8")
    assert not result.stderr, result.stderr.decode("utf-8")
    text = output.read_text(encoding="utf-8")
    match = re.search(r"## Canonical path inventory\n.*?```json\n(.*?)\n```", text, re.S)
    assert match, "Generator omitted reconstructible canonical path inventory"
    inventory = json.loads(match.group(1))
    expected = {
        source + "renamed.c": ("R100", source + "original.c", "authored-source"),
        source + "edited-new.c": ("R", source + "edited-old.c", "authored-source"),
        source + "copied.c": ("C100", source + "copy-source.c", "authored-source"),
        source + "modified.c": ("M", "", "authored-source"),
        source + "deleted.c": ("D", "", "authored-source"),
        source + "new space.cpp": ("R100", source + "old space.cpp", "authored-source"),
        source + "added.cpp": ("A", "", "authored-source"),
        "assets/data.bin": ("M", "", "artifact"),
        "android/helpers/mode.sh": ("M", "", "build-script"),
        **{
            source + name: ("M", "", "authored-source")
            for name in ("space name.cpp", "caf\u00e9.c", "tab\tname.c", "line\nname.c")
        },
    }
    assert len(inventory) == len(expected) == 13
    assert {item["path"] for item in inventory} == set(expected)
    changed_lines = {
        source + "edited-new.c": 15,
        source + "modified.c": 20,
        **{source + name: 1 for name in ("space name.cpp", "caf\u00e9.c", "tab\tname.c", "line\nname.c")},
    }
    for item in inventory:
        status, old_path, kind = expected[item["path"]]
        assert item["status"].startswith(status), item
        assert item["old_path"] == old_path and item["kind"] == kind, item
        assert item["risk"] == (
            "mechanical" if kind == "artifact" else "medium" if kind == "build-script" else "high"
        ), item
        if status not in ("A", "C100") and kind != "artifact":
            assert sum(h["OldCount"] for h in item["hunks"]) == item["deleted"], item
            assert sum(h["NewCount"] for h in item["hunks"]) == item["added"], item
        if item["path"] in changed_lines:
            line = changed_lines[item["path"]]
            assert len(item["hunks"]) == 1, item
            hunk = item["hunks"][0]
            assert (hunk["OldStart"], hunk["NewStart"], hunk["OldCount"], hunk["NewCount"]) == (line, line, 1, 1), item
            assert item["scopes"] == [f"diff hunks 1-1, new L{line}-L{line}"], item
        if status == "D":
            assert item["scopes"] == ["diff hunks 1-1, old L1-L1"], item
        if status == "C100":
            assert item["scopes"] == ["L1-L32"], item
        if kind != "artifact":
            assert item["scopes"], item
    assert "Changed paths: 13" in text
    assert " => " not in text
    # Control paths stay reversible both in the JSON inventory and readable queue.
    queue = text.split("## Review queue", 1)[1].split("## Chunk completion notes", 1)[0]
    assert "tab\\tname.c" in queue and "line\\nname.c" in queue
    before = re.sub(rb"(?m)^- Generated: [^\n]+", b"- Generated: 2000-01-01 00:00:00 UTC", output.read_bytes())
    output.write_bytes(before)
    modified = output.stat().st_mtime_ns
    refusal = subprocess.run(command, capture_output=True, timeout=60)
    assert refusal.returncode != 0 and b"Output already exists" in refusal.stderr
    assert output.read_bytes() == before and output.stat().st_mtime_ns == modified
    rerun = subprocess.run(command + ["-Overwrite"], capture_output=True, timeout=60)
    assert rerun.returncode == 0, rerun.stderr.decode("utf-8")
    assert output.read_bytes() == before and output.stat().st_mtime_ns == modified
    (workspace / "canonical-path-evidence.json").write_text(
        json.dumps(
            {"base": base, "head": head, "paths": inventory, "ledger_sha256": hashlib.sha256(before).hexdigest()},
            indent=2,
            ensure_ascii=True,
        )
        + "\n",
        encoding="ascii",
    )
    print(
        "Virtual Git histories: 13 canonical paths, copies/renames/control names, exact hunks/scopes and deterministic overwrite/refusal PASS"
    )


if __name__ == "__main__":
    workspace = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    if not workspace.is_relative_to((root / "android/temp").resolve()):
        raise SystemExit("Fixture workspace must be inside android/temp")
    run_fixture(workspace, Path(sys.argv[2]).resolve())
