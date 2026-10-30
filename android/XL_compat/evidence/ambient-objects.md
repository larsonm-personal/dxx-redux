# Ambient sound objects

Stationary XL sound-effect objects now have an owned enabled state, native
identity, permanent spatial audio ownership and saved lifetime state. The
implementation extends [external linked audio](external-audio.md); that earlier
resource-only result did not establish ambient playback.

## Implemented behavior

Admission accepts type-20 sound objects with the source reader's normalized
sound identity, render-13 payload, no movement/controller and no multiplayer
flag. Filename termination, ASCII lowercase normalization, default enabled state
and float-to-fixed volume conversion follow the pinned reader. Invalid names,
negative volume, out-of-range conversion and unsupported object shapes remain
explicit rejections. Selected WAV ownership and decoding remain prerequisites.

The native object is an invisible, stationary shadow. Its audio payload lives in
`XL_compat`, and the narrow D2 FVI hook excludes active XL effect objects from
Redux's fixed collision table. A real ordinary-object positive control proves
that the collision query can otherwise hit the same object position.

Initialization follows the actual source effect-list order and creates permanent
class-1 external loops with authored volume and a 256-unit range. Initially
disabled sources still own a stopped loop. Wall and object-attached enable/disable
actions preserve target order, ordinary-object early stops, permanent reset and
post-action alternate/disabled flags. Real powerup removal consumes attached
actions. Signature reuse and deletion invalidate the old sound ownership.

Source loading compacts invalid object/segment indices. The later wall-trigger
cleanup additionally removes negative sides other than -1 by swapping in the
last target; attached actions skip that second pass. This is distinct from
reaching a valid ordinary object and stopping an action's remaining targets.

Trailer schema 24 carries the ambient enabled/signature/lifetime table. Restore
validates that table against native objects and checks retained ambient owners
against their authored WAV, volume, range and loop bounds before publishing any
owned runtime state. Fourteen malformed complete, checksummed trailers must fail
without changing a byte of a recaptured trailer. Four of those corrupt stopped
owners in ways accepted by generic audio validation but rejected by the authored
ambient binding. Valid restoration then recovers the earned state.

## Source and native evidence

The reference remains `arbruijn/d2x-xl` at
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Hash-checked source adapters execute
the actual object reader, FRound, effect-list insertion, SetSoundSources,
enable/disable consumers and post-action flag fragment. They are independent
source-function comparisons, not a run of the complete XL engine.

- Reader controls compare 1,055 volumes, four overflow rejections and 125 effect
  profiles. Original Pumo comparison covers 14 levels, 3,182 objects and 795
  effect payloads
- Ten initializer controls cover disabled objects, filtering, list order,
  zero/positive volume and literal `.wav` appending
- 4,096 source comparisons cover target normalization for wall and attached lists
- Each loaded runtime profile produces three initializer-owner receipts and six
  activation receipts: four wall actions and two real attached-owner removals
- The shared integration also checks actual PCM, mixed wall/object targets,
  atomic trailer rejection/recovery, collision, signature reuse and deletion

The full Windows named/ambient audio integration passes for level/game-data
profiles 15/33, 15/34, 17/36, 23/40 and 27/40. The legacy fixtures preserve the
same dry geometry and actions while encoding older object headers, wall widths,
light indices and object-reference tables. They omit the independently gated
legacy CLR override and retain embedded colors. This does not qualify an
original legacy package. The actual source reader matches their 156 object
records and nine sound payloads; the separate object-trigger reader matches two
attached records and their owner references in each legacy profile.

Legacy sound records have no enabled byte and therefore start enabled. The
shared test checks that default and initial playback before disabling one owner
and running the same activation/lifetime controls. Modern profiles separately
check an authored initially disabled owner. The 25/40 profile has reader coverage;
it is not included in this five-profile loaded ambient run.

