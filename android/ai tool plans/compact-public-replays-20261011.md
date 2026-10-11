# Compact public level-start replays

Reconstruct the lost public corpus under the user's new limit: every checked-in
replay is at most 100000 bytes uncompressed, starts at an ordinary level start,
and contains no save/checkpoint payload or hidden sidecar save data.

1. Restore source from the published save branch and official pinned build tools
2. Capture scripted normal controls through production recording on public levels
3. Keep compact controls/terminal results in the replay; preserve live frame and
   SIM RNG oracle hashes separately, never derive expected values from replay
4. Require meaningful observed actions, two independent captures and clean replay
   repetitions; retain failures and qualify native/imported scope honestly
5. Enforce size/schema/provenance with negative tests, extend public runtime CI,
   publish a separate stacked draft promptly and monitor it to a terminal result

No existing retail replay or golden may change. No geometry, player pose or live
object may be staged to fake a level start. Investigate attainable pickup/door
routes before claiming those dimensions. Full-game parity remains incomplete.

## Implemented evidence and limits

- Six ordinary-level production recordings cover physical wall contact, firing,
  movement, a real pickup and an existing door in both public games
- Replay files are 12753-36324 bytes; replay plus immutable live-oracle companion
  is 25790-70045 bytes. Admission rejects oversized, encoded or save payloads
- Two fresh captures reproduce all six replay/oracle pairs byte-for-byte. Every
  native replay matches all live frame/diagnostic hashes, terminal fields and SIM
  events in repeated real Debug playback. Resource gain and pickup-slot retirement
  are separately asserted at their exact observed frame boundaries
- Native D1/imported D1 `flight` is the only required exact gameplay-state/SIM-RNG
  paired case. Door and pickup remain native-only required coverage; their imported
  probes retain original comparison outcomes and cannot satisfy the paired gate
- Ordinary native/imported starts assign player signatures 1/0. Full diagnostics,
  object and world comparisons fail and remain unqualified. The door trace has two
  otherwise-identical SIM events with those different player-context signatures;
  no identity normalization is introduced. Full-game qualification is INCOMPLETE
- Reproduced the production Debug imported-D1 null-free trap on first text load.
  Two null guards preserve existing non-null ownership, text restoration and unload
- A capture-only FX reseed initially broke a robot sound timer. Removing that
  artificial seed preserves ordinary level initialization and fixes live/replay
  fidelity without excluding any fields or changing expected values
- Existing retail recordings and goldens remain untouched

## Verification

Fresh Debug executables and capture fixtures build for both engines. Existing
paired comparator tests pass (64 tests). Automation catalog validation passes;
direct master-catalog discovery confirms host-only explicit scheduling and a
1800-second timeout. The aggregate catalog subprocess wrapper is locally blocked
by this environment's missing `/proc/self/task/<tid>/children`, exit 125; its
assertions remain unchanged except adding the new scheduling contract and run in
normal CI. Final integration, quality and CI outcomes are recorded in the PR.
