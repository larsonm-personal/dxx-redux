# Saved coop late-join inventory

- [x] Trace restore metadata and late-join inventory delivery in D1 and D2
- [x] Reproduce a saved active client joining after the host restores alone
- [x] Retain saved players missing from the restored session for late joining
- [x] Run the paired emulator regression for D1 and D2 and scoped code quality

The absent cache currently loads only metadata.absent_players. Saved active
players missing from the new lobby are discarded. Preserve their records using
the existing identity matcher, with current saved players ahead of older absent
records when the bounded cache is full. Keep the existing recovery ledger and
inventory delivery path responsible for granting gear exactly once.

Baseline D2 paired emulator run: save acknowledged plasma (primary_flags=9),
six homing missiles and laser level 2. After a solo host restore, the client
joined successfully with primary_flags=1, homing_ammo=0 and laser_level=0.
The new `test_lan.ps1 -SavedLateJoin` scenario failed as expected.

Validation completed:

- Android x86_64 D1/D2 native build and debug APK assembly passed
- `test_lan.ps1 -Game d2 -SavedLateJoin -SkipBuild` passed
- `test_lan.ps1 -Game d1 -SavedLateJoin -SkipBuild` passed
- Both clients recovered plasma, six homing missiles and laser level 2 after
  their hosts restored alone
- D2 host diagnostics confirmed the saved slot-1 player was retained as absent
  and its saved inventory was sent on late join
- Scoped mixed-language formatting/lint and `git diff --check` passed

For the ABI-injected debug build, install the freshly produced test APK from
`android/app/build/intermediates/apk/debug/app-debug.apk` with `adb install -r -t`.
The old `outputs/apk/debug/app-debug.apk` can remain stale for that build mode.
