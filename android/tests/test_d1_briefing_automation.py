"""Compile production Android briefing recognition/routing against window stubs."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHARED = ROOT / "android/app/src/main/cpp/shared"


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class BriefingAutomationTest(unittest.TestCase):
    def test_production_window_recognition_and_dispatch(self) -> None:
        compiler = shutil.which("cl") or shutil.which("c++")
        self.assertIsNotNone(compiler, "Run in a C++ developer shell")
        automation = (SHARED / "game_automate.cpp").read_text()
        titles = (ROOT / "d1/main/titles.c").read_text()
        functions = "\n".join(
            [function(titles, "int android_d1_briefing_is_window(")]
            + [
                function(automation, signature)
                for signature in (
                    "static bool select_dispatch_front_menu_key(",
                    "static bool can_direct_dispatch_front_key_command(",
                    "static const char *describe_window_handler(",
                )
            ]
        )
        source = PREAMBLE + '#include "android_briefing_text.h"\n' + functions + MAIN
        with tempfile.TemporaryDirectory(prefix="d1-briefing-") as directory:
            folder = Path(directory)
            (folder / "test.cpp").write_text(source)
            for defines in ([], ["ANDROID"], ["ANDROID", "DXX_BUILD_DESCENT_II"]):
                with self.subTest(defines=defines):
                    if Path(compiler).stem.lower() == "cl":
                        command = [compiler, "/nologo", "/EHsc", "/W4", "/WX"]
                        command += [f"/D{value}" for value in defines]
                        command += [f"/I{SHARED}", "test.cpp", "/Fe:test.exe"]
                    else:
                        command = [compiler, "-Wall", "-Wextra", "-Werror"]
                        command += [f"-D{value}" for value in defines]
                        command += ["-I", str(SHARED), "test.cpp", "-o", "test.exe"]
                    subprocess.run(command, cwd=folder, check=True)
                    subprocess.run([str(folder / "test.exe")], cwd=folder, check=True)


PREAMBLE = r"""
#include <cassert>
#include <cstring>
#include <initializer_list>
void test_log(const char *, ...) {}
#define LOGI(...) test_log(__VA_ARGS__)
const int EVENT_KEY_COMMAND = 1, KEY_ESC = 27, KEY_ENTER = 13;
struct d_event { int type; int keycode; };
struct window;
using callback = int (*)(window *, d_event *, void *);
struct window { callback handler; void *data; };
using newmenu = void;
using listbox = void;
using kc_menu = void;
using automap = void;
struct automate_key_event { int type; int keycode; };
window *front_window = nullptr, *delivered_window = nullptr;
int direct_calls = 0, window_events = 0, delivered_type = 0, delivered_key = 0;
bool briefing_closed = false;
window *window_get_front() { return front_window; }
callback window_get_callback(window *wind) { return wind->handler; }
void *window_get_data(window *wind) { return wind->data; }
#define HANDLER(name) int name(window *, d_event *, void *) { ++direct_calls; return 0; }
HANDLER(newmenu_handler)
HANDLER(newmenu_key_command)
HANDLER(listbox_handler)
HANDLER(listbox_key_command)
HANDLER(kconfig_handler)
HANDLER(scores_handler)
HANDLER(credits_handler)
HANDLER(game_handler)
HANDLER(automap_handler)
HANDLER(automap_key_command)
HANDLER(title_handler)
HANDLER(MovieHandler)
HANDLER(unknown_handler)
HANDLER(imported_d1_handler)
int briefing_handler(window *, d_event *event, void *) {
    briefing_closed = event->type == EVENT_KEY_COMMAND && event->keycode == KEY_ESC;
    return 1;
}
bool d1_in_d2_briefing_is_handler(callback cb) { return cb == imported_d1_handler; }
void window_send_event(window *wind, d_event *event) {
    ++window_events;
    delivered_window = wind;
    delivered_type = event->type;
    delivered_key = event->keycode;
    wind->handler(wind, event, wind->data);
}
"""

MAIN = r"""
int main() {
    int data = 1;
    window unknown{unknown_handler, &data}, menu{newmenu_handler, &data};
    window briefing{briefing_handler, &data}, empty{nullptr, &data};
    assert(!android_d1_briefing_is_window(nullptr));
    assert(!android_d1_briefing_is_window(&menu));
    assert(!android_d1_briefing_is_window(&unknown));
    assert(android_d1_briefing_is_window(&briefing));
    assert(!can_direct_dispatch_front_key_command());
    assert(!select_dispatch_front_menu_key(KEY_ESC, "escape"));
    assert(!strcmp(describe_window_handler(nullptr), "none"));
    for (window *wind : {&unknown, &empty}) {
        front_window = wind;
        assert(!can_direct_dispatch_front_key_command());
        assert(!select_dispatch_front_menu_key(KEY_ESC, "escape"));
        assert(!strcmp(describe_window_handler(wind), "unknown"));
    }
    front_window = &menu;
    assert(can_direct_dispatch_front_key_command());
    assert(select_dispatch_front_menu_key(KEY_ENTER, "enter"));
    assert(direct_calls == 1 && window_events == 0);
    assert(!strcmp(describe_window_handler(&menu), "newmenu"));
    direct_calls = 0;
    front_window = &briefing;
    assert(!strcmp(describe_window_handler(&unknown), "unknown"));
#if defined(ANDROID) || defined(DXX_BUILD_DESCENT_II)
    briefing.data = nullptr;
    assert(!can_direct_dispatch_front_key_command());
    assert(!select_dispatch_front_menu_key(KEY_ESC, "escape"));
    assert(window_events == 0);
    briefing.data = &data;
    assert(can_direct_dispatch_front_key_command());
    assert(!strcmp(describe_window_handler(&briefing), "briefing"));
    assert(select_dispatch_front_menu_key(KEY_ESC, "escape"));
    assert(window_events == 1 && direct_calls == 0 && briefing_closed);
    assert(delivered_window == &briefing && delivered_type == EVENT_KEY_COMMAND && delivered_key == KEY_ESC);
#else
    assert(!can_direct_dispatch_front_key_command());
    assert(!select_dispatch_front_menu_key(KEY_ESC, "escape"));
    assert(!strcmp(describe_window_handler(&briefing), "unknown"));
#endif
#ifdef DXX_BUILD_DESCENT_II
    window imported{imported_d1_handler, &data};
    front_window = &imported;
    assert(can_direct_dispatch_front_key_command());
    assert(!strcmp(describe_window_handler(&imported), "briefing"));
    assert(select_dispatch_front_menu_key(KEY_ESC, "escape"));
    assert(window_events == 2 && delivered_window == &imported);
#endif
}
"""

if __name__ == "__main__":
    unittest.main()
