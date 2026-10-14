# Public D1-in-D2 fault probes

## Scope

Find observable native D1 versus imported D1 mismatches using pinned public shareware data. Preserve the raw evidence, including identity/reference differences; do not declare them harmless without downstream checks. Existing retail and public gates remain unchanged. New fault reproductions can intentionally fail imported parity and must say so explicitly.

## Sequence

1. Reuse the fixed-gameplay-view diagnostic executables to replay existing compact native D1 inputs and identify concrete leads. These copied binaries have stale embedded source labels and are diagnostic only
2. Investigate the earliest object, reference, RNG, timing and gameplay differences. Prioritize projectile ownership/explosions, pickup/autoselection, and doors/triggers based on actual traces
3. Develop small ordinary-start live captures, with only player input controls and no injected world state, warps, saves or cheats. Use headed Xvfb screenshots for route inspection
4. Publish the reviewed fixture checkpoint after the final fixed-view base, then build independent required targets in this worktree without modifying the fixed-view checkout
5. Preserve independent live provenance and separately bounded immutable oracles. Keep every replay and oracle at or below 100000 bytes
6. Verify both true headless and headed playback and causal negative controls. Keep raw mismatches and identify the first meaningful divergence
7. Admit new cases in a focused stacked draft change only after the baseline settles. Do not merge

## Workspace and storage

Source worktree: `d1-fault-probes`, initially detached at `bb78d6d1a72a41d5ecd23bfe4796c720122f053a`. Fixed-view source and builds remain untouched. Diagnostic executables are ordinary copies in `temp/diagnostic-binaries`; isolated HOME is mandatory. Lossless archival of frozen earlier evidence has made room for independent fixture, headed and console targets, estimated below 600 MiB peak. Reuse pinned dependency sources read-only. Preserve the shared 4 GiB free-space reserve and compress raw traces without altering their records.

## Initial diagnostic executable identity

- Native D1: SHA-256 `0395c6adcaf4e6d5c6d1c8a040ee8143a4ee74c0406fecadf34afdba1d03c4f0`
- Imported D1 through D2: SHA-256 `53b8e638074b503e0d05f12311d484813313eb1ee23f3eb06d7ae8b8b3471475`

These contain the fixed gameplay-view implementation but predate the final parent/source identity. They cannot establish final published provenance.

## Verified exploratory results

- Existing loot no longer gains the extra 200 points after the narrow ownership
  repair. The diagnostic-only declaration remains because raw signatures,
  references, actor context and world effects still differ
- Native-written `d1match` comparison pilot changes only the D2 missile-camera
  preference. Existing native D1 ordered-secondary inputs then replay successfully
  under D2 with strict projection validation; normal D2 defaults remain unchanged
- Self-splash: 139 ordinary-control frames, one concussion at frame 48 and player
  splash at 51. Native/imported gameplay and SIM match in this bounded scene; raw
  player and projectile-parent signatures remain different
- Monitor phase: 181 frames, authored texture 349/eclip 24 at segment 11 side 0.
  Native startup timer 4369 versus imported zero; imported animation advances one
  frame early. A clear paired view at frame 170 shows native misc066#4 versus
  imported misc066#5 with exact matching camera states. Collision consequences
  are unproven. No repair is included
- The raw phase checker compares native capture plus limited existing replay trace
  fields separately. A known-fault match never converts parity failure to a pass.
  Wrong native expected phase rejects both live and console observations
- Screenshot-on/off native monitor captures have identical compact input bytes,
  all complete frame hashes, SIM events, terminal state and raw phase/camera state
  after adding the production post-SIM map-discovery call
- Mine fault: 424 ordinary-control frames, real authored proximity pickup at
  377, mine at 399, concussion at 407. The matched-profile input is 73186 bytes;
  native and imported strict terminal checks pass. First physical divergence is
  raw boundary 410, after concussion splash during SIM 409. D2 applies radial
  force to the mine while D1 excludes weapons. All 424 summary/RNG boundaries
  still match, exposing summary-only coverage as insufficient. An independent
  no-concussion control matches all 25 mine trajectory samples. Raw player/parent
  signatures and SIM actor context remain failed and explicitly retained

## Review and qualification

The four native fixture files have independent source review. Review found and
fixed a capture presentation prerequisite (new probes now use the existing native
3/4 configuration profile before graphics initialization) and required successful
post-SIM actual-camera map discovery. No production guard or replay header was
edited to bypass validation. Phase-checker review is complete with 17 passing
tests, including malformed JSONL/result/gzip rejection. Mine causal-checker and
fail-fast capture-helper reviews are closing bounded evidence-validation gaps.

Current source base is published `9be53d6d74feede5b3df4cf7dda19fc10b5f9fc2`.
Diagnostic fixture objects are built only in this worktree and linked against
stable engine objects read-only, with source/executable/object hashes retained.
They remain exploratory. The final source checkpoint will stack after the
fixed-view owner's reviewed admission and its test-only followup
`f64081737d8ad2d24d195b34de575533f2a1ab34`, then use independent clean native
builds and final live captures. A packed-field compile failure once left an old
diagnostic executable available; those attempted captures are explicitly invalid.
The corrected identity captures match the earlier valid inputs exactly. The new
capture helper forbids invoking any executable after a failed build or source
identity mismatch.

Automation catalog validation and direct host-only catalog generation work. The
aggregate catalog regression is blocked by the existing Linux process supervisor's
unavailable `/proc/self/task/<pid>/children` path in this execution environment.
Scoped C++/Python/PowerShell formatting works. Repository-local lockfile-pinned
Prettier dependencies were installed from the normal registry with scripts
disabled; no global tooling or lockfile changes were needed.
