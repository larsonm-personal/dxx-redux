# Rejoin Sync Failure - Log Analysis (Build 1112)

## Overview

Two rejoin attempts from Player78 (joiner) to Player95 (host), D2 coop, level 1.
Both failed. First: verify_objects sees PLAYER=1, GHOST=0, SYNC FAILED.
Second: gets into game but desynced, crashes.

## The Smoking Gun

**Every UDP packet has its LAST object corrupted to all-zeros.**

The host sends 7 objects per packet. The client correctly parses the first 6, but the
7th always reads as: type=0, id=0, owner=0, remote=0, seg=0

This is 100% reproducible across ALL 15+ packets in the first rejoin attempt.

## Packet Structure Constants

- UPID_MAX_SIZE = 2048
- sizeof(object_rw) = 264 bytes (packed, __attribute__((packed)))
- Per-object wire format: 4 (objnum) + 1 (owner) + 4 (remote_objnum) + 264 (body) = 273 bytes
- Packet header: 9 bytes (1 UPID + 4 token + 4 nobj)
- Init marker: 9 bytes (4 objnum=-1 + 1 owner + 4 placeholder)
- First packet: 9 + 9 + 7*273 = 1929 bytes (fits in 2048)
- Subsequent packets: 9 + 7*273 = 1920 bytes (fits in 2048)
- Send-side size check: `loc + sizeof(object_rw) + 9 > UPID_MAX_SIZE-1` = `loc + 273 > 2047`
  After 7 objects+init: loc=1929. 1929+273=2202 > 2047, so 8th doesn't fit. Correct.

## Detailed Packet-by-Packet Trace (First Rejoin Attempt)

### Host sends (player_num=2, Highest=109):

Mode 0 pass (owner=-1):
105 objects sent: indices 0-109, skipping gaps (28, 41-44).

Mode 1 pass (owner=1):
3 objects sent: obj[42] owner=1, obj[43] owner=1, obj[44] owner=1.

Total: 108 objects. Confirmed by `send_objects: finished, obj_count=108`.

### Client receives (my_pnum=2):

Packet format per frame: 6 good objects + 1 garbage object = 7 items per packet.

#### Packet 1 (nobj=8: init + 7 objects)
- INIT marker: init_objects(), mode=1, my_pnum=2, object_count=0
- Objects placed at mode=1 (owner=-1, objnum=remote_objnum):
  - idx=0 type=4(PLAYER) id=0 seg=1   -> matches host obj[0]
  - idx=1 type=7(ROBOT) id=35 seg=82  -> matches host obj[1]
  - idx=2 type=7 id=20 seg=86         -> matches host obj[2]
  - idx=3 type=7 id=5 seg=27          -> matches host obj[3]
  - idx=4 type=7 id=2 seg=28          -> matches host obj[4]
  - idx=5 type=7 id=3 seg=29          -> matches host obj[5]
- **CORRUPT 7th object**: owner=0 -> MODE0 branch fires!
  - special_reset_objects() called (num_objects=6, free list rebuilt)
  - mode set to 0
  - obj_allocate() returns 6
  - type=0, id=0, seg=0 placed at idx=6
  - **Host obj[6] (type=7 id=38) is LOST**

#### Subsequent Packets (nobj=7 each)
Same pattern repeats for every packet:
- 6 objects correctly placed via objnum=remote_objnum (owner=-1 matches)
  (note: mode=0, so Int3() fires on `mode != 1` but continues in release)
- 7th object: owner=0 -> obj_allocate() returns ascending indices (7, 8, 9, ...)
  - OVERWRITES previously placed valid objects!

### Objects LOST (never placed) -- every 7th from the host stream:

