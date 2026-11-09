# Separate pinned Pumo staging verification

Draft staging-only packet based on public controller `361e1164eac660c4a817b926ea963d656c1468fc`. No private workflow is qualified until this controller reaches the trusted `cmake` branch. This draft does not request a merge or deployment.

## Private inputs and trust boundary

- Manual `pumo-stage` runs only the trusted controller at `github.workflow_sha` under the existing `retail-ci` workflow/ref environment gate. It rejects nonempty `candidate_pr` or `candidate_sha`. There is no candidate checkout, executable, CTest, or native qualification. Scheduled XL113 FAST, retail/input/addon pins and the fixed readiness denominator stay unchanged.
- The full archive is pinned to the private assets commit `dc6565f6b5400d7c700fa8378e766b2354b6ceb6`, manifest blob `0e9667afdab347338fe3e6f186530f435bfc66ad`, reconstruction helper blob `27c297f214437536b081fe3538d28d2a299e6939`, manifest SHA-256 `e670d3c67d620f92b4b6991ccfcf09038ce6da846edf163aa1670b5be39f19e0`, and original 442,181,363-byte archive SHA-256 `82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf`. The private reconstruction helper is pinned but never executed by CI. Trusted public controller code reconstructs and stages.
- The separately sourced camera addon is pinned at the same private commit as `xl-resources/d2x-xl.hog`, blob `e0ae5bf48bdc097838bd96f0e11df5f6abca14e4`, 3,815,963 bytes, SHA-256 `e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6`. It is staged at `data/d2x-xl.hog` only after its own pin check. No base-retail checkout is needed for this staging-only profile; retail remains separate and unchanged.

## Bounded staging

Trusted extraction checks all 53 ordered chunks, original archive bytes, 2,672 regular entries, 51 directories and 1,677,820,774 expanded bytes. It rejects links, special files, unsafe names, case-insensitive duplicates, file/directory overlaps, missing entries, and expansion over 2 GiB or 4,096 entries before creating an exclusive output. It streams and rechecks extracted member sizes and hashes. Only `missions/single/pmines.hog` and `.mn2` map to `missions/`; `mods/pmines/**` retains its paths. These are 2,669 archive-only runtime files and 1,673,579,754 bytes. The separately pinned camera makes 2,670 files and 1,677,395,717 bytes. The original archive keeps the manual, readme and disclaimer privately, and is never republished.

The job uses fresh temporary storage and removes only its own private checkout, archive, staged runtime and Python environment. It uploads no private artifacts or raw logs. The public CI suite runs asset-free synthetic tests for pins, missing/corrupt chunks, path/link/overlap/limit rejection and existing outputs. A local proof reconstructed and staged the merged archive and camera, with the expected inventory and bytes.

## Native qualification remains separate

Existing Pumo owners remain on the reviewed DECE engine identity. A preserved local fresh-engine prototype earned six of eight native owner passes, then failed Tulev1 spawn and Guided input because both contracts hard-pin the historical DECE executable SHA-256. It does **not** qualify a fresh candidate engine or this staging-only profile. A baseline-only controller design needs separate review; this packet does not override source pins, upload binaries, or offer an alternate runtime mode. Formal later owners may be evaluated independently using their approved DECE route.

The Ubuntu 24.04 staging prerequisite is [Noble's `libarchive13t64`](https://packages.ubuntu.com/noble/amd64/libarchive13t64) and `libarchive-c==5.3`. No SDL, GL, Xvfb or native build prerequisite is needed. Hosted secret-backed staging remains unrun before explicit `cmake` deployment.
