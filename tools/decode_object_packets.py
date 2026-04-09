#!/usr/bin/env python3
"""Decode UPID_OBJECT_DATA packet hex dumps from DXX multiplayer PKTDUMP logs.

Usage:
  python decode_object_packets.py <logfile>        -- decode all PKTDUMP lines
  python decode_object_packets.py <logfile> --diff  -- compare TX vs RX to show lost objects
  python decode_object_packets.py --hex <hexstring> -- decode a single raw hex packet

Log lines are expected in the logcat format containing "PKTDUMP" followed by:
  TX len=NNNN <hex>  or  RX len=NNNN <hex>
"""

import sys
import struct
import re
from collections import OrderedDict

# --- Constants ---

UPID_OBJECT_DATA = 0x0b
SIZEOF_OBJECT_RW = 264  # verified: 9(hdr) + 7*273 = 1920 per continuation packet
PER_OBJ_HEADER = 9      # 4 (local objnum) + 1 (owner) + 4 (remote objnum)

OBJ_TYPE_NAMES = {
    0: "WALL", 1: "FIREBALL", 2: "ROBOT", 3: "HOSTAGE", 4: "PLAYER",
    5: "WEAPON", 6: "CAMERA", 7: "POWERUP", 8: "DEBRIS", 9: "CNTRLCEN",
    10: "FLARE", 11: "CLUTTER", 12: "GHOST", 13: "LIGHT", 14: "COOP",
    15: "MARKER", 255: "NONE",
}

# object_rw field offsets (packed, no WORDS_NEED_ALIGNMENT)
# int signature        @ 0  (4)
# ubyte type           @ 4  (1)
# ubyte id             @ 5  (1)
# short next           @ 6  (2)
# short prev           @ 8  (2)
# ubyte control_type   @ 10 (1)
# ubyte movement_type  @ 11 (1)
# ubyte render_type    @ 12 (1)
# ubyte flags          @ 13 (1)
# short segnum         @ 14 (2)
# short attached_obj   @ 16 (2)
# vms_vector pos       @ 18 (12) - 3x int32
# vms_matrix orient    @ 30 (36) - 9x int32
# fix size             @ 66 (4)
# fix shields          @ 70 (4)
# vms_vector last_pos  @ 74 (12)
# sbyte contains_type  @ 86 (1)
# sbyte contains_id    @ 87 (1)
# sbyte contains_count @ 88 (1)
# sbyte matcen_creator @ 89 (1)
# fix lifeleft         @ 90 (4)


def fix_to_float(fix_val):
    """Convert fixed-point (16.16) to float"""
    return fix_val / 65536.0


def decode_object_rw_key_fields(data):
    """Extract key identification fields from a 264-byte object_rw blob"""
    if len(data) < 94:
        return {"error": f"body too short ({len(data)} bytes)"}

    sig = struct.unpack_from('<i', data, 0)[0]
    obj_type = data[4]
    obj_id = data[5]
    next_obj, prev_obj = struct.unpack_from('<hh', data, 6)
    ctrl_type = data[10]
    move_type = data[11]
    render_type = data[12]
    flags = data[13]
    segnum = struct.unpack_from('<h', data, 14)[0]
    attached = struct.unpack_from('<h', data, 16)[0]
    px, py, pz = struct.unpack_from('<iii', data, 18)
    size = struct.unpack_from('<i', data, 66)[0]
    shields = struct.unpack_from('<i', data, 70)[0]
    contains_type = struct.unpack_from('<b', data, 86)[0]
    contains_id = struct.unpack_from('<b', data, 87)[0]
    contains_count = struct.unpack_from('<b', data, 88)[0]
    lifeleft = struct.unpack_from('<i', data, 90)[0]

    type_name = OBJ_TYPE_NAMES.get(obj_type, f"?{obj_type}")
    return {
        "sig": sig, "type": type_name, "type_num": obj_type, "id": obj_id,
        "segnum": segnum, "flags": flags,
        "ctrl": ctrl_type, "move": move_type, "render": render_type,
        "pos": (fix_to_float(px), fix_to_float(py), fix_to_float(pz)),
        "size": fix_to_float(size), "shields": fix_to_float(shields),
        "contains": (contains_type, contains_id, contains_count),
        "lifeleft": fix_to_float(lifeleft),
        "next": next_obj, "prev": prev_obj, "attached": attached,
    }


