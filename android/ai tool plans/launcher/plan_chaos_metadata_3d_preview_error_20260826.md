# Chaos metadata 3D preview error diagnosis

## Goal

Determine why the launcher metadata browser reports an error when opening the
3D preview for the `chaos.msn` mission backed by `chaos.hog`.

## Plan

1. [Complete] Trace the metadata browser preview action, mission source
   resolution, and error reporting path.
2. [Complete] Compare the `chaos.msn` metadata and HOG contents with the
   previewer's assumptions and reproduce the failing input where practical.
3. [Complete] Record the concrete root cause and the smallest appropriate fix
   or next debugging step.

## Findings

- Direct non-base HOG targets parsed their adjacent mission descriptor and
  recorded its filename, but did not set `extraDataDir` to the descriptor's
  directory.
- The preview request therefore mounted `chaos.hog` without mounting the
  adjacent `chaos.msn`. Native preview mission loading could not discover the
  descriptor before loading `chaos1.rdl`.
- Direct descriptor targets and archive-backed targets already mount or stage
  the descriptor directory, so the defect was specific to opening metadata
  from a directly stored HOG.

## Resolution

- Direct non-base HOG targets now expose their adjacent descriptor directory
  as `extraDataDir`, matching the existing direct-descriptor behavior.
- Added target and final preview-request assertions covering the adjacent
  descriptor mount.
- Focused unit tests passed. The rebuilt APK also completed all three Android
  native ABI builds. An emulator rerun was blocked before preview launch by a
  separate metadata worker process crash while re-analyzing all five Chaos
  levels; this does not affect the request-construction regression coverage.
