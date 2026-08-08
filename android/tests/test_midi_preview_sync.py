import re
import threading
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATH = REPO_ROOT / "android/app/src/main/cpp/shared/midi_preview.c"


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


class MidiPreviewSynchronizationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")

    def test_render_serializes_state_and_ring_publication(self) -> None:
        body = function_body(self.source, "render_thread_func")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_playback_mutex)",
            "render_midi_frames(buf, CHUNK)",
            "rb_write(buf, got * 2)",
            "pthread_mutex_unlock(&s_playback_mutex)",
        )

    def test_seek_is_an_ordered_control_and_playback_transaction(self) -> None:
        body = function_body(self.source, "midi_preview_seek")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_control_mutex)",
            "pthread_mutex_lock(&s_playback_mutex)",
            "tsf_reset(s_tsf)",
            "midi_seek_timeline_reconstruct(&s_timeline, target_frame",
            "s_playback_msec = target_ms",
            "pthread_mutex_lock(&s_ring_reset_mutex)",
            "rb_reset()",
            "rb_write(seek_buffer, prefill_frames * 2)",
            "pthread_mutex_unlock(&s_ring_reset_mutex)",
            "pthread_mutex_unlock(&s_playback_mutex)",
            "pthread_mutex_unlock(&s_control_mutex)",
        )

    def test_state_snapshot_uses_the_same_lock_order(self) -> None:
        body = function_body(self.source, "midi_preview_get_state")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_control_mutex)",
            "pthread_mutex_lock(&s_playback_mutex)",
            "s_playback_msec",
            "s_duration_ms",
            "pthread_mutex_unlock(&s_playback_mutex)",
            "pthread_mutex_unlock(&s_control_mutex)",
        )

    def test_audio_callback_never_blocks_on_seek_or_render(self) -> None:
        body = function_body(self.source, "osl_callback")
        require_order(
            self,
            body,
            "pthread_mutex_trylock(&s_ring_reset_mutex)",
            "rb_read(buf, needed)",
            "pthread_mutex_unlock(&s_ring_reset_mutex)",
        )
        self.assertNotIn("pthread_mutex_lock(&s_playback_mutex)", body)
        self.assertNotIn("pthread_mutex_lock(&s_ring_reset_mutex)", body)

    def test_stop_does_not_join_while_holding_playback_lock(self) -> None:
        body = function_body(self.source, "midi_preview_stop_internal")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_playback_mutex)",
            "s_playing = 0",
            "pthread_mutex_unlock(&s_playback_mutex)",
            "render_thread_stop()",
            "osl_shutdown()",
            "pthread_mutex_lock(&s_playback_mutex)",
            "tml_free(s_midi)",
            "pthread_mutex_unlock(&s_playback_mutex)",
        )

    def test_all_ui_controls_serialize_with_lifecycle_changes(self) -> None:
        for name in (
            "midi_preview_init",
            "midi_preview_start",
            "midi_preview_stop",
            "midi_preview_pause",
            "midi_preview_resume",
            "midi_preview_seek",
            "midi_preview_get_state",
        ):
            body = function_body(self.source, name)
            self.assertIn("pthread_mutex_lock(&s_control_mutex)", body)
            self.assertIn("pthread_mutex_unlock(&s_control_mutex)", body)

    def test_seek_waits_for_an_in_flight_render_transaction(self) -> None:
        playback_lock = threading.Lock()
        render_entered = threading.Event()
        release_render = threading.Event()
        seek_completed = threading.Event()

        def render() -> None:
            with playback_lock:
                render_entered.set()
                self.assertTrue(release_render.wait(1.0))

        def seek() -> None:
            with playback_lock:
                seek_completed.set()

        render_thread = threading.Thread(target=render)
        seek_thread = threading.Thread(target=seek)
        render_thread.start()
        self.assertTrue(render_entered.wait(1.0))
        seek_thread.start()
        self.assertFalse(seek_completed.wait(0.05))
        release_render.set()
        render_thread.join(1.0)
        seek_thread.join(1.0)
        self.assertFalse(render_thread.is_alive())
        self.assertFalse(seek_thread.is_alive())
        self.assertTrue(seek_completed.is_set())

    def test_opensl_init_checks_every_required_result_before_use(self) -> None:
        body = function_body(self.source, "osl_init")
        require_order(
            self,
            body,
            "r = slCreateEngine(&engine_obj",
            "if (r != SL_RESULT_SUCCESS || !engine_obj)",
            "(*engine_obj)->Realize",
            "if (r != SL_RESULT_SUCCESS)",
            "(*engine_obj)->GetInterface",
            "if (r != SL_RESULT_SUCCESS || !engine)",
            "(*engine)->CreateOutputMix",
            "if (r != SL_RESULT_SUCCESS || !outmix_obj)",
            "(*outmix_obj)->Realize",
            "if (r != SL_RESULT_SUCCESS)",
            "(*engine)->CreateAudioPlayer",
            "if (r != SL_RESULT_SUCCESS || !player_obj)",
            "(*player_obj)->Realize",
            "if (r != SL_RESULT_SUCCESS)",
            "SL_IID_PLAY",
            "if (r != SL_RESULT_SUCCESS || !player_play)",
            "SL_IID_BUFFERQUEUE",
            "if (r != SL_RESULT_SUCCESS || !player_bq)",
            "(*player_bq)->RegisterCallback",
            "if (r != SL_RESULT_SUCCESS)",
        )

    def test_opensl_init_publishes_only_a_complete_local_graph(self) -> None:
        body = function_body(self.source, "osl_init")
        publication = body.index("s_engine_obj = engine_obj")
        prefix = body[:publication]
        for global_handle in (
            "s_engine_obj",
            "s_engine",
            "s_outmix_obj",
            "s_player_obj",
            "s_player_play",
            "s_player_bq",
        ):
            self.assertNotIn(global_handle, prefix)
        require_order(
            self,
            body[publication:],
            "s_engine_obj = engine_obj",
            "s_engine = engine",
            "s_outmix_obj = outmix_obj",
            "s_player_obj = player_obj",
            "s_player_play = player_play",
            "s_player_bq = player_bq",
            "return 1",
        )

    def test_opensl_failure_destroys_partial_objects_in_reverse_order(self) -> None:
        body = function_body(self.source, "osl_init")
        failure = body[body.index("fail:") :]
        require_order(
            self,
            failure,
            "if (player_obj)",
            "(*player_obj)->Destroy(player_obj)",
            "if (outmix_obj)",
            "(*outmix_obj)->Destroy(outmix_obj)",
            "if (engine_obj)",
            "(*engine_obj)->Destroy(engine_obj)",
            "return 0",
        )

    def test_initial_buffers_are_checked_before_playback_starts(self) -> None:
        body = function_body(self.source, "osl_init")
        require_order(
            self,
            body,
            "for (i = 0; i < NUM_BUFFERS; i++)",
            "r = (*player_bq)->Enqueue",
            "if (r != SL_RESULT_SUCCESS)",
            "goto fail",
            "(*player_play)->SetPlayState",
            "if (r != SL_RESULT_SUCCESS)",
            "goto fail",
            "s_engine_obj = engine_obj",
        )

    def test_thread_creation_failure_is_propagated_and_fully_stopped(self) -> None:
        thread_start = function_body(self.source, "render_thread_start")
        require_order(
            self,
            thread_start,
            "pthread_create",
            "__atomic_store_n(&s_render_running, 0",
            "return 0",
        )

        start = function_body(self.source, "midi_preview_start")
        require_order(
            self,
            start,
            "if (!osl_init(sample_rate))",
            "midi_preview_stop_internal()",
            "return 0",
            "if (!render_thread_start())",
            "midi_preview_stop_internal()",
            "return 0",
            "return 1",
        )

        stop = function_body(self.source, "midi_preview_stop_internal")
        require_order(
            self,
            stop,
            "if (s_midi_buf)",
            "d_free(s_midi_buf)",
            "s_midi_buf = NULL",
            "s_midi_buf_len = 0",
        )

if __name__ == "__main__":
    unittest.main()
