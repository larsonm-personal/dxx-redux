# Co-op join black screen and analysis log preview cap

Date: 2026-08-15
Status: complete

## Requirements

- Diagnose and fix the joining phone remaining on a black screen
- Verify the metadata handoff timeout and distinguish it from later launch or network stalls
- Add durable logging around any silent launch stages
- Cap the immediately visible Advanced-tab analysis history at 10 items
- Preserve the full exported analysis log

## Plan

- [x] Reconstruct the joining-phone timeline from the supplied all-category log
- [x] Trace the matching launcher, metadata handoff, game startup, and network code paths
- [x] Add missing stage diagnostics and implement the root-cause fix
- [x] Cap the Advanced-tab preview at 10 entries without truncating storage or export
- [x] Add regression coverage and run scoped quality, builds, and two-device verification

## Findings

- The supplied log completed the route metadata handoff in 15 ms and entered native auto-join
- The host sends a 2,133-byte Android-authenticated game-info packet
- The engine and localhost proxy receive buffers were only 2,048 bytes, so every game-info reply was truncated and rejected
- The Guide-Bot ownership integration scripts used a stale introspection path for the route request generation
