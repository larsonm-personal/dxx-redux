import re
import threading
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATH = REPO_ROOT / "android/app/src/main/cpp/shared/cd_preview.c"


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


class CdPreviewSynchronizationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")

    def test_render_serializes_decoder_and_ring_publication(self) -> None:
        body = function_body(self.source, "render_thread_func")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_playback_mutex)",
            "render_cd_frames(buf, CHUNK)",
            "rb_write(buf, got * 2)",
            "pthread_mutex_unlock(&s_playback_mutex)",
        )

    def test_seek_replaces_decoder_ring_and_opensl_queue_atomically(self) -> None:
        body = function_body(self.source, "cd_preview_seek")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_control_mutex)",
            "pthread_mutex_lock(&s_playback_mutex)",
            "__atomic_store_n(&s_output_enabled, 0",
            "pthread_mutex_lock(&s_ring_reset_mutex)",
            "s_read_sector = target",
            "rb_reset()",
            "osl_reprime_queue_locked()",
            "__atomic_store_n(&s_output_enabled, s_paused ? 0 : 1",
            "pthread_mutex_unlock(&s_ring_reset_mutex)",
            "pthread_mutex_unlock(&s_playback_mutex)",
            "pthread_mutex_unlock(&s_control_mutex)",
        )

    def test_callback_serializes_read_and_enqueue_without_blocking(self) -> None:
        body = function_body(self.source, "osl_callback")
        require_order(
            self,
            body,
            "pthread_mutex_trylock(&s_ring_reset_mutex)",
            "rb_read(buf, needed)",
            "(*bq)->Enqueue",
            "pthread_mutex_unlock(&s_ring_reset_mutex)",
        )
        self.assertNotIn("pthread_mutex_lock(&s_playback_mutex)", body)
        self.assertNotIn("pthread_mutex_lock(&s_ring_reset_mutex)", body)

    def test_stop_does_not_join_while_holding_playback_lock(self) -> None:
        body = function_body(self.source, "cd_preview_stop_internal")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_playback_mutex)",
            "s_playing = 0",
            "pthread_mutex_unlock(&s_playback_mutex)",
            "render_thread_stop()",
            "osl_shutdown()",
            "pthread_mutex_lock(&s_playback_mutex)",
            "close_bin_files()",
            "pthread_mutex_unlock(&s_playback_mutex)",
        )

    def test_state_snapshot_uses_control_and_playback_lock_order(self) -> None:
        body = function_body(self.source, "cd_preview_get_state")
        require_order(
            self,
            body,
            "pthread_mutex_lock(&s_control_mutex)",
            "pthread_mutex_lock(&s_playback_mutex)",
            "s_num_sectors",
            "s_output_frames",
            "s_playing",
            "s_paused",
            "pthread_mutex_unlock(&s_playback_mutex)",
            "pthread_mutex_unlock(&s_control_mutex)",
        )

    def test_ui_controls_serialize_with_lifecycle_changes(self) -> None:
        for name in (
            "cd_preview_start_multi",
            "cd_preview_start_multi_fd",
            "cd_preview_stop",
            "cd_preview_pause",
            "cd_preview_resume",
            "cd_preview_seek",
            "cd_preview_get_state",
        ):
            body = function_body(self.source, name)
            self.assertIn("pthread_mutex_lock(&s_control_mutex)", body)
            self.assertIn("pthread_mutex_unlock(&s_control_mutex)", body)

    def test_completed_seek_clears_a_callback_that_was_already_in_flight(self) -> None:
        queue_lock = threading.Lock()
        callback_entered = threading.Event()
        release_callback = threading.Event()
        seek_completed = threading.Event()
        queue = ["old-buffer"]

        def callback() -> None:
            with queue_lock:
                callback_entered.set()
                self.assertTrue(release_callback.wait(1.0))
                queue.append("old-callback-buffer")

        def seek() -> None:
            with queue_lock:
                queue.clear()
                queue.extend(("new-silence-0", "new-silence-1"))
                seek_completed.set()

        callback_thread = threading.Thread(target=callback)
        seek_thread = threading.Thread(target=seek)
        callback_thread.start()
        self.assertTrue(callback_entered.wait(1.0))
        seek_thread.start()
        self.assertFalse(seek_completed.wait(0.05))
        release_callback.set()
        callback_thread.join(1.0)
        seek_thread.join(1.0)
        self.assertFalse(callback_thread.is_alive())
        self.assertFalse(seek_thread.is_alive())
        self.assertEqual(["new-silence-0", "new-silence-1"], queue)

    def test_opensl_graph_is_checked_before_global_publication(self) -> None:
        body = function_body(self.source, "osl_init")
        require_order(
            self,
            body,
            "r = slCreateEngine(&engine_obj",
            "if (r != SL_RESULT_SUCCESS || !engine_obj)",
            "(*engine_obj)->Realize",
            "(*engine_obj)->GetInterface",
            "if (r != SL_RESULT_SUCCESS || !engine)",
            "(*engine)->CreateOutputMix",
            "if (r != SL_RESULT_SUCCESS || !outmix_obj)",
            "(*outmix_obj)->Realize",
            "(*engine)->CreateAudioPlayer",
            "if (r != SL_RESULT_SUCCESS || !player_obj)",
            "(*player_obj)->Realize",
            "SL_IID_PLAY",
            "if (r != SL_RESULT_SUCCESS || !player_play)",
            "SL_IID_BUFFERQUEUE",
            "if (r != SL_RESULT_SUCCESS || !player_bq)",
            "(*player_bq)->RegisterCallback",
            "r = (*player_bq)->Enqueue",
            "(*player_play)->SetPlayState",
            "s_engine_obj = engine_obj",
        )

    def test_opensl_failure_unwinds_and_thread_failure_rejects_start(self) -> None:
        init = function_body(self.source, "osl_init")
        failure = init[init.index("fail:") :]
        require_order(
            self,
            failure,
            "(*player_obj)->Destroy(player_obj)",
            "(*outmix_obj)->Destroy(outmix_obj)",
            "(*engine_obj)->Destroy(engine_obj)",
            "return 0",
        )
        thread_start = function_body(self.source, "render_thread_start")
        require_order(
            self,
            thread_start,
            "pthread_create",
            "__atomic_store_n(&s_render_running, 0",
            "return 0",
        )
        start = function_body(self.source, "cd_preview_start_common")
        require_order(
            self,
            start,
            "if (!osl_init(sample_rate))",
            "return 0",
            "if (!render_thread_start())",
            "cd_preview_stop_internal()",
            "return 0",
            "return 1",
        )

    def test_runtime_enqueue_failure_reaches_terminal_state(self) -> None:
        callback = function_body(self.source, "osl_callback")
        require_order(
            self,
            callback,
            "r = (*bq)->Enqueue",
            "if (r != SL_RESULT_SUCCESS)",
            "__atomic_store_n(&s_output_enabled, 0",
            "__atomic_store_n(&s_output_failed, 1",
        )
        render = function_body(self.source, "render_thread_func")
        require_order(
            self,
            render,
            "__atomic_load_n(&s_output_failed",
            "s_playing = 0",
            "stop = 1",
        )


if __name__ == "__main__":
    unittest.main()
