# Parallel initial LAN cooperative level loading

User priority: overlap the initial host/client LoadLevel operations. Preserve
PR103 and the separate retry experiment. This branch starts at matched-control
964ed12b747f615e379b6e708022d7eb36a85b72; faster retry policy stays disabled so
its effect is not mixed into concurrent loading.

Design:
1. Add optional UPID_LAUNCH_ADMISSION (36), with the existing authenticated
   sequence identity but an additional signed purpose byte. Existing readiness
   REQUEST signatures remain byte-for-byte compatible. Retagging admission as
   readiness must fail authentication.
2. Explicit launcher LAN flag from existing mp_is_lan intent scopes both peers.
   Client additionally requires the active auto-join call, cooperative STARTING
   game, non-observer and no restore/travel. Send after mission, level and
   can-join validation, immediately before initial StartNewLevel.
3. Host accepts admission only in launcher auto-host cooperative STARTING, for
   its selected initial level. Admit through the current signature/counter/
   roster path. Ignore the packet in WAITING/PLAYING. Preserve proof context
   binding across state changes; no admission can satisfy level readiness.
4. Post-load REQUEST/proof/SYNC remains unchanged. Lost or unsupported admission
   falls back to current serialized behavior without waiting for a new ack.
   Older hosts reject unknown packet types; no old readiness payload changes.
5. Trace signed admission send/receive and local load boundaries. Match the
   existing six-sample harness/control. Use peer event ordering and a clock
   alignment bound if plotting actual cross-device overlap; never subtract raw
   device clocks.
6. Exercise slow host, slow client, dropped/duplicate admission, cancellation
   during load and subsequent retry. Assert no SYNC release before the client
   completes loading, and retain disconnect/timeout behavior (15 s native
   timeout) instead of marking an absent client ready.
7. Extend existing native auth/policy tests and an owned dual-emulator scenario
   runner. Run D1 and D2 plus catalog checks and scoped mixed formatting.

No merge, cmake push, history rewrite or security-setting changes.

Implementation checkpoint:
- D1/D2 net_udp objects compile; reconnect auth, initial sync retry and coop
  transition policy CTest targets pass (3/3 each).
- Scoped mixed formatting and automation catalog validation pass.
- Local master catalog validation cannot execute: supervisor cannot read
  /proc/self/task/<pid>/children, also outside the command sandbox. This is an
  execution-environment limitation; no catalog result is inferred.
- Existing dual-emulator measurement owner now runs the same six samples,
  followed by five fault cases per game. Debug-only app-private fault settings
  are removed after each case. No fault holds enter the six timing samples.
- cmake rechecked at 66e5e27da9850a9f142ac5c13d9e65432d48f4e3;
  PR103 remains open/unmerged at 01401425aff549eefd8b5a9e9fd8a8458d01a9b8.
- Runtime overlap and fault results pending CI; not yet established.
