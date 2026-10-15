# UUD1 Texture Fixes Redux adaptation notes

## Original mod

UUD1TP by Xfing. The author's artwork, credits, history and original installation instructions remain in `uud1tp.rtf`, preserved byte-for-byte. These separate notes describe the Redux adaptation.

[Author's announcement and download](https://descentbb.net/viewtopic.php?f=22&p=391643).

## What DXX-Revival changed

The original package replaces `descent.pig` and three First Strike level files. Redux packages the differences as a removable DXA: PNG images and transparency masks, indexed bitmap replacements preserving palette indices and flags, and semantic patches that check the expected base data before applying changes.

The author's door animation timing change is represented as a property patch. The author's surface changes in First Strike levels 3, 17 and 20 are represented as 72 surface replacements, rather than shipping whole RDL files. These reproduce Xfing's intended fixes; Redux did not originate those artwork or gameplay changes. Full PIG, HOG and RDL replacements are omitted.

## Using this pack

Requires matching D1 retail v1.4a/v1.5 PIG and HOG data and an updated Android DXX-Revival build with indexed bitmap, property and level-surface patch support. Applies to D1 and native D1-in-D2. The exact required checksums are listed below; other data versions are not interchangeable.

Place above high-resolution texture packs to retain UUD's corrected artwork. Restart the game session after changing mods. Use the launcher's mod import and enable controls; the preserved original README describes the author's original package installation.

## Required base-file checksums

In the active file set, open each base file's information popup and compare its `SHA-256` value with the list below. Mod Details also shows these checks under `Base Files`, including the expected SHA-256 and any mismatching actual value. These are checksums of the original game-data files, not of the downloaded mod or its documentation ZIP.

All required files below must be present and match their full SHA-256. File sizes are included as an additional identification aid; size alone is not sufficient. These values come from this pack's `metadata/manifest.json` compatibility requirements.

| File | Size (bytes) | Required SHA-256 |
| --- | ---: | --- |
| `descent.pig` | 4,920,305 | `093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe` |
| `descent.hog` | 6,856,701 | `83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052` |

The PIG baseline is used for texture indices and the property patch; the HOG baseline is used for the First Strike level-surface patches. Both checks are required for this pack.

## Conversion provenance

[Texture conversion implementation at the audited revision](https://github.com/larsonm-personal/dxx-redux/tree/c42522f9c1f4e795d717a564712e784cd32af33e/game_data/mods/xfing/convert-xfing-minimal-dxa.ps1).

Original `uud1tp.zip` SHA-256: `373e6b90dfb979e2657d8cafbd9bb2ba27eadb3af7a9a65437bd08d519e36a00`.
Original `uud1tp.rtf` SHA-256: `42ae478f014008e9ee66a4679decced91b7c72abd348e3f57a22916658baeafe`.
