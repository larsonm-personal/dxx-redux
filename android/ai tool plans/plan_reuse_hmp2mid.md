# Plan: Reuse hmp.c HMP-to-MIDI conversion in Android preview

## Problem
midi_preview.c contains a standalone ~230-line reimplementation of the
HMP-to-MIDI conversion algorithm from d2/misc/hmp.c. The game engine's
hmp.c is already compiled into the same shared library (dxx-redux-d2 and
dxx-redux-d1), so we can reuse its conversion logic directly by adding a
memory-buffer entry point.

## Approach
The only function in hmp.c that uses PHYSFS is `hmp_open()`. All downstream
functions (`hmptrk2mid`, `hmp2mid`, `hmp_close`) operate on in-memory data
structures (`hmp_file`, `hmp_track`). Adding `hmp_open_mem()` that parses
from a memory buffer enables reuse of the entire conversion pipeline.

## Changes

### Phase 1: Add hmp_open_mem + hmp2mid_mem to game engine [done]
- d2/misc/hmp.c: add `hmp_open_mem()` and `hmp2mid_mem()` at the bottom
  - hmp_open_mem: parses HMP from const unsigned char* buffer
  - hmp2mid_mem: calls hmp_open_mem + existing hmptrk2mid
  - Both guarded with comment marking them as android port additions
- d2/include/hmp.h: declare both new functions
- d1/misc/hmp.c: same additions, matching d1 style (d_malloc vs CALLOC, etc.)
- d1/include/hmp.h: same declarations

### Phase 2: Remove standalone reimplementation from midi_preview.c [done]
- Remove: write_be16, write_be32, midbuf_t, mb_* helpers, hmptrk2mid,
  hmp2mid_mem, HMP_TRACKS define
- Keep: read_le32 (used by HOG reader functions)
- Add: extern declaration for hmp2mid_mem from hmp.c
- Update header comment to note reuse of hmp.c

### Phase 3: Update midi_enumeration.c [done]
- Should need no changes (already has correct extern declaration)
- Verify extern signature matches

### Phase 4: Build and lint [done]
- assembleDebug
- run-code-quality.ps1 --fix
- Verify clean build

## Key facts
- d_realloc resolves to realloc in NDEBUG, and to mem_realloc(which wraps
  malloc) in debug -- the returned pointer is standard-freeable either way
- hmptrk2mid() is static in hmp.c -- hmp2mid_mem() calls it from the same
  file, so no visibility issues
- The global `ubyte tempo[19]` array in hmp.c is used by both hmp2mid
  and hmp2mid_mem for the tempo track header
- num_tracks read without INTEL_INT, matching existing hmp_open behavior
- tempo read with INTEL_INT, matching existing hmp_open behavior

## Lines removed from midi_preview.c
~230 lines of standalone HMP conversion code replaced by ~3-line extern
declaration, with the actual implementation living in the game engine's
hmp.c where it belongs.
