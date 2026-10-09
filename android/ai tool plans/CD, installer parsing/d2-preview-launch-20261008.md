# D2 Preview launch

- [x] Reproduce full original-CD import and launch with the obsolete test exclusion removed
- [x] Diagnose any remaining engine or automation failure using device logs and introspection
- [x] Apply the smallest necessary fix and require the expected first level
- [x] Run the full integration test, scoped formatting and relevant checks

The old May exclusion described an intro/menu timeout. Since then, split SOW
assembly was corrected: the Preview HAM and PIG were previously truncated by
per-volume overwrite. Start by testing those complete assets before changing
engine behavior. Preserve unrelated worktree edits and the physical device.

## Reproduction and fix

The original CD imports with the correct pinned hashes, exits the intro and
reaches New Game. Mission discovery then terminates the engine with
`Could not find required mission file <d2.mn2>`.

The engine's built-in mission table recognizes only the 2,292,566-byte download
PC demo HOG, not the 2,292,749-byte Preview CD HOG. A complete member comparison
found identical member names and byte-identical contents except `orderd2.pcx`
(38,703 bytes on CD versus 38,520 bytes in the download). All three SL2 levels
are identical. The HOG is valid demo data, not an incomplete retail installation.

Add the CD variant to the existing engine format constants, mission discovery,
built-in mission loading, and shareware behavior predicate. This is a small
cross-platform D2 format fix; D1 has no corresponding D2 mission classifier.
Remove the disc-specific regression exclusion. The existing automation already
requires level 1 and in-game screen state; its stale level-name oracle is corrected below.

The first fixed-engine run reached in-game level 1, then correctly failed its
level-name assertion. The generated oracle still named Corliss Steam Mine.
The actual `d2leva-1.sl2` embeds `Ahayweh Gate` at byte 40549, followed by a
newline; the original CD and download demo level bytes are identical. Correct
the generator and spec to this independently verified name, and extend the
existing generator integration fixture to keep a launchable D2 demo expectation.

## Verification

- Android debug APK build passed, including arm64-v8a, armeabi-v7a and x86_64
- Full original-CD regression passed on emulator-5582: 1 passed, 0 skipped,
  0 failed; all three installed hashes matched, and level 1 was Ahayweh Gate
- The saved disc result now records `test_mode: full`, `status: pass`, and
  `level_reached: Ahayweh Gate`
- Evidence: `temp/d2-preview-launch-verified/summary.json` and the accompanying
  source log; the pre-fix failure is in `temp/d2-preview-launch-logcat.log`
- No physical-device or unrelated worktree changes were made

The extended spec-generator integration test, both automation catalog checks,
scoped code-quality checks and diff whitespace checks passed. The final full
CD run completed with 1 passed, 0 skipped and 0 failed.
