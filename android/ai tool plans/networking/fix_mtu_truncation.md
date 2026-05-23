# Plan: Fix object sync packet loss from IP fragmentation

## Problem
- Host sends UPID_OBJECT_DATA packets up to ~1920 bytes (7 objects * 273 bytes per object + 9 header)
- These exceed the 1500-byte WiFi MTU and get split into 2 IP fragments each
- UDP datagrams are atomic (delivered whole or dropped), but IP fragmentation means
  losing any single fragment drops the entire datagram
- On WiFi, fragment loss is common. Over ~16 fragmented packets, 2-3 entire datagrams
  are dropped, losing ~15 objects total
- verify_objects detects: remote=110 local=95 diff=15

## Root cause
- `send_objects()` used `UPID_MAX_SIZE-1` (2047) as the per-packet size limit
- Packets of ~1920 bytes exceed the typical 1500-byte MTU
- IP fragments them; any lost fragment silently drops the whole datagram
- This is NOT "UDP truncation" -- UDP never truncates. The datagrams are simply lost

## Fix
1. [x] Add `UPID_MTU_SAFE` constant (1472 = 1500 - 20 IP hdr - 8 UDP hdr) in net_udp.h
2. [x] Change the send_objects loop break condition from UPID_MAX_SIZE-1 to UPID_MTU_SAFE-1
3. [x] Keep UPID_MAX_SIZE at 2048 for receive buffers (unchanged)
4. [x] Same changes in both D1 and D2

## Impact
- With limit 1472: max 5 objects per packet (5*273 + 9 = 1374 bytes)
- For 110 objects: ~22 packets instead of ~16 (more packets, each safe)
- Slightly more packets during rejoin, but rejoin is already throttled at 50/sec
- No effect on other packet types (mdata already limited to 1024)

## Audit results (thorough receive-path audit)

### Original upstream code (pre build 1114)
- `net_udp_read_object_packet(ubyte *data)` -- no `data_len` parameter at all
- `length` is available at the call site (`net_udp_process_packet`) but NOT passed
- Parser trusts `nobj` from payload bytes 5-8 as the sole loop bound
- Stack buffer `packet[UPID_MAX_SIZE]` is never zeroed, reused across multiple
  `recvfrom()` calls within `net_udp_listen()`
- Defense: `valid_size()` only checks `data_len > UPID_MAX_SIZE` (upper bound)
  -- no lower bound check, so a 50-byte packet claiming nobj=7 would pass

### Is there a parsing bug?
No. Under normal operation, the send side sets nobj to exactly match the
serialized content:
- `obj_count_frame` is initialized to 0 (or 1 for INIT) each call
- Incremented once per serialized object
- Written to the packet header AFTER the serialization loop
- Packet sent with exactly `loc` bytes
- The INIT marker produces exactly 9 bytes (no body), which the receiver
  correctly handles via `continue` (no body read)

The send/receive are perfectly symmetric. nobj always matches the actual
content. The parser never reads past the datagram boundary in normal operation.

### Buffer stale-data scenario
If a large packet arrives, then a smaller packet arrives in the same
net_udp_listen() call, the stack buffer tail contains data from the first
packet. But this only matters if the parser reads past data_len, which it
doesn't because nobj correctly bounds the loop.

### What actually causes the 15-object loss
Whole-datagram loss. Each 1920-byte packet gets IP-fragmented into 2 fragments
on a 1500-byte WiFi MTU link. Loss of either fragment silently drops the
entire datagram. Over ~16 packets, 2-3 are lost, accounting for ~15 objects.
This is NOT truncation (UDP never truncates) and NOT a parsing bug.

### What the UPID_MTU_SAFE fix actually does
By keeping packets under 1472 bytes (5 objects, 1374 bytes), each fits in a
single IP frame. No fragmentation means no fragment loss. Dramatically reduces
the probability of packet loss during object sync.

### What the bounds checks (build 1114) do
Defense-in-depth. They protect against corrupted nobj or hypothetical future
changes that might break the send/receive symmetry. They don't prevent
whole-datagram loss (nothing can at the application layer without ACK/retry).
