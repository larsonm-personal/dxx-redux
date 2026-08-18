import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "android/app/src/main/cpp/SDL_androidaudio.c").read_text(encoding="utf-8")
CMAKE = (REPO_ROOT / "android/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
MUSIC = (REPO_ROOT / "android/app/src/main/cpp/shared/digi_tsf_music.c").read_text(encoding="utf-8")
MUSIC_CONTROL = (REPO_ROOT / "android/app/src/main/cpp/shared/android_music_control.c").read_text(
    encoding="utf-8"
)
MAIN_ACTIVITY = (
    REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/MainActivity.kt"
).read_text(encoding="utf-8")
MUSIC_PANEL = (
    REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt"
).read_text(encoding="utf-8")
SETUP_ACTIVITY = (
    REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/SetupActivity.kt"
).read_text(encoding="utf-8")
SETUP_CONFIG = (
    REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/SetupConfigFiles.kt"
).read_text(encoding="utf-8")
MOD_MANAGER = (
    REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/ModManager.kt"
).read_text(encoding="utf-8")
SETUP_SECTIONS = (
    REPO_ROOT / "android/app/src/main/java/com/dxxredux/app/SetupSections.kt"
).read_text(encoding="utf-8")
STATE_SHARED = (
    REPO_ROOT / "android/app/src/main/cpp/shared/state_android_shared.c"
).read_text(encoding="utf-8")
SAVE_META = (REPO_ROOT / "android/app/src/main/cpp/shared/android_save_meta.h").read_text(
    encoding="utf-8"
)


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
            self.assertIn("tsf_atomic_store_int(&g_source_finished, 1);", region)
            self.assertNotIn("tsf_atomic_store_int(&g_playing, 0);", region)

        callback_start = MUSIC.index("static void tsf_music_callback(void *udata, Uint8 *stream, int len)")
        callback = MUSIC[callback_start : MUSIC.index("#else /* !ANDROID", callback_start)]
        self.assertLess(callback.index("rb_read(out, needed)"), callback.index("rb_available() == 0"))

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
            body = function_body(
                MUSIC_CONTROL, f"Java_com_dxxredux_app_MainActivity_{query}"
            )
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
        self.assertIn("ModManager(filesDir, context)", SETUP_SECTIONS)
        self.assertNotIn("PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE", MAIN_ACTIVITY)
        self.assertNotIn("PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE", MUSIC_PANEL)


if __name__ == "__main__":
    unittest.main()
