# Demo Installers

This directory contains known Descent 1 and Descent 2 demo packages used for local extraction, Android import support, and regression test data setup.

## Extraction Model

The DOS demo packages are ZIP-compatible archives. `desc14sw.exe` is a PKLITE self-extracting ZIP with a DOS executable preamble, so readers must scan to the first ZIP local file header before using normal ZIP entry processing. The DOS packages contain `.sow` files, which are ARJ-style archives already handled by the native `sow_extract` code.

The Mac demo packages are StuffIt archives. `Descent Shareware.sit` is a StuffIt 5 archive containing the D1 game data directly. `Descent II Preview.sit` is a StuffIt 5 archive containing a Mac installer application; its data fork is an STi archive containing the D2 demo data files. The `descent2preview` and `descent_demo` downloads are classic `SIT!` archives, sometimes wrapped in BinHex `.hqx`; Android decodes BinHex to the data fork before passing the result to StuffIt extraction.

The Android launcher follows these models:

1. Identify known demo packages by filename or package SHA-256 with `DemoInstallerPackages`
2. For DOS packages, open the file as ZIP, skipping any self-extractor preamble
3. Extract direct game files or nested `.sow` files to a temp directory
4. Expand sorted `.sow` archives with `DiscImportBridge.extractSowFiles()` in append mode so split installer chunks reconstruct the final files
5. For Mac packages, stage the `.sit` file and extract it with `DiscImportBridge.extractStuffitFiles()`, including any nested STi installer
6. Hash the final game data files and import them through the normal asset manifest path

The PC-side helpers are `game_data/extract_dos_demos.ps1` and `game_data/extract_mac_demos.ps1`. The DOS helper uses DOSBox-X to run the DOS installers and produce reference extracted trees. The Mac helper uses the pinned `unar` tool to extract StuffIt and nested STi payloads. Run the Mac helper with `-WriteOracle` to refresh `mac_stuffit_oracles.json`, which records the external-tool oracle hashes used by the native regression test. `game_data/hash_assets.ps1` records extracted file hashes in `android/app/src/main/assets/known_versions.json5`, including helper outputs under `game_data/demo installers/*_extracted`.

## PC-Side StuffIt Transition Plan

1. Keep `extract_mac_demos.ps1 -WriteOracle` as the external-tool oracle refresh path while the in-tree StuffIt parser is still being hardened.
2. Build and run `test_stuffit_demo_oracles` to require that `stuffit_extract.c` produces the same sizes and SHA-256 hashes as the oracle file for each available `.sit` installer.
3. Add a small host CLI wrapper around `stuffit_extract.c` for PC helper use, reusing the same `dxx_android_mac_disc_extract_extensions` filter as Android.
4. Add `-UseInTree` to `extract_mac_demos.ps1`, compare its output against `mac_stuffit_oracles.json`, then make it the default once the oracle test has covered the supported installer set.
5. Keep the external `unar` path only as `-WriteOracle` or remove it after the in-tree path is the maintained source of extracted demo files.

## Packages

