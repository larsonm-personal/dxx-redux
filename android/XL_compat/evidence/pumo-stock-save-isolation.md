# Original Pumo and stock save isolation

Why: the phone candidate changes XL rendering and preserves earlier save/loading
repairs. Verify that returning to Counterstrike and loading both mission saves
keeps player state and XL resource ownership separate.

The exact delivered universal APK is source
`4abb2b47dab5915d898959089fed8b826d669372`, version29080, SHA-256
`5d535b74c71de7a416a0f7f0555fb40e51215f59b69bd94796fce33f149f7788`.
This adds test coverage only; no runtime rebuild or bundle replacement is needed.

## Ordinary actions and results

`test_xl_pog_runtime.ps1 -OriginalHub -MissionFlow -PmcPlaytest -SaveIsolation`
uses the existing original-resource arguments and protected staging. The reusable
scripts are `test_xl_pumo_save_isolation.jsonc` and its `_cold` companion.
The exact resource invocation is retained in
`temp/xl-phone-fog-20261102/run-save-isolation.ps1`.

- Start original Temporal Hub through New Game, fire ordinary lasers to consume
  energy, and quicksave. Start Counterstrike and save its untouched initial state
  in a separate named slot through the Save Game menu.
- Native Load Game follows the most recently saved mission. Reselect each mission
  through New Game and Abort Game, which updates its abort autosave without
  overwriting either manual slot. Load Pumo's quick slot and stock's named slot.
- Assert original level names, Pumo energy below100, stock energy exactly100,
  unpaused front game window, no cheats/death, XL active only for Pumo and fog
  inactive in stock. Normal firing consumes energy after the stock restore.
- Warm: **68/68 PASS**, run `f930442cb3e149a48366eb060bf2c06e`, 80436ms.
- Fresh process: **39/39 PASS**, run `c61004aa194141ebabaf3a5ac2c8daa2`, 42802ms.
  Game PID changed from25298 to25928. Both manual saves survived the process stop
  and mission reselection; the new-game initial state cannot satisfy Pumo's
  reduced-energy assertion.
- Owned Android14 x86_64 SwiftShader, AVD `DxxPhoneFog20261102`, serial5556.
  All286 protected files match before/after restoration. Final rendered stock
  frame inspected. Receipts, trace, state, screenshot and hashes:
  `temp/xl-phone-fog-20261102/save-isolation-4abb2b4-pass`.

## Limits and retained setup failures

No physical Samsung, human traversal, launcher Save Explorer selection, exact
older user save recovery or earned mission progression is claimed. The manual
slots are separate mission save sets; this test does not assert that the native
menu aggregates every mission.

The first setup used scalar100 for float introspection100.0 and was corrected to
an exact numeric range. The next incorrectly searched the stock load menu for a
Pumo quicksave. Both failed receipts remain in `save-isolation-numeric-setup`
and `save-isolation-menu-scope`; all286 files were restored after each failure.
Neither failure establishes a gameplay/save defect.
