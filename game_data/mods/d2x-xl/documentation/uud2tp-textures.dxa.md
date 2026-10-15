# UUD2 Texture Fixes and Extras Redux adaptation notes

## Original mod

UUD2TP by Xfing. The author's documentation remains in `UUD2TP.rtf`, preserved byte-for-byte, including credits and original installation instructions. These notes describe Redux's packaging and compatibility changes.

[Author's announcement and download](https://www.descentbb.net/viewtopic.php?p=393747).

## What DXX-Revival changed

Converted changed and extra textures from seven PIG palette sets into PNG images/masks and indexed bitmap replacements. Added bitmap slots and their palette/flag behavior are retained. The complete HAM replacement is represented as semantic patches for textures, effects and wall animations, with baseline checks. The author's monitor, lava, door and texture fixes remain the author's work.

The mod does not ship complete PIG/HAM replacements. The upstream demonstration mission `pogtest.hog` / `pogtest.mn2` and custom palette `descent.256` are also omitted from this global mod. Missions using the seventh, D1-style palette still need their matching custom palette/PIG assets; ordinary six-palette D2 missions do not require those optional assets.

## Using this pack

Requires D2 retail v1.2 data with the matching base HAM and PIG checksums listed below, and an updated Android DXX-Revival build supporting indexed bitmap additions and semantic HAM patches. This is for D2, not the D1 engine.

Place above high-resolution packs to retain UUD's corrected artwork. UUD2SP can be enabled above or below this pack: its adapted field patches compose with this texture patch. Restart the game session after changing mods. Import and enable through the launcher rather than following the original package's manual file-replacement instructions.

## Required base-file checksums

In the active file set, open each base file's information popup and compare its `SHA-256` value with the list below. Mod Details also shows these checks under `Base Files`, including the expected SHA-256 and any mismatching actual value. These are checksums of the original game-data files, not of the downloaded mod or its documentation ZIP.

All required files below must be present and match their full SHA-256. File sizes are included as an additional identification aid; size alone is not sufficient. These values come from this pack's `metadata/manifest.json` compatibility requirements.

| File | Size (bytes) | Required SHA-256 |
| --- | ---: | --- |
| `descent2.ham` | 1,307,598 | `5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d` |
| `groupa.pig` | 4,931,752 | `facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b` |
| `alien1.pig` | 5,016,232 | `811fc58caa3e2a72cdfa07d7530b2bb0ca71836a6a2d8a3cb401e4284949c233` |
| `alien2.pig` | 4,912,053 | `75ef8fa0cba03410c61ad1b58f57dcb1481f1f302985828aab0af90639926055` |
| `fire.pig` | 4,973,944 | `26a5a5f4e91456abf31f79578d0922e7bc3348b6aa92489a84033de83f358156` |
| `ice.pig` | 4,926,111 | `ae6152ef69502b00e51a98d8f04b21f2855a332cd2988ecceb3b909a49fa26a1` |
| `water.pig` | 4,833,463 | `de88ead87dcb32f16936b3e2a08b81a2248440f29e6f8be0c4c3a5f9fe4b63c1` |

### Optional D1-style palette baseline

The following file is marked optional in the manifest. It is not required for ordinary six-palette D2 missions and is not a mandatory launcher compatibility check. It identifies the baseline used for the seventh, D1-style texture set when a custom mission uses it; that mission still needs its matching palette/PIG assets. This is Pumo's D1-style PIG for D2, not D1 retail's `descent.pig`.

| File | Size (bytes) | Baseline SHA-256 |
| --- | ---: | --- |
| `descent.pig` | 4,987,419 | `58868c54cf32c83a411f008cba7753d879217fb1aae5a51527695e156c4560e5` |

## Conversion provenance

[Texture conversion implementation at the audited revision](https://github.com/larsonm-personal/dxx-redux/tree/c42522f9c1f4e795d717a564712e784cd32af33e/game_data/mods/xfing/convert-xfing-minimal-dxa.ps1).

Original `uud2tp.zip` SHA-256: `bd2bdaf3d674238356972e8b64566bee038f1e6bff19f980c61532ee61345b6a`.
Original `UUD2TP.rtf` SHA-256: `43fb7dd746332cab0af1f02267a9e294377c790d8000420b2c92cea404de73c5`.

The optional seventh-palette conversion uses Pumo's original 2015 D1-palette PIG as its baseline; this is distinct from the already-patched UUD PIG. Its SHA-256 is `58868c54cf32c83a411f008cba7753d879217fb1aae5a51527695e156c4560e5`.
