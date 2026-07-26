# Report 20260725 final failure investigation

## Goal

Investigate and dispose of `test_secret_reveal_automap_d2` and `test_lan` without masking state leaks or network failures with longer timeouts.

## Plan

- [x] Select the final two failures and record scope
- [x] Reconstruct the secret-reveal failure from durable automation and device diagnostics
- [x] Reconstruct the LAN join failure across both emulator and proxy logs
- [x] Fix narrow test or product defects when supported by evidence
- [x] Validate fixes with order-sensitive setup and cleanup paths
- [x] Record any remaining disposition and evidence

## Disposition

### test_secret_reveal_automap_d2

The reported failure occurred at the launcher button step, before D2 or the
automap started. The script cleared launcher preferences after SetupActivity
had already rendered its button inventory, then depended on that stale
inventory to expose `Descent 2`. The test now uses the launcher automation
`enter_game` action, which runs the normal launch preflight directly. This
keeps the test focused on secret-area automap behavior.

Validation:

- Passed after launcher recovery and dependency re-provisioning.
- Passed again after `test_secret_area_baseline -RequireAssets`.
- The second sequence included a cold emulator restart and repushed all three
  declared D2 dependencies before the test passed.

### test_lan

The failed run started the joiner when the host Android process existed, but
before the host engine reached its network lobby. The joiner proxy sent 29
game-info requests and received no response. The test also had no preflight
that distinguished a broken shared-Wi-Fi path from a game protocol failure.

The test now:

- verifies EMU2 can reach EMU1 over shared Wi-Fi;
- waits for host introspection to report network mode with one connected
  player before starting the joiner's finite request sequence;
- retains the existing 120-second multiplayer timeout unchanged.

Validation:

- Passed direct LAN with both peers in-game and two connected players.
- Passed again immediately after `test_gog_installer_redbook_unified`, the
  predecessor from the reported batch. That sequence rebuilt EMU2 state,
  restored game data, verified direct reachability, observed the host lobby,
  and synchronized both peers.
