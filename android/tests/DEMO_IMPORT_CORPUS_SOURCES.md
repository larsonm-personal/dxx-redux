# Original demo import corpus availability

Checked 2026-10-15 against the public [demo assets release](https://github.com/larsonm-personal/dxx-redux/releases/tag/demo_installers). The full-corpus assertions in `test_demo_import.ps1` require all 11 original archives and has no subset selector. Total pinned bytes: 62,532,081. Four published packages do not constitute a complete import-corpus result. The full owner remains pending unavailable-source provisioning, not excluded as retail-only.

`helpers/setup_demo_import_corpus.py` validates the complete oracle/source-manifest identities, accepts a verified preseeded cache, and fails before downloading when an original has no reviewed URL. It never substitutes another archive that happens to extract equivalent data.

## Verified published mapping

| Local oracle filename | Release asset | SHA-256 |
| --- | --- | --- |
| desc14sw.exe | desc14sw.exe | `3dadb7fbc01efce2904d0908c55d9a9cf1f402e83bf771970552efaca15efcb0` |
| Descent Shareware.sit | Descent.Shareware.sit | `f45c338df4bc4ceda38e6541f14b8dc93b543fd07d90a2c5d5118d2001c12ad2` |
| d2demo10.zip | d2demo10.zip | `f8d005670fe5cd17e07ca9bf4022f1045aed436639c37f1e83dd647e14fcec1f` |
| Descent II Preview.sit | Descent.II.Preview.sit | `4b5b7739b9da59472bcdca92f23957f90247bedd84ef8bded57d37d5d229f6d6` |

## Originals still needed

| Original filename | Required SHA-256 |
| --- | --- |
| descent 1 demo 1-4.zip | `64741386ad88d7a60a9529383affb4d2415e11d907ea6dbab8a8a66e1c20b745` |
| descent 1 demo mac.zip | `622ab2d5328f5c5dd9f804e59b8a8769ed85fc25f92428a3cfb5ff53aed95d07` |
| Descent_demo.HQX | `e485a1570cb6079d3ec55a52ed9150792f5ef450b653e5db9748a305fed2dfe4` |
| descent_demo.sit_.hqx | `87375e89e71f5d43e342ec5666f71347fe2797f2a80838c00dac71f1ae181ebe` |
| descent 2 demo 1-0.zip | `a7c31eae6dfd22e1f6a4c0b9fb2dfb2e25197831bc43c3e9d65734c7fa446c4d` |
| descent2preview.sit | `5b9c359e47e4e458f655ef5a28e6110ea1deee60d79a08f7ebdb2144ec9263fd` |
| descent2preview.sit_.hqx | `b7c55f60f11a1d0d72658f8a30fecdebef9251e0e86eeff747888fc4f56fcd19` |

Supply exact original bytes in `game_data/demo installers`, then run `python3 android/helpers/setup_demo_import_corpus.py --repo .` for offline verification. `--download` permits only the four reviewed URLs after every otherwise-unavailable original is already cached. Original package size and hash checks remain mandatory. The source manifest and import oracle are authoritative.
