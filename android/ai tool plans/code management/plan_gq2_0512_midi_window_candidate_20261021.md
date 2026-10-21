# MIDI comparison window candidate, 2026-10-21

Diagnosis and future implementation only. Local observation0512-OBS-MIDI-WINDOW; no canonical GQF/GQR ID admitted yet

## Trigger and narrow impact

android/tests/midi_diff.py compare176-259 rejects duration<=0 and negative starts/tolerance, then rejects windows beyond finite file ends. An explicit NaN duration makes those comparisons false. window118-129 and the note-state selectors select no events because time < start+NaN is false. With valid ordinary SMFs, finite starts and ordinary finite tolerance, the channel counts/states and empty SysEx comparisons can all pass. CLI262 onward accepts a float duration; JSON serialization currently permits nonfinite values

No SMF/comparator/CLI/native/media execution was performed. Maintained DOS manifests use finite durations, so this is not evidence that stock capture comparisons falsely passed. NaN tolerance fails the time-error acceptance predicate, and infinite duration is rejected by finite file end checks; avoid a blanket claim that every nonfinite input passes

## Distinct ownership and preservation

GQF0144/GQR0131 covers successful converter output semantics, metadata decoder acceptance and actual game repeat selection. GQF0307/GQR0293 covers a different actual boss-audio validator and undefined NumPy metric. Neither is this comparator window admission boundary. Completed strict JSON normalizer behavior is not on this comparison path. Do not broaden the fix into a generic numeric framework or reopen completed converter extraction/native format repairs

Keep independent SMF decoding, ordered message identity/count, timing tolerance, SysEx, reconstructed note state, explicit retained prefix context, no clock stretching/alignment, complete finite file windows and legacy/all-device negative controls. Keep explicit opening-only level8 limits and missing-reference SKIP2

## Repair and acceptance

Allowed paths: android/tests/midi_diff.py and existing android/tests/test_hmp_playback.py, with a narrowly registered comparator fixture only if necessary for actual CLI coverage

Require all explicit start/window/tolerance values and any derived duration to be finite and within their existing domains before accepting a comparison. Validate optional duration without breaking None-derived behavior. Preserve legitimate silence; rejection is based on invalid numeric admission, not an invented rule requiring a note in every window. Strict finite report serialization may be a final barrier, not a substitute for correct comparison admission

Use the existing ordinary smf helper and actual compare/CLI entry. Cover NaN duration and nonfinite starts/tolerance, derived-duration handling, zero/negative/partial windows, valid finite matching/mismatching events and silent valid windows. Require rejection/nonzero exit and no success report for invalid admission; preserve actual finite mismatches and negative controls. Tests must call production comparator rather than duplicate filtering logic. No generated corrupt media or codec/resource probe is necessary

After implementation run the relevant actual comparator/playback fixture through its supported exporter entry, CLI controls and scoped quality. Existing standalone discovery can skip when exporter is absent; do not count that as executed comparator acceptance

Provisional impact34 LOW, H/M/B/C/R12/0/2/10/10: narrow test-oracle admission, no inherited reduction, concrete source path and small local correction with existing ordinary fixture helper. Final canonical disposition remains pending. No code/test edit or execution, new ID/status/rating, canonical append or inherited saving
