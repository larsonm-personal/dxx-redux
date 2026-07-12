# Jukebox sidecar/name extraction plan

## Goal

Move duplicated Android playlist-sidecar path construction and decoded-name or
basename fallback from both inherited jukebox files into `track_names`.

## Boundary and validation

- Keep playlist parsing, RNG order, playback, and overlay notification local.
- Pass the playlist path or read-only song list to narrow shared helpers.
- Preserve slash/backslash handling, extension stripping, and static-buffer
  lifetime.
- Build both games and exercise custom music with and without decoded names.
