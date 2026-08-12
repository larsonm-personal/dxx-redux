import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "android/app/src/main/cpp/SDL_androidaudio.c").read_text(encoding="utf-8")
CMAKE = (REPO_ROOT / "android/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
MUSIC = (REPO_ROOT / "android/app/src/main/cpp/shared/digi_tsf_music.c").read_text(encoding="utf-8")


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


if __name__ == "__main__":
    unittest.main()
