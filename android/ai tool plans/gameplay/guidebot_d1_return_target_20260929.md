# D1-in-D2 Classic Guidebot objectives

- Trace command acceptance and return-path gating in the supplied device log
- Reproduce a stale remembered player segment in the real-level host test
- Correct optional D1 Guidebot return targeting without refreshing native enemy awareness
- Run scoped quality, Windows build and D1/D2 navigation regression coverage

Evidence: debuglog_20260929_170625.txt accepts energy-center commands at
17:14:12.718 and 17:14:14.283 (special=6), but mode=8 continues patrolling
an old return path ending in segment 23 while the player is in segments 18/19
D1 disables continuous cloak-cache tracking; D2 return paths assume that tracking
keeps Believed_player_seg current for an uncloaked player

## Result

Return-path creation now uses the current player segment for an uncloaked
optional D1 companion, without changing Believed_player_seg or the cloak cache
Ordinary D2 and cloaked-player targeting retain their existing behavior

The real-level regression failed on the old return target and endpoint, then
passed with the fix. It also checks energy-center command acceptance and
resumption on rejoin, with the center inside Classic's finite search depth

- Windows D2 CMake build passed
- Scoped code quality and diff whitespace checks passed
- Classic navigation/reference and native save-mode suites passed twice on
  D1 level 2 and D2 levels 1 and 11
- No Android device playthrough was performed
