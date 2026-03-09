# Disc Hashing Strategy

## Overview
`known_discs.json` identifies BIN/CUE disc images by comparing per-track SHA1 hashes against a database of known Descent II and Descent I disc releases.

## Why SHA1?
- **Redump convention**: The disc preservation community (redump.org) uses SHA1 hashes of full raw 2352-byte sector data per track as the standard fingerprint. By matching this convention, our hashes can be cross-referenced against redump's verified dumps.
- **Strength**: SHA1 provides 160-bit collision resistance — more than enough for disc identification (we're not using it for security, just matching).
- **Speed**: SHA1 is fast enough to hash a full CD image (~700 MB) in seconds on modern hardware and in acceptable time on mobile devices.

## What gets hashed
Each track in a BIN/CUE image is hashed independently:
- **Raw 2352-byte sectors** — the full sector including sync, header, ECC, etc. This matches what redump databases store.
- The CUE sheet tells us where each track starts and how large it is. We hash exactly the byte range `[start_sector * 2352, (start_sector + num_sectors) * 2352)` from the BIN file.
- Data tracks and audio tracks are both hashed the same way.

## Per-track vs. whole-disc hashing
We hash per-track rather than hashing the entire BIN file because:
1. **Partial matching**: GOG and other distributors sometimes ship truncated images (e.g., fewer audio tracks than the original retail CD). Per-track hashing lets us match the tracks that *are* present.
2. **Multi-file BIN/CUE**: Some rips split each track into a separate .bin file. Per-track hashing works with both single-file and multi-file layouts.
3. **Disc variants**: Different pressings of the same game may have identical data tracks but different audio mastering. Per-track hashing reveals exactly which tracks differ.

## Matching algorithm
For each known disc in the database, we compare SHA1s in order starting from track 1:
- Count **consecutive matches** from the beginning (most important — ensures data track identity)
- Count **total matches** as a tiebreaker
- The disc with the most consecutive matches wins; ties broken by total match count
- A disc with 0 consecutive matches is not considered a match

This gives the best result for partial images: if someone has tracks 1-5 of a 15-track disc, we'll still identify it correctly (5 consecutive matches).

## JSON format
```json
{
  "id": "d2-gog-v1.2",
  "label": "Descent II v1.2 (GOG)",
  "game": "d2",
  "legacy_disc_id": "0x7d0ff809",
  "track_mapping": { "title": 2, "credits": 3, "first_level": 4 },
  "tracks": [
    {"track": 1, "type": "data",  "sha1": "..."},
    {"track": 2, "type": "audio", "sha1": "...", "name": "Title"}
  ]
}
```

- `legacy_disc_id`: The CD-ROM disc ID used by the original game engine (computed from track count and MSF offsets via CDDB-style algorithm). Passed through to the C engine for backwards compatibility with `songs_haved2_cd()`.
- `track_mapping`: Maps logical roles (title track, credits track, first level track) to 1-based physical track numbers. `-1` means the disc doesn't have that role.
- `name`: Human-readable track name from the CUE sheet TITLE field or manual identification.

## Adding new discs
1. Import the BIN/CUE image through the app's disc import flow — it computes SHA1s automatically.
2. If the disc doesn't match any known entry, add it to the `discs` array with hashes, track mapping, and a descriptive label.
3. Test identification by re-importing the same image and verifying the match.
