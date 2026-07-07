# Coop level 7 new log review

## Goal
- Review the new attached coop/texture diagnostics log
- Identify what it says about the D2 level 7 multiplayer texture issue
- Avoid code changes unless the log points to a small, clear fix

## Plan
- [done] Extract key events and diagnostics from the attached log
- [done] Compare log tags with the current texture and multiplayer code paths
- [done] Summarize likely cause and recommended next step

## Findings
- This run is a fresh D2 level 7 coop launch, not a restore path:
  `use_restore=false`, `restore_slot=-1`, and the game later logs no restore slot file
- The post-level-load texture reset did run before gameplay:
  `event=post_level_load_flush`, followed by a legacy texmerge flush and Android merged-wall cache clear
- The level segment signature is stable from load through gameplay:
  `segment_sig=e8f97cd8`, so the visible wall issue is not explained by segment texture fields changing after load
- The tracked wall is still `seg=169 side=0 face=0`, base `tmap1=73` (`rock198`), overlay `tmap2=0x132` (`ceil035`), orientation `0`
- That wall is now using `route=old_texmerge merge_impl=auto_old_texmerge reason=coop_plain_transparent_overlay`, so the Android GPU cached merge path is bypassed for this face
- The source bitmap hashes are stable:
  `rock198=0xa60484ff`, `ceil035=0xc831cd9b`
- The legacy CPU merged bitmap is stable and uploaded as RGB:
  `merged_hash=0x2820c6f1`, handle `1690`, no transparent pixels
- The persistent invalid `tmap1=910` references are still present, but they are separate segment refs from the tracked visible wall and should not be treated as the cause of this wall unless a future probe lands on one of them

## Next Step
- Compare `merged_hash=0x2820c6f1` against a known-good state for the same wall, either from single-player level 7 or from a capture after the wall visually flips correct
- If the good state has the same merged hash, the remaining bug is in upload/draw/sampling state
- If the good state has a different merged hash, the remaining bug is in the CPU texmerge source data or texmerge cache content
