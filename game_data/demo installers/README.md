# Demo Installers

This directory contains known Descent 1 and Descent 2 demo packages used for local extraction, Android import support, and regression test data setup.

## Extraction Model

The DOS demo packages are ZIP-compatible archives. `desc14sw.exe` is a PKLITE self-extracting ZIP with a DOS executable preamble, so readers must scan to the first ZIP local file header before using normal ZIP entry processing. The DOS packages contain `.sow` files, which are ARJ-style archives already handled by the native `sow_extract` code.

The Android launcher follows the same model:

1. Identify known demo packages by filename or package SHA-256 with `DemoInstallerPackages`
2. Open the file as ZIP, skipping any self-extractor preamble
3. Extract direct game files or nested `.sow` files to a temp directory
4. Expand sorted `.sow` archives with `DiscImportBridge.extractSowFiles()` in append mode so split installer chunks reconstruct the final files
5. Hash the final game data files and import them through the normal asset manifest path

The PC-side helper is `game_data/extract_dos_demos.ps1`. It uses DOSBox-X to run the DOS installers and produce the reference extracted trees. `game_data/hash_assets.ps1` then records extracted file hashes in `android/app/src/main/assets/known_versions.json5`.

## Packages

| Package | SHA-256 | Contents | Final game data |
| --- | --- | --- | --- |
| `desc14sw.exe` | `3dadb7fbc01efce2904d0908c55d9a9cf1f402e83bf771970552efaca15efcb0` | Self-extracting ZIP: `INSTALL.EXE`, `DESCENT1.SOW`, `DESCENT2.SOW` | `descent.hog`, `descent.pig` as D1 Demo v1.4 |
| `descent 1 demo 1-4.zip` | `64741386ad88d7a60a9529383affb4d2415e11d907ea6dbab8a8a66e1c20b745` | ZIP: `INSTALL.EXE`, `DESCENT1.SOW`, `DESCENT2.SOW`, `LICENCE.TXT` | `descent.hog`, `descent.pig` as D1 Demo v1.4 |
| `descent 1 demo mac.zip` | `622ab2d5328f5c5dd9f804e59b8a8769ed85fc25f92428a3cfb5ff53aed95d07` | ZIP with `Descent Shareware/Data/descent.hog`, `Descent Shareware/Data/descent.pig`, `d1xr-mac-sounds.dxa`, and Mac support/readme files | `descent.hog`, `descent.pig` as D1 Demo (Mac). The `.dxa` is a separate optional sound mod |
| `descent 2 demo 1-0.zip` | `a7c31eae6dfd22e1f6a4c0b9fb2dfb2e25197831bc43c3e9d65734c7fa446c4d` | ZIP: `INSTALL.EXE`, `D2_1.SOW`, `D2_2.SOW`, `D2_3.SOW`, `FILE_ID.DIZ`, `README.TXT` | `d2demo.hog`, `d2demo.ham`, `d2demo.pig`, `d2demo.dem` as D2 Demo v1.0 |
| `d2demo10.zip` | `f8d005670fe5cd17e07ca9bf4022f1045aed436639c37f1e83dd647e14fcec1f` | ZIP: `INSTALL.EXE`, `D2_1.SOW`, `D2_2.SOW`, `D2_3.SOW`, `FILE_ID.DIZ` | Same D2 Demo v1.0 game data target as `descent 2 demo 1-0.zip` |

## Reference Extracted Files

`descent 1 demo 1-4_extracted`:

| File | SHA-256 | Size |
| --- | --- | --- |
| `DESCENT.HOG` | `26d1e31e7709dfe6dddf17ccd37f5c82e866dce49a0faf07e90ba3213b288eab` | 2339773 |
| `DESCENT.PIG` | `710f1c1bafc4c2fcb9623ebe701e2fff34c21b5d3d3e0fe164c1162615971a54` | 2509799 |

`d1 mac extracted`:

| File | SHA-256 | Size |
| --- | --- | --- |
| `descent.hog` | `b70528d0c9daeb8137f05a5a699d0bf884058398a6ab4a97307807a1c0cee9be` | 3370339 |
| `descent.pig` | `b4608a1d0e6191ac6f07410d9714c591c77605a84bccdb882c2611bd885a2905` | 2714487 |

`descent 2 demo 1-0_extracted`:

| File | SHA-256 | Size |
| --- | --- | --- |
| `D2DEMO.DEM` | `8c6e2d43ba88166d17759d90e3817edd0c3ef0a33861ef35a51a8cd4db89c892` | 355173 |
| `D2DEMO.HAM` | `747ccf2494916892061e13601cd8695c35e46f2a99062fff3e3f298da94b9be6` | 1961015 |
| `D2DEMO.HOG` | `b6bf5514b7f2c25ff516c46e9d49eef5862b10667a95365631e7a64a10adc47e` | 2292566 |
| `D2DEMO.PIG` | `368f9ea56fe8eb8b6e4636ab5eba60bfffdf692fe10100d604fedf654d7d8989` | 2800295 |

The Android launcher imports lowercase canonical filenames. Version labels are resolved by SHA-256 through `known_versions.json5`, so copied or renamed demo files still identify as demo data when their content hash matches a known entry.