| Package | SHA-256 | Contents | Final game data |
| --- | --- | --- | --- |
| `desc14sw.exe` | `3dadb7fbc01efce2904d0908c55d9a9cf1f402e83bf771970552efaca15efcb0` | Self-extracting ZIP: `INSTALL.EXE`, `DESCENT1.SOW`, `DESCENT2.SOW` | `descent.hog`, `descent.pig` as D1 Demo v1.4 |
| `descent 1 demo 1-4.zip` | `64741386ad88d7a60a9529383affb4d2415e11d907ea6dbab8a8a66e1c20b745` | ZIP: `INSTALL.EXE`, `DESCENT1.SOW`, `DESCENT2.SOW`, `LICENCE.TXT` | `descent.hog`, `descent.pig` as D1 Demo v1.4 |
| `descent 1 demo mac.zip` | `622ab2d5328f5c5dd9f804e59b8a8769ed85fc25f92428a3cfb5ff53aed95d07` | ZIP with `Descent Shareware/Data/descent.hog`, `Descent Shareware/Data/descent.pig`, `d1xr-mac-sounds.dxa`, and Mac support/readme files | `descent.hog`, `descent.pig` as D1 Demo (Mac). The `.dxa` is a separate optional sound mod |
| `Descent Shareware.sit` | `f45c338df4bc4ceda38e6541f14b8dc93b543fd07d90a2c5d5118d2001c12ad2` | StuffIt 5 archive with `Descent Shareware/Data/descent.hog`, `Descent Shareware/Data/descent.pig`, Mac app files, joystick configs, and readmes | `descent.hog`, `descent.pig` as D1 Demo (Mac) |
| `Descent_demo.HQX` | `e485a1570cb6079d3ec55a52ed9150792f5ef450b653e5db9748a305fed2dfe4` | BinHex-wrapped classic `SIT!` archive. Data fork SHA-256: `c42427b2fe2c88d0ffd6d5a37f7a3fa1165aff4a98f8072c28b34ff79b7c2128` | `descent.hog`, `descent.pig` as D1 Demo (Mac) |
| `descent_demo.sit_.hqx` | `87375e89e71f5d43e342ec5666f71347fe2797f2a80838c00dac71f1ae181ebe` | BinHex-wrapped classic `SIT!` archive. Data fork SHA-256: `73818125a52908adf71fbb7801572bbee15370256468ee4e063a0a412fdfc0b4` | `descent.hog`, `descent.pig` as D1 Demo (Mac) |
| `descent 2 demo 1-0.zip` | `a7c31eae6dfd22e1f6a4c0b9fb2dfb2e25197831bc43c3e9d65734c7fa446c4d` | ZIP: `INSTALL.EXE`, `D2_1.SOW`, `D2_2.SOW`, `D2_3.SOW`, `FILE_ID.DIZ`, `README.TXT` | `d2demo.hog`, `d2demo.ham`, `d2demo.pig`, `d2demo.dem` as D2 Demo v1.0 |
| `d2demo10.zip` | `f8d005670fe5cd17e07ca9bf4022f1045aed436639c37f1e83dd647e14fcec1f` | ZIP: `INSTALL.EXE`, `D2_1.SOW`, `D2_2.SOW`, `D2_3.SOW`, `FILE_ID.DIZ` | Same D2 Demo v1.0 game data target as `descent 2 demo 1-0.zip` |
| `Descent II Preview.sit` | `4b5b7739b9da59472bcdca92f23957f90247bedd84ef8bded57d37d5d229f6d6` | StuffIt 5 archive containing `Install Descent II Preview`; the installer data fork is an STi archive | `d2demo.hog`, `d2demo.ham`, `d2demo.pig`, `descent2.s11`, `exit.ham` as D2 Demo (Mac) |
| `descent2preview.sit` | `5b9c359e47e4e458f655ef5a28e6110ea1deee60d79a08f7ebdb2144ec9263fd` | Classic `SIT!` archive containing a direct `Descent II Preview folder/Data` tree | `d2demo.hog`, `d2demo.ham`, `d2demo.pig`, `descent2.s11`, `exit.ham` as D2 Demo (Mac) |
| `descent2preview.sit_.hqx` | `b7c55f60f11a1d0d72658f8a30fecdebef9251e0e86eeff747888fc4f56fcd19` | BinHex-wrapped `descent2preview.sit`. Data fork SHA-256: `5b9c359e47e4e458f655ef5a28e6110ea1deee60d79a08f7ebdb2144ec9263fd` | Same D2 Demo (Mac) game data target as `descent2preview.sit` |

## Reference Extracted Files

`descent 1 demo 1-4_extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `DESCENT.HOG` | `26d1e31e7709dfe6dddf17ccd37f5c82e866dce49a0faf07e90ba3213b288eab` | 2339773 | D1 Demo v1.4 |
| `DESCENT.PIG` | `710f1c1bafc4c2fcb9623ebe701e2fff34c21b5d3d3e0fe164c1162615971a54` | 2509799 | D1 Demo v1.4 |

`desc14sw.exe` direct SOW import:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `DESCENT.HOG` | `26d1e31e7709dfe6dddf17ccd37f5c82e866dce49a0faf07e90ba3213b288eab` | 2339773 | D1 Demo v1.4 |
| `DESCENT.PIG` | `b67865e513452a35887a20270d17fdfb5af1a2edaaae247bc523489f1d84f9ac` | 5092871 | D1 Demo v1.4 |

`d1 mac extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `descent.hog` | `b70528d0c9daeb8137f05a5a699d0bf884058398a6ab4a97307807a1c0cee9be` | 3370339 | D1 Demo (Mac) |
| `descent.pig` | `b4608a1d0e6191ac6f07410d9714c591c77605a84bccdb882c2611bd885a2905` | 2714487 | D1 Demo (Mac) |

