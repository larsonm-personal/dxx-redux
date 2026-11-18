"""Inventory additional local D2X-XL downloads for the on-demand readiness survey."""

import argparse
import json
from pathlib import Path
import tempfile

import xl_readiness as readiness
from nightly_corpus_stage import extract_archive


def inventory(downloads, pinned, work, seven_zip):
    work.mkdir(parents=True, exist_ok=False)
    known = {row["sha256"]: row["path"] for row in json.loads(pinned.read_text())["archives"]}
    archives, cases, issues, duplicates = [], [], [], []
    for source in sorted(downloads.rglob("*")):
        if not source.is_file() or source.suffix.lower() not in (".zip", ".rar", ".7z"):
            continue
        name = source.relative_to(downloads).as_posix()
        sha = readiness.digest(source)
        if sha in known:
            duplicates.append({"path": name, "same_bytes_as": known[sha], "sha256": sha})
            continue
        archives.append({"path": name, "size": source.stat().st_size, "sha256": sha})
        with tempfile.TemporaryDirectory(dir=work.parent) as temp:
            root = Path(temp) / "package"
            try:
                extract_archive(source, root, seven_zip)
                descriptors = sorted(p for p in root.rglob("*") if p.suffix.lower() in (".mn2", ".msn") and p.is_file())
                if not descriptors:
                    issues.append(
                        {
                            "archive": name,
                            "reason": "No loose mission descriptor",
                            "members": [p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()],
                        }
                    )
                for descriptor in descriptors:
                    relative = descriptor.relative_to(root).as_posix()
                    try:
                        advertised = readiness.levels(descriptor)
                    except (OSError, ValueError) as error:
                        issues.append({"archive": name, "descriptor": relative, "reason": str(error)})
                        continue
                    descriptor_sha = readiness.digest(descriptor)
                    for level in advertised:
                        cases.append(
                            {
                                **level,
                                "archive": name,
                                "descriptor": relative,
                                "descriptor_sha256": descriptor_sha,
                                "mission": descriptor.stem.lower(),
                                "id": f"{name}/{relative}:{level['level']}",
                            }
                        )
            except (ValueError, OSError) as error:
                issues.append({"archive": name, "reason": str(error)})
        print(f"{name} ({len(cases)} levels inventoried)", flush=True)
    readiness.write(
        work / "inventory.json",
        {
            "schema": "dxx-xl-local-inventory-v1",
            "asset_commit": None,
            "selection_seed": "all-local",
            "archives": archives,
            "cases": cases,
            "selected": [c["id"] for c in cases],
        },
    )
    readiness.write(work / "issues.json", issues)
    readiness.write(work / "duplicates.json", duplicates)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--downloads", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--seven-zip", type=Path)
    parser.add_argument("--pinned", type=Path, default=Path(__file__).with_name("xl_readiness_inventory.json"))
    args = parser.parse_args()
    inventory(args.downloads, args.pinned, args.work, args.seven_zip)
