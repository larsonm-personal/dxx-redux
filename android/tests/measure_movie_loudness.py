"""Measure owned D2 MVL movie audio with the music_spectral Python environment.

Example: python android/tests/measure_movie_loudness.py --library path/intro-h.mvl
    --library path/other-h.mvl --output temp/movie-audio-normalization
FFmpeg decodes complete soundtracks; no automatic leveling is applied
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import imageio_ffmpeg
import numpy as np

from compare_music_spectra import ROOT, RATE, levels, write_wav


def movies(library):
    """Read the engine's DMVL directory layout (13-byte name, LE32 size)."""
    with library.open("rb") as source:
        if source.read(4) != b"DMVL":
            raise ValueError(f"Not a DMVL library: {library}")
        count = struct.unpack("<I", source.read(4))[0]
        entries = [struct.unpack("<13sI", source.read(17)) for _ in range(count)]
        for raw_name, size in entries:
            name = raw_name.split(b"\0")[0].decode("ascii")
            if Path(name).name != name:
                raise ValueError(f"Invalid movie name: {name}")
            data = source.read(size)
            if len(data) != size:
                raise ValueError(f"Truncated movie: {name}")
            yield name, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scale", type=float, help="Override net movie gain for comparison")
    args = parser.parse_args()
    header = (ROOT / "android/app/src/main/cpp/shared/music_playback_levels.h").read_text()

    def constant(name):
        return float(re.search(r"#define " + name + r" ([-\d.]+)f", header)[1])

    scale = args.scale
    if scale is None:
        scale = constant("AUDIO_MOVIE_VOLUME_SCALE") * constant("AUDIO_GAMEPLAY_HEADROOM_SCALE")
    args.output.mkdir(parents=True, exist_ok=True)
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    report = dict(
        method="Complete FFmpeg-decoded soundtracks at 48 kHz stereo; integrated LUFS and true peak; fixed gain, no compression",
        limitation="Source measurements, not device output; engine resampling and startup trim can change results",
        ffmpeg=subprocess.run([ffmpeg, "-version"], capture_output=True, check=True).stdout.decode().splitlines()[0],
        calibration=header,
        net_scale=scale,
        results=[],
    )
    for index, library in enumerate(args.library):
        for name, data in movies(library):
            movie = args.output / f"{index}-{library.stem}-{name}"
            movie.write_bytes(data)
            try:
                decoded = subprocess.run(
                    [
                        ffmpeg,
                        "-v",
                        "error",
                        "-i",
                        str(movie),
                        "-map",
                        "0:a:0",
                        "-f",
                        "f32le",
                        "-ar",
                        str(RATE),
                        "-ac",
                        "2",
                        "-",
                    ],
                    capture_output=True,
                    timeout=180,
                )
                if decoded.returncode:
                    raise RuntimeError(decoded.stderr.decode(errors="replace"))
                pcm = np.frombuffer(decoded.stdout, dtype="<f4").reshape(-1, 2)
                row = dict(
                    library=str(library),
                    movie=name,
                    sha256=hashlib.sha256(data).hexdigest(),
                    seconds=len(pcm) / RATE,
                    original=levels(ffmpeg, pcm),
                    adjusted=levels(ffmpeg, pcm * scale),
                )
                report["results"].append(row)
                print(
                    f"{library.stem}/{name}: {row['original']['lufs']:.2f} -> {row['adjusted']['lufs']:.2f} LUFS; peak {row['adjusted']['true_peak_dbfs']:.2f}",
                    flush=True,
                )
                # Short source and adjusted excerpts for listening, never independently normalized
                write_wav(movie.with_suffix(".original.wav"), pcm[: 30 * RATE])
                write_wav(movie.with_suffix(".adjusted.wav"), pcm[: 30 * RATE] * scale)
            finally:
                movie.unlink()
    path = args.output / "report.json"
    content = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if not path.exists() or path.read_text() != content:
        path.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    main()
