# Metadata start secret diff check

## Goal
- Explain why the corrected metadata start source changed many secret fixture rows.
- Make the start-source helper match normal single-player startup closely enough that secret scanning does not use coop starts.

## Plan
- [x] Compare old and refreshed fixtures by stable secret signatures, ignoring display order.
- [x] Check game startup rules for player start objects.
- [x] Patch the metadata helper to ignore coop-only starts for launcher/headless single-player analysis.
- [x] Refresh and verify the baseline again, then re-check secret count/signature changes.

## Notes
- D1 stable secret signatures did not change after the first start-source fix.
- D2 showed real secret identity/count changes on several levels, which pointed to the helper choosing a different player-like object than normal single-player startup.
- `gameseq_init_network_players()` uses `OBJ_PLAYER`/`OBJ_GHOST` for non-coop startup and only admits `OBJ_COOP` in coop mode. Launcher metadata should model the non-coop route.
- Ignoring `OBJ_COOP` did not change the refreshed baseline, so the big fixture movement is from replacing stale `Player_init[]` start data with the loaded level's actual `OBJ_PLAYER`/`OBJ_GHOST` start.
- Compared against `HEAD`, D1 stable secret signatures did not change. D2 had 12 levels with raw/final/count movement and 8 with stable secret signature changes.
