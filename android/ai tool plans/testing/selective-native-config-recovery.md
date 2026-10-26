# Selective native configuration test recovery

Source: `test-overnight-config-fuzz` at `9273bbc011f8e66f65b50f4dcf706acd6389bcfc`.
Base: `cmake` at `a6036eacf4725dfaaab56630baca813f80c99dba`.

Recover the useful persistence invariant through the existing real-engine
`test_upstream_compat` owner. The first slice writes A, restarts and reads A,
changes to B, restarts and reads B, then restores A and compares the complete
serialized file. Both D1 and D2 use their production config reader/writer.
Distinct native processes and an isolated directory avoid retained-global or
personal-profile false positives. Wrong-profile, altered-volume and missing-file
controls must fail for the intended reason; a crash or timeout is not rejection
success. No game assets are needed.

This tests audio/graphics/display setting persistence, not effective playback,
menu widgets, visual correctness or keyboard bindings. Existing Android config
policy coverage remains unchanged. No production code or settings are changed.

Follow-on candidates are native keyboard cancel/rebind/persistence and mixer
PCM/source transitions. Recover those only with actual native execution and
negative controls. Do not resurrect the old broad observation bridge, dated
reports, image-difference smoke, or overlapping save-restoration campaign.
The old branch remains intact until selective recovery is qualified.

## Local qualification

Both Debug native engine targets built from this branch. In each engine,
`test_config_policy` and `test_config_roundtrip` passed (four CTests total).
The roundtrip owner runs six positive native processes and four deliberate
rejections; negative acceptance requires exit 1 and the exact intended failure
message, not merely any nonzero exit. No private assets, menu bridge, or Python
model substitutes for config I/O.

Local scoped quality tooling could not load its managed dependency location;
local catalog execution also stopped at its 4 GiB free-space reserve. These are
setup limitations, not passes. Hosted checks remain required before completion.
