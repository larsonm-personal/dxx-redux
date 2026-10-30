# XL gameplay sound consumers

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_63_gameplay_sounds`, above PR #212, commit `7882ea5c4`.
The authorized block ends October 30 at 04:57:30 UTC.

Connect ordinary sound creation and lifecycle to the owned XL mixer. Keep Redux
as the runtime alternate and implement policies in `android/XL_compat`.

1. Route immediate, moving-object and fixed-position engine APIs together with
   reset, destroy, synchronization and registry queries. Retain demo recording
   calls and avoid treating a successful one-shot's negative owner result as a
   request to fall back to Redux. Keep saved identities full-width internally.
2. Exercise the actual engine APIs with captured PCM on native and Android:
   translated/negative/invalid samples, one-shot consumption, duplicate owners,
   movement, deferred startup, destruction, pause, save and stock isolation.
   Update exhaustion controls to fill distinct owners rather than depending on
   Redux's duplicate-allocation behavior.
3. Audit explicit caller classes and source sample selection at weapon fire,
   explosions, robots and afterburner. Verify source decisions and actual output.
   Ordinary dispatch alone does not qualify ambient emission or replacements.
4. Run relevant Windows/Linux/Android builds and controlled runtime checks,
   publish validated increments, and monitor the stack without merging.

Source reference: `2576a98b5de310e3ac7c9777df2677d1627b8bae`, particularly
`audio/objectaudio.cpp`, `weapons/createweapon.cpp`, `effects/fireball.cpp`,
`ai/aiboss.cpp` and `main/game.cpp`. No campaign navigation or admission expansion
without evidence for all required consumers.

## Dispatch checkpoint

The engine boundary now routes ordinary creation, linked lifecycle and queries
to XL, retaining Redux for stock levels. Shared native/Android controls require
one XL voice and no Redux playback, plus exact logical demo events. The source's
reverse-table alias is preserved when recording existing owners.

The broader 52-case runtime selection found a legacy-water geometry guard and
two trailer comparisons racing the live audio callback. The guard now tests an
active XL source rather than the old pilot identity. Atomicity checks freeze the
callback while retaining complete byte comparisons. Distinct sound positions
exercise registry exhaustion without relying on duplicate allocations.

Final Windows audio and arena runs pass. Seven final Linux cases pass in 114.60
seconds, covering all three failures, wide object/wall/trigger controls and the
complete audio runner (55.06 seconds). The other 46 cases passed in the initial
selection and did not need another run. Final Android verification passed all
101 steps in 348.171 seconds, with 1,001,984 checked samples and all 26 protected
files restored exactly. Both automation catalogs and scoped formatting pass.
See `android/XL_compat/evidence/gameplay-dispatch.md`.

Continue with explicit gameplay caller classes and source sample choices.
Keep ambient emission and replacement qualification separate; this checkpoint
does not expand admission or establish complete sound fidelity.

The next explosion adapter must use the source classification table in
`objects/object.cpp`, not merely a nonzero damage radius. `IsSplashDamageWeapon`
tests `OBJ_IS_SPLASHDMG_WEAPON`, set for mega/Earthshaker families and specified
robot variants. Other damaging explosions can still select standard sound 11.
The pinned sound-option initialization defaults missile flight sounds off;
optional addon playback should not be inferred from the generic firing cue.
