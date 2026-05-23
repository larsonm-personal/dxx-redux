# metl154 visibility backoff and occlusion probe

## Goal

- find a camera position that still targets the problematic metl154 view but is backed off enough to see the leaked face clearly
- test whether a face behind the problematic draw, starting with 81/1/0 rock331, flips between visible and occluded when the merged-wall problem path is toggled
- convert that observation into a stronger regression test if the signal is reliable

## Plan

- [x] inspect the existing automation and introspection hooks for face-relative movement, direct warp support, and current camera state capture
- [x] run on-emulator experiments with the current stock regression setup, varying `face_view` distance and capturing introspection until the problem face is visible from a reproducible position
- [x] determine whether a coordinate-and-heading warp helper is needed to lock the successful camera position, and implement it only if `face_view` is insufficient
- [x] probe the occlusion hypothesis for faces behind the leaked draw, starting with 81/1/0, by toggling the known problem path on and off and checking whether the behind-face visibility changes in a measurable way
- [x] update or add an automation script that encodes the strongest reliable regression signal found, then run the script and capture its artifacts

## Notes

- `face_view` at `distance: 12.0` keeps the stock metl154 target on `83/3/1` while surfacing a non-center `rock331` cover in `merged_wall_snapshot.covers`
- the surfaced behind-face signal was `cover_seg=82`, `cover_side=1`, `cover_face=0`, `cover_bot=rock331`, not the initial `81/1/0` guess
- a direct coordinate-and-heading warp helper was not needed for this tranche because the existing `face_view` support was sufficient
- the strengthened canonical regression passed when invoked through `SETUP_AUTOMATE` with an absolute on-device script path; the current relative-path launcher flow can get polluted by a stale malformed `game_scripts...` script name and should be debugged separately