"""Epoch source contracts and exact Android save-callsign reader execution.

These inspect production wiring. They do not claim runtime coverage of Android
rewind, input-demo replay, or the D2 secret-return transition.
"""
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SHARED = ROOT / "android/app/src/main/cpp/shared"


class SaveEpochContractTest(unittest.TestCase):
    def test_exact_android_callsign_readers_accept_legacy_and_epoch_headers(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "cc is required for the exact-source callsign reader fixture")
        temp_root = ROOT / "android/temp"
        temp_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="save-callsign-", dir=temp_root) as temporary:
            work = Path(temporary)
            for game in ("d1", "d2"):
                with self.subTest(game=game):
                    source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
                    player = (ROOT / game / "main/player.h").read_text(encoding="utf-8")
                    weapon = (ROOT / game / "main/weapon.h").read_text(encoding="utf-8")
                    names = ("STATE_COMPATIBLE_VERSION", "THUMBNAIL_W", "THUMBNAIL_H",
                             "THUMBNAIL_PALETTE_BYTES", "THUMBNAIL_RGB_BYTES",
                             "STATE_THUMBNAIL_PALETTE_VERSION", "STATE_THUMBNAIL_RGB_VERSION",
                             "DESC_LENGTH", "CALLSIGN_LEN", "MAX_PRIMARY_WEAPONS", "MAX_SECONDARY_WEAPONS")
                    definitions = "\n".join(re.search(r"^#define\s+" + name + r"\b[^\n]*",
                                                      source + player + weapon, re.M).group(0) for name in names)
                    start = player.index("typedef struct player_rw {")
                    record = player[start:player.index("} __pack__ player_rw;", start) + len("} __pack__ player_rw;")]
                    start = source.index("static int state_thumbnail_has_palette(")
                    thumbnail = source[start:source.index("static grs_bitmap *state_read_thumbnail", start)]
                    start = source.index("int state_get_save_file_callsign(")
                    reader = source[start:source.index("\n}\n", start) + 3]
                    prefix = definitions + "\n" + record + '\n#include "save_epoch_runtime.h"\n' + thumbnail
                    prefix += '\nstatic char dgss_id[4] = "DGSS";\n'
                    header = work / "save_callsign_reader_under_test.h"
                    binary = work / game

                    def compile_reader(body):
                        header.write_text(prefix + body, encoding="utf-8")
                        result = subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                                        "-I", str(work), "-I", str(SHARED),
                                        str(ROOT / "android/tests/save_callsign_reader_fixture.c"),
                                        "-o", str(binary)], capture_output=True, text=True,
                                       env={**os.environ, "TMPDIR": str(work)}, timeout=20)
                        self.assertEqual(result.returncode, 0, result.stderr)

                    compile_reader(reader)
                    size = int(subprocess.check_output([str(binary), "--player-size"], text=True))
                    compatible = int(re.search(r"#define\s+STATE_COMPATIBLE_VERSION\s+(\d+)", source).group(1))
                    version = int(re.search(r"#define SAVE_EPOCH_" + game.upper() + r"_VERSION (\d+)",
                                            (SHARED / "save_epoch_format.h").read_text()).group(1))
                    rgb_version = int(re.search(r"#define\s+STATE_THUMBNAIL_RGB_VERSION\s+(\d+)", source).group(1))
                    palette_version = int(re.search(r"#define\s+STATE_THUMBNAIL_PALETTE_VERSION\s+(\d+)", source).group(1))
                    maximum = ((1 << 63) - 1) // 128

                    def fixture(saved_version, epoch, endian="<", pathname=False):
                        thumbnail_size = 15000 if saved_version >= rgb_version else 5000 + (768 if saved_version >= palette_version else 0)
                        mission = b"\0" * 8 + b"\1" + b"mission\0".ljust(128, b"\0") if pathname else b"\0" * 9
                        data = b"DGSS" + struct.pack(endian + "I", saved_version) + b"\0" * (20 + thumbnail_size)
                        data += struct.pack(endian + "I", 0) + mission + struct.pack(endian + "iii", 1, 2, 0)
                        offset = len(data)
                        if saved_version >= version:
                            data += struct.pack(endian + "II", epoch & 0xffffffff, epoch >> 32)
                        return data + b"player\0".ljust(size, b"\0"), offset

                    def check(data, expected="player", success=True):
                        path = work / "fixture.sav"
                        path.write_bytes(data)
                        result = subprocess.run([str(binary), str(path), expected], capture_output=True, timeout=5)
                        if success:
                            self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
                        else:
                            self.assertEqual(result.returncode, 5, "pre-fix reader must reproduce the callsign mismatch")

                    for endian in ("<", ">"):
                        for pathname in (False, True):
                            for legacy in (compatible, version - 1):
                                check(fixture(legacy, 0, endian, pathname)[0])
                            for epoch in (0, 0xb5, (1 << 40) + 0x47, maximum):
                                check(fixture(version, epoch, endian, pathname)[0])
                        valid, offset = fixture(version, 0xb5, endian)
                        for length in range(8):
                            check(valid[:offset + length], "--reject")
                        for invalid in (maximum + 1, (1 << 64) - 1):
                            check(fixture(version, invalid, endian)[0], "--reject")
                        check(valid[:-1], "--reject")

                    # Removing only the epoch advance recreates the pre-fix offset bug
                    old_reader = re.sub(r"\t/\* New save headers.*?\n\t}\n", "", reader, count=1, flags=re.S)
                    self.assertNotEqual(old_reader, reader)
                    compile_reader(old_reader.replace("\tfix64 saved_epoch;\n", ""))
                    check(fixture(version - 1, 0)[0])
                    check(fixture(version, 0xb5)[0], success=False)

    def test_new_versions_are_explicit(self):
        source = (SHARED / "save_epoch_format.h").read_text(encoding="utf-8")
        self.assertRegex(source, r"#define\s+SAVE_EPOCH_D1_VERSION\s+23\b")
        self.assertRegex(source, r"#define\s+SAVE_EPOCH_D2_VERSION\s+47\b")
        self.assertRegex(source, r"#define\s+SAVE_EPOCH_DISK_BYTES\s+8\b")
        self.assertIn("INT64_MAX / 128", source)
        self.assertIn("epoch >= 0 && epoch <= SAVE_EPOCH_MAX", source)

    def test_reader_checks_both_words_before_publishing(self):
        source = (SHARED / "save_epoch_runtime.h").read_text(encoding="utf-8")
        reader = source[source.index("static inline int save_epoch_read"):]
        self.assertIn("PHYSFS_read(fp, words, sizeof(words), 1) != 1", reader)
        self.assertIn("words[0] = SWAPINT(words[0])", reader)
        self.assertIn("words[1] = SWAPINT(words[1])", reader)
        self.assertLess(reader.index("value > SAVE_EPOCH_MAX"), reader.index("*epoch ="))
        self.assertNotIn("GameTime64 =", reader)

    def test_android_save_callsign_reader_consumes_epoch_before_player(self):
        for game in ("d1", "d2"):
            with self.subTest(game=game):
                source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
                reader = source[source.index("int state_get_save_file_callsign("):]
                guard = "version >= SAVE_EPOCH_" + game.upper() + "_VERSION"
                self.assertRegex(reader, guard + r" && !save_epoch_read\(fp, swap, &saved_epoch\)\) \{\s*"
                                               r"PHYSFS_close\(fp\);\s*return 0;\s*\}")
                self.assertLess(reader.index("PHYSFSX_readSXE32(fp, swap)"), reader.index(guard))
                self.assertLess(reader.index(guard), reader.index("PHYSFS_read(fp, &pl_rw"))
                self.assertLess(reader.index("PHYSFS_read(fp, &pl_rw"), reader.index("strncpy(callsign"))
                self.assertNotIn("GameTime64 =", reader)

    def test_rewind_replay_then_saved_epoch_precedence(self):
        for game in ("d1", "d2"):
            with self.subTest(game=game):
                source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
                start = source.index("//Restore GameTime")
                end = source.index("// Start new game", start)
                selection = source[start:end]
                self.assertLess(selection.index("!save_epoch_read"), selection.index("GameTime64 ="))
                self.assertLess(selection.index("android_rewind_restore_game_time64_active()"),
                                selection.index("input_demo_replay_has_checkpoint()"))
                self.assertRegex(selection, r"android_rewind_restore_game_time64\(\);\s*else\s*\{")
                self.assertRegex(selection, r"GameTime64 = \(fix64\)tmptime32;\s*"
                                           r"if \(input_demo_replay_has_checkpoint\(\)\)\s*"
                                           r"GameTime64 \+= input_demo_replay_checkpoint_start_gt\(\);\s*else if")
                self.assertIn("GameTime64 = saved_epoch", selection)
                expected_guard = "version >= SAVE_EPOCH_D2_VERSION && !secret_restore" if game == "d2" else "version >= SAVE_EPOCH_D1_VERSION"
                self.assertIn("else if (" + expected_guard + ")", selection)
                # The header must reject before the restore marks the world replaced
                self.assertLess(selection.index("!save_epoch_read"), selection.index("*world_changed = 1"))

    def test_native_and_translated_boss_active_zero_collision_is_preserved(self):
        for game in ("d1", "d2"):
            with self.subTest(game=game):
                source = (ROOT / game / "main/ai.c").read_text(encoding="utf-8")
                self.assertRegex(source, r"Boss_dying_start_time = tmptime32 \? GameTime64 \+ \(fix64\)tmptime32 : 0;\s*"
                                         r"if \(tmptime32 && !Boss_dying_start_time\)\s*Boss_dying_start_time = -1;")
        translated = (ROOT / "d2/main/d1_in_d2/d1_save_translate.c").read_text(encoding="utf-8")
        self.assertRegex(translated, r"state->boss_dying_start_time = time_delta \? GameTime64 \+ \(fix64\) time_delta : 0;\s*"
                                    r"if \(time_delta && !state->boss_dying_start_time\)\s*state->boss_dying_start_time = -1;")

    def test_writers_keep_zero_legacy_clock_before_authoritative_epoch(self):
        for game in ("d1", "d2"):
            with self.subTest(game=game):
                source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
                start = source.rfind("//Save GameTime", 0, source.index("!save_epoch_write(fp, GameTime64)"))
                end = source.index("//Save player info", start)
                header = source[start:end]
                self.assertRegex(header, r"tmptime32 = 0;\s*"
                                         r"PHYSFS_write\(fp, &tmptime32, sizeof\(fix\), 1\);\s*"
                                         r"if \(!save_epoch_write\(fp, GameTime64\)\)")


if __name__ == "__main__":
    unittest.main()
