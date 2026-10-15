# D1 High-Res Sounds Redux adaptation notes

## Original audio

D2X-XL sound-pack material credited to DizzyRox, Novacron and Aus-RED-5. These notes describe Redux's conversion, not new sound design by Redux. The retained `hires-sounds.7z` source contains no original documentation files.

## What DXX-Revival changed

Converted WAV samples to raw unsigned 8-bit mono PCM at 22,050 Hz, stored under `Sounds/` with `.r22` filenames. Conversion mixes channels to mono, resamples where needed, and converts sample encoding to the engine's expected format. D1 audio comes from the source archive's `sounds/d1` directory.

Only the applicable game's sounds are included. The separate `sounds/d2x-xl` effects are omitted because they do not have corresponding base-game sound IDs. This pack does not enable D2X-XL-specific sound features or change the game's sound assignments.

## Using this pack

Applies to D1 and native D1-in-D2 using Android Redux's 22 kHz replacement loader. It can be combined with texture and presentation packs. Restart the game session after changing mods; Mod Details reports loaded and played sounds.

## Conversion provenance

[Sound conversion implementation at the audited revision](https://github.com/larsonm-personal/dxx-redux/tree/c42522f9c1f4e795d717a564712e784cd32af33e/game_data/mods/d2x-xl/convert_d2xxl_sounds.ps1).

Original input: `hires-sounds.7z`, SHA-256 `39cab9e2aed4c6ec9e3b811bbdf2c880d4acd87e4c66a9e1f80f1b25aef5ef6b`.
