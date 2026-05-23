# Round 4 MP Fixes

## Items
1. Host game defaults persistence + scrollable dialog + game-specific presets
2. Swap net events/stats overlay positions (admin tray + overlay panels)
3. Net events status: "connected (direct/relay)" + holepunch method + host/client
4. Active games: 30s reporting interval + clickable join with lobby_id + ICE
5. Host lobby refresh after game ends (new END_GAME message)
6. MPDIAG extra newlines (strip trailing whitespace in NetLog.log)

## Implementation Order
- [x] 6. MPDIAG extra newlines - NetLog.kt strip
- [x] 2. Swap overlay positions
- [x] 3. Net events status detail
- [ ] 1. Host game defaults persistence
- [ ] 4. Active games join flow (server + client)
- [ ] 5. Host lobby refresh (server + client)
- [ ] Build + lint pass
- [ ] Server build + test
