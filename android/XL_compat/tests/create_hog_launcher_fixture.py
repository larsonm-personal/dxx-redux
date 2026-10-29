"""Derive an import/music fixture without changing any original mission member."""

import argparse
import hashlib
import io
import json
import pathlib
import struct
import wave
import zipfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("hog", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    original = args.hog.read_bytes()
    if sha(original) != "1b8b75b829a865191b3dd6686d3eababfd9de692464a1a5dcc5b1410e13bc6c0":
        raise ValueError("Requires pinned derived xhogs.hog from xl_extended_hog")
    offset, members = 3, 0
    while offset < len(original):
        size = struct.unpack_from("<i", original, offset + 13)[0]
        offset += 17 + (256 if size < 0 else 0) + abs(size)
        members += 1
    if offset != len(original):
        raise ValueError("Original member extent differs")
    events = bytes([0, 0x90, 60, 64, 0xA5, 0x40, 0x80, 60, 0, 0, 0xFF, 0x2F, 0])
    midi = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(events)) + events
    audio_out = io.BytesIO()
    with wave.open(audio_out, "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(22050)
        audio.writeframes(b"\0\0" * 22050 * 2)
    audio = audio_out.getvalue()
    midi_name, audio_name = "Long mission preview.mid", "Long mission preview.wav"
    hog = bytearray(original)
    for name, alias, payload in ((midi_name, "alias.mid", midi), (audio_name, "alias.wav", audio)):
        hog += (
            alias.encode().ljust(13, b"\0")
            + struct.pack("<i", -len(payload))
            + name.encode().ljust(256, b"\0")
            + payload
        )
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / "xl-hog-launcher.zip"
    descriptor = b"d2x-name = Derived XL xhogi\ntype = normal\nnum_levels = 1\nneptunxl.rl2\n"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
        for name, data in (("xhogi.mn2", descriptor), ("xhogi.hog", hog)):
            info = zipfile.ZipInfo(name, (2000, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            output.writestr(info, data)
    evidence = {
        "mod_file": archive.name,
        "hog_path": "xhogi.hog",
        "members": members + 2,
        "hog_sha256": sha(hog),
        "midi_name": midi_name,
        "midi_sha256": sha(midi),
        "audio_name": audio_name,
        "audio_sha256": sha(audio),
    }
    (args.output / "fixture.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(evidence))


if __name__ == "__main__":
    main()