| Host obj | Type | ID | Seg | Notes |
|---|---|---|---|---|
| 6 | 7 (ROBOT) | 38 | 199 | |
| 13 | 2 (POWERUP) | 38 | 93 | |
| 20 | 2 (POWERUP) | 39 | 150 | |
| 27 | 3 (HOSTAGE) | 0 | 189 | |
| 35 | 7 (ROBOT) | 1 | 183 | |
| **46** | **12 (GHOST)** | **6** | **82** | **GHOST LOST** |
| 53 | 7 (ROBOT) | 2 | 14 | |
| 60 | 2 (POWERUP) | 38 | 35 | |
| 67 | 7 (ROBOT) | 2 | 209 | |
| 74 | 7 (ROBOT) | 2 | 17 | |
| 81 | 2 (POWERUP) | 37 | 115 | |
| 88 | 2 (POWERUP) | 37 | 176 | |
| **95** | **7 (ROBOT)** | **22** | **205** | |
| 102 | 7 (ROBOT) | 22 | 225 | |
| 109 | 7 (ROBOT) | 2 | 227 | |

15 objects lost, including GHOST id=6 (obj[46]).

### Objects OVERWRITTEN by garbage (obj_allocate returns these indices):

| Idx | Was Type | Was ID | Overwritten By |
|---|---|---|---|
| 6 | (empty) | | garbage type=0 |
| 7 | 2 (POWERUP) | 39 | garbage type=0 |
| 8 | 7 (ROBOT) | 1 | garbage type=0 |
| 9 | 7 (ROBOT) | 1 | garbage type=0 |
| 10 | 7 (ROBOT) | 13 | garbage type=0 |
| 11 | 2 (POWERUP) | 37 | garbage type=0 |
| 12 | 2 (POWERUP) | 39 | garbage type=0 |
| 13 | (free, obj[13] was lost) | | garbage type=0 |
| 14 | 2 (POWERUP) | 39 | garbage type=0 |
| 15 | 2 (POWERUP) | 37 | garbage type=0 |
| **16** | **12 (GHOST)** | **1** | **garbage type=0** |
| 17 | 2 (POWERUP) | 37 | garbage type=0 |
| 18 | 2 (POWERUP) | 37 | garbage type=0 |
| 19 | 9 (CNTRLCEN) | 2 | garbage type=0 |
| 20 | (free, obj[20] was lost) | | garbage type=0 |

Then mode-1 objects (owner=1) via obj_allocate at indices 21, 22, 23:
| 21 | 2 (POWERUP) | 38 | type=7 ROBOT id=1 (owner=1) |
| 22 | 2 (POWERUP) | 38 | type=7 ROBOT id=11 (owner=1) |
| 23 | 7 (ROBOT) | 23 | type=7 ROBOT id=2 (owner=1) |

### Final Object State (First Attempt)

After all packets, before verify_objects:
- Highest_object_index = 23 (set by last obj_allocate, NOT by direct placement)
- Objects at indices 24-108 WERE placed correctly via direct placement but are
  INVISIBLE to verify_objects because it only scans 0..Highest_object_index
- Objects at indices 0-5: correctly placed from first packet
- Objects at indices 6-20: garbage (type=0) from obj_allocate
- Objects at indices 21-23: mode-1 objects (robots owned by player 1)

verify_objects scans 0..23:
- PLAYER=1 (idx 0)
- GHOST=0 (ghost at idx 16 was overwritten; other ghosts at 45,66,89,90 are above Highest)
- nplayers=1 < max_numplayers=4 -> SYNC FAILED

### Why `special_reset_objects()` is NOT called at end-marker

The end-marker code:
```c
if (mode == 1) {
    special_reset_objects();
    mode = 0;
}
```
But mode is already 0 (set by the first garbage object in packet 1). So the final
special_reset_objects that would correctly rebuild Highest_object_index is SKIPPED.

## Second Rejoin Attempt

The host re-sends with player_num=3, Highest=112. Key differences:
- Static vars in read_object_packet retain state from first attempt (mode=0)
- The init-marker packet (nobj=8) was apparently lost or logcat dropped its log lines
  - Evidence: object_count=0 and mode=0 in first visible packet, but my_pnum shows
    inconsistent values suggesting the init DID partially process
- Without init_objects(), the object array retains state from BOTH the level load AND
  the first failed attempt
- All the overwrite warnings show objects from the first attempt being re-written
- Same 7th-object corruption pattern
- End result: verify passes (nplayers=7 > max_numplayers=4) but object state is
  corrupted. Desync and crash follow.

## Root Cause Analysis

### Primary Cause: 7th Object Corruption