`Descent Shareware_extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `descent.hog` | `7af6bbd6aa5e356cae406ea43ab8b47b69d27bd555b513d884c68ccafe9aaf42` | 3387843 | D1 Demo (Mac) |
| `descent.pig` | `b4608a1d0e6191ac6f07410d9714c591c77605a84bccdb882c2611bd885a2905` | 2714487 | D1 Demo (Mac) |

`Descent_demo_extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `descent.hog` | `7af6bbd6aa5e356cae406ea43ab8b47b69d27bd555b513d884c68ccafe9aaf42` | 3387843 | D1 Demo (Mac) |
| `descent.pig` | `b4608a1d0e6191ac6f07410d9714c591c77605a84bccdb882c2611bd885a2905` | 2714487 | D1 Demo (Mac) |

`descent_demo.sit__extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `descent.hog` | `b70528d0c9daeb8137f05a5a699d0bf884058398a6ab4a97307807a1c0cee9be` | 3370339 | D1 Demo (Mac) |
| `descent.pig` | `b4608a1d0e6191ac6f07410d9714c591c77605a84bccdb882c2611bd885a2905` | 2714487 | D1 Demo (Mac) |

`descent 2 demo 1-0_extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `D2DEMO.DEM` | `8c6e2d43ba88166d17759d90e3817edd0c3ef0a33861ef35a51a8cd4db89c892` | 355173 | D2 Demo v1.0 |
| `D2DEMO.HAM` | `747ccf2494916892061e13601cd8695c35e46f2a99062fff3e3f298da94b9be6` | 1961015 | D2 Demo v1.0 |
| `D2DEMO.HOG` | `b6bf5514b7f2c25ff516c46e9d49eef5862b10667a95365631e7a64a10adc47e` | 2292566 | D2 Demo v1.0 |
| `D2DEMO.PIG` | `368f9ea56fe8eb8b6e4636ab5eba60bfffdf692fe10100d604fedf654d7d8989` | 2800295 | D2 Demo v1.0 |

`Descent II Preview_extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `d2demo.ham` | `b3d94652282859e188f9530b63d77b37289ac973bce402025d10021eaffc7a92` | 1307598 | D2 Demo (Mac) |
| `D2DEMO.HOG` | `e39285e4346f3066cf4ad745abcf3dc4bdf142df7c0395a42b26ae291282696b` | 4292746 | D2 Demo (Mac) |
| `d2demo.pig` | `88e834d13f15bfe502e32570a44302326e6486f685cb95e12b3b81d0a14b8642` | 4929684 | D2 Demo (Mac) |
| `DESCENT2.S11` | `d444c6f93476f8941936164d2981387a26b0a25e3f9d5e930ef96bfbb86c1e68` | 2602492 | D2 Demo (Mac), byte-identical to D2 v1.2 |
| `EXIT.HAM` | `c2f1fbc0e39a53d1d92336c45e59e8d79c50bb36c008a4c2bf9bf80f235226b7` | 31932 | D2 Demo (Mac) |

`descent2preview_extracted` and `descent2preview.sit__extracted`:

| File | SHA-256 | Size | Known version |
| --- | --- | --- | --- |
| `d2demo.ham` | `b3d94652282859e188f9530b63d77b37289ac973bce402025d10021eaffc7a92` | 1307598 | D2 Demo (Mac) |
| `D2DEMO.HOG` | `e39285e4346f3066cf4ad745abcf3dc4bdf142df7c0395a42b26ae291282696b` | 4292746 | D2 Demo (Mac) |
| `d2demo.pig` | `88e834d13f15bfe502e32570a44302326e6486f685cb95e12b3b81d0a14b8642` | 4929684 | D2 Demo (Mac) |
| `DESCENT2.S11` | `d444c6f93476f8941936164d2981387a26b0a25e3f9d5e930ef96bfbb86c1e68` | 2602492 | D2 Demo (Mac), byte-identical to D2 v1.2 |
| `EXIT.HAM` | `c2f1fbc0e39a53d1d92336c45e59e8d79c50bb36c008a4c2bf9bf80f235226b7` | 31932 | D2 Demo (Mac) |

The Android launcher imports lowercase canonical filenames. Version labels are resolved by SHA-256 through `known_versions.json5`, so copied or renamed demo files still identify as demo data when their content hash matches a known entry.