The initial full Linux lane passed 105 of 106 tests. The failing high-trigger
endpoint test constructed an inventory without segment bounds; source-style
validation consequently removed its target. Supplying the actual table bounds
preserves the intended endpoint test. Its complete regression then passes on
Windows and Linux, including save, demo and stock controls. The fresh full Linux
acceptance run passes all 106 required tests in 389.08 seconds, including the
five-profile audio integration in 73.23 seconds. Its final receipt also validates
16 format witnesses and three modern source levels. No campaign traversal or
level-two completion is inferred from this lane.

## Android evidence

The first extended run passed all ten ambient PCM controls, then caught a test
setup error: expected spatial gains were captured before restoring the real
listener orientation. Synchronizing against the restored viewer before earning
persistence state fixes that mismatch without weakening the comparison.

The final run passes 144/144 steps in 467.970 seconds at 48 kHz, checking
1,812,992 samples and 145 output cases, including 24 ambient cases. Both 23/40
and 27/40 preserve enabled state, signatures, deleted sound objects, consumed
attached owners and earned playback through checkpoint rewind and disk restore.
All 26 protected files are restored with matching hashes. Run ID:
`9d6e995db94a4effa56a25917eee379a`. The existing 480-second timeout is unchanged.

Qualified binary SHA-256 values:

- Windows: `41a8a87d82e49f277a2a65bf7d3c0296ce55476121fb7a01bad3e6712cb3fd82`
- Linux: `efd7f8bcfc5cb3b43d91fed694a96a35f02b7b9465e559839577b71c7415640b`
- Android APK: `a2010e8677423fe42b6dee2b0106615b6f6d57f224c08511e2bd75fa5974ef21`

The final shared helper obtains the loaded filename directly, allowing the
legacy host profiles too. Both automation catalogs and scoped formatting pass.

## Original Pumo requirements

The refreshed original HOG remains SHA-256
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`.
All 14 levels inspect completely. All 644 sound objects retain their authored
records; their 41 owned WAVs were separately verified at both output rates.
The current inventory has no `effect_sounds` blocker. Thirteen levels retain
independent decoded blockers. `mvend.rl2` has no inventory blocker, which does
not establish runtime resource admission or gameplay fidelity.

| Level      | Sound objects | Remaining inventory findings |
| ---------- | ------------: | ---------------------------: |
| mvlev1.rl2 |            14 |                           13 |
| pm-hub.rl2 |            17 |                          298 |
| mvlev2.rl2 |            20 |                           12 |
| mvlev3.rl2 |            26 |                           12 |
| mvlev4.rl2 |            20 |                           11 |
| mvlev6.rl2 |            18 |                           11 |
| tulev1.rl2 |            44 |                           17 |
| tulev2.rl2 |           136 |                           25 |
| tulev4.rl2 |            39 |                           23 |
| mvend.rl2  |             1 |                            0 |
| mvlev5.rl2 |            21 |                            9 |
| pmlev1.rl2 |            38 |                           20 |
| pmlev2.rl2 |            43 |                           30 |
| pmlev3.rl2 |           207 |                           77 |

These are per-record diagnostics and category findings, not percentages of
implemented gameplay. Remaining requirements include particles, lightning,
other effect/object types, segment functions/properties, wider wall flags and
unimplemented actions. Static explosion ambience, classic WAV replacement
installation and global addons also remain separate audio work. No original
campaign completion is claimed.

Artifacts are under `temp/xl-ambient-audio-20261030` in
`C:/local/dxx-redux/temp/xl-batch2-20261027`. Source/activation logs use
`ambient-source-*`, `ambient-target-*`, `ambient-activation-*` and `legacy-*`.
Final Android receipts use `ambient-final-android-*`; the current original inventory is
`pumo-current-ambient.json`. Tests extend existing owners without adding a CI job
or changing timeout bounds. Public PR #220 remains a draft while the final CI
checkpoint is reviewed; this evidence does not establish campaign completion.
