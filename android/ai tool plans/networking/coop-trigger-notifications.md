# Co-op trigger notifications

- D2 displays shot-trigger feedback only on the activating client; MULTI_TRIGGER has no shot flag and remote wall/light state can already differ when processed
- Send the exact generated notification in a separate reliable co-op packet, prefixing the callsign on every client and preserving TF_NO_MESSAGE, shot filtering, pluralization and effect-change checks
- Keep replicated trigger execution silent; notification receipt must never activate a trigger
- D1 has no corresponding trigger HUD messages, so no D1 behavior changes are needed
- Append the packet type and bump D2 protocol versions to reject incompatible peers
- Verify with a focused native notification transport harness, scoped formatting and both Windows engine builds

Status: implemented

+- The separate packet contains the activating player and a bounded 64-byte message, sent with reliable priority 2
+- Receivers accept the original player or host relay, validate the player and co-op mode, and display literal message text without executing triggers
+- Android's existing world-visit packet fencing automatically scopes the new packet to the current mine
+- Native simulated transport checks passed for all 12 messages, singular/plural wording, local/remote parity, silent triggers, fly-throughs, solo mode, invalid senders, self echoes, disconnected players, unterminated payloads and three-player host relay
+- Scoped code quality checks and both Windows engine builds passed; final D2 rebuild, native notification harness, upstream compatibility and co-op gameplay fence tests passed
+- Live multiplayer/device testing has not been performed
