# Host Migration Rejoin Fix

## Problem
When a player leaves a coop game and becomes the new host via host migration,
other players trying to rejoin get stuck during object sync (level_sync timeout
after ~40 seconds).

## Root Cause
After host migration, `Multi_master_playernum` is set to the new master's
player slot (e.g., slot 1).  When a client does `auto_join`, it blindly calls
`change_playernum_to(1)`.  The GAME_INFO response from the host includes the
master slot in an `#ifdef __ANDROID__` extension byte.  The client sets
`Multi_master_playernum = 1` from this field.

Now `Player_num == Multi_master_playernum` (both 1), so `multi_i_am_master()`
returns TRUE on the client.  The packet security check `valid_sender()` drops
all `UPID_OBJECT_DATA` packets with "received by game master".  The object sync
never completes and the join times out.

## Packet flow (failing scenario)
1. Host (player 1 after migration) sends GAME_INFO with master_slot=1
2. Client receives GAME_INFO, sets Multi_master_playernum=1
3. Client has Player_num=1 (from change_playernum_to(1))
4. Client sends UPID_REQUEST (works -- no master check on send path)
5. Host starts sending UPID_OBJECT_DATA (1920-byte packets)
6. Client drops every UPID_OBJECT_DATA via valid_sender() check
7. After ~40s the join times out

## Fix (applied)
In `auto_join` and `net_udp_do_join_game`, after GAME_INFO is processed (which
sets `Multi_master_playernum`), pick a temp player number that does NOT collide
with the master slot:

```c
int pnum = 1;
if (pnum == multi_who_is_master())
    pnum = (multi_who_is_master() + 1) % MAX_PLAYERS;
change_playernum_to(pnum);
```

The temp number is only used until the host assigns the real slot via the INIT
marker in the first object data packet (`change_playernum_to(my_pnum)` inside
`net_udp_read_object_packet`).

## Files Changed
- [x] d2/main/net_udp.c -- auto_join: adjust Player_num after GAME_INFO
- [x] d2/main/net_udp.c -- net_udp_do_join_game: same adjustment
- [x] d1/main/net_udp.c -- auto_join: defensive same fix
- [x] d1/main/net_udp.c -- net_udp_do_join_game: defensive same fix

## Verification
- [x] Android debug bundle builds successfully (d1 + d2)
- [ ] LAN coop test: host migration then rejoin (needs emulator)
