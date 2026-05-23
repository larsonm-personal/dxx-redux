# Plan: Redbook Audio Fix, Texture Introspection, DXA Readme, Launcher Checks, Overlay

## Task 1: Redbook audio regression test + fix
- Extend an existing redbook-related test to add a bin/cue CD as audio source via SAF
- Assert the in-game track list is non-empty
- Fix the bug until the test passes

## Task 2: Add hires texture introspection to a non-modded test
- Pick an existing test (e.g. test_launch_to_automap)
- Add assertions: hires_textures.hires_count == 0, maybe verify max_hires_w

## Task 3: Add README to converted DXA files
- D2: "original files for d2x-xl by Aus-RED-5D, DizzyRox, MetalBeast, Novacron, Theftbot"
- D1: "original files for d2x-xl by DizzyRox, Novacron, Aus-RED-5"

## Task 4: Launcher error for oversized textures
- Before game launch, inspect DXA mod textures for sizes exceeding GL_MAX_TEXTURE_SIZE
- Show error in launcher if detected

## Task 5: In-game texture usage diagnostic overlay
- Show % of hires textures loaded, max texture res, FPS
- Could be a video info overlay toggle

## Status
- [ ] Task 1
- [ ] Task 2
- [ ] Task 3
- [ ] Task 4
- [ ] Task 5
