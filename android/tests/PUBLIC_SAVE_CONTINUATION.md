# Ordinary public save persistence

Run from the repository root with built `test_upstream_compat` executables and the pinned public shareware data:

```powershell
./android/tests/test_public_save_continuation.ps1 -D1Exe <d1-fixture> -D2Exe <d2-fixture> -D1Assets <d1-data> -D2Assets <d2-data>
```

On Linux, run the command under Xvfb: ordinary saves render a thumbnail. Every invocation stages only validated public assets in a fresh isolated output directory and generates its own native saves. No retail data, save fixture, or capture oracle is updated.

## Qualified promises

Both engines reach a checkpoint after 360 ordinary flight frames. D1 also reaches a real active-door checkpoint using ordinary controls. The gate compares exact values before saving, after saving and immediately after ordinary restore:

- Player inventory, health, ammunition, selection, score and statistics
- Live object slot identities, health, contents, pose and physics, plus applicable weapon parent and hit storage
- Complete allocator state and tick, walls, active doors and triggers
- Exact SIM and FX RNG state, persisted FX draw count, and the ordinary load's deliberate absolute SIM draw-count reset to zero

The two flight cases then run 60 identical forward/fire control frames on the uninterrupted and restored branches. Selected player state, both RNG states, FX count and per-step SIM draw deltas must match exactly. Real player projectiles and energy use are required on both branches. The D1 door checkpoint requires live weapon and active-door storage; its 180-frame continuation is diagnostic only.

The report lists the precise field contract. Omitted promises do not turn into implicit equality claims: full lossless engine object/world streams and every strict field difference are retained separately. No fields are normalized in these diagnostics.

## Explicit qualification limit

Full trajectory equivalence is **UNQUALIFIED**. Ordinary saves reset `GameTime` to zero, rebase absolute timers, rebuild segment object links and reset some runtime/cosmetic state. In the public flight case, trajectory differences occur despite matching selected inventory/outcome/RNG state. The active-door continuation can also diverge in player health, SIM state and door timing. These observations are not hidden or promoted into a full-world pass, and the gate does not change engine save format or clock behavior.

Saving itself can change observed state. In native D1, the thumbnail reveals automap segments and normalizes a station timer. `strict-differences.jsonl.gz` preserves those save-time changes as well as restore and continuation differences.

## Evidence and rejection checks

Each case retains `before-save.jsonl.gz`, `uninterrupted.jsonl.gz`, `resumed.jsonl.gz`, `strict-differences.jsonl.gz`, the runtime native save, ordinary capture, logs and a machine-readable report. Reports include SHA-256 identities, selected promises, nonvacuous witnesses and the first gameplay differences. Both trace readers use the existing strict replay schemas and require restored/frame/terminal evidence. Each branch's complete terminal object/world snapshots must equal its last post-frame state, including both RNG streams; terminal-only mutations cannot pass as diagnostics.

Run `./android/tests/test_public_save_profiles.ps1` for synthetic rejection tests. Public CI runs these and the complete three-case native integration. The test catalog registers the native runner as an explicit caller-supplied-binary test with a 900-second master timeout.