def decode_packet(hex_str, label=""):
    """Decode a single UPID_OBJECT_DATA packet from hex string.
    Returns dict with header info and list of decoded objects."""

    data = bytes.fromhex(hex_str)
    result = {"label": label, "raw_len": len(data), "objects": [], "truncated": False}

    if len(data) < 9:
        result["error"] = f"packet too short ({len(data)} < 9)"
        return result

    pkt_type = data[0]
    token = struct.unpack_from('<I', data, 1)[0]
    nobj = struct.unpack_from('<i', data, 5)[0]
    result["type"] = pkt_type
    result["token"] = token
    result["nobj_declared"] = nobj

    if pkt_type != UPID_OBJECT_DATA:
        result["error"] = f"not UPID_OBJECT_DATA (got 0x{pkt_type:02x})"
        return result

    loc = 9
    objects_parsed = 0

    for i in range(nobj):
        if loc + PER_OBJ_HEADER > len(data):
            result["truncated"] = True
            result["truncated_at"] = f"header {i}/{nobj}, loc={loc}, avail={len(data)}"
            break

        local_objnum = struct.unpack_from('<i', data, loc)[0]
        owner = struct.unpack_from('<b', data, loc + 4)[0]
        remote_objnum = struct.unpack_from('<i', data, loc + 5)[0]
        loc += PER_OBJ_HEADER

        entry = OrderedDict()
        entry["entry_idx"] = i

        if local_objnum == -1:
            entry["marker"] = "INIT"
            entry["player_num"] = owner
            objects_parsed += 1
            result["objects"].append(entry)
            continue
        elif local_objnum == -2:
            entry["marker"] = "END"
            entry["player_num"] = owner
            entry["total_obj_count"] = remote_objnum
            objects_parsed += 1
            result["objects"].append(entry)
            continue

        entry["local_objnum"] = local_objnum
        entry["owner"] = owner
        entry["remote_objnum"] = remote_objnum

        if loc + SIZEOF_OBJECT_RW > len(data):
            result["truncated"] = True
            result["truncated_at"] = f"body {i}/{nobj}, loc={loc}, need={loc + SIZEOF_OBJECT_RW}, avail={len(data)}"
            entry["body_truncated"] = True
            # Try to decode whatever partial bytes we have
            partial = data[loc:]
            if len(partial) >= 16:
                entry["partial_fields"] = {
                    "type": OBJ_TYPE_NAMES.get(partial[4], f"?{partial[4]}"),
                    "id": partial[5],
                    "segnum": struct.unpack_from('<h', partial, 14)[0] if len(partial) >= 16 else "?",
                }
            objects_parsed += 1
            result["objects"].append(entry)
            break

        body = data[loc:loc + SIZEOF_OBJECT_RW]
        loc += SIZEOF_OBJECT_RW
        entry.update(decode_object_rw_key_fields(body))
        objects_parsed += 1
        result["objects"].append(entry)

    result["objects_parsed"] = objects_parsed
    expected_full = 9 + sum(
        PER_OBJ_HEADER + (SIZEOF_OBJECT_RW if "marker" not in o else 0)
        for o in result["objects"]
    )
    # Estimate what full packet size should have been
    if result["truncated"]:
        remaining = nobj - len([o for o in result["objects"] if "marker" not in o and not o.get("body_truncated")])
        result["bytes_lost"] = len(data) - result["raw_len"]  # always 0 for raw, useful in diff
        result["objects_lost_in_packet"] = nobj - objects_parsed

    return result


def format_object(obj, indent="  "):
    """Format a decoded object entry for display"""
    if "marker" in obj:
        if obj["marker"] == "INIT":
            return f"{indent}[INIT] player_num={obj['player_num']}"
        elif obj["marker"] == "END":
            return f"{indent}[END] player_num={obj['player_num']} total_count={obj['total_obj_count']}"
    parts = [f"{indent}local={obj.get('local_objnum','?'):>3d}"]
    parts.append(f"remote={obj.get('remote_objnum','?'):>3d}")
    parts.append(f"owner={obj.get('owner','?'):>2d}")
    if obj.get("body_truncated"):
        pf = obj.get("partial_fields", {})
        parts.append(f"TRUNCATED (partial: type={pf.get('type','?')} id={pf.get('id','?')} seg={pf.get('segnum','?')})")
        return " ".join(parts)
    parts.append(f"type={obj.get('type','?'):<10s}")
    parts.append(f"id={obj.get('id','?'):>3d}")
    parts.append(f"seg={obj.get('segnum','?'):>4d}")
    parts.append(f"shields={obj.get('shields',0):>8.1f}")
    if obj.get("contains", (0, 0, 0))[2] > 0:
        ct, ci, cc = obj["contains"]
        parts.append(f"contains=({ct},{ci},{cc})")
    return " ".join(parts)


def format_packet(pkt):
    """Format a decoded packet for display"""
    lines = []
    hdr = f"--- {pkt['label']} ---  len={pkt['raw_len']}  token=0x{pkt.get('token',0):08x}  nobj={pkt.get('nobj_declared','?')}"
    lines.append(hdr)
    if "error" in pkt:
        lines.append(f"  ERROR: {pkt['error']}")
        return "\n".join(lines)
    for obj in pkt["objects"]:
        lines.append(format_object(obj))
    if pkt.get("truncated"):
        lines.append(f"  *** TRUNCATED: {pkt['truncated_at']}")
    return "\n".join(lines)


