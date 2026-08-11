#!/usr/bin/env python3
"""Guard legacy Save Explorer loadability against unvalidated headers."""

from pathlib import Path


SOURCE = Path(__file__).parents[1] / "app/src/main/cpp/jni_resume_save.cpp"


def main() -> None:
    text = SOURCE.read_text(encoding="utf-8")
    required = (
        '!loadable && !meta_valid && have_preview && scope == "single"',
        'have_preview ? meta_error : "legacy_save_header_invalid"',
    )
    missing = [fragment for fragment in required if fragment not in text]
    if missing:
        raise SystemExit(f"Legacy save header loadability guards missing: {missing}")
    print("Legacy save header loadability guards passed")


if __name__ == "__main__":
    main()
