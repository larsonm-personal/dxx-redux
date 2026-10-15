# Preserve ordinary save epochs

## Goal and format
Restore ordinary gameplay at its saved fixed64 epoch, before decoding relative timestamps. Keep the legacy fixed32 time field zero, all relative timestamp wire meanings, and explicit activity state. Existing effects time remains an animation reference written from the same clock; it is not an early authoritative source because native saves lack an early locator.

One format generation: native D1 23 / Android D1 24; ordinary D2 native/Android47; imported D1-in-D2 native/Android48. An eight-byte low-word/high-word epoch follows legacy time. D2 appends52 bytes of gameplay seismic state and a32-bit flash-timer activity flag after the existing runtime tail. No locator/scanner framework.

## Scope and safeguards
- Checked header read rejects truncation, negative epochs and values above INT64_MAX/128 before time/world replacement. Writer applies the same bound before opening the output file.
- Ordinary restore adopts the epoch. Replay/rewind retain existing override precedence; secret transfer initially retains legacy destination clock behavior, while consuming and validating the new field.
- Native old layouts remain readable; ordinary legacy saves without the new epoch retain their historical zero-based clock fallback. Android keeps its established current-format-only policy. Translator preserves legacy fix32 game_time for checkpoint arithmetic and exposes validated epoch metadata without overwriting the caller's chosen epoch.
- Seismic absolute times rebase from saved to selected epoch, preserving inactive slots. Only gameplay seismic state is added; no new FX RNG or sound cadence state is recorded. Sound restart/FX draw counts are cosmetic and intentionally not matched.
- Flash relative time is restored against the selected epoch; explicit active flag protects inactive zero after large-epoch delta clamping. Preserve the loaded palette and flash timestamp across final window activation, which otherwise restores the menu palette and clears that timestamp.
- D2 robot death start times now add the selected epoch when decoding active relative values. Zero remains inactive; existing same-frame one-tick sentinel behavior remains.
- Existing wiggle offset remains necessary for saves originating in already-rebased sessions.

## Qualification plan
1. Compile all native D1/D2 targets from latest integrated source; run CTest and existing Python/source contracts.
2. Run prior40 registered save cases plus focused epoch: high64, repeated, cold, opposite endian, malformed headers, D2 AI phase with zero-clock negative control, replay and secret selection.
3. Run real pickup/cooldown boundaries and seismic gameplay continuation, including differently seeded FX with identical SIM/physics where supported.
4. Update existing public runner registration and malformed format/legacy fixtures; run native-D1 translation and wall corruption checks.
5. Independent review before publication. No PowerShell execution. Android runtime remains unqualified unless separately exercised.

## Qualification outcome
Both native all-target builds pass, followed by151 CTests (D1 71, D2 80),61 Python/source contracts, all67 registered native processes and the public persistence aggregate parser. Wall coverage passes all three profiles/five phases/both endian imports and1340 atomic rejections. Different FX seeds produce identical seismic simulation/physics. Actual normal and synthetic public-map secret entry discard prior-mine seismic state; immediate fresh-level save remains valid.

The first full run exposed a raw D2 robot-death delta reader and stale Fusion due-now assertions. Final readers preserve absolute active times and nonzero sentinels; native and translated tiny-epoch boss controls pass. The aggregate parser now requires exact saved-epoch equality and rejects clock0 resets. Native gameplay sources were unchanged between the final67-process run and that Python-only parser correction.

Independent source review cleared epoch preflight, flash activation preservation, death timestamps and seismic lifecycle reset. Android device execution and retail campaign content remain unqualified. Secret tests use the unchanged public map through real secret-entry code. Full raw object/world trajectories remain explicitly unqualified by the aggregate report; this change does not claim every existing save-state difference is solved.

Evidence and exact source/binary/asset hashes are in /tmp/dxx-save-epoch-build/REPORT.md and companion manifests. Earlier failing evidence is retained losslessly with per-file hashes. No push or merge authorized by this implementation task.
