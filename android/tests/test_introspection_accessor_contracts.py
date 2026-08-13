import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
SHARED = REPO / "android/app/src/main/cpp/shared"
SIGNATURES = (
    "const char *newmenu_get_title(newmenu *menu)",
    "const char *newmenu_get_subtitle(newmenu *menu)",
    "int newmenu_get_scroll_offset(newmenu *menu)",
    "int newmenu_get_is_scroll_box(newmenu *menu)",
    "int newmenu_get_android_wrapped_text(newmenu *menu)",
    "int newmenu_get_android_original_nitems(newmenu *menu)",
    "const char *listbox_get_title(listbox *lb)",
    "void *window_get_data(window *wind)",
    "int (*window_get_callback(window *wind))(window *, d_event *, void *)",
)


class IntrospectionAccessorContractsTest(unittest.TestCase):
    def test_inherited_files_keep_only_private_layout_adapters(self) -> None:
        self.assertEqual(list(SHARED.glob("game_*introspect*.inc")), [])
        for game in ("d1", "d2"):
            menu_source = (REPO / game / "main/newmenu.c").read_text(encoding="utf-8")
            window_source = (REPO / game / "arch/sdl/window.c").read_text(encoding="utf-8")
            for relative in ("main/newmenu.c", "arch/sdl/window.c"):
                source = (REPO / game / relative).read_text(encoding="utf-8")
                for signature in SIGNATURES:
                    self.assertNotIn(
                        signature,
                        source,
                        f"{game}/{relative}: duplicated {signature}",
                    )
            self.assertEqual(menu_source.count("void game_menu_introspect_read("), 1)
            self.assertEqual(menu_source.count("game_listbox_introspect_read_title("), 1)
            self.assertEqual(window_source.count("void game_window_introspect_read("), 1)
            self.assertIn("snapshot->title = menu->title;", menu_source)
            self.assertIn("snapshot->subtitle = menu->subtitle;", menu_source)
            self.assertIn("snapshot->scroll_offset = menu->scroll_offset;", menu_source)
            self.assertIn("snapshot->is_scroll_box = menu->is_scroll_box;", menu_source)
            self.assertIn("return lb->title;", menu_source)
            self.assertIn("snapshot->data = wind->data;", window_source)
            self.assertIn("snapshot->callback = wind->w_callback;", window_source)

            menu_header = (REPO / game / "main/newmenu.h").read_text(encoding="utf-8")
            window_header = (REPO / game / "arch/include/window.h").read_text(encoding="utf-8")
            self.assertEqual(menu_header.count('#include "game_menu_introspect_accessors.h"'), 1)
            self.assertEqual(window_header.count('#include "game_window_introspect_accessors.h"'), 1)

    def test_shared_owners_preserve_api_and_field_behavior(self) -> None:
        menu_header = (SHARED / "game_menu_introspect_accessors.h").read_text(encoding="utf-8")
        menu_body = (SHARED / "game_menu_introspect_accessors.c").read_text(encoding="utf-8")
        window_header = (SHARED / "game_window_introspect_accessors.h").read_text(
            encoding="utf-8"
        )
        window_body = (SHARED / "game_window_introspect_accessors.c").read_text(
            encoding="utf-8"
        )

        for text in (menu_header, menu_body, window_header, window_body):
            self.assertIn("#ifdef INTROSPECT_ON", text)
        for signature in SIGNATURES:
            self.assertIn(signature + ";", menu_header + window_header)
            self.assertIn(signature, menu_body + window_body)

        expected_returns = {
            "newmenu_get_title": "read_menu(menu).title",
            "newmenu_get_subtitle": "read_menu(menu).subtitle",
            "newmenu_get_scroll_offset": "read_menu(menu).scroll_offset",
            "newmenu_get_is_scroll_box": "read_menu(menu).is_scroll_box",
            "newmenu_get_android_wrapped_text": "read_menu(menu).android_wrapped_text",
            "newmenu_get_android_original_nitems": "read_menu(menu).android_original_nitems",
            "listbox_get_title": "game_listbox_introspect_read_title(lb)",
            "window_get_data": "read_window(wind).data",
            "window_get_callback": "read_window(wind).callback",
        }
        bodies = "\n".join((menu_body, window_body))
        for symbol, field in expected_returns.items():
            self.assertRegex(
                bodies,
                re.compile(rf"\b{symbol}\s*\([^)]*\).*?return {re.escape(field)};", re.S),
            )

        for forbidden in ("menu->", "lb->", "wind->"):
            self.assertNotIn(forbidden, bodies)
        for symbol in expected_returns:
            self.assertIn(symbol, menu_header + window_header)

        cmake = (REPO / "android/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertEqual(cmake.count("shared/game_menu_introspect_accessors.c"), 2)
        self.assertEqual(cmake.count("shared/game_window_introspect_accessors.c"), 2)


if __name__ == "__main__":
    unittest.main()