def extract_pktdump_lines(log_text):
    """Extract PKTDUMP lines from logcat output.
    Returns list of (direction, declared_len, hex_data) tuples."""
    # Match patterns like:
    #   PKTDUMP TX len=1929 0b...
    #   [netlog] PKTDUMP TX len=1929 0b...
    pattern = re.compile(r'PKTDUMP\s+(TX|RX)\s+len=(\d+)\s+([0-9a-fA-F]+)')
    results = []
    for line in log_text.splitlines():
        m = pattern.search(line)
        if m:
            direction = m.group(1)
            declared_len = int(m.group(2))
            hex_data = m.group(3)
            results.append((direction, declared_len, hex_data))
    return results


def analyze_diff(tx_packets, rx_packets):
    """Compare TX and RX packet sets to identify lost objects"""
    print("\n=== TX vs RX Comparison ===\n")

    # Collect all TX objects by (local_objnum, remote_objnum)
    tx_objects = OrderedDict()
    tx_init = None
    tx_end = None
    for pkt in tx_packets:
        for obj in pkt["objects"]:
            if "marker" in obj:
                if obj["marker"] == "INIT":
                    tx_init = obj
                elif obj["marker"] == "END":
                    tx_end = obj
                continue
            key = obj.get("local_objnum", -99)
            tx_objects[key] = obj

    # Collect all RX objects
    rx_objects = OrderedDict()
    rx_received_count = 0
    rx_truncation_count = 0
    for pkt in rx_packets:
        for obj in pkt["objects"]:
            if "marker" in obj:
                continue
            if obj.get("body_truncated"):
                rx_truncation_count += 1
                continue
            key = obj.get("local_objnum", -99)
            rx_objects[key] = obj
            rx_received_count += 1

    print(f"TX sent {len(tx_objects)} objects")
    if tx_end:
        print(f"TX END marker: total_count={tx_end['total_obj_count']}")
    print(f"RX received {rx_received_count} complete objects, {rx_truncation_count} truncated")

    # Find missing
    missing = []
    for key, obj in tx_objects.items():
        if key not in rx_objects:
            missing.append(obj)

    if missing:
        print(f"\n--- {len(missing)} objects LOST (sent but not received) ---")
        for obj in missing:
            print(format_object(obj))
    else:
        print("\nNo objects lost")

    # Check for truncated packets
    trunc_pkts = [p for p in rx_packets if p.get("truncated")]
    if trunc_pkts:
        print(f"\n--- {len(trunc_pkts)} RX packets were truncated ---")
        for p in trunc_pkts:
            print(f"  {p['label']}: declared={p['nobj_declared']} received, "
                  f"truncated at: {p['truncated_at']}")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    diff_mode = "--diff" in sys.argv

    if sys.argv[1] == "--hex":
        if len(sys.argv) < 3:
            print("Usage: decode_object_packets.py --hex <hexstring>")
            sys.exit(1)
        pkt = decode_packet(sys.argv[2], "CLI")
        print(format_packet(pkt))
        sys.exit(0)

    log_file = sys.argv[1]
    with open(log_file, 'r', encoding='utf-8', errors='replace') as f:
        log_text = f.read()

    entries = extract_pktdump_lines(log_text)
    if not entries:
        print(f"No PKTDUMP lines found in {log_file}")
        sys.exit(1)

    print(f"Found {len(entries)} PKTDUMP lines\n")

    tx_packets = []
    rx_packets = []

    for idx, (direction, declared_len, hex_data) in enumerate(entries):
        actual_bytes = len(hex_data) // 2
        label = f"PKT#{idx} {direction} (declared={declared_len}, hex_bytes={actual_bytes})"
        pkt = decode_packet(hex_data, label)

        if declared_len != actual_bytes:
            # The hex dump might be truncated vs the declared length
            pkt["declared_len_mismatch"] = f"declared={declared_len} actual_hex={actual_bytes}"

        if direction == "TX":
            tx_packets.append(pkt)
        else:
            rx_packets.append(pkt)

        if not diff_mode:
            print(format_packet(pkt))
            if "declared_len_mismatch" in pkt:
                print(f"  !! LENGTH MISMATCH: {pkt['declared_len_mismatch']}")
            print()

    if diff_mode and tx_packets and rx_packets:
        analyze_diff(tx_packets, rx_packets)
    elif diff_mode:
        print("Need both TX and RX packets for diff mode")
        # Still print what we have
        for pkt in tx_packets + rx_packets:
            print(format_packet(pkt))
            print()

    # Summary
    print(f"\n=== Summary ===")
    print(f"TX packets: {len(tx_packets)}")
    print(f"RX packets: {len(rx_packets)}")
    tx_obj_count = sum(
        len([o for o in p["objects"] if "marker" not in o and not o.get("body_truncated")])
        for p in tx_packets
    )
    rx_obj_count = sum(
        len([o for o in p["objects"] if "marker" not in o and not o.get("body_truncated")])
        for p in rx_packets
    )
    rx_trunc = sum(1 for p in rx_packets if p.get("truncated"))
    print(f"TX objects: {tx_obj_count}")
    print(f"RX objects: {rx_obj_count} ({rx_trunc} packets truncated)")
    if tx_obj_count > rx_obj_count:
        print(f"LOST: {tx_obj_count - rx_obj_count} objects")


if __name__ == "__main__":
    main()
