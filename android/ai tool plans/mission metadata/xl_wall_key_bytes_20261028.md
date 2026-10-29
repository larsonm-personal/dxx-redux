# Full wall-key byte audit

Continue the authorized run on xl_more_47_wall_keys, based on PR #192, through
2026-10-29 04:54:16 UTC. Preserve the root checkout and original assets.

The inspector currently rejects wall-key bytes above 15. Pumo has 73 walls
with value 255 across nine levels. The pinned CWall stores keys as uint8_t;
player hit checks use exact blue/red/gold values, and closed-wall passage uses
exact blue/red values. Other byte values must remain unchanged, not be mapped
to KEY_NONE or masked: robot consumers also inspect the raw byte.

1. Audit source and Redux consumers before changing the gate. Retain all raw
   bytes and source distinctions, including locked doors and robot key masks.
2. Compare actual native closed-wall and player-hit decisions with extracted
   XL source for every byte. Keep the existing broad canonical matrix, and
   exercise extra raw values without multiplying irrelevant rendering cases.
3. Verify all admitted layout translations, ordinary loading, physical blocked
   crossings, disk/rewind and stock isolation. Extend the existing Android
   controlled fixture and rerun it on a normal APK.
4. Replace the unsupported key-byte classification with an observation only
   after evidence passes. Keep wider wall flags and other independent gates.
   Refresh original counts without claiming campaign qualification, publish the
   reviewed increment and inspect CI.

The source audit, five-layout byte matrix, native physical/hit/brain comparisons
and all 90 Android controls pass. Pumo loses only 73 key-value rejections across
nine levels. The complete 40-case Windows public selection and both catalogs
pass. Native mount-path controls isolate three timeout failures without changing
the binary, assertions or timeouts; retain the failed attempt separately from
the clean complete owner run. Detailed acceptance is recorded in
[wall-key-bytes.md](../../XL_compat/evidence/wall-key-bytes.md).
