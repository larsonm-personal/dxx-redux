import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
SHARED = REPO / "android/app/src/main/cpp/shared"
HEADER = SHARED / "rbaudio_bin.h"
DECLARATIONS = (
    "int RBANextTrack(void);",
    "int RBAPrevTrack(void);",
    "int RBAPlaySpecificTrack(int track);",
    "int RBAGetCurrentTrackInfo(int *out_track, char *out_name, int name_size,\n"
    "                           int *out_source_index);",
    "int RBAGetNumAudioTracks(void);",
    "const char *RBAGetTrackName(int track);",
    "int RBAIsAudioTrack(int track);",
    "const char *RBAGetInitStatus(void);",
)


class RbaudioBinHeaderContractsTest(unittest.TestCase):
    @staticmethod
    def _compiler(language: str) -> str:
        executable = "clang" if language == "c" else "clang++"
        suffix = ".exe" if os.name == "nt" else ""
        ndk_roots = (
            os.environ.get("ANDROID_NDK_HOME"),
            os.environ.get("ANDROID_NDK_ROOT"),
            str(REPO.parent / "android-ndk-r27d"),
        )
        for root in filter(None, ndk_roots):
            candidate = (
                Path(root)
                / "toolchains/llvm/prebuilt"
                / ("windows-x86_64" if os.name == "nt" else "linux-x86_64")
                / "bin"
                / f"{executable}{suffix}"
            )
            if candidate.is_file():
                return str(candidate)
        compiler = shutil.which(executable)
        if compiler:
            return compiler
        raise RuntimeError(f"{executable} is required for the rbaudio header contract")

    def test_shared_header_is_the_only_declaration_owner(self) -> None:
        shared = HEADER.read_text(encoding="utf-8")
        self.assertIn("#ifndef RBAUDIO_BIN_H", shared)
        self.assertIn("#define RBAUDIO_BIN_H", shared)
        self.assertNotIn("#ifdef ANDROID", shared)
        self.assertFalse((SHARED / "rbaudio_android.h").exists())
        self.assertEqual(list(SHARED.glob("*rbaudio*.inc")), [])
        for declaration in DECLARATIONS:
            self.assertEqual(shared.count(declaration), 1)

        for game in ("d1", "d2"):
            inherited = (REPO / game / "include/rbaudio.h").read_text(encoding="utf-8")
            self.assertEqual(inherited.count('#include "rbaudio_bin.h"'), 1)
            for declaration in DECLARATIONS:
                self.assertNotIn(declaration, inherited)

    def test_c_and_cpp_signatures_compile_for_both_games(self) -> None:
        c_source = """
#include "rbaudio.h"
int (*next_track)(void) = RBANextTrack;
int (*previous_track)(void) = RBAPrevTrack;
int (*specific_track)(int) = RBAPlaySpecificTrack;
int (*track_info)(int *, char *, int, int *) = RBAGetCurrentTrackInfo;
int (*audio_track_count)(void) = RBAGetNumAudioTracks;
const char *(*track_name)(int) = RBAGetTrackName;
int (*is_audio_track)(int) = RBAIsAudioTrack;
const char *(*init_status)(void) = RBAGetInitStatus;
"""
        cpp_source = 'extern "C" {\n#include "rbaudio.h"\n}\n' + c_source.split(
            "\n", 2
        )[2]
        with tempfile.TemporaryDirectory(dir=REPO / "temp") as temp:
            temp_path = Path(temp)
            for game in ("d1", "d2"):
                for language, source in (("c", c_source), ("cpp", cpp_source)):
                    source_path = temp_path / f"{game}.{language}"
                    source_path.write_text(source, encoding="ascii")
                    subprocess.run(
                        [
                            self._compiler(language),
                            "-Werror",
                            "-c",
                            str(source_path),
                            "-o",
                            str(temp_path / f"{game}-{language}.obj"),
                            "-I",
                            str(REPO / game / "include"),
                            "-I",
                            str(SHARED),
                        ],
                        check=True,
                        capture_output=True,
                        text=True,
                    )

    def test_production_consumers_retain_rbaudio_owner(self) -> None:
        consumers = (
            "android/app/src/main/cpp/jni_music_control.c",
            "android/app/src/main/cpp/shared/game_automate.cpp",
            "android/app/src/main/cpp/shared/rbaudio_bin.c",
            "android/app/src/main/cpp/shared/songs_android_shared.c",
        )
        for relative in consumers:
            source = (REPO / relative).read_text(encoding="utf-8")
            self.assertIn('#include "rbaudio.h"', source, relative)


if __name__ == "__main__":
    unittest.main()
