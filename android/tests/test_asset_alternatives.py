"""Run production movie/sound selection code against controlled asset availability.

Run with python from a C++ developer shell (MSVC cl or c++ on PATH).
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from test_coop_trigger_messages import function

ROOT = Path(__file__).resolve().parents[2]


class AssetAlternativesTest(unittest.TestCase):
    def test_movie_and_sound_fallbacks(self):
        compiler = shutil.which("cl") or shutil.which("c++")
        self.assertIsNotNone(compiler, "Run in a C++ developer shell")
        movie = (ROOT / "d2/main/movie.c").read_text()
        piggy = (ROOT / "d2/main/piggy.c").read_text()
        titles = (ROOT / "d2/main/titles.c").read_text()
        functions = "\n".join([
            function(movie, "static int init_movie("),
            "static char extra_robot_movie_file[FILENAME_LEN+2];",
            function(movie, "void close_extra_robot_movie("),
            function(movie, "void init_extra_robot_movie("),
            function(piggy, "int read_sndfile("),
            function(titles, "static char *select_screen_resolution("),
        ])
        song_extensions = "\n".join(re.findall(r"^#define SONG_EXT_.*$", (ROOT / "d2/main/songs.h").read_text(), re.M))
        for game in ("d1", "d2"):
            songs = (ROOT / game / "main/songs.c").read_text()
            functions += "\n" + function(songs, "int songs_play_file(").replace("songs_play_file(", f"songs_play_file_{game}(")
        (ROOT / "temp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="asset-alternatives-", dir=ROOT / "temp") as directory:
            folder = Path(directory)
            (folder / "test.cpp").write_text(PREAMBLE + song_extensions + "\n" + functions + MAIN)
            for mixer in (False, True):
                if Path(compiler).stem.lower() == "cl":
                    command = [compiler, "/nologo", "/EHsc", "/W3", "/D_CRT_SECURE_NO_WARNINGS", "test.cpp", "/Fe:test.exe"]
                    if mixer:
                        command.append("/DUSE_SDLMIXER")
                else:
                    command = [compiler, "-Wall", "-Wextra", "test.cpp", "-o", "test.exe"]
                    if mixer:
                        command.append("-DUSE_SDLMIXER")
                subprocess.run(command, cwd=folder, check=True)
                subprocess.run([str(folder / "test.exe")], cwd=folder, check=True)


PREAMBLE = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <set>
#include <vector>
#define FILENAME_LEN 13
#define PATH_MAX 1024
#define CON_NORMAL 0
#define CON_URGENT 1
#define HIRESMODE GameArg.GfxMovieHires
#define SAMPLE_RATE_11K 11025
#define SAMPLE_RATE_22K 22050
#define SNDFILE_ID 0x444e5344
#define SNDFILE_VERSION 1
#define DEFAULT_SNDFILE ((Piggy_hamfile_version < 3) ? "d2demo.ham" : (GameArg.SndDigiSampleRate == SAMPLE_RATE_22K ? "descent2.s22" : "descent2.s11"))
struct { int GfxMovieHires, SysNoMovies, SndDigiSampleRate, SndNoSound, SndDisableSdlMixer; } GameArg;
std::set<std::string> available, mounted;
std::vector<std::string> attempts;
int PHYSFSX_contfile_init(const char *name, int) {
    attempts.emplace_back(name);
    if (!available.count(name)) return 0;
    mounted.insert(name); return 1;
}
int PHYSFSX_contfile_close(const char *name) { return static_cast<int>(mounted.erase(name)); }
const char *PHYSFS_getLastError() { return "unavailable"; }
int PHYSFSX_exists(const char *name, int) { return available.count(name) != 0; }
void con_printf(int, const char *, ...) {}
void songs_stop_all() {}
#ifdef _WIN32
#define d_stricmp _stricmp
#else
#include <strings.h>
#define d_stricmp strcasecmp
#endif
int digi_win32_play_midi_song(char *, int) { return 1; }
int mix_play_file(char *, int, void (*)()) { return 2; }
typedef unsigned char ubyte;
typedef FILE PHYSFS_file;
int Piggy_hamfile_version = 3, Num_sound_files, resets;
int SoundOffset[1];
void *SoundBits;
char LastSndfileDir[PATH_MAX];
struct DiskSoundHeader { char name[8]; int length, offset; };
struct digi_sound { int length; ubyte *data; };
PHYSFS_file *PHYSFSX_openReadBuffered(const char *name) { return fopen(name, "rb"); }
int PHYSFSX_readInt(PHYSFS_file *f) { int n; assert(fread(&n, sizeof(n), 1, f) == 1); return n; }
int PHYSFS_tell(PHYSFS_file *f) { return static_cast<int>(ftell(f)); }
void PHYSFS_close(PHYSFS_file *f) { fclose(f); }
void PHYSFSEXT_locateCorrectCase(char *) {}
const char *PHYSFS_getRealDir(const char *) { return "fixture"; }
void digi_close() { ++resets; }
void digi_init() {}
void DiskSoundHeader_read(DiskSoundHeader *h, PHYSFS_file *f) { assert(fread(h, sizeof(*h), 1, f) == 1); }
void piggy_register_sound(digi_sound *s, const char *name, int) {
    assert(s->length == 4 && !strcmp(name, "test")); ++Num_sound_files;
}
int piggy_is_needed(int) { return 1; }
void d_free(void *p) { free(p); }
void *d_malloc(int n) { return malloc(n); }
void Error(const char *) { abort(); }
void bank(const char *name, bool valid = true) {
    FILE *f = fopen(name, "wb"); assert(f);
    int header[] = {valid ? SNDFILE_ID : 0, SNDFILE_VERSION, 1};
    DiskSoundHeader sound = {{'t','e','s','t',0,0,0,0}, 4, 0};
    fwrite(header, sizeof(header), 1, f); fwrite(&sound, sizeof(sound), 1, f);
    fwrite("abcd", 4, 1, f); fclose(f);
}
'''

MAIN = r'''
int main() {
    char low[] = "end01.pcx", high[] = "end01b.pcx";
    for (int preferred : {0, 1}) {
        GameArg.GfxMovieHires = preferred;
        available = {low}; assert(!strcmp(select_screen_resolution(low, high), low));
        available = {high}; assert(!strcmp(select_screen_resolution(low, high), high));
        available = {low, high};
        assert(!strcmp(select_screen_resolution(low, high), preferred ? high : low));
        available.clear();
        assert(!strcmp(select_screen_resolution(low, high), preferred ? high : low));
    }
    // All library categories, both preferences, and both one-file-only cases
    for (const char *base : {"robots", "intro", "other", "oem", "custom"}) {
        char name[16]; snprintf(name, sizeof(name), "%s", base);
        for (int preferred : {0, 1}) {
            GameArg.GfxMovieHires = preferred;
            for (int present : {0, 1}) {
                std::string selected = std::string(base) + (present ? "-h.mvl" : "-l.mvl");
                available = {selected}; attempts.clear(); mounted.clear();
                assert(init_movie(name, 1) == present + 1);
                assert(mounted.count(selected));
                assert(attempts.size() == (preferred == present ? 1u : 2u));
            }
            available = {std::string(base) + "-h.mvl", std::string(base) + "-l.mvl"};
            assert(init_movie(name, 1) == preferred + 1);
            available.clear(); assert(init_movie(name, 1) == 0);
        }
    }
    GameArg.GfxMovieHires = 1; available = {"custom-l.mvl", "next-h.mvl"}; mounted.clear();
    char custom[] = "custom", next[] = "next";
    init_extra_robot_movie(custom); assert(mounted.count("custom-l.mvl"));
    init_extra_robot_movie(next); assert(!mounted.count("custom-l.mvl") && mounted.count("next-h.mvl"));
    GameArg.SysNoMovies = 1; init_extra_robot_movie(custom); assert(mounted.empty());

    for (int preferred : {SAMPLE_RATE_11K, SAMPLE_RATE_22K}) {
        for (int present : {SAMPLE_RATE_11K, SAMPLE_RATE_22K}) {
            for (int disabled : {0, 1}) {
                remove("descent2.s11"); remove("descent2.s22");
                bank(present == SAMPLE_RATE_11K ? "descent2.s11" : "descent2.s22");
                GameArg.SndDigiSampleRate = preferred; GameArg.SndNoSound = disabled;
                GameArg.SndDisableSdlMixer = 0; resets = 0;
                assert(read_sndfile()); assert(GameArg.SndDigiSampleRate == present);
                assert(!strcmp(LastSndfileDir, "fixture"));
                FILE *f = PHYSFSX_openReadBuffered(DEFAULT_SNDFILE); assert(f);
                fseek(f, SoundOffset[0], SEEK_SET); char data[4]; assert(fread(data, 4, 1, f) == 1);
                assert(!memcmp(data, "abcd", 4)); fclose(f);
#ifdef USE_SDLMIXER
                assert(resets == 0);
#else
                assert(resets == (!disabled && preferred != present ? 1 : 0));
#endif
            }
        }
        bank("descent2.s11"); bank("descent2.s22");
        GameArg.SndDigiSampleRate = preferred;
        assert(read_sndfile()); assert(GameArg.SndDigiSampleRate == preferred);
    }
    remove("descent2.s11"); remove("descent2.s22");
    GameArg.SndDigiSampleRate = SAMPLE_RATE_22K; assert(!read_sndfile());
    assert(GameArg.SndDigiSampleRate == SAMPLE_RATE_22K);
    bank("descent2.s11", false); assert(!read_sndfile());
    assert(GameArg.SndDigiSampleRate == SAMPLE_RATE_22K);
    remove("descent2.s11"); bank("descent2.s11");
    GameArg.SndNoSound = 0; GameArg.SndDisableSdlMixer = 1; resets = 0;
    assert(read_sndfile()); assert(resets == 1);
    free(SoundBits);
    for (const char *extension : {"hmp", "HMQ", "mid", "MIDI", "wav", "ogg", "flac", "mp3", "txt"}) {
        char filename[32]; snprintf(filename, sizeof(filename), "track.%s", extension);
        bool supported = !strcmp(extension, "hmp") || !strcmp(extension, "HMQ");
#ifdef USE_SDLMIXER
        supported = strcmp(extension, "txt") != 0;
#elif !defined(_WIN32)
        supported = false;
#endif
        assert((songs_play_file_d1(filename, 0, NULL) != 0) == supported);
        assert((songs_play_file_d2(filename, 0, NULL) != 0) == supported);
    }
    puts("PASS: movie resolution, mission unload, sound rate and payload fallbacks");
}
'''


if __name__ == "__main__":
    unittest.main()
