"""Keep the compact D1 presentation map aligned with both symbolic text headers."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def text_symbols(game):
    header = (ROOT / game / "main/text.h").read_text(encoding="utf-8")
    return {
        name: (int(index), literal)
        for name, index, literal in re.findall(
            r"^#define\s+(TXT_\w+)\s+dxx_gettext\((\d+),\s*(.*)\)\s*$",
            header,
            re.MULTILINE,
        )
    }


class D1TextMappingTest(unittest.TestCase):
    def test_every_symbol_matches_its_source_game_ordinal(self):
        d1 = text_symbols("d1")
        d2 = text_symbols("d2")
        source = (ROOT / "d2/main/d1_in_d2/d1_in_d2_presentation.c").read_text(encoding="utf-8")
        ranges = [
            tuple(map(int, match))
            for match in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(-?\d+)\s*\}", source)
        ]
        self.assertGreater(len(d1), 500)
        self.assertGreater(len(d2), 600)
        for name, (index, _) in d2.items():
            if name == "TXT_HELP":  # D2's caller supplies a program-name argument
                continue
            mapped = [index + offset for first, last, offset in ranges if first <= index <= last]
            with self.subTest(symbol=name):
                self.assertEqual(mapped, [d1[name][0]] if name in d1 else [])


if __name__ == "__main__":
    unittest.main()