Every 7th object (the last in each 7-object packet) is received as all-zeros. This
causes:
1. owner=0 triggers the MODE0 branch instead of mode-1 placement
2. First occurrence calls special_reset_objects() prematurely and sets mode=0
3. obj_allocate() returns indices that collide with later direct placements
4. Final special_reset_objects at end-marker is skipped (mode already 0)
5. Highest_object_index is never set above 23 (only obj_allocate updates it)

### Why All Zeros?

The corruption is systematic: EVERY packet's LAST object is zeros. Not random
corruption. The first 6 objects in each packet are perfect matches with host data.

Hypotheses for investigation:

#### H1: The host is NOT ACTUALLY writing the 7th object to the buffer (STRONGEST)
The MPDIAG log shows the host logging obj[6] BEFORE writing it. What if the
size-check or loop logic has an off-by-one that breaks BEFORE writing the 7th
object, but AFTER incrementing obj_count_frame to include it?

Key code path:
```c
obj_count_frame++;
obj_count++;
remote_objnum = objnum_local_to_remote(i, &owner);
// MPDIAG log here -- logs the object data
PUT_INTEL_INT(object_buffer+loc, i);  // writes the object
...
```
The MPDIAG fires AFTER obj_count_frame++ but BEFORE writing. If the loop breaks
between MPDIAG and PUT_INTEL_INT... but there's no break point there.

HOWEVER: look at the size check placement:
```c
if ( loc + sizeof(object_rw) + 9 > UPID_MAX_SIZE-1 )
    break;
obj_count_frame++;
```
The size check is BEFORE obj_count_frame++. So if the size check passes, the
object IS counted AND written. If it fails, the object is NOT counted.

This hypothesis seems impossible given the code structure. BUT: verify by adding
a post-write MPDIAG that logs loc AFTER writing each object, to confirm the 7th
object's data is actually in the buffer.

#### H2: The send succeeds but recvfrom returns fewer bytes than sent
If `recvfrom` returns a truncated datagram, the last object would read from
uninitialized stack memory in the receive buffer (which could be zeros on Android
due to stack initialization). The `packet` buffer in `net_udp_listen` is NOT
zeroed -- it's `ubyte packet[UPID_MAX_SIZE]` on the stack.

To investigate: add a log of `msglen` from recvfrom for UPID_OBJECT_DATA packets.
Compare with expected size (9 + nobj * 273 bytes, adjusted for init marker).

Possible causes of truncation:
- WiFi MTU issues (1920-1929 bytes is above 1500 MTU but should fragment at IP layer)
- Android socket receive buffer too small
- `recvfrom` with MSG_TRUNC on Android?

#### H3: IP fragmentation causes partial packet delivery
UDP is reassembled at IP layer, so partial delivery shouldn't happen. However, if
the socket has MSG_TRUNC behavior or the kernel drops the reassembled datagram but
delivers a partial one...

This is unlikely on modern Linux/Android kernels but worth investigating.

#### H4: The MPDIAG macro itself causes a side effect (RULED OUT)
The bug existed in build 1111 before the per-object MPDIAG was added. The
256-byte stack buffer in the macro doesn't overflow for these format strings.

#### H5: WiFi / UDP delivery issue specific to these packet sizes
Packets are 1920-1929 bytes. The WiFi link between 192.168.88.21 and 192.168.88.46
might have an effective MTU that causes fragmentation, and one fragment is
consistently lost. If the LAST fragment is lost, the kernel would discard the
entire datagram (not deliver a partial one). So this should manifest as whole-packet
loss, not corruption of the last object.

UNLESS: the socket has `IP_RECVERR` or similar option set, or the kernel is doing
something non-standard for large UDP on WiFi.

## Critical Code Path Observations

### `net_udp_read_object_packet` has NO length parameter!

```c
// net_udp_process_packet receives `length`:
void net_udp_process_packet(ubyte *data, struct _sockaddr sender_addr, int length, int from_relay)

// But for UPID_OBJECT_DATA, it calls WITHOUT passing length:
case UPID_OBJECT_DATA:
    net_udp_read_object_packet(data);   // <-- no length param!
    break;
```

