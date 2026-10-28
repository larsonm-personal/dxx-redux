# Legacy 17/36 repair centers

Extend the existing XL repair implementation to audited 17/36 combined segment
types. Keep Redux's alternate repair implementation and the unaudited 15/33
repair gate. No original mission is qualified by derived controls.

1. Verify the pinned source Upgrade mapping and its repair caller
2. Derive a hash-pinned Observatory fixture with repair room 0 and dry room 6
3. Compare rate/cap/cue samples and actual player frames with extracted source;
   exercise physical crossings, save through stock, corrupt state and Redux
4. Register and run Android healing/audio/automap/disk/rewind integration
5. Run focused modern/legacy regressions, source compiler checks, both catalogs
   and scoped formatting; record exact evidence and keep original gates honest
6. Commit and push this isolated branch; open a stacked PR and require green CI

## Results

Source/native/Android acceptance and hashes are recorded in
[legacy repair evidence](../../XL_compat/evidence/legacy-repair.md). The native
contracts caught and fixed the viewer's stale blocked status. Modern behavior,
Redux's alternate rate, shared persistence schema and unrelated admission gates
remain covered. No original mission is promoted.
