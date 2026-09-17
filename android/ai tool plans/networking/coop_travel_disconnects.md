# Continue secret travel after a client disconnects

Requested 2026-09-16. Host loss remains a session-ending event during travel.

- Host authoritatively removes disconnected clients from live acknowledgments
- Keep captured roster/portable records immutable through transfers and rollback
- Keep arrival slots/checksums stable even if a client leaves after placement
- Remove departed clients from in-flight transfer waits; synchronize removals to peers
- Preserve ordinary disconnect inventory bookkeeping and reject late roster additions
- Validate departure during freezing, capture, loading, commit and release with retained LAN automation; keep save/load blocked until settled

Implemented:

- Host removes departed non-host clients from live policy and release masks
- Captured checkpoint/portable roster remains immutable; surviving ships retain
  stable arrival positions and checksums even if another ship has disappeared
- In-flight travel transfers stop waiting for disconnected recipients
- Host roster snapshots cause surviving peers to apply the same disconnect,
  including snapshots from a new rollback generation; late additions are rejected
- Fresh level synchronization no longer revives departed slots as CONNECT_WAITING
- Host loss during travel remains a session-ending event

Validation on 2026-09-16:

- Both Windows builds, D1/D2 transition policy tests, scoped formatting and the
  automation catalog check passed
- Android x86_64 assembly passed, including the final rollback-generation
  roster adjustment after the phase tests
- Five two-emulator LAN scenarios passed: capturing 17:43:28, loading 17:44:44,
  freezing 17:50:02, committed 17:51:09, release 17:52:17
- Every scenario used the real engine disconnect path, completed host travel to
  Counterstrike secret -2 with one connected player, and successfully saved there
- Policy tests also cover three participants, removing the initiator while still
  requiring the remaining client's acknowledgment at every barrier

Repeat with `android/tests/test_lan.ps1 -Game d2 -InitialLevel 8
-SecretDisconnectPhase <freezing|capturing|loading|committed|release>
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`.

Evidence: `temp/coop-disconnect-capturing.log`, `temp/coop-disconnect-loading.log`,
`temp/coop-disconnect-freezing-verified.log`,
`temp/coop-disconnect-committed-verified.log`,
`temp/coop-disconnect-release-verified.log`, `temp/coop-disconnect-catalog.log`,
`temp/coop-disconnect-windows-sync.log`, `temp/coop-disconnect-android-final.log`.

Not exercised by these integration checks: a third connected device surviving
alongside the host, disconnect during rollback, physical network-loss detection
timing, and loading the resulting save. Existing disconnect inventory tracking,
restore mapping, and transition save/load blocking remain in use.

