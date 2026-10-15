"""Preflight recorded asset identities; this never adds fingerprints to old demos."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
REQUIRED = {
    "native_d1": {"d1_hog", "d1_pig"},
    "native_d2": {"d2_hog", "d2_pig", "d2_ham"},
    "d1_in_d2": {"d1_hog", "d1_pig"},
}
ROLES = {"d1_hog", "d1_pig", "d2_hog", "d2_pig", "d2_ham", "mission_hog"}
ROLE_NAMES = {
    "d1_hog": ("descent.hog",),
    "d1_pig": ("descent.pig",),
    "d2_hog": ("descent2.hog", "d2demo.hog"),
    "d2_ham": ("descent2.ham", "d2demo.ham"),
}


class AssetError(ValueError):
    pass


def _object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise AssetError(f"Duplicate asset/header JSON field: {key}")
        value[key] = item
    return value


def read_header(path):
    with Path(path).open(encoding="utf-8-sig") as stream:
        for line in stream:
            if line.strip() and not line.lstrip().startswith("//"):
                value = json.loads(line, object_pairs_hook=_object)
                if not isinstance(value, dict) or value.get("type") != "header":
                    raise AssetError("Recording must begin with a header")
                return value
    raise AssetError("Recording has no header")


def fingerprints(header):
    """Missing/unknown is unverified; malformed or falsely complete is an error."""
    value = header.get("asset_fingerprints")
    if "asset_fingerprints" not in header:
        return None
    if not isinstance(value, dict):
        raise AssetError("Malformed asset_fingerprints")
    status, mode, files = (value.get(key) for key in ("status", "mode", "files"))
    keys = {"status", "mode", "files"} | ({"error"} if status == "unknown" else set())
    if (
        set(value) != keys
        or status not in ("complete", "unknown")
        or not isinstance(mode, str)
        or mode not in REQUIRED
        or not isinstance(files, list)
    ):
        raise AssetError("Malformed asset fingerprint status, mode or fields")
    if status == "unknown" and (not isinstance(value["error"], str) or not value["error"].strip()):
        raise AssetError("Unknown asset fingerprints require an explicit reason")
    expected_game = "d1" if mode == "native_d1" else "d2"
    if header.get("game") != expected_game:
        raise AssetError("Asset mode disagrees with the recorded engine")
    roles = set()
    for item in files:
        if not isinstance(item, dict) or set(item) not in (
            {"role", "name", "size", "sha256"},
            {"role", "name", "size", "sha256", "container"},
        ):
            raise AssetError("Malformed asset file fields")
        role, name = item["role"], item["name"]
        if not isinstance(role, str) or role not in ROLES or role in roles:
            raise AssetError("Unknown or duplicate asset role")
        if (
            not isinstance(name, str)
            or not name
            or name in (".", "..")
            or any(char in "/\\" or ord(char) < 32 or ord(char) == 127 for char in name)
        ):
            raise AssetError("Asset name must be a plain basename")
        if type(item["size"]) is not int or item["size"] <= 0:
            raise AssetError("Asset size must be a positive integer")
        if not isinstance(item["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
            raise AssetError("Asset SHA-256 must contain 64 lowercase hexadecimal characters")
        if "container" in item and (
            role.endswith("_hog") or item["container"] not in ("d1_hog", "d2_hog", "mission_hog")
        ):
            raise AssetError("Asset container must name a recorded HOG role")
        roles.add(role)
    if any(item.get("container") not in roles for item in files if "container" in item):
        raise AssetError("Asset container is missing from recorded files")
    allowed = REQUIRED[mode] | {"mission_hog"}
    if mode == "d1_in_d2":
        allowed |= REQUIRED["native_d2"]
    if not roles <= allowed or (status == "complete" and not REQUIRED[mode] <= roles):
        raise AssetError("Incomplete or incompatible asset role set")
    if (
        status == "complete"
        and mode == "d1_in_d2"
        and roles & REQUIRED["native_d2"]
        and not REQUIRED["native_d2"] <= roles
    ):
        raise AssetError("Imported D1 companion assets require D2 HOG, HAM and PIG")
    return value


def pinned_assets(game, repo=ROOT):
    """Use the existing extraction oracle, cross-checked with the source manifest."""
    archive = {"d1": "desc14sw.exe", "d2": "d2demo10.zip"}[game]
    oracle = json.loads((repo / "android/tests/fixtures/demo_import_oracles.json").read_text())
    manifest = json.loads((repo / "game_data/test_data_manifest.json").read_text())
    rows = [row for row in oracle["archives"] if row["archive"] == archive]
    sources = [row for row in manifest["files"] if row["path"] == "game_data/demo installers/" + archive]
    if len(rows) != 1 or len(sources) != 1 or rows[0]["sha256"] != sources[0]["sha256"]:
        raise AssetError("Shareware source manifest and extraction oracle disagree")
    return {row["file"].lower(): row for row in rows[0]["files"]}


def shareware_game(header, repo=ROOT):
    value = fingerprints(header)
    if value is None or value["status"] != "complete":
        return None
    game = "d2" if value["mode"] == "native_d2" else "d1"
    expected = {}
    for source_game in ("d1", "d2"):
        if any(row["role"].startswith(source_game + "_") for row in value["files"]):
            pinned = pinned_assets(source_game, repo)
            expected.update(
                {
                    source_game + "_" + name.rsplit(".", 1)[1]: row
                    for name, row in pinned.items()
                    if name.endswith((".hog", ".pig", ".ham"))
                }
            )
    for row in value["files"]:
        # A custom mission archive is checked against its recorded bytes at playback
        if row["role"] == "mission_hog":
            continue
        known = expected.get(row["role"])
        if known is None or (row["size"], row["sha256"]) != (known["size"], known["sha256"]):
            return None
    return game


def digest(path):
    sha = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()


def _loose_path(row, data, children):
    candidates = children
    if row["role"] == "mission_hog" and (data / "missions").is_dir():
        candidates = children + [path for path in (data / "missions").rglob("*") if path.is_file()]
    matches = []
    for name in ROLE_NAMES.get(row["role"], ()) + (row["name"].lower(),):
        matches = [path for path in candidates if path.name.lower() == name]
        if matches:
            break
    if len(matches) > 1:
        raise AssetError(f"Ambiguous recorded asset: {row['role']}/{row['name']}")
    if not matches:
        # A diagnostic basename can change; prove identical bytes, never edition by name
        extension = "." + row["role"].rsplit("_", 1)[1]
        matches = [
            path
            for path in candidates
            if path.suffix.lower() == extension and path.stat().st_size == row["size"] and digest(path) == row["sha256"]
        ]
    if len(matches) != 1:
        raise AssetError(f"Missing or ambiguous recorded asset: {row['role']}/{row['name']}")
    path = matches[0]
    if path.stat().st_size != row["size"] or digest(path) != row["sha256"]:
        raise AssetError(f"Recorded asset mismatch: {row['role']}/{path.name}")
    return path


def _hog_member(path, names, expected=None):
    """Bounded DHF directory/member hashing, never extraction or game-format parsing."""
    names = [name.lower() for name in names]
    matches, identical, seen = {}, [], set()
    length = path.stat().st_size
    with path.open("rb") as stream:
        if stream.read(3) != b"DHF":
            raise AssetError("Unsupported asset container (requires DHF HOG): " + str(path))
        while stream.tell() < length:
            header = stream.read(17)
            if len(header) != 17 or len(seen) >= 65536:
                raise AssetError("Truncated or oversized HOG directory")
            name = header[:13].split(b"\0", 1)[0].decode("ascii").lower()
            size = struct.unpack_from("<I", header, 13)[0]
            if not name or name in seen or size > 128 * 1024 * 1024 or size > length - stream.tell():
                raise AssetError("Ambiguous, truncated or oversized HOG member")
            seen.add(name)
            if name in names or (expected is not None and size == expected["size"]):
                sha = hashlib.sha256()
                left = size
                while left:
                    block = stream.read(min(left, 1024 * 1024))
                    if not block:
                        raise AssetError("Truncated HOG member")
                    sha.update(block)
                    left -= len(block)
                identity = (name, size, sha.hexdigest())
                if name in names:
                    matches[name] = identity
                if expected is not None and (size, identity[2]) == (expected["size"], expected["sha256"]):
                    identical.append(identity)
            else:
                stream.seek(size, 1)
    for name in names:
        if name in matches:
            return matches[name]
    if len(identical) > 1:
        raise AssetError("Ambiguous matching asset members in HOG")
    return identical[0] if identical else None


def _check_member(row, member):
    if member is None or member[1:] != (row["size"], row["sha256"]):
        raise AssetError(f"Recorded asset member mismatch: {row['role']}/{row['name']}")


def _selected_identity(names, hogs, children):
    # Filename fallbacks are tried in order, with mission HOG then loose data
    # then appended base HOGs searched for each individual logical filename
    for name in names:
        if "mission_hog" in hogs:
            member = _hog_member(hogs["mission_hog"], (name,))
            if member is not None:
                return member
        loose = [path for path in children if path.name.lower() == name]
        if len(loose) > 1:
            raise AssetError("Ambiguous loose asset shadows recorded HOG member")
        if loose:
            path = loose[0]
            return (path.name.lower(), path.stat().st_size, digest(path))
        for role, path in hogs.items():
            if role != "mission_hog":
                member = _hog_member(path, (name,))
                if member is not None:
                    return member
    return None


def verify_header(header, data, engine, imported=False, allow_unverified=False, repo=ROOT):
    value = fingerprints(header)
    recorded = header.get("game")
    if recorded not in ("d1", "d2") or engine not in ("d1", "d2"):
        raise AssetError("Invalid replay engine")
    if imported:
        if recorded != "d1" or engine != "d2":
            raise AssetError("Imported replay requires a D1 recording and the D2 engine")
    elif engine != recorded:
        raise AssetError("Replay engine differs from the recording")
    if value is None or value["status"] == "unknown":
        if not allow_unverified:
            raise AssetError(
                "UNVERIFIED assets: record a fresh demo, or explicitly opt in with -AllowUnverifiedAssets/--allow-unverified-assets"
            )
    if value is not None:
        data = Path(data)
        children = [path for path in data.iterdir() if path.is_file()]
        # Verify every whole container before trusting any member payload
        hogs = {row["role"]: _loose_path(row, data, children) for row in value["files"] if row["role"].endswith("_hog")}
        for row in value["files"]:
            if row["role"].endswith("_hog"):
                continue
            names = ROLE_NAMES.get(row["role"], ()) + (row["name"].lower(),)
            container = row.get("container")
            member = _hog_member(hogs[container], names, row) if container else None
            if container:
                _check_member(row, member)
                names += (member[0],)
            if not container:
                path = _loose_path(row, data, children)
                names += (path.name.lower(),)
            _check_member(row, _selected_identity(names, hogs, children))
    verified = value is not None and value["status"] == "complete"
    return {
        "status": "verified" if verified else "unverified",
        "shareware_game": shareware_game(header, repo) if verified else None,
    }


def select_shareware(paths, game="all", repo=ROOT):
    selected = []
    for path in sorted(set(Path(path) for path in paths)):
        try:
            header = read_header(path)
            recognized = shareware_game(header, repo)
        except (OSError, ValueError) as error:
            print(f"Excluded unverified recording {path}: {error}", file=sys.stderr)
            continue
        if recognized and (game == "all" or header["game"] == game):
            selected.append(path)
    if not selected:
        raise AssetError(
            "No verified shareware recordings selected; historical, incomplete and unknown assets are excluded"
        )
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inspect", "verify", "select-shareware"))
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--data", type=Path)
    parser.add_argument("--game", choices=("all", "d1", "d2"), default="all")
    parser.add_argument("--imported", action="store_true")
    parser.add_argument("--allow-unverified-assets", action="store_true")
    args = parser.parse_args()
    try:
        if args.command == "select-shareware":
            paths = [path for root in args.paths for path in (root.rglob("*.dximdemo") if root.is_dir() else [root])]
            print(json.dumps([str(path.resolve()) for path in select_shareware(paths, args.game, args.repo)]))
        else:
            if len(args.paths) != 1:
                raise AssetError("Inspect/verify requires one recording")
            header = read_header(args.paths[0])
            value = fingerprints(header)
            if args.command == "inspect":
                result = {
                    "status": value["status"] if value else "unknown",
                    "mode": value["mode"] if value else None,
                    "shareware_game": shareware_game(header, args.repo),
                }
            else:
                if args.data is None or args.game == "all":
                    raise AssetError("Verification requires --data and --game")
                result = verify_header(
                    header, args.data, args.game, args.imported, args.allow_unverified_assets, args.repo
                )
            print(json.dumps(result))
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Asset verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
