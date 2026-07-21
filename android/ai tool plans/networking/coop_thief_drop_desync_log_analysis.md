# Coop thief drop desync log analysis

## Scope

Diagnose the supplied two-player client log for evidence explaining why thief-dropped powerups appeared on only one peer. This pass is read-only apart from this required plan file.

## Plan

- [x] Reconstruct the log timeline around the thief destruction and powerup creation
- [x] Trace relevant multiplayer thief-death, object-drop, and object-sync code paths
- [x] Correlate log fields with code and rank plausible causes
- [x] Record conclusions, remaining uncertainty, and the most useful next diagnostic

## Findings

- The supplied join-client log covers D2 level 5 from 15:18:22 through 15:35:41, but it has no thief, robot-explosion, stolen-item, powerup-spawn, object-map, or packet-loss event. It cannot directly identify the peer or the objects that diverged.
- On thief death, `multi_explode_robot_sub` invokes `drop_stolen_items` independently on every peer. The robot-explosion packet carries only a thief flag, not the stolen-item list or the created object identities.
- Despite the name, `Stolen_items` also holds the thief's built-in co-op death loot. `init_thief_for_level` seeds three shield and three energy entries, so the reported objects follow this same path even if the thief stole nothing.
- `drop_stolen_items` calls `drop_powerup` directly. These objects are not sent through the existing reliable `MULTI_CREATE_ROBOT_POWERUPS` path and are not explicitly assigned equivalent multiplayer object mappings.
- Drop trajectories consume each peer's simulation RNG independently. Full-death-spew mine drops also choose independent random vectors and may resolve to different positions or segments.
- The shared `Stolen_items` snapshot and robot explosion are separate reliable UDP messages. Reliability only retries and deduplicates them; the receiver processes them immediately without enforcing packet-number order. In co-op the theft snapshot and explosion can also originate from different peers, so there may be no shared ordering at all.
- For the three shield and three energy baseline drops, packet ordering is a weaker explanation because both peers seed the same entries at level start. The leading causes are independent RNG trajectories making the objects occupy different locations, or a local creation failure on one peer, such as temporary object-table exhaustion. Missing authoritative object identities and mappings make either divergence persistent. Packet ordering remains relevant to genuinely stolen additions.

## Recommended next diagnostic

Log thief explosion send/receive, the complete stolen-item snapshot and index, each created object's local number/signature/type/segment/position, creation failure, and local/remote mapping on both peers. The durable fix should make one authoritative peer create and transmit thief loot identities and positions instead of relying on independent peer recreation.

## Defensive implementation

- [x] Make the explosion sender authoritative for thief-dropped powerup creation
- [x] Reuse the reliable single-powerup packet so remote object mappings are explicit
- [x] Stop thief powerup motion and log send, receive-skip, and creation failure details
- [x] Run scoped formatting and Windows build/test validation

Validation completed with the scoped code-quality wrapper, the full Windows D1/D2 build, and `:app:assembleDebug` for all configured Android ABIs. The CMake build trees register no CTest tests.
