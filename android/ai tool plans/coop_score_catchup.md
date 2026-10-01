# Co-op score-screen catch-up

## Evidence and intent

The September 29 phone logs show the host loading level 4 while the client still
sends level 3 end-level packets. The host rejects those packets while waiting
for level synchronization, and both peers time out despite incoming traffic.
Allow staggered progress and retry the completed level's status so peers can
catch up without requiring simultaneous score dismissal.

## Plan

1. Retain each peer's final score-screen packet in shared Android code, scoped
   to the session and world visit
2. During the next level's synchronization, authenticate late end-level traffic,
   refresh liveness and replay the retained status without applying old world
   state to the destination level; cover both host-first and client-first
3. Add diagnostic counters and a packet-loss integration case that suppresses
   the original completion updates, then verifies automatic catch-up
4. Format scoped changes, build both Android engines, and run the integration
   case for native D1 and D1 content in D2

## Verification

- Implemented in `shared/net/net_udp_score_catchup.h` with small Android-only
  transport hooks in both engines. Cache resets on network initialization and
  requires the same session, the source visit, the adjacent ordinary level,
  the authenticated peer and the level-sync waiting state
- Late score traffic refreshes liveness and triggers rate-limited replies;
  it does not change destination connection states, statistics or world state
- Scoped code quality and `git diff --check`: passed
- Android debug x86_64 APK, including both native engines: built successfully
  with no warnings from the new code
- `test_lan.ps1 -Game d2 -ScoreCatchup host -SkipBuild`: passed
- `test_lan.ps1 -Game d1 -ScoreCatchup client -SkipBuild`: passed
- Each loss test dropped two original completion packets and the first two
  recovery replies, observed eight retry opportunities, and reached playable
  level 2 with both peers connected. Movement, firing, touch overlays and Back
  menu navigation also passed
- Initial D2 setup was blocked by a degraded emulator's texture loading; the
  emulator was restarted. An ensuing run recovered the transition but timed
  out on the later client firing check; captured state showed firing afterward.
  The full unchanged D2 test passed on repeat
- Evidence: `android/temp/score_catchup_build.log`,
  `score_catchup_format.log`, `score_catchup_d2_host.log`,
  `score_catchup_d1_client.log`, and the matching `_evidence.log` files

This handles staggered ordinary level transitions while peers remain in the
session. Genuine inactivity still uses the existing disconnect timeout; save,
secret-travel and campaign-ending synchronization retain their own protocols
