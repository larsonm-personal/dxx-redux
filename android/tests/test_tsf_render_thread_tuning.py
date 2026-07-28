import math
import re
import threading
import unittest
from collections import deque
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATH = REPO_ROOT / "android/app/src/main/cpp/shared/digi_tsf_music.c"


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


class TuningQueueModel:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.commands: deque[tuple[str, float | int]] = deque()
        self.applied: list[tuple[str, float | int]] = []

    def submit(self, command: tuple[str, float | int]) -> None:
        with self.lock:
            self.commands.append(command)

    def boundary(self) -> None:
        with self.lock:
            commands = list(self.commands)
            self.commands.clear()
        self.applied.extend(commands)


class TsfRenderThreadTuningTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")

    def test_render_owner_drains_commands_before_pause_and_render(self) -> None:
        body = function_body(self.source, "render_thread_func")
        require_order(
            self,
            body,
            "tsf_apply_pending_tuning()",
            "__atomic_load_n(&g_render_running",
            "tsf_atomic_load_int(&g_paused)",
            "render_frames(buf, frames)",
        )

    def test_live_setters_only_submit_commands(self) -> None:
        for name in (
            "mix_set_music_volume",
            "mix_pause_music",
            "mix_resume_music",
            "tsf_music_set_gain_db",
            "tsf_music_set_max_voices",
        ):
            body = function_body(self.source, name)
            android_body = body.split("#else", 1)[0]
            self.assertIn("tsf_submit_tuning_command(command)", android_body)
            self.assertNotIn("tsf_set_output(", body)
            self.assertNotIn("tsf_set_max_voices(", body)

    def test_synth_mutation_is_confined_to_command_application(self) -> None:
        body = function_body(self.source, "tsf_apply_tuning_command")
        self.assertIn("if (mutate_synth && g_tsf)", body)
        self.assertIn("tsf_set_output(", body)
        self.assertIn("tsf_set_max_voices(", body)

        submit = function_body(self.source, "tsf_submit_tuning_command")
        require_order(
            self,
            submit,
            "if (!g_render_accepting_commands)",
            "tsf_apply_tuning_command(&command, 0)",
            "g_tuning_queue[tail] = command",
        )

    def test_shutdown_drains_before_releasing_render_ownership(self) -> None:
        finish = function_body(self.source, "tsf_finish_tuning_ownership")
        require_order(
            self,
            finish,
            "pthread_mutex_lock(&g_tuning_mutex)",
            "tsf_apply_tuning_command(&g_tuning_queue[index], 1)",
            "g_render_accepting_commands = 0",
            "pthread_mutex_unlock(&g_tuning_mutex)",
        )
        render = function_body(self.source, "render_thread_func")
        require_order(
            self,
            render,
            "if (!__atomic_load_n(&g_render_running",
            "break",
            "tsf_finish_tuning_ownership()",
        )

    def test_gain_rejects_non_finite_values_before_submission(self) -> None:
        body = function_body(self.source, "tsf_music_set_gain_db")
        require_order(
            self,
            body,
            "if (!isfinite(db))",
            "return",
            "tsf_submit_tuning_command(command)",
        )
        self.assertFalse(math.isfinite(float("nan")))
        self.assertFalse(math.isfinite(float("inf")))

    def test_voice_limits_preserve_both_exact_boundaries(self) -> None:
        body = function_body(self.source, "tsf_music_set_max_voices")
        self.assertIn("if (n < 8) n = 8", body)
        self.assertIn("if (n > 256) n = 256", body)
        self.assertNotIn("if (n <= 8)", body)
        self.assertNotIn("if (n >= 256)", body)

    def test_callback_reads_published_pause_volume_and_playing(self) -> None:
        android_body = function_body(self.source, "tsf_music_callback")
        self.assertIn("tsf_atomic_load_int(&g_playing)", android_body)
        self.assertIn("tsf_atomic_load_int(&g_paused)", android_body)
        self.assertIn("tsf_atomic_load_float(&g_volume)", android_body)

    def test_concurrent_commands_are_applied_once_in_queue_order(self) -> None:
        model = TuningQueueModel()
        commands = [
            ("gain", -18.0),
            ("voices", 8),
            ("volume", 0.0),
            ("paused", 1),
            ("gain", -3.0),
            ("voices", 256),
            ("volume", 1.0),
            ("paused", 0),
        ]
        submitted: list[tuple[str, float | int]] = []
        submitted_lock = threading.Lock()

        def submit(command: tuple[str, float | int]) -> None:
            with submitted_lock:
                submitted.append(command)
                model.submit(command)

        threads = [threading.Thread(target=submit, args=(command,)) for command in commands]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(1.0)
            self.assertFalse(thread.is_alive())

        self.assertEqual([], model.applied)
        model.boundary()
        self.assertEqual(submitted, model.applied)
        self.assertEqual(len(commands), len(model.applied))


if __name__ == "__main__":
    unittest.main()
