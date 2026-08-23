import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class MultiplayerSourceContractsTest(unittest.TestCase):
    def test_paired_advanced_menus_fit_android_items(self) -> None:
        for game, expected in (("d1", [50, 49]), ("d2", [56, 55])):
            source = (ROOT / game / "main/net_udp.c").read_text(encoding="utf-8")
            start = source.index("void net_udp_more_game_options")
            body = source[start : source.index("int net_udp_more_options_handler", start)]
            self.assertEqual(expected, [int(value) for value in re.findall(r"newmenu_item m\[(\d+)\]", body)][:2])
            android_start = body.index("#ifdef __ANDROID__")
            android = body[android_start : body.index("#endif", android_start)]
            self.assertEqual(2, android.count("Assert(opt < SDL_arraysize(m));"))
            self.assertIn("Assert(opt == SDL_arraysize(m));", android)

    def test_auto_host_validates_domains_before_network_publication(self) -> None:
        source = (ROOT / "android/app/src/main/cpp/shared/net/net_udp_android_autonet_shared.c").read_text(encoding="utf-8")
        self.assertLess(
            source.index("level_num < 1 || level_num > Last_level"),
            source.index("net_udp_init();", source.index("int net_udp_auto_host")),
        )
        publication = source.index("Netgame.gamemode = mode")
        for check in (
            "mode != NETGAME_ANARCHY && mode != NETGAME_COOPERATIVE",
            "difficulty < 0 || difficulty >= NDL",
            "max_players < 2 || max_players > MAX_PLAYERS",
        ):
            self.assertLess(source.index(check), publication)

    def test_notification_builder_supports_pre_channel_android(self) -> None:
        source = (
            ROOT / "android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerForegroundService.kt"
        ).read_text(encoding="utf-8")
        builder = source[source.index("val builder =") : source.index("val notification =")]
        self.assertIn("Build.VERSION.SDK_INT >= Build.VERSION_CODES.O", builder)
        self.assertLess(builder.index("Notification.Builder(this, CHANNEL_ID)"), builder.index("} else {"))
        self.assertGreater(builder.index("Notification.Builder(this)"), builder.index("} else {"))

    def test_reactor_pause_is_consumed_while_automap_is_front(self) -> None:
        actions = (
            ROOT / "android/app/src/main/cpp/shared/android_meta_actions.c"
        ).read_text(encoding="utf-8")
        handler = actions[
            actions.index("int android_handle_ingame_saveload_request") :
            actions.index("int android_matcen_mode_apply_pending")
        ]
        self.assertIn("android_reactor_pause_toggle_apply_pending()", handler)

        for game in ("d1", "d2"):
            source = (ROOT / game / "main/automap.c").read_text(encoding="utf-8")
            idle = source[source.index("case EVENT_IDLE:") : source.index("case EVENT_JOYSTICK_BUTTON_UP:")]
            self.assertIn("android_reactor_pause_toggle_apply_pending();", idle)


if __name__ == "__main__":
    unittest.main()
