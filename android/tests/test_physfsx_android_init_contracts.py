import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
SHARED = (REPO / "android/app/src/main/cpp/shared/physfsx_android_shared.c").read_text(
    encoding="utf-8"
)


class PhysfsxAndroidInitContracts(unittest.TestCase):
    def test_shared_owner_preserves_initialization_order_and_diagnostics(self):
        expected = [
            "PHYSFS_init(argv0)",
            'snprintf(error, error_size, "PhysicsFS initialization failed: %s"',
            "PHYSFS_permitSymbolicLinks(1)",
            "physfsx_android_setup_search_paths(game_dir, data_dir, &ops, &result)",
            'snprintf(error, error_size, "Android content setup failed during %s for %s: %s"',
            "InitArgsAndroid(argc, argv)",
        ]
        positions = [SHARED.index(fragment) for fragment in expected]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("result.operation, result.path, result.detail", SHARED)
        self.assertIn('Error("%s", error)', SHARED)
        self.assertRegex(
            SHARED,
            r'if \(!init_search_paths\(argv\[0\], game_dir, data_dir, error, error_size\)\)\s*return 0;',
        )

    def test_games_retain_only_parameterized_android_init_seams(self):
        for game, game_dir in (("d1", "d1x-redux"), ("d2", "d2x-redux")):
            source = (REPO / game / "misc/physfsx.c").read_text(encoding="utf-8")
            android_block = re.search(
                r'#ifdef ANDROID\n\tphysfsx_android_init\(argc, argv, "([^"]+)"\);'
                r"\n\treturn;\n#endif",
                source,
            )
            self.assertIsNotNone(android_block)
            self.assertEqual(android_block.group(1), game_dir)
            self.assertEqual(source.count("physfsx_android_init("), 1)
            self.assertEqual(source.count("PHYSFS_init(argv[0])"), 1)
            self.assertLess(android_block.end(), source.index("PHYSFS_init(argv[0])"))


if __name__ == "__main__":
    unittest.main()
