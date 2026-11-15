# XL gameplay continuation window: November 12, 2026

This is the explicitly invoked bounded follow-up window, started about 21:07 PST.
Work is sequential, without agents. Nothing was merged or sent upstream.
The delivered milestone prepares incoming HAM definitions independently through
the real native reader and makes named-audio save observations pure. Full G2,
Android gameplay acceptance, sustained connected gameplay and 90% playability
remain unproved.

## Published source

All PRs are non-draft in `larsonm-personal/dxx-redux`, linked in
[stack 471](https://github.com/larsonm-personal/dxx-redux/stack/471).

| Layer                              | PR                                                            | Published native/implementation head       | Immediate parent                                      |
| ---------------------------------- | ------------------------------------------------------------- | ------------------------------------------ | ----------------------------------------------------- |
| Incoming HAM destination and owner | [469](https://github.com/larsonm-personal/dxx-redux/pull/469) | `b00c75304b41a84241d9038c4898f3c84a46997d` | fork cmake `bf40de9bc3b85c2279fe8163c621bb6c052dec12` |
| Recorded replay projection         | [470](https://github.com/larsonm-personal/dxx-redux/pull/470) | `f555025e604e0c79e4c3391f4ab3761ccbfa5181` | layer 04                                              |
| Pure audio observations            | [472](https://github.com/larsonm-personal/dxx-redux/pull/472) | `283d63d9d88de0103a1edf9a82c64843a1161c16` | layer 05                                              |

PR 468 was already externally merged when refreshed. Its published head and fork
merge tree were identical, so layer 04 starts from that resulting fork base.
The initial three stashes and unrelated inputs were preserved. WSL recovery was
authorized by the user; the registered retained image recovered after shutdown
and restart, without re-import or replacement.

## Behavior and boundaries

The single native HAM reader now accepts explicit destination tables and model
ownership callbacks. The destination inventory includes all written counts,
definitions, ship/reactor, gauge/cockpit and exit fields. Model initialization
uses scoped scratch state. Existing live-loading patch and Android capture hooks
remain at their original lifecycle boundary. These necessary native codec/state
owner changes are the substantial `d2/` exception; XL admission and retained
ownership live in `android/XL_compat/runtime/prepared_ham.*`.

`XlPreparedHam` retains origin-checked source bytes and independently decoded
definitions/model payloads through the native codecs. A private read-only
PhysicsFS adapter avoids a duplicate HAM parser. Equal-count incoming targets,
model ownership, moves, malformed data and discard are covered while the live
world, lookup and SIM RNG remain unchanged. It is not yet the incoming
`XlPreparedResources` context and does not publish whole-mission resources.

The replay wrapper now configures recorded dimensions and the established
normalized widescreen projection. Its first revision also accepted a deliberately
corrupt aspect value, failing hosted ordered-input validation. The correction
only selects normalized projection for exactly 65536 and preserves the native
strict mismatch check for unsupported metadata. The exact hosted negative now
rejects naturally before any object boundary; its unmodified positive passes.
No original assertion, fixture or timeout was weakened.

Named-audio snapshotting no longer destroys/reconciles live owners or clears live
loop bindings. It normalizes retained copies while the normal frame boundary
owns live retirement. The canonical regression failed against the old behavior
and now compares live owner/channel/loop state, both RNG streams and actual
subsequent PCM against an unobserved control. The serialized schema is unchanged.

## Local evidence

Receipts are retained under `temp/xl-g2-window-20261112/`; earlier failures have
not been deleted or relabeled as passing.

| Evidence                                              | Result and scope                                                                                            |
| ----------------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| Windows/Linux developer-ON and ordinary-OFF D2 builds | PASS on audio source                                                                                        |
| Windows canonical named-audio owner                   | PASS, `audio-final`, exit 0; both layouts, source references, scopes, banks, static and seismic controls    |
| Linux canonical named-audio owner                     | PASS, `audio-observation-linux`                                                                             |
| Linux assertions-enabled audio owner                  | PASS, `audio-observation-linux-assertions`; Debug `-g -O1`, no `NDEBUG`                                     |
| HAM Observatory                                       | Windows runtime and Linux runtime/full Debug PASS, including 12 firing and four contact reference controls  |
| Windows focused HAM assertions                        | PASS; full Windows Debug attempts exceeded their existing deadline                                          |
| Stock upstream compatibility                          | Windows 1/1 and Linux 1/1 PASS after HAM changes                                                            |
| Automation catalogs                                   | Windows both owners PASS; Linux master owner PASS in isolated native-Git clone after projection correction  |
| Ordinary audio-source short replay                    | Windows/Linux PASS: 12 object frames, 16 world records, 21 SIM and 12 FX events; strict terminal/clean exit |
| Ordinary audio-source water continuation              | PASS: clean exit, terminal, 1,235 object frames, 1,239 world/XL records, 196 SIM and 1,808 FX events        |
| Scoped quality                                        | PASS; temporary scope diagnostics removed before final Windows build                                        |

The short recording has five setup position assists. Water continuation uses
controlled navigation and its original `-xl-water-physics` experimental profile.
The earlier current-schema water comparison passed 1,235 object frames, 1,239
world/XL records, 196 SIM and 1,808 FX events before the audio slice. These are
controlled continuation checks, not unassisted connected gameplay or Android
qualification.

Retained failures include the original `birth-atomic` energy change (91 to 100)
after late XL rejection; old save schema 40 versus current 41; an old water trace
with unsupported world schema; wrong asset/configuration/admission launch
profiles; WSL setup and mounted-filesystem deadlines; unoptimized Debug audio
timeout; and two Windows sound-scope deadlines. Fresh Windows audio fixtures
passed without increasing deadlines. A retry of a timed-out fixture exposed
leftover classic replacement files, which is not a valid clean rerun.

`checkpoint-identities.json` records earlier source/binary/input identities;
[qualification identities](../XL_compat/evidence/qualification-window-20261112.json) record the final audio
binaries and replay inputs. Authentic Neptune geometry remains SHA-256
`4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303`.

## Catalog and remaining transaction

The fixed catalog remains 1,111 IDs: 1,020 normal and 91 secret entries across
23 archives and 356 descriptor pairs. All archive/descriptor hashes and advertised
ordinal lists were verified. Authored types give 1,109 `normal` entries, one
`anarchy` exclusion (Stadium) and one unresolved `D2X-XL` entry (Azure Catacombs).
The conservative single-player denominator is **1,110**, requiring **999** fully
qualified successes. No full playability numerator is established. See
[catalog accounting](../XL_compat/evidence/catalog-accounting-20261112.md) and its
linked descriptor evidence. Four historical sampled classic loads are not XL
playability; first rejections conceal unknown later blockers.

The [window ownership map](xl_gameplay_window_20261112.md) remains the transaction
map. Open work includes VHAM/HXM and resource-bank contexts; binding retained HAM
to resource preparation; fallible POG/sample/model allocation before publication;
geometry/world/native runtime/AI/XL semantic validation against the target epoch;
retaining validated owners through commit; clocks/RNG/lifecycle publication; and
discard/continuation controls through the actual save boundary. `birth-atomic`
still fails and must not be replaced with a narrow success assertion.

Revised planning estimates are 16-32 hours for remaining G2 and roughly 60-120
hours for the remaining focused engineering, both low confidence. Catalog-wide
playability qualification is additional and unestimated. The upper bound has not
fallen: independently decoded HAM and pure observation remove prerequisites,
but do not solve whole-world publication or resource-bank ownership.

## Hosted qualification status

PR 469 selected public owners and private run
[43375288766](https://github.com/larsonm-personal/dxx-redux/actions/runs/43375288766),
attempt 1, pass on `b00c75304`: 113 required tests, 16 format witnesses and three
modern source levels. PR 470 corrective head `f555025e` passes private run
[43379549135](https://github.com/larsonm-personal/dxx-redux/actions/runs/43379549135),
attempt 1, with the same required counts. Broad master coverage is explicitly
incomplete in these receipts. Sampled readiness remains failed at 4/8 and is
separate from regression qualification.

At the pre-publication snapshot, November 12 at 23:34 PST, PR 470 has 33 successful
selected checks, six intentional skips and the separately failed readiness
diagnostic. Its downstream native and headed replay owners have passed. PR 472's
public ASan and ordinary save-integrity workflows are still running; its private
run [43383830406](https://github.com/larsonm-personal/dxx-redux/actions/runs/43383830406)
passes on `283d63d9`, attempt 1, with 113 required tests, 16 format witnesses and
three modern source levels.

This committed report records that pre-publication snapshot. Final qualification
of the pushed handoff candidate is recorded in the handoff PR's description and
checks in stack 471 after publication, without changing the tested native source.
Pending checks do not count as acceptance. Next action in this window is to
finish those owners, repair any applicable failure and qualify the handoff push.
The completed ordinary comparisons, next plan and kickoff are linked here.

The private controllers for runs 43375288766, 43379549135 and 43383830406 use
`bf40de9bc3b85c2279fe8163c621bb6c052dec12`; candidate identities are separate,
respectively `b00c75304`, `f555025e` and `283d63d9`. All three private runs pass.

| Omitted owner                      | Actual selection reason                                                                                        |
| ---------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| Extra XL developer Release compile | Restricted to two named historical Pumo branches; normal Linux and Windows developer compile owners run        |
| Inspect retained online receipt    | Requires manual dispatch with `evidence_run_id`                                                                |
| Pinned Pumo staging                | Requires manual `pumo-stage` campaign                                                                          |
| Public timing export               | Requires an explicit evidence run ID                                                                           |
| Server Rust                        | Stack Rust profile excludes this non-server diff                                                               |
| Save nightly progress              | Scheduled/manual ledger publication excludes PR/workflow-run and XL-fast campaign work                         |
| Private JVM/Android matrix         | XL-fast host profile selects the host owner; separate public Android/JVM selections are recorded independently |

These are intentional selection exclusions, not passed tests. No failure-blocked
required owner is being reclassified as an intentional skip.

## Packages, review and next invocation

Current native packages are under `temp/xl-g2-window-20261112/packages/`.
`windows-d1/d1x-redux.exe` and `windows-d2/d2x-redux.exe` have their runtime DLLs
and pass `-help` from those staged folders. Retail/mission assets are not bundled.
They use verified root-bound CMake caches in `temp/xl-day-20261110/build-d1`
and `build-d2-reference`. The normal universal Android build passes through
`android/1_build_aab_apk.ps1 -BuildType 1`, including signature/package checks
and both game libraries for armeabi-v7a, arm64-v8a and x86_64. It produces
`dxx-redux-283d63d.aab` and `dxx-redux-283d63d-universal.apk`; the latter is the
development-signed `com.dxxrevival.app` package. Its SHA-256 is
`427dc2c2f7b238ac137a9999e72e6c85bfd05608397a12b6a53f9da976c32a8d`.
All package/native-library hashes are in the qualification manifest. Build logs
are `build-universal-android.log` and `packages/dxx-redux-283d63d.*.log`.
No local test APK was used and no new device gameplay test was performed.

Launch Windows from the staged directory using `d1x-redux.exe -hogdir <D1-data>`
or `d2x-redux.exe -hogdir <D2-data> -extra-dir <mission-directory>`. Select the
mission normally; retain any required authentic addons and normal time-equipment
controls. Developer pose hooks are not needed to launch these builds. The water
validation's experimental flag is a test profile, not blanket mission admission.
For Android, import the user's retail, addon and mission assets through the
launcher and enable the mission normally. No fresh local Android gameplay or
Android-to-host replay proof is claimed by the package build.

Review immutable implementation differences with:

```text
git diff bf40de9bc...b00c75304
git diff b00c75304...f555025e
git diff f555025e...283d63d9
git diff bf40de9bc...283d63d9
```

G3 simulation/presentation separation, G4 catalog-ranked authored predicates,
G5 earned connected combat/navigation and mission lifecycle, G6 Android/durable
recording qualification, and complete G7 delivery acceptance remain open. This
window does not rename those unfinished gates as completed deferred work.

The [next bounded plan](xl_gameplay_next_12_hours_20261112_continuation.md) and
[copy/paste kickoff](xl_gameplay_kickoff_20261112_continuation.txt) preserve the
same fork-only publication, active CI repair, validation reserve and no-merge
constraints. They do not automatically start another window.