The function only knows `nobj` from the packet header. It blindly reads `nobj` objects
from the buffer with NO bounds checking. If recvfrom returned fewer bytes than
expected, the function reads uninitialized stack memory.

### The receive buffer is NOT zeroed

```c
void net_udp_listen() {
    ubyte packet[UPID_MAX_SIZE];   // stack memory, NOT zeroed
    ...
    size = udp_receive_packet(0, packet, UPID_MAX_SIZE, &sender_addr);
    while (size > 0) {
        net_udp_process_packet(packet, sender_addr, size, 0);
        size = udp_receive_packet(0, packet, UPID_MAX_SIZE, &sender_addr);
    }
}
```

The `packet` buffer is reused across loop iterations. `udp_receive_packet` zeroes ONE
byte at `packet[msglen]` but leaves the rest untouched. Bytes beyond the received data
contain either:
- data from a previous packet in the same loop iteration
- uninitialized stack garbage (first iteration)

### The only UPID_OBJECT_DATA validation is too-large check

```c
case UPID_OBJECT_DATA: if(data_len > UPID_MAX_SIZE) { rv = 0; } break;
```

There is NO minimum-size check. A truncated packet passes validation.

### Packet sizes vs WiFi MTU

- First data packet: 1929 bytes
- Subsequent data packets: 1920 bytes
- Typical WiFi MTU: 1500 bytes (IP payload: 1472 bytes after UDP/IP headers)
- Object [6] starts at byte offset 1656 in first packet, 1647 in subsequent packets
- Both are ABOVE the 1472-byte UDP payload limit for 1500-byte MTU

If packets are IP-fragmented and one fragment is lost, `recvfrom` should drop the
entire datagram (standard UDP behavior). BUT: if something on Android delivers a
partial datagram, the 7th object's bytes would be whatever is in the stack buffer.

The fact that the 7th object is consistently ALL ZEROS (not random garbage) suggests
EITHER:
- Stack memory at those offsets happens to be zero (possible with Android security
  features like stack zeroing)
- The previous packet in the loop iteration was the same size, and its 7th object
  was ALSO zeros (circular reasoning -- first packet must explain it)
- The kernel IS delivering partial datagrams (fragment 1 only) with the rest
  zero-filled

## Recommended Next Steps

### Build 1113: Full packet hex dump diagnostic

Goal: determine definitively whether the 7th object is (a) never written to the
send buffer, or (b) written correctly but lost/truncated in transit.

Dump the ENTIRE packet contents as base64 on both sides. This gives us byte-perfect
comparison without worrying about log truncation of long hex strings. base64 of
~1920 bytes is ~2560 chars, which fits in a single logcat line.

#### Send side (net_udp_send_objects, before dxx_sendto)
- Log `loc` (total bytes being sent)
- Base64-encode and log the full `object_buffer[0..loc-1]`

#### Receive side (net_udp_read_object_packet)
- Pass `length` from net_udp_process_packet into net_udp_read_object_packet
- Log `length` (msglen from recvfrom)
- Base64-encode and log the full `data[0..length-1]`

#### Interpretation
- If `length < loc`: **truncation**. recvfrom is delivering fewer bytes than sent
- If `length == loc` and base64 matches: bug is in the parsing code, not the data
- If `length == loc` and base64 differs: in-transit corruption
- If `length == loc` and the host's last 273 bytes are non-zero but client's are
  zeros: truncation with length lying (kernel bug) or stack zeroing

This single build answers the core question. Everything else follows from the answer.

### Non-negotiable fixes (real bugs, independent of diagnostic outcome)

These are correctness defects regardless of what causes the 7th-object corruption.
They should be applied no matter what the hex dump shows.

#### Fix 1: Pass packet length to read_object_packet, bounds-check every read

```c
void net_udp_read_object_packet(ubyte *data, int data_len)
{
    ...
    // Before reading each object:
    if (loc + 9 + sizeof(object_rw) > data_len) {
        con_printf(CON_URGENT, "read_object_packet: TRUNCATED at obj %d, "
                   "loc=%d data_len=%d, aborting rejoin\n", i, loc, data_len);
        // discard this rejoin attempt cleanly rather than half-applying garbage
        break;
    }
    ...
}
```

