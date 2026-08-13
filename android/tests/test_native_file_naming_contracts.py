import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


class NativeFileNamingContractsTest(unittest.TestCase):
    def test_one_to_one_implementations_match_header_stems(self) -> None:
        pairs = (
            (
                "android/app/src/main/cpp/shared/rbaudio_bin.c",
                "android/app/src/main/cpp/shared/rbaudio_bin.h",
            ),
            (
                "android/app/src/main/cpp/shared/android_music_control.c",
                "android/app/src/main/cpp/shared/android_music_control.h",
            ),
            (
                "android/app/src/main/cpp/shared/net/net_udp_reconnect_jni.c",
                "android/app/src/main/cpp/shared/net/net_udp_reconnect_jni.h",
            ),
            (
                "android/app/src/main/cpp/shared/secretarea.c",
                "d1/main/secretarea.h",
            ),
            (
                "android/app/src/main/cpp/shared/secretarea.c",
                "d2/main/secretarea.h",
            ),
            (
                "android/app/src/main/cpp/shared/pngfile.c",
                "d1/include/pngfile.h",
            ),
            (
                "android/app/src/main/cpp/shared/pngfile.c",
                "d2/include/pngfile.h",
            ),
        )
        for source_relative, header_relative in pairs:
            source = REPO / source_relative
            header = REPO / header_relative
            self.assertTrue(source.is_file(), source_relative)
            self.assertTrue(header.is_file(), header_relative)
            self.assertEqual(source.stem, header.stem, (source_relative, header_relative))
            self.assertIn(f'#include "{header.name}"', source.read_text(encoding="utf-8"))

    def test_superseded_mismatched_paths_do_not_return(self) -> None:
        old_paths = (
            "android/app/src/main/cpp/shared/rbaudio_android.h",
            "android/app/src/main/cpp/jni_music_control.c",
            "android/app/src/main/cpp/jni_udp_reconnect.c",
            "android/app/src/main/cpp/shared/secret_area_game_adapter.c",
            "android/app/src/main/cpp/shared/pngfile_stb.c",
        )
        for relative in old_paths:
            self.assertFalse((REPO / relative).exists(), relative)

    def test_build_files_use_current_source_paths(self) -> None:
        android_cmake = (REPO / "android/app/src/main/cpp/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        for relative in (
            "shared/android_music_control.c",
            "shared/net/net_udp_reconnect_jni.c",
            "shared/pngfile.c",
        ):
            self.assertIn(relative, android_cmake)

        for game in ("d1", "d2"):
            game_cmake = (REPO / game / "main/CMakeLists.txt").read_text(
                encoding="utf-8"
            )
            self.assertIn("shared/secretarea.c", game_cmake)


if __name__ == "__main__":
    unittest.main()
