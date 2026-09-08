# Coop spew pickup feedback

User log debuglog_20260906_224248.txt. Code inspection confirms the recovery collision hook bypasses do_powerup, and both host/client grant application paths update inventory without sound or palette flash. Bonus energy/shields and timed powers follow the ordinary pickup path, explaining the fraction with feedback.

- [x] Add presentation-only feedback after local committed physical grants; preserve source powerup ID in receipts
- [x] Regression tests: local host/client, remote collector silence, reordered/duplicate grants, reduced-flash setting and refused pickup
- [x] Scoped formatting, D1/D2 native tests, Android build and paired D2 pickup test passed (`temp/spew-feedback-lan.log`)

No protocol layout or save-format change. Inventory/rejoin restoration remains separate and does not replay physical pickup feedback. Never call do_powerup to present a grant, since that would duplicate inventory/rewards.

User requested an architectural rethink while the paired test was running. Finished validation of this small feedback correction; no architectural rewrite applied. See coop_recovery_simplification_20260906.md.
