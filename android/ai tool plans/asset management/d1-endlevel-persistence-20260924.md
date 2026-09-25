# D1-in-D2 continuation: flyout cosmetics and SIM isolation

Status: corrected implementation and bounded host/Android verification complete

## Corrected scope

Exit animation repeatability is not a gameplay fidelity requirement. Remove
the uncommitted history serializer, save versions 19/38 and associated
persistence tests. Reset transient state on rendered sequence entry in both
engines. Retain the independent guard against incomplete active-flyout saves

Verify actual flyout completion and unchanged SIM RNG state/count while
varying FX seeds and rendering frequency. Cosmetic trajectories, explosion
placement and FX draw counts are diagnostics, not equality gates. Do not
restore or reseed SIM RNG to conceal graphics-side consumption

Build both engines, run relevant host/integration checks and Android builds
Update the continuation handoff so the superseded animation persistence work
does not become a requirement for the remaining gameplay-state audit

## Corrected verification

Corrected verification passes: both CMake builds, D1 CTest 53/53, D2 CTest
61/61, 53 comparator tests, scoped quality and Android assembleDebug for all
three ABIs. Each engine completes 951 actual flyout frames with varied FX
seeds/render counts and unchanged SIM RNG. On emulator-5556, native D1 and
imported D1 each complete all 16 automation steps, including active-flyout
capture rejection, preserved playable history, normal restore/rewind and
Spreadfire rendering. Device evidence is in
`temp/d1-launch-runtime-20260924-105200` (native) and
`temp/d1-launch-runtime-20260924-105330` (imported). Logs use
`temp/d1-flyout-sim-isolation-*.log`; the corrected source/binary/evidence
capsule is `temp/d1-flyout-sim-isolation-source/manifest.json`

This is bounded host and x86_64 emulator verification, not new full-corpus,
live-network or ARM64 runtime qualification

## Superseded investigation

The following records the superseded investigation and its historical evidence

Continue from the schema-8 repeated-flyout milestone in
`d1-in-d2-continuation-20260924.md`. Preserve the existing frozen evidence and
the user's edits to `android/outstanding_bugs.md`

1. Audit endlevel state consumers, initialization and save/rewind entry points
2. Extend the actual flyout fixture with a post-flyout checkpoint and subsequent
   restore, retaining before-fix evidence of any changed future behavior
3. Exercise active-flyout memory capture and define its supported boundary from
   the production callers and observed result
4. Repair demonstrated persistence/capture defects in their shared owner, with
   native D1, imported D1 and ordinary D2 coverage as applicable
5. Run scoped quality, both relevant CMake builds and regression suites, plus
   applicable Android validation; update the ledger and continuation handoff

Do not reset native process history indiscriminately or claim full-corpus,
network or platform qualification from this bounded milestone

## Reproduction and contract

The actual native post-flyout save captured waits 7068/2197 and sound count 0
After another actual flyout, ordinary restore retained 5582/12 and count 3
The same fixture successfully wrote an active-flyout save, although that format
has no active sequence/camera/phase record. Before-fix source, saves and traces
are retained in `temp/d1-endlevel-persistence-before`

Only `explosion_wait1`, `explosion_wait2` and `sound_count` are read before
assignment by a subsequent flyout. They are two signed 32-bit countdowns and
one integer counter (0 through 6), independent of the game-time origin
`timer` is assigned when entering lookback/outside/stopped, `bank_rate` when
entering outside, and `ext_expl_halflife` before enabling the external explosion
Other phase geometry, actor/camera pointers and the explosion object are
initialized before their active consumers. Inactive values remain observable
in the raw schema-8 traces but are not additional saved history

New saves append a shared 12-byte history record (native D1 version 19, D2
version 38). Readers validate the whole record before application; the native
translator stages it before committing. Older saves initialize missing history
to zero rather than inheriting unrelated process state. Normal new-game/level
preparation still retains native carryover. D2 secret-world restores preserve
current history, following the existing cadence/ship-runtime skip-apply rule

Both actual writers refuse saves while `Endlevel_sequence` is active, before
opening the file. This also protects Android's memory adapter and autosaves
Android rewind skips capture during flyouts without discarding prior playable
history. This does not add active-flyout resume support or qualify requesting
rewind from inside an active presentation

## Verification

- Four actual flyouts per engine, with a checkpoint/contamination/restore branch:
  1268 native/imported raw frame observations match. The restored branch also
  matches uninterrupted subsequent movement, active explosion state and SIM/FX
  draws. The host probe rejects a save in the actual initial flyout phase
- Codec checks cover both byte orders, every 12-byte truncation boundary and
  invalid sound counters without partial application
- Seven populated native checkpoint/import/re-save scenarios and 28 resumed
  frames pass, including immediate prior formats 18/37 and older formats
- Both host suites pass (53 D1, 61 D2), as do 53 comparator tests, 21 campaign
  observations and three ordinary-D2 populated save/restore controls
- Scoped quality and all three Android ABI builds pass
- Android x86-64 native D1 and imported D1 pass 16/16 each. Memory restore and
  actual authoritative rewind retain `[-123, 4567, 6]` after contamination
  The memory adapter rejects all four explicit flyout phase flags without
  writing a partial save, and the capture entry retains playable history
  Both engines also retain identical Spreadfire state and GPU pixels
- Both device helpers exit 0, restore the original app directories and remove
  their backup. Runs: `temp/d1-launch-runtime-20260924-102435` (native) and
  `temp/d1-launch-runtime-20260924-102637` (imported)

Source, host binaries, APK, assets' hashes and completed evidence are frozen in
`temp/d1-endlevel-persistence-source/manifest.json`. Logs use
`temp/d1-endlevel-persistence-*.log`. Existing frozen capsules remain unchanged
APK SHA-256:
`32391418a87bf2c55bb3a55e41a5f1e9e0acabf4f4af29631a44607ca2f5c03f`

Next: boss/network effects, complete death phase and multiplayer bump timing
Full corpus, live network, active-presentation restore and ARM64 runtime remain
separate qualification work
