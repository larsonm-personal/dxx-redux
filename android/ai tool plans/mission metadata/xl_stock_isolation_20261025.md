# Unified XL stock isolation validation

Audit newly unconditional hooks and run existing integration coverage against the
normal unified binary, including stock D2 saves, graphics/menu paths and D1.
Rebuild the full-engine upstream compatibility fixture before running its stock
physics, weapons, triggers, save and audio checks. Exercise XL-to-stock cleanup
with the existing controlled Konflict test if necessary. Preserve prior work.

Artifacts: temp/xl-stock-isolation-20261025. Record actual coverage and failures;
no level-navigation or combat playthrough claims.

Status: complete. Targeted tests passed; one desktop screenshot isolation leak fixed.

Findings:

- Fixed a real desktop-only isolation leak: the former XL build switch selected
  GL_BACK for every screenshot. Normal builds now retain GL_FRONT; the private
  developer probe alone uses GL_BACK. Android GLES is unaffected by this fix.
- The full-engine stock fixture initially failed its old expectation that an
  invalid D2 descriptor destroys the current D1 mission. Unified preflight
  intentionally retains it. The assertion now verifies exact identity retention,
  plus unchanged D1 model, bitmap and sound storage, and frees the retained test
  mission. No production admission/teardown behavior was changed to pass the test.
- Rebuilt test_upstream_compat, classic trigger demo and mixed-rate audio: 3/3 pass.
  This exercises actual D2 engine objects with XL compiled in, including stock
  physics, weapons, collision, lights, AI and D1 compatibility assets.
- Normal desktop D2 public flight replay: 360 frames, expected terminal summary,
  clean exit and valid state/RNG trace structure. This is not full-frame or pixel
  equivalence evidence.
- Normal Android APK: D2 save/load dispatch 93/93 and D1 save/load dispatch 86/86.
  D2 final introspection confirms XL inactive and zero XL shaded vertices. D1 uses
  its own engine and has no XL introspection fields.
- APK SHA-256: f521f98a44c4a93eb13da5d1c480808bb2d020c93a1a816d7f30d6b1cfbc4689.
- Scoped formatting/lint and git diff --check passed.
- Android D2 graphics capabilities: 41/41 steps; runtime texture/filter/menu/HUD
  option changes and graphics confirmation: 88/88 steps. Anisotropic filtering
  unavailable on this emulator, so its nonzero branches were explicitly skipped.
  Detailed logs: android/temp/runtime-texture-options-20261025-163508.

Coverage limits: this is targeted stock-regression evidence, not proof of all
missions, pixel-identical rendering, performance parity or multiplayer behavior.
Only emulator-5554 was used. Existing tests were reused; no 3D navigation was
attempted. The desktop screenshot fix does not change the tested Android GLES
binary. The normal desktop executable was rebuilt after the fix before replay.

- Fresh original Konflict transition integration passed 33/33 steps. It asserted
  XL runtime inactive, zero XL shaded vertices, zero activated XL triggers and zero
  unresolved emitters in Counterstrike, then restored the XL save successfully.
  Run: 2d8a500af48c4786ad62685421fc1f89; durable result copied to
  temp/xl-stock-isolation-20261025/transition-result.json.