The receiver must never trust `nobj` more than `msglen`. A truncated packet should
result in a clean rejoin abort, not silent corruption of the object table.

#### Fix 2: Update Highest_object_index during direct placement

Currently only `obj_allocate()` updates Highest_object_index. Direct placement
(`objnum = remote_objnum`) can place objects at index 90+ while Highest stays at 23.
After direct placement, add:
```c
if (objnum > Highest_object_index)
    Highest_object_index = objnum;
```

#### Fix 3: Unconditional special_reset_objects at end-marker

Currently:
```c
if (mode == 1) { special_reset_objects(); mode = 0; }
```
A single bad object flipping mode to 0 means the final free-list rebuild is skipped
and Highest_object_index is wrong. Change to always call special_reset_objects()
at the end-marker, regardless of mode.

#### Fix 4: Treat truncated/corrupt object packet as rejoin failure

If the bounds check in Fix 1 fires, do not continue the rejoin. Set a flag that
causes verify_objects to report failure, or send a "rejoin aborted" message back
to the host so it can retry, rather than letting the client enter the game with a
corrupt object table.

### Robustness improvement (optional, second-order)

#### Smaller packet sizing

Reducing max packet size from 2047 to ~1400 bytes (5 objects/packet instead of 7)
avoids IP fragmentation over 1500-MTU links. This is a latency/reliability tuning
knob, not a correctness fix. With bounds-checked receive (Fix 1), fragmentation
can at worst cause packet loss -> clean rejoin retry, never silent corruption.

Do this AFTER the non-negotiable fixes, if at all. The game already works over the
internet where MTU can be anything -- making the protocol depend on a specific MTU
is brittle. The right answer is: detect truncation, fail cleanly, retry.

### Design principle

There will always be a link, tunnel, VPN, or WiFi setup with a smaller effective
MTU. Designing packets around a guessed MTU trades "silent corruption" for "works
on most networks." The correct fix is: make the receiver length-aware so that
truncation causes a clean retry, not a desync. MTU-sized packets are a performance
optimization (fewer fragments = fewer drops = faster rejoins), not a correctness
requirement.

---

## Build 1113: Root Cause CONFIRMED

### Hex dump evidence

Build 1113 added full packet hex dumps (single-line, static 4096-byte buffer) and
crash breadcrumbs. The data_len parameter was already plumbed in from build 1112
diagnostics.

Key log line from client:
```
12:51:51.134 MPDIAG: read_object_packet: nobj=8 mode=0 object_count=0 data_len=1500
```

**data_len=1500** -- the WiFi MTU. The host sent 1929 bytes (9-byte header + 9-byte
INIT marker + 7 * 273-byte objects). recvfrom returned exactly 1500.

### Where the truncation bites

- Object 7 starts at byte offset 1656 (9 header + 9 init + 6*273)
- The entire 7th object_rw (264 bytes) lies beyond byte 1500
- Reading data[1656..1919] reads uninitialized stack memory
- On Android/ARM64, stack memory was zeros -> "all zeros" corruption pattern
- On other platforms, it would be random garbage -> different failure mode

### Crash sequence (from breadcrumbs)

```
[152] read_obj_pkt: nobj=8 len=1500
[153] pktdump: RX len=1500
[154] pktdump: hex done
[155] pktdump: done
```

Pktdump completes successfully. The crash happens AFTERWARD during object
processing -- specifically from Assert/Int3 failures when reading garbage fields
from the truncated region. The SIGABRT was from assert() firing on invalid data.

### Hypothesis resolution

