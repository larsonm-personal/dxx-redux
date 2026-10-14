# Shared mod component results: 2026-10-14

## Outcome and stopping condition

No confirmed production defect in the tested component scope. The finite configuration and public-payload domains passed, followed by bounded same-process resource checks. Testing stopped after coverage saturation rather than repeating equivalent short cycles until morning.

These are host fixtures compiling the actual Android shared loaders, not Android-device or desktop-game integration tests. No production behavior changes, retail downloads, commits, merges, or remote writes were made by this component task.

## Final dependency fidelity

- Production `android_mod_assets.cpp`, game-specific `ignorecase.c`, and `pngfile.c` are compiled directly, with actual game headers and small test-only platform adapters
- Final runs statically link Android-pinned PhysFS **3.2.0**, verified against dependency SHA-256 `1991500eaeb8d5325e3a8361847ff3bf8e03ec89252b7915e1f25b3f8ab5d560`
- Runtime/header version checks pass; no dynamic PhysFS is loaded
- Texture KTX 4.4.2 and stb sources match production dependency pins
- ASan/UBSan instrument the loaders and pinned dependency builds; LeakSanitizer is explicitly unavailable under the executor's ptrace environment
- Earlier host-PhysFS 3.0.2 results remain separate. Final normalized semantic observations are identical under 3.2.0

## PCM loader

Seed **20261019**, 1,500 transitions per game-header variant:

| Measurement                                    |     D1 | D2 / imported D1 |
| ---------------------------------------------- | -----: | ---------------: |
| Enumerated bounded configuration states        |    256 |            1,024 |
| Distinct ordered transitions                   |  1,437 |            1,369 |
| Resident replacements checked byte-for-byte    |  6,900 |            8,389 |
| Resident-corruption / logging controls         | 13,800 |           16,778 |
| Separate incorrect-owner / PCM oracle controls |      2 |                2 |

The domain covers ordered applicable public/authored PCM packs, native/imported content identity, mixer versus legacy fixed-rate configuration, four output rates, and both reset-before-unmount and unmount-before-reset. Public D1 packs use root in D1 and `d1-mods` in D2; D2 packs stay at D2 root. There are **304 public PCM members** and **3,336 directed payload/context/backend/rate checks**, followed by configuration enumeration and seeded transitions.

Expected bytes come from independent Python ZIP extraction. Assertions check resident bytes, source owner, format, length, rate, original pointer restoration, repeated apply, logging switches, and fingerprint rejection of changed resident bytes. The played callback is invoked explicitly; **actual mixer playback is not tested**. Original sound slots contain bounded synthetic PCM, not engine-loaded game banks.

Final results and reproducible scenarios:

- `temp/host-mod-pinned-domain/summary.json`
- `temp/host-mod-pinned-domain/verification-notes.txt`
- `temp/host-mod-pinned-domain/source-and-binary-sha256.json`

## CPU texture decoder and lookup cache

Both game-header variants pass **256 transitions**, all eligible public assets, and independent source/dimension/payload checks. The D2 corpus includes all **5,460 distinct public KTX2/PNG archive members**, with D1 assets mounted in the imported namespace.

- 19,192 successful asset queries, each repeated: **38,384 CPU reads**
- **8,216 expected missing-asset reads**
- D1: all 5 ordered mount states and 24 distinct transitions
- D2: all 65 ordered mount states and 114 distinct transitions
- Six deliberately incorrect source/dimension/payload controls reject, including under Python `-O`
- Wrong-required-version guard rejects; stale-cache controls recover after the actual cache clear
- Timeout tests retain partial logs and classify incomplete runs

An exploratory all-four-packs-under-`d1-mods` case triggers the index safety rejection under both PhysFS versions. That placement is not produced by the launcher for these packs. The valid split, D2 packs at root and D1 packs under `d1-mods`, passes. The unreachable case is preserved separately and is **not a verified product bug**.

Final evidence: `temp/host-texture-physfs320-20261014_062500/summary.json` and its verification notes.

## Resource and supervision checks

Separate same-process **2,000-transition** replays per variant pass with pinned PhysFS 3.2.0, both sanitized and non-sanitized. Open descriptors remain exactly baseline 4 plus mounted archives and return to 4 after teardown.

Non-sanitized post-warm-up RSS remains within:

- D1: **16,200-16,272 KiB**
- D2: **16,676-16,780 KiB**

ASan RSS warms toward approximately 400 MiB. This is reported as an instrumentation/allocator-retention observation, not a leak finding. The non-sanitized comparison shows no unbounded trend in the tested interval. Details: `temp/host-mod-resource/pinned-physfs320-summary.json`.

The exploratory supervisor owns a separate process group, kills/reaps native descendants on timeout, bounds artifact retention, and preserves a 4 GiB reserve. Forced timeout controls verified cleanup and `timeout_incomplete` classification. Between-batch exit 75 was a deliberate management pause, not a product failure.

## Remaining gaps

No claims are made for GPU texels/upload/rendering, actual audio mixing, Android lifecycle or SAF behavior, launcher/menu-to-runtime wiring, gameplay save/reload, UUD retail banks, D1 PCX/FNT presentation, or the combined gameplay texture/sound/music/menu cross-product. Exact dependency-version parity does not remove these platform and integration boundaries.

## Reusable files and validation

- `android/tests/host_mod_transitions/`: CMake fixture, seeded/exhaustive runner, replay instructions
- `android/tests/host_texture_transitions/`: CPU fixture, runner, version-pinned PhysFS helper, replay instructions
- Scoped mixed-language formatting passes; relevant CMake builds, independent negative controls, and post-format replays pass
- Source and asset fingerprints, exact scenarios, logs, and per-case observations are retained with the final evidence
