"""Exercise both object-packet decoder CLIs with native-format diagnostic records."""

from pathlib import Path
import re
import struct
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = Path(sys.argv[1]).resolve()
DECODERS = {
    "python": [sys.executable, str(ROOT / "android/tools/decode_object_packets.py")],
    "powershell": ["pwsh", "-NoProfile", "-File", str(ROOT / "android/tools/decode_object_packets.ps1")],
}


def object_packet(number=5, owner=0):
    body = bytearray(264)
    struct.pack_into("<iBBhhBBBBhh", body, 0, 123, 2, 7, -1, -1, 1, 1, 1, 0, 12, -1)
    struct.pack_into("<iii", body, 18, 65536, 131072, -65536)
    struct.pack_into("<i", body, 70, 100 * 65536)
    return struct.pack("<BIiibi", 11, 1, 1, number, owner, number) + body


def record(direction, packet, prefix="", marker="[PKTDUMP]"):
    return f"{prefix}{marker} {direction} len={len(packet)} {packet.hex()}"


class DecoderCliTest(unittest.TestCase):
    def cli(self, decoder, text=None, raw=None, diff=False):
        command = list(DECODERS[decoder])
        if raw is not None:
            command += ["--hex" if decoder == "python" else "-Hex", raw.hex()]
        else:
            path = WORKSPACE / "capture.log"
            path.write_text(text, encoding="utf-8", newline="\n")
            command += [str(path)]
            if diff:
                command += ["--diff" if decoder == "python" else "-Diff"]
        return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", cwd=ROOT, timeout=20)

    def check_capture(self, lines, tx, rx, tx_objects, rx_objects, diff=False):
        for decoder in DECODERS:
            with self.subTest(decoder=decoder, diff=diff):
                result = self.cli(decoder, text="\r\n".join(lines) + "\r\n", diff=diff)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(result.stderr, "")
                for label, count in [
                    ("TX packets", tx),
                    ("RX packets", rx),
                    ("TX objects", tx_objects),
                    ("RX objects", rx_objects),
                ]:
                    self.assertRegex(result.stdout, rf"(?m)^{label}: {count}(?:$|[ \r])")
                self.assertIn(f"Found {tx + rx} PKTDUMP lines", result.stdout)
                if not diff:
                    headers = re.findall(r"--- PKT#(\d+) (TX|RX) ", result.stdout)
                    self.assertEqual(len(headers), tx + rx)
                    self.assertEqual([int(index) for index, _ in headers], list(range(tx + rx)))
                self.assertNotIn("TRUNCATED", result.stdout)
                self.assertNotIn("ERROR:", result.stdout)
                if diff and tx and rx:
                    self.assertIn(f"TX sent {tx_objects} objects", result.stdout)
                    self.assertIn(f"RX received {rx_objects} complete objects", result.stdout)

    def test_zero_records_reject_cleanly_in_normal_and_diff_modes(self):
        for decoder in DECODERS:
            for diff in (False, True):
                with self.subTest(decoder=decoder, diff=diff):
                    result = self.cli(decoder, text="unrelated log message\n", diff=diff)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertEqual(result.stderr, "")
                    self.assertIn("No PKTDUMP lines found", result.stdout)
                    self.assertNotIn("Summary", result.stdout)

    def test_one_tx_record_from_logcat(self):
        self.check_capture([record("TX", object_packet(), "10-08 05:00:00.001 123 456 I DXX: ")], 1, 0, 1, 0)

    def test_one_rx_record_from_netlog(self):
        self.check_capture([record("RX", object_packet(), "[netlog] ")], 0, 1, 0, 1)

    def test_one_record_diff_still_decodes_available_direction(self):
        for direction in ("TX", "RX"):
            self.check_capture(
                [record(direction, object_packet())],
                int(direction == "TX"),
                int(direction == "RX"),
                int(direction == "TX"),
                int(direction == "RX"),
                diff=True,
            )

    def test_many_records_preserve_order_counts_and_one_transfer_diff(self):
        init = struct.pack("<BIiiBi", 11, 1, 1, -1, 0, 0)
        end = struct.pack("<BIiiBi", 11, 1, 1, -2, 0, 2)
        lines = [
            record(direction, packet, "[netlog] ")
            for packet in (init, object_packet(5), object_packet(6), end)
            for direction in ("TX", "RX")
        ]
        self.check_capture(lines, 4, 4, 2, 2)
        self.check_capture(lines, 4, 4, 2, 2, diff=True)

    def test_plain_marker_and_uppercase_hex_remain_supported(self):
        line = record("TX", object_packet(), marker="PKTDUMP")
        header, hex_data = line.rsplit(" ", 1)
        self.check_capture([header + " " + hex_data.upper() + "  "], 1, 0, 1, 0)

    def test_single_transfer_diff_reports_the_missing_object(self):
        text = "\n".join(
            [record("TX", object_packet(5)), record("TX", object_packet(6)), record("RX", object_packet(5))]
        )
        for decoder in DECODERS:
            with self.subTest(decoder=decoder):
                result = self.cli(decoder, text=text, diff=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(result.stderr, "")
                self.assertIn("TX sent 2 objects", result.stdout)
                self.assertIn("RX received 1 complete objects", result.stdout)
                self.assertIn("--- 1 objects LOST", result.stdout)
                self.assertRegex(result.stdout, r"local=\s*6\s+remote=\s*6")

    def test_nonrecords_cannot_contribute_partial_hex_matches(self):
        valid = record("TX", object_packet())
        nonrecords = [
            valid.replace("[PKTDUMP]", "[PKTDUMP"),
            valid.replace("[PKTDUMP]", "NOTPKTDUMP"),
            valid.replace(" TX ", " OTHER "),
            valid.replace(" TX ", " tx "),
            valid + "0",
            valid + "zz",
            valid + " trailing text",
            valid.replace(" len=", " len=-"),
            "[PKTDUMP] TX len=0 ",
        ]
        self.check_capture(nonrecords + [valid, record("RX", object_packet())], 1, 1, 1, 1)

    def test_raw_hex_path_keeps_decoded_object_fields(self):
        for decoder in DECODERS:
            with self.subTest(decoder=decoder):
                result = self.cli(decoder, raw=object_packet())
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(result.stderr, "")
                self.assertIn("len=282", result.stdout)
                self.assertIn("token=0x00000001", result.stdout)
                self.assertRegex(
                    result.stdout,
                    r"local=\s*5\s+remote=\s*5\s+owner=\s*0\s+type=ROBOT\s+id=\s*7\s+seg=\s*12\s+shields=\s*100\.0",
                )

    def test_signed_owner_boundaries_in_raw_objects_and_markers(self):
        for owner in (-128, -1, 0, 127):
            for packet, expected in [
                (object_packet(owner=owner), rf"owner=\s*{owner}\s+type=ROBOT"),
                (struct.pack("<BIiibi", 11, 1, 1, -1, owner, 0), rf"\[INIT\] player_num={owner}(?:\s|$)"),
                (struct.pack("<BIiibi", 11, 1, 1, -2, owner, 1), rf"\[END\] player_num={owner} total_count=1"),
            ]:
                for decoder in DECODERS:
                    with self.subTest(owner=owner, decoder=decoder, expected=expected):
                        result = self.cli(decoder, raw=packet)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertEqual(result.stderr, "")
                        self.assertRegex(result.stdout, expected)

    def test_signed_owner_log_and_diff_keep_single_transfer_counts(self):
        for owner in (-128, -1, 0, 127):
            init = struct.pack("<BIiibi", 11, 1, 1, -1, owner, 0)
            end = struct.pack("<BIiibi", 11, 1, 1, -2, owner, 1)
            lines = [
                record(direction, packet)
                for packet in (init, object_packet(owner=owner), end)
                for direction in ("TX", "RX")
            ]
            self.check_capture(lines, 3, 3, 1, 1)
            self.check_capture(lines, 3, 3, 1, 1, diff=True)

    def test_incomplete_raw_packets_have_intentional_diagnostics_and_failure_status(self):
        samples = [
            (bytes.fromhex("0b00"), "ERROR: packet too short (2 < 9)"),
            (struct.pack("<BIi", 12, 1, 0), "ERROR: not UPID_OBJECT_DATA (got 0x0c)"),
            (object_packet()[:13], "TRUNCATED: header 0/1"),
            (object_packet()[:18], "TRUNCATED (partial: type=? id=? seg=?)"),
            (object_packet()[:34], "TRUNCATED (partial: type=ROBOT id=7 seg=12)"),
        ]
        for decoder in DECODERS:
            for packet, diagnostic in samples:
                with self.subTest(decoder=decoder, diagnostic=diagnostic):
                    result = self.cli(decoder, raw=packet)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertEqual(result.stderr, "")
                    self.assertIn(diagnostic, result.stdout)
                    self.assertNotIn("LOST:", result.stdout)

    def test_incomplete_logged_packets_keep_diagnostics_and_prevent_loss_claims(self):
        samples = [
            (record("RX", bytes.fromhex("0b00")), "ERROR: packet too short (2 < 9)"),
            (record("RX", struct.pack("<BIi", 12, 1, 0)), "ERROR: not UPID_OBJECT_DATA (got 0x0c)"),
            (record("RX", object_packet()[:34]), "TRUNCATED (partial: type=ROBOT id=7 seg=12)"),
            (
                record("RX", object_packet()).replace("len=282", "len=283"),
                "LENGTH MISMATCH: declared=283 actual_hex=282",
            ),
        ]
        for decoder in DECODERS:
            for diff in (False, True):
                for line, diagnostic in samples:
                    with self.subTest(decoder=decoder, diff=diff, diagnostic=diagnostic):
                        result = self.cli(decoder, text=record("TX", object_packet()) + "\n" + line, diff=diff)
                        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                        self.assertEqual(result.stderr, "")
                        self.assertIn(diagnostic, result.stdout)
                        self.assertIn("TX packets: 1", result.stdout)
                        self.assertIn("RX packets: 1", result.stdout)
                        self.assertNotIn("No objects lost", result.stdout)
                        self.assertNotIn("LOST:", result.stdout)
                        if diff:
                            self.assertIn("Incomplete packet decode prevents comparison", result.stdout)


if __name__ == "__main__":
    if not WORKSPACE.is_relative_to((ROOT / "android/temp").resolve()):
        raise SystemExit("Fixture workspace must be inside android/temp")
    unittest.main(argv=[sys.argv[0]], verbosity=2)