- **H1 (host doesn't write 7th object)**: RULED OUT. Hex dump shows host sends the
  full buffer. The 7th object IS in the send buffer.
- **H2 (recvfrom returns fewer bytes)**: **CONFIRMED**. data_len=1500, host sent 1929.
  WiFi MTU truncation delivers only the first 1500 bytes.
- **H3 (IP fragmentation partial)**: Confirmed variant: the second IP fragment is
  lost, and on this platform/kernel, recvfrom delivers the partial datagram rather
  than discarding it. This is unusual but observed.
- **H4 (MPDIAG side effect)**: RULED OUT (existed before diagnostics).

## Build 1114: Fixes Implemented

### Fix 1: Bounds-checking in read_object_packet (DONE, both D1 and D2)

Two bounds checks added:
1. **Header check**: `if (loc + 9 > data_len) break;` before reading the 9-byte
   per-object header (objnum + owner + remote_objnum)
2. **Body check**: `if (loc + sizeof(object_rw) > data_len) break;` before reading
   the object_rw data via multi_object_rw_to_object

When truncation is detected, the loop breaks cleanly. The existing verify_objects
check at the end-marker catches the object count mismatch and shows "sync failed"
instead of crashing.

### Fix 2: Highest_object_index update during MODE1 placement (DONE, both D1 and D2)

After `objnum = remote_objnum` in the MODE1 branch, added:
```c
if (objnum > Highest_object_index)
    Highest_object_index = objnum;
```

Previously, only obj_allocate() updated Highest_object_index. Direct placement at
high indices (e.g., index 109) left Highest at 0, causing verify_objects and
special_reset_objects to miss all directly-placed objects. Note: special_reset_objects
scans ALL MAX_OBJECTS so it would have found them, but only if it was called (see Fix 3).

### Fix 3: Unconditional special_reset_objects at end-marker (DONE, both D1 and D2)

Changed from `if (mode == 1) { special_reset_objects(); }` to always calling
special_reset_objects() at the end-marker. A single corrupt/truncated object
flipping mode to 0 prematurely caused the final free-list rebuild to be skipped.
Now the free list and Highest_object_index are always rebuilt correctly before
verify_objects runs.

### Fix 4 (Assert/Int3 breadcrumbs): DONE, both D1 and D2

Added `#ifdef __ANDROID__` override in dxxerror.h:
- `Assert(expr)` logs `crash_breadcrumb_v("ASSERT FAIL: %s at %s:%d", ...)` before
  calling `assert(expr)`. Uses expression form (ternary) to remain compatible with
  comma expressions like `SEG_PTR_2_NUM`.
- `Int3()` logs `crash_breadcrumb_v("Int3 at %s:%d", ...)` instead of being a no-op.

### Net effect of fixes

With truncation at 1500 bytes:
- **Before**: reads garbage past buffer -> corrupt objects -> wrong mode transitions
  -> Highest_object_index stuck at 23 -> special_reset skipped -> SIGABRT or desync
- **After**: bounds check fires at object 7 -> loop breaks -> 6 of 7 objects placed
  correctly -> end-marker calls special_reset unconditionally -> verify_objects sees
  count mismatch (some objects lost) -> clean "sync failed" -> user can retry

The sync will still fail when packets are truncated (lost objects can't be recovered
without a resend mechanism). But it fails CLEANLY instead of crashing. Future work
could add retry/resend logic for truncated packets.

### Status: Build 1114 compiles, deployed for testing

## Appendix: Object Type Reference

| Type | Name |
|---|---|
| 0 | OBJ_WALL |
| 2 | OBJ_POWERUP |
| 3 | OBJ_HOSTAGE |
| 4 | OBJ_PLAYER |
| 7 | OBJ_ROBOT |
| 9 | OBJ_CNTRLCEN |
| 12 | OBJ_GHOST |
| 14 | OBJ_COOP |

## Appendix: Timeline

| Time | Event |
|---|---|
| 08:47:21.645 | Client auto_join (2nd attempt to reach menu) |
| 08:47:22.544 | Host: UPID_REQUEST from Player78 |
| 08:47:28.585 | Host: welcome_player, starts sending objects (INIT player_num=2 Highest=109) |
| 08:47:28.585-29.185 | Host: sends 108 objects across ~15 packets at 50fps |
| 08:47:29.083-29.677 | Client: receives and processes all packets |
| 08:47:29.677 | Client: end marker, verify_objects -> SYNC FAILED |
| 08:47:48.310 | Client: auto_join again (3rd attempt) |
| 08:47:51.225 | Host: welcome_player, resends (INIT player_num=3 Highest=112) |
| 08:47:51.763-52.357 | Client: receives packets (init marker log possibly lost) |
| 08:47:52.358 | Client: end marker, verify_objects -> passes with nplayers=7 (stale data) |
| (shortly after) | Game enters, desynced, crashes |
