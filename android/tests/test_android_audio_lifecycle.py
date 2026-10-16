import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "android/app/src/main/cpp/SDL_androidaudio.c").read_text(encoding="utf-8")
CMAKE = (REPO_ROOT / "android/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
MUSIC = (REPO_ROOT / "android/app/src/main/cpp/shared/digi_tsf_music.c").read_text(encoding="utf-8")
MUSIC_CONTROL = (REPO_ROOT / "android/app/src/main/cpp/shared/android_music_control.c").read_text(encoding="utf-8")
MAIN_ACTIVITY = (REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/MainActivity.kt").read_text(encoding="utf-8")
MUSIC_PANEL = (REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt").read_text(
    encoding="utf-8"
)
SETUP_ACTIVITY = (REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/SetupActivity.kt").read_text(encoding="utf-8")
SETUP_CONFIG = (REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/SetupConfigFiles.kt").read_text(
    encoding="utf-8"
)
MOD_MANAGER = (REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/ModManager.kt").read_text(encoding="utf-8")
SETUP_SECTIONS = (REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/SetupSections.kt").read_text(encoding="utf-8")
STATE_SHARED = (REPO_ROOT / "android/app/src/main/cpp/shared/state_android_shared.c").read_text(encoding="utf-8")
SAVE_META = (REPO_ROOT / "android/app/src/main/cpp/shared/android_save_meta.h").read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.DOTALL)
    if not match:
        raise AssertionError(f"missing function {name}")
    start = match.end() - 1
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1 : index]
    raise AssertionError(f"unterminated function {name}")


def kotlin_function_body(source: str, name: str) -> str:
    match = re.search(rf"\bfun\s+{name}\s*\([^)]*\)[^={{]*\{{", source, re.DOTALL)
    if not match:
        raise AssertionError(f"missing Kotlin function {name}")
    start = match.end() - 1
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1 : index]
    raise AssertionError(f"unterminated Kotlin function {name}")


def require_order(test: unittest.TestCase, text: str, *needles: str) -> None:
    position = 0
    for needle in needles:
        found = text.find(needle, position)
        test.assertNotEqual(-1, found, f"missing ordered token: {needle}")
        position = found + len(needle)


class AndroidAudioLifecycleTest(unittest.TestCase):
    def test_open_checks_every_required_opensl_step(self) -> None:
        body = function_body(SOURCE, "ANDROIDAUD_OpenAudio")
        require_order(
            self,
            body,
            "slCreateEngine",
            "!h->engineObject",
            "->Realize",
            "SL_IID_ENGINE",
            "!h->engineEngine",
            "CreateOutputMix",
            "!h->outputMixObject",
            "->Realize",
            "CreateAudioPlayer",
            "!h->playerObject",
            "->Realize",
            "SL_IID_PLAY",
            "!h->playerPlay",
            "SL_IID_BUFFERQUEUE",
            "!h->playerBufferQueue",
            "RegisterCallback",
            "Initial buffer enqueue failed",
            "callback_enabled, 1",
            "SetPlayState",
            "g_player_play = h->playerPlay",
            "return 0",
            "fail:",
            "ANDROIDAUD_DestroyState(h)",
            "return -1",
        )

    def test_partial_open_and_close_share_reverse_cleanup(self) -> None:
        cleanup = function_body(SOURCE, "ANDROIDAUD_DestroyState")
        require_order(
            self,
            cleanup,
            "callback_enabled, 0",
            "SL_PLAYSTATE_STOPPED",
            "->Clear",
            "callback_inflight",
            "->Destroy(h->playerObject)",
            "->Destroy(h->outputMixObject)",
            "->Destroy(h->engineObject)",
            "SDL_FreeAudioMem(h->mixbuf)",
            "SDL_FreeAudioMem(h->playbuf[i])",
        )
        close = function_body(SOURCE, "ANDROIDAUD_CloseAudio")
        self.assertIn("ANDROIDAUD_DestroyState(h)", close)

    def test_callback_gate_and_enqueue_failure_are_terminal(self) -> None:
        callback = function_body(SOURCE, "bqPlayerCallback")
        require_order(
            self,
            callback,
            "callback_inflight, 1",
            "callback_enabled",
            "SDL_mutexP(audio->mixer_lock)",
            "->Enqueue",
            "output_failed, 1",
            "callback_enabled, 0",
            "audio->enabled = 0",
            "g_player_play = NULL",
            "callback_inflight, 1",
        )

    def test_generated_sdl_closes_backend_before_mixer_lock(self) -> None:
        require_order(
            self,
            CMAKE,
            "Android backend closes before mixer lock destruction",
            "audio->CloseAudio(audio)",
            "SDL_DestroyMutex(audio->mixer_lock)",
        )

    def test_music_source_eof_drains_the_pcm_ring(self) -> None:
        midi_end = MUSIC[MUSIC.index("/* End of MIDI") : MUSIC.index("/* ── PCM render")]
        pcm_start = MUSIC.index("size_t idx = (size_t) g_pcm_pos")
        pcm_end = MUSIC[pcm_start : MUSIC.index("/* ══", pcm_start)]
        for region in (midi_end, pcm_end):
            self.assertIn("*source_finished = 1;", region)
            self.assertNotIn("tsf_atomic_store_int(&g_source_finished, 1);", region)
            self.assertNotIn("tsf_atomic_store_int(&g_playing, 0);", region)

        producer = function_body(MUSIC, "render_thread_func")
        require_order(
            self,
            producer,
            "int source_finished = 0;",
            "pcm_ring_write(&g_rb, buf, got * 2);",
            "if (source_finished)",
            "tsf_atomic_store_int(&g_source_finished, 1);",
        )

        callback_start = MUSIC.index("static void tsf_music_callback(void *udata, Uint8 *stream, int len)")
        callback = MUSIC[callback_start : MUSIC.index("#else /* !ANDROID", callback_start)]
        self.assertLess(
            callback.index("pcm_ring_read(&g_rb, out, needed)"), callback.index("pcm_ring_available(&g_rb) == 0")
        )

    def test_music_completion_is_polled_on_paired_event_threads(self) -> None:
        self.assertIn("void mix_poll_music(void)", MUSIC)
        for game in ("d1", "d2"):
            event = (REPO_ROOT / game / "arch/sdl/event.c").read_text(encoding="utf-8")
            self.assertIn("mix_poll_music();", event)

    def test_music_overlay_uses_engine_published_snapshots(self) -> None:
        publish = function_body(MUSIC_CONTROL, "music_publish_snapshot")
        apply = function_body(MUSIC_CONTROL, "android_music_control_apply_pending")
        self.assertIn("music_publish_snapshot();", apply)
        for query in (
            "nativeGetTrackName",
            "nativeGetCurrentTrackNum",
            "nativeGetNumAudioTracks",
            "nativeGetTotalTracks",
            "nativeIsAudioTrack",
            "nativeGetCurrentTrackInfo",
            "nativeGetMusicType",
            "nativeGetMusicOverlayState",
            "nativeGetTrackList",
        ):
            body = function_body(MUSIC_CONTROL, f"Java_com_dxxredux_app_MainActivity_{query}")
            self.assertIn("pthread_mutex_lock(&g_music_snapshot_mutex)", body)
            self.assertNotIn("songs_get_", body)
            self.assertNotIn("RBAGet", body)
            self.assertNotIn("RBAIs", body)
        self.assertIn("songs_get_track_info", publish)
        self.assertIn("music_alloc_track_list", publish)
        self.assertIn("RBAGetTrackName", publish)

    def test_music_command_refresh_waits_for_published_snapshot(self) -> None:
        dequeue = function_body(MUSIC_CONTROL, "music_dequeue")
        apply = function_body(MUSIC_CONTROL, "android_music_control_apply_pending")
        pending = function_body(
            MUSIC_CONTROL,
            "Java_com_dxxredux_app_MainActivity_nativeIsMusicSourceChangePending",
        )
        self.assertIn("g_music_command_applying = 1;", dequeue)
        require_order(
            self,
            apply,
            "music_publish_snapshot();",
            "g_music_command_applying = 0;",
        )
        self.assertIn("g_music_command_count != 0 || g_music_command_applying", pending)
        self.assertIn("g_music_snapshot_current_track = track;", MUSIC_CONTROL)

        panel_play = kotlin_function_body(MUSIC_PANEL, "playTrack")
        panel_after_change = kotlin_function_body(MUSIC_PANEL, "afterNativeChange")
        self.assertIn("nativePlaySpecificTrack(track)", panel_play)
        self.assertIn("afterNativeChange", panel_play)
        self.assertIn("if (queued) onStateChanged()", panel_after_change)
        self.assertNotIn("refreshState()", panel_after_change)
        self.assertNotIn("sourceRefreshRunnable", MUSIC_PANEL)

        schedule = kotlin_function_body(MAIN_ACTIVITY, "scheduleMusicStateRefresh")
        self.assertIn("postDelayed(musicStateRefreshRunnable", schedule)
        self.assertRegex(
            MAIN_ACTIVITY,
            r"nativePrevTrack\(\) != 0\) scheduleMusicStateRefresh\(\)",
        )
        self.assertRegex(
            MAIN_ACTIVITY,
            r"nativeNextTrack\(\) != 0\) scheduleMusicStateRefresh\(\)",
        )

    def test_save_music_source_overrides_pilot_and_replays_matching_source(self) -> None:
        for game in ("d1", "d2"):
            state = (REPO_ROOT / game / "main/state.c").read_text(encoding="utf-8")
            self.assertIn("state_android_restore_music_source_from_meta", state)

        self.assertNotIn("resumeCandidate?.musicType", SETUP_ACTIVITY)
        self.assertIn("ANDROID_SAVE_META_MUSIC_MISSION 4", SAVE_META)
        self.assertIn("android_music_get_prefer_mission_soundtrack()", STATE_SHARED)
        restore = function_body(STATE_SHARED, "state_android_restore_music_source_from_meta")
        require_order(
            self,
            restore,
            "GameCfg.MusicType = music_type",
            "android_music_set_prefer_mission_soundtrack(prefer_mission)",
            "android_music_replay_current()",
        )
        self.assertIn('"mission",', SETUP_CONFIG)
        self.assertIn('source == "mission" && missionHasSoundtrack', SETUP_CONFIG)
        self.assertIn("selectBundledMusicForNewMission", MOD_MANAGER)
        self.assertIn("ModManager(filesDir, context, setDir)", SETUP_SECTIONS)
        self.assertNotIn("PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE", MAIN_ACTIVITY)
        self.assertNotIn("PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE", MUSIC_PANEL)



class GraphicsBackgroundRecoveryTest(unittest.TestCase):
    """Compile real lifecycle/graphics functions; mock only engine/platform boundaries."""

    def test_both_activity_background_paths_follow_graphics_cancellation(self) -> None:
        pause = kotlin_function_body(MAIN_ACTIVITY, "onPause")
        require_order(self, pause, "cancelForLifecycle()", "nativeGraphicsSafetyUiState(false, false)")
        for name in ("onStop", "onTrimMemory"):
            self.assertIn("applyBackgroundPause()", kotlin_function_body(MAIN_ACTIVITY, name))
        self.assertIn("nativeOnPause()", kotlin_function_body(MAIN_ACTIVITY, "applyBackgroundPause"))

    def test_compiled_background_recovery_before_quiesce_and_park(self) -> None:
        compiler = shutil.which("cl") or shutil.which("c++")
        self.assertIsNotNone(compiler, "Run in a C++ developer shell")
        shared = REPO_ROOT / "android/app/src/main/cpp/shared"
        source = (shared.parent / "android_input.c").read_text(encoding="utf-8")
        graphics = (shared / "android_graphics_safety.cpp").read_text(encoding="utf-8")
        lifecycle = (
            "void android_lifecycle_actions_game_tick(int screen_is_game, int has_game_window, "
            "int game_window_is_front, int multiplayer_active) {"
            + function_body(source, "android_lifecycle_actions_game_tick")
            + "}\n"
        )
        recovery = "\n".join(
            signature + " {" + function_body(graphics, name) + "}\n"
            for name, signature in (
                (
                    "android_graphics_safety_decide",
                    'extern "C" int android_graphics_safety_decide(uint64_t id, int accept, const char *reason)',
                ),
                (
                    "android_graphics_safety_ui_state",
                    'extern "C" void android_graphics_safety_ui_state(int foreground, int blocked)',
                ),
                ("android_graphics_safety_event_tick", 'extern "C" void android_graphics_safety_event_tick(void)'),
            )
        )
        # Remove just the new service call to reproduce the unsafe production ordering
        old_lifecycle = lifecycle.replace("\t\tandroid_graphics_safety_event_tick();\n", "")
        self.assertNotEqual(lifecycle, old_lifecycle)
        early_lifecycle = old_lifecycle.replace(
            "\t\tif (!android_lifecycle_diagnostics_take_pending",
            "\t\tandroid_graphics_safety_event_tick();\n\t\tif (!android_lifecycle_diagnostics_take_pending",
        )
        (REPO_ROOT / "temp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="graphics-background-", dir=REPO_ROOT / "temp") as directory:
            folder = Path(directory)
            for game in ("d1", "d2"):
                event = (REPO_ROOT / game / "arch/sdl/event.c").read_text(encoding="utf-8")
                # Preserve the engine's actual Android preamble and call ordering
                preamble = function_body(event, "event_process").split("\ttimer_update();", 1)[0]
                for mode, body in (
                    ("fixed", lifecycle),
                    ("unsafe", old_lifecycle),
                    ("before_request", early_lifecycle),
                    ("software", lifecycle),
                ):
                    with self.subTest(game=game, mode=mode):
                        code = (
                            GRAPHICS_BACKGROUND_FIXTURE
                            + recovery
                            + "\n#ifndef OGL\n"
                            "#define android_graphics_safety_event_tick software_build_must_not_call_graphics_recovery\n"
                            "#endif\n"
                            + body
                            + "\n#ifndef OGL\n#undef android_graphics_safety_event_tick\n#endif\n"
                            + "void event_preamble() {"
                            + preamble
                            + "(void)event; (void)wind;}\n"
                            + GRAPHICS_BACKGROUND_MAIN
                        )
                        (folder / "test.cpp").write_text(code, encoding="utf-8")
                        defines = ["ANDROID=1"] + (["OGL=1"] if mode != "software" else [])
                        if game == "d2":
                            defines.append("DXX_BUILD_DESCENT_II=1")
                        if Path(compiler).stem.lower() == "cl":
                            command = [
                                compiler, "/nologo", "/EHsc", "/std:c++17", "/W4", "/WX",
                                f"/I{shared}", *[f"/D{value}" for value in defines], "test.cpp", "/Fe:test.exe",
                            ]
                        else:
                            command = [
                                compiler, "-std=c++17", "-pthread", "-Wall", "-Wextra", "-Werror",
                                "-I", str(shared), *[f"-D{value}" for value in defines], "test.cpp", "-o", "test.exe",
                            ]
                        build = subprocess.run(command, cwd=folder, capture_output=True, text=True, timeout=60)
                        self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                        run = subprocess.run(
                            [str(folder / "test.exe")], cwd=folder, capture_output=True, text=True, timeout=10
                        )
                        if mode in ("unsafe", "before_request"):
                            self.assertNotEqual(run.returncode, 0, "Old ordering unexpectedly recovered before parking")
                            self.assertIn("pending graphics rollback at park", run.stderr)
                        else:
                            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                            self.assertIn("PASS", run.stdout)


GRAPHICS_BACKGROUND_FIXTURE = r"""
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#define access _access
#define getpid _getpid
#define F_OK 0
#else
#include <unistd.h>
#endif
#include "android_graphics_safety.h"
#include "graphics_safety_store.h"
#include "android_lifecycle_diagnostics.h"
#include "android_pause.h"
#define ANDROID_SAVE_META_SLOT_AUTO_MINIMIZE 9
#define ANDROID_SAVE_META_KIND_AUTO_MINIMIZE 1
#define ANDROID_SAVE_DESC_AUTO_MINIMIZE "[auto] save"
#include "android_idle_saver.h"

// Disk, GL and audio are boundaries; both state machines below are production code
std::mutex mutex;
std::string files_root, storage_failure, first_run_marker;
bool initialized = true, failure_notified = false, ui_foreground = true, ui_blocked = false;
bool restore_pending = false, accepted_pending = false, preparing = false, armed = false;
bool candidate_ready = false, first_run_pending = false, first_run_trial = false;
bool retry_first_run = false, current_known = false;
enum preview_phase { preview_none, preview_offering, preview_editing, preview_live, preview_settling };
preview_phase preview = preview_none;
bool queued[5] = {};
uint64_t staged_poll_at = 0, prepared_at = 0, deadline = 0, active_id = 1, serial = 0;
uint64_t preview_apply_deadline = 0, quiet_until = 0, candidate_revision = 0, applied_revision = 0;
uint64_t staged_generation = 0;
constexpr uint64_t overlay_prepare_timeout_ms = 5000;
graphics_safety_snapshot observed_current = {}, pending_candidate = {};
graphics_safety_record record = {};
std::thread::id game_thread = std::this_thread::get_id();
int restores = 0, parks = 0, checkpoints = 0, audio_pauses = 0;
bool recovery_before_checkpoint = true;
bool recovery_before_audio = true, wake_foreground = false, inject_background_at_take = false;
int pending_visibility = 0;
uint64_t g_android_resume_menu_session = 0;

void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
uint64_t now_ms() { return 1000; }
bool eligible() { return ui_foreground; }
void notify() {}
#define DLOG_GRAPHICS 0
void debug_log_force(int, const char *, ...) {}
int graphics_safety_read_record(const char *, graphics_safety_record *out) { *out = record; return 1; }
int graphics_safety_flush_staged(const char *) { return 1; }
int graphics_safety_decide(const char *, uint64_t id, int accept, uint64_t, const char *) {
    check(!accept && id == active_id, "unexpected graphics decision");
    if (record.phase == GRAPHICS_SAFE_IDLE) return 0;
    record.phase = GRAPHICS_SAFE_RESTORING;
    return 2;
}
bool apply_snapshot(const graphics_safety_snapshot &target, bool persist) {
    check(std::this_thread::get_id() == game_thread, "rollback left the game thread");
    // A separate observer avoids try_lock on a mutex already owned by this thread
    bool unlocked = false;
    std::thread observer([&] { unlocked = mutex.try_lock(); if (unlocked) mutex.unlock(); });
    observer.join();
    check(unlocked, "graphics mutex held during GL recovery");
    check(!persist && target.values[0] == 7, "accepted graphics snapshot was not restored");
    ++restores;
    return true;
}
int graphics_safety_complete_restore(const char *, uint64_t) {
    record.phase = GRAPHICS_SAFE_IDLE;
    return 1;
}
int graphics_safety_preview(const char *, const graphics_safety_snapshot *, uint64_t, int, int) {
    throw std::runtime_error("background rollback reopened the preview");
}

void request_background() {
    // MainActivity.onPause completes this synchronously before onStop/trim publishes background
    std::thread ui([] { android_graphics_safety_ui_state(0, 0); });
    ui.join();
    pending_visibility = ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND;
}
int android_lifecycle_diagnostics_take_pending(int *visibility, int *, uint64_t *generation) {
    if (inject_background_at_take) {
        inject_background_at_take = false;
        request_background();
    }
    if (!pending_visibility) return 0;
    *visibility = pending_visibility;
    *generation = 1;
    return 1;
}
void android_lifecycle_diagnostics_set_suspend_state(int) {}
void android_lifecycle_diagnostics_acknowledge(uint64_t, int) { pending_visibility = 0; }
void android_lifecycle_diagnostics_checkpoint_request(uint64_t, int) {
    recovery_before_checkpoint &= !restore_pending;
    ++checkpoints;
}
int android_lifecycle_diagnostics_checkpoint_writing() { return 1; }
void android_lifecycle_diagnostics_checkpoint_finish(int) {}
int android_lifecycle_diagnostics_park_until_wake(uint64_t) {
    ++parks;
    check(!restore_pending, "pending graphics rollback at park");
    check(record.phase == GRAPHICS_SAFE_IDLE, "graphics store still restoring at park");
    pending_visibility = wake_foreground ? ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND : 0;
    return wake_foreground ? ANDROID_LIFECYCLE_WAKE_FOREGROUND : ANDROID_LIFECYCLE_WAKE_EXTERNAL;
}
void androidaud_background_pause() {
    recovery_before_audio &= !restore_pending;
    ++audio_pauses;
}
void mix_background_pause() {}
void RBABackgroundPause() {}
int android_idle_saver_state() { return ANDROID_IDLE_COUNTING; }
int state_android_save_lifecycle_checkpoint(int, const char *, int) { return 1; }
void android_log_autosave_gate(const char *) {}
void android_pause_get_snapshot(android_pause_snapshot *out) { *out = {}; out->session = 1; }
int android_pause_queue_action(int) { return 1; }
void android_pause_publish() {}
void android_profile_resume() {}
void android_idle_saver_foreground_audio() {}
int android_reload_live_gamepad_config() { return 0; }
void kc_set_controls() {}
struct d_event {};
struct window {} game_window;
window *Game_wind = &game_window;
window *window_get_front() { return Game_wind; }
int Screen_mode = 1, Game_mode = 0;
#define SCREEN_MENU 0
#define SCREEN_GAME 1
#define GM_MULTI 1
#define ANDROID_OUTER_LIFECYCLE 0
void android_profile_outer_mark(int) {}
void android_pause_tick() {}
void android_lifecycle_diagnostics_game_tick(int, int, int, int, int) {}
void android_overlay_game_tick() {}
void android_idle_saver_tick(int, int) {}
"""

GRAPHICS_BACKGROUND_MAIN = r"""
void reset_trial() {
    record = {};
    record.accepted.values[0] = 7;
    record.phase = GRAPHICS_SAFE_CHALLENGE;
    restore_pending = false;
    armed = true;
    preparing = accepted_pending = false;
    preview = preview_none;
    ui_foreground = true;
    deadline = 6000;
    restores = parks = checkpoints = audio_pauses = 0;
    recovery_before_audio = recovery_before_checkpoint = true;
    pending_visibility = 0;
    wake_foreground = inject_background_at_take = false;
    g_android_resume_menu_session = 0;
}
int main() {
    try {
#ifdef OGL
        // No gameplay checkpoint in the first case, so the old ordering reaches the park gate
        for (int phase : {GRAPHICS_SAFE_CHALLENGE, GRAPHICS_SAFE_PREPARING, GRAPHICS_SAFE_PREVIEW})
        for (int gameplay : {0, 1}) for (int front : {0, 1}) for (bool wake : {false, true}) {
            reset_trial();
            record.phase = phase;
            armed = phase == GRAPHICS_SAFE_CHALLENGE;
            preparing = phase == GRAPHICS_SAFE_PREPARING;
            preview = phase == GRAPHICS_SAFE_PREVIEW ? preview_live : preview_none;
            Screen_mode = gameplay ? SCREEN_GAME : SCREEN_MENU;
            Game_wind = gameplay ? &game_window : nullptr;
            wake_foreground = wake;
            request_background();
            android_lifecycle_actions_game_tick(gameplay, gameplay, front, 0);
            check(restores == 1 && parks == 1 && checkpoints == 1 && recovery_before_audio && recovery_before_checkpoint,
                  "background ordering did not restore exactly once before audio/checkpoint/park");
            android_graphics_safety_event_tick();
            android_lifecycle_actions_game_tick(gameplay, gameplay, front, 0);
            check(restores == 1 && parks == 1, "repeat event tick repeated recovery or parking");
        }
        reset_trial();
        Screen_mode = SCREEN_GAME;
        Game_wind = &game_window;
        request_background();
        event_preamble();
        check(restores == 1 && parks == 1 && recovery_before_audio,
              "engine event preamble parked before restoration");

        // A background arrives after an earlier recovery tick but just as lifecycle consumes it
        reset_trial();
        android_graphics_safety_event_tick();
        inject_background_at_take = true;
        event_preamble();
        check(restores == 1 && parks == 1 && recovery_before_audio,
              "inter-call background request escaped restoration");

        reset_trial();
        request_background();
        android_lifecycle_actions_game_tick(1, 1, 1, 1);
        check(restores == 1 && parks == 0 && recovery_before_audio,
              "multiplayer background path missed restoration");

        reset_trial();
        pending_visibility = ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND;
        android_lifecycle_actions_game_tick(1, 1, 1, 0);
        check(restores == 0 && parks == 0 && audio_pauses == 0,
              "foreground request performed background work");
#else
        record.phase = GRAPHICS_SAFE_IDLE;
        pending_visibility = ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND;
        android_lifecycle_actions_game_tick(0, 0, 0, 0);
        check(parks == 1 && restores == 0, "software lifecycle path changed");
#endif
        std::puts("PASS: exact-source background recovery ordering");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
"""


if __name__ == "__main__":
    unittest.main()
