# Store preview boss edit

- Preserve the accepted combined video's first and third gameplay pictures/audio and smooth filtered fly-out
- Capture the D1 level 7 replay in D2 with the featured full-screen presentation, progress rows, boss health, no rear camera and 110-degree main-view FoV
- Keep trilinear filtering, 4x MSAA and 16x AF; record without screenshot interruptions
- Replace D1 level 18 with the boss fight around the pinned featured frame
- Move launcher/import/menu/guidebot content after the second gameplay clip, compress through the first guidebot page to four seconds and mute it
- Pin source intervals and hashes in an offline reproducible edit; reuse native boss audio from the tempo capture with MIDI at its original tempo
- Verify native presentation, output timing, muted opening, retained pictures/audio and geometric fly-out motion; run scoped quality checks and asset integration
- Store review media under ignored android/temp/store-assets_20261005_boss_video

## Result

- Captured the full native D1-in-D2 replay with 110-degree FoV, 2400x1080 rendering, trilinear filtering, 4x MSAA and 16x AF
- Pinned simulation seconds 97-107, a continuous recording span containing the featured boss/laser moment and subsequent explosions
- Final order: D2 level 9 0-5s, boss 5-15s, silent launcher/guidebot 15-19s, D1 level 5 19-24s, fly-out 24-30s
- Native boss source supplies 253 picture frames for ten seconds; exported at 30 fps without optical-flow interpolation
- Verified unchanged decoded pictures for the retained clips, identical reviewed PCM in the master, original-tempo native boss MIDI, silent delivered launcher audio and smooth fly-out geometry
- Native replay differences are the established D1-in-D2 game/mission/endlevel metadata differences only
- Repeat with generate-store-boss-preview.ps1; source hashes and all offline composition inputs are archived in the ignored output folder
