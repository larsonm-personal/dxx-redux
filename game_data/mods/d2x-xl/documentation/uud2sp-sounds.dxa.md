# UUD2SP Sound Fixes and Restorations Redux adaptation notes

## Original mod

UUD2SP 1.4 by Xfing, with contributions credited in the original `UUD2SP.rtf`. That document is preserved byte-for-byte. The restored robot/door effects and playback-rate fixes are the author's work.

[Author's announcement and download](https://descentbb.net/viewtopic.php?t=21386).

## What DXX-Revival changed

The author's `descent2.s11` and `descent2.s22` sound banks are preserved byte-for-byte, without resampling. The original package's full `descent2.ham` replacement is replaced by semantic patches for 307 changed fields, allowing it to compose with the UUD2TP texture patch instead of overwriting its HAM changes.

One field, door clip 9 timing, is aligned with the author's combined UUD2TP+SP release. The original HAM's HAXMED editor trailer is recorded in metadata and intentionally omitted from runtime patching. The archive contains no full HAM override.

## Using this pack

Requires D2 retail v1.2 data with the standard HAM checksum listed below and Android DXX-Revival's semantic HAM patch and sound-bank support. This pack is for D2. It can be placed above or below UUD2TP.

Place above D2 High-Res Sounds to retain UUD2SP's effects. Reversing the order selects high-resolution PCM for overlapping effects; UUD2SP's extra sounds and assignments remain. Restart the game session after changing mods. Mod Details reports loaded and played sounds. Import and enable through the launcher; the original README describes installation of the original package.

## Required base-file checksums

In the active file set, open each base file's information popup and compare its `SHA-256` value with the list below. Mod Details also shows these checks under `Base Files`, including the expected SHA-256 and any mismatching actual value. These are checksums of the original game-data files, not of the downloaded mod or its documentation ZIP.

All required files below must be present and match their full SHA-256. File sizes are included as an additional identification aid; size alone is not sufficient. These values come from this pack's `metadata/manifest.json` compatibility requirements.

| File | Size (bytes) | Required SHA-256 |
| --- | ---: | --- |
| `descent2.ham` | 1,307,598 | `5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d` |

## Conversion provenance

[Sound conversion implementation at the audited revision](https://github.com/larsonm-personal/dxx-redux/tree/c42522f9c1f4e795d717a564712e784cd32af33e/game_data/mods/xfing/d2x_sp/convert-uud2sp-ham-patch-dxa.ps1).

Original `UUD2SP1_4.dxa` SHA-256: `d5a9b61f53ecc327bc25837f717516c9c1260a9e750452900481cb8d905b2569`.
Original `UUD2SP.rtf` SHA-256: `46160a60dab0bc2bdd5d3c84cd8f6fb4e758ea49a4108f94cdbe2a2e3bff57d6`.
