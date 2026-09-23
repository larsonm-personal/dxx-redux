# Downloadable SoundFont source catalog

Research current author/distributor sources and GitHub release assets. Expand
the existing Kotlin catalog (comments are supported without a new JSONC parser)
with working release downloads and commented future-hosting candidates. Preserve
the no-GPL requirement, distinguish asset licenses from repository/tool licenses,
and document format, size and redistribution blockers honestly.

1. Verify release versions, file sizes, origin URLs and license evidence
2. Activate compatible release SF2 assets; retain other candidates as commented
   descriptors with future GitHub hosting notes and remaining prerequisites
3. Update catalog tests/documentation and build the debug APK
4. Exercise a real release download, retained Info and playback on the emulator

Do not publish release assets or add soundfont binaries to the APK. Do not enable
known-incompatible or uncleared banks just because a third-party mirror exists.
Keep the FM default and existing user renderer choice unchanged.

## Research results (2026-09-22)

Catalog: `android/app/src/main/java/com/dxxredux/app/SoundfontCatalog.kt`.
It contains 20 bank descriptors: one active and 19 commented out. This retains
the existing typed list rather than adding a separate JSONC parsing mechanism.

### Active release

- [GeneralUser GS author/source](https://github.com/mrbumpy409/GeneralUser-GS)
- [Codetta soundfont-bundle release](https://github.com/krtw00/codetta/releases/tag/soundfont-bundle), published 2026-05-20
- Asset: `GeneralUser-GS.sf2`, 32,319,396 bytes (30.8 MiB)
- SHA-256: `9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe`
- Internal INAM: `GeneralUser GS 2.0.3 BETA`; ICOP: `1997-2025 by S. Christian Collins`
- [Full license v2.0](https://github.com/mrbumpy409/GeneralUser-GS/blob/main/documentation/LICENSE.txt) is retained in the app, including the author's sample-provenance caveat

The downloaded Codetta release asset and the author's current repository SF2
were both hashed locally and are byte-identical.

The author's repository has no GitHub Releases. Codetta supplies a direct SF2
release mirror of 2.0.3. The more recently published
[aldakit soundfonts-v1 mirror](https://github.com/shakfu/aldakit/releases/tag/soundfonts-v1)
contains 2.0.2 instead (32,322,864 bytes, SHA-256
`c278464b823daf9c52106c0957f752817da0e52964817ff682fe3a8d2f8446ce`).
Inspecting the actual RIFF INFO fields prevents choosing an older bank merely
because the mirror's release date is newer. The active asset is labeled beta
honestly. TinySoundFont's incomplete advanced modulator/effect support means
successful loading is not a claim of exact FluidSynth or hardware equivalence.

### Commented candidates

Where no original GitHub project exists, preserve the actual author/distribution
website rather than inventing a GitHub landing page. Future-hosting notes are
conditional on the stated prerequisites, not claims that redistribution has
already been cleared.

| Bank | Source / license evidence | Why commented out |
| --- | --- | --- |
| FluidR3 GM 3.1 | [Release](https://github.com/pianobooster/fluid-soundfont/releases/tag/v3.1), [MIT COPYING](https://github.com/pianobooster/fluid-soundfont/blob/main/COPYING) | Release exists: 148,398,306 bytes exceeds 64 MiB; large-bank validation required |
| OPL-3 FM 128M 1.0 | [Author/distribution README and CC BY-SA 4.0](https://github.com/Mindwerks/opl3-soundfont), [release](https://github.com/Mindwerks/opl3-soundfont/releases/tag/1.0) | Release exists: 135,020,964 bytes exceeds 64 MiB; sampled FM bank, distinct from ymfm |
| FluidR3Mono GM | [MuseScore retained license and ancestry](https://github.com/musescore/MuseScore/blob/main/share/sound/FluidR3Mono_License.md) | Locate/pin exact original SF2, decoded size and complete notices; future GitHub hosting |
| MuseScore General 0.2.1 | [Distribution, MIT license and sample-source CSV](https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/) | About 206 MiB SF2; future GitHub hosting and larger-bank validation |
| MS Basic | [Source and retained notices](https://github.com/musescore/MuseScore/tree/main/share/sound) | Current distribution uses SF3; inspect exact bank/credits and conversion before future hosting |
| Chaos Bank 1.9 | [RKhive download](https://rkhive.com/banks.html), [archive CC0 statement](https://rkhive.com/legal.html) | ZIP, future SF2 hosting; verify embedded notices and archive's authority over included samples |
| JNS-GM 2 | [Author-attributed listing](https://www.polyphone-soundfonts.com/documents/27-instrument-sets/55-jns-gm-2), [RKhive archive](https://rkhive.com/banks.html) | ZIP, future SF2 hosting; original attribution/terms need checking |
| Masterpiece | [RKhive archive](https://rkhive.com/banks.html) | ZIP, future SF2 hosting; bank-level license/author verification |
| Unison | [RKhive archive](https://rkhive.com/banks.html) | ZIP, future SF2 hosting; bank-level license/author verification |
| Music Theory 2 | [RKhive archive](https://rkhive.com/banks.html) | ZIP, future SF2 hosting; verify GM coverage and bank-level permissions |
| Florestan Basic GM GS | [Author's download page](https://dev.nando.audio/pages/soundfonts.html) | ZIP, future SF2 hosting; author identifies Sound Canvas samples but no complete redistribution grant reviewed |
| Vintage Dreams Waves 2.1 | [Ian Wilson's author page and restrictions](https://analoguesque.x10host.com/SoundFonts/) | Verify version-specific redistribution terms and drum compatibility before future hosting; do not call it public domain |
| Arachno 1.0 | [Author's documentation and permissions](https://www.arachnosoft.com/main/soundfont.php?documentation=fullscreen&language=english) | Roughly 150 MB; original/third-party permissions required for commercial uses; future hosting conditional |
| FatBoy | [Author landing page](https://fatboy.site/), [MIDI.js distributor's attribution](https://github.com/gleitz/midi-js-soundfonts) | Original site unavailable during review; CC BY-SA 3.0 reported by distributor, not independently cleared; large bank |
| Musyng Kite | [MIDI.js distributor](https://github.com/gleitz/midi-js-soundfonts) | Reported CC BY-SA 3.0 and about 1.75 GB; locate original license/credits and SF2, future hosting only after validation |
| Timbres of Heaven 4.00(G) | [MidKar distribution](https://midkar.com/SoundFonts/TOH.html) | 7z, large bank, exact-version redistribution permission unresolved; future hosting conditional |
| Aspirin 160 GMGS 2015 | [SynthFont distribution](https://www.synthfont.com/soundfonts.html) | sfPack, original notices unverified; future SF2 hosting |
| bennetng AnotherGS 2.1 | [SynthFont distribution](https://www.synthfont.com/soundfonts.html) | sfArk, original notices unverified; future SF2 hosting |
| JCLive 2.1 | [SynthFont distribution](https://www.synthfont.com/soundfonts.html) | sfArk, original notices unverified; future SF2 hosting |

RKhive's site-wide CC0 statement does not independently establish every bank's
sample rights. Likewise, a repository that renders a bank to MP3/JavaScript is
not a compatible SF2 release source. These distinctions are recorded directly
beside the commented descriptors to avoid activating them accidentally.

TimGM6mb, FreePats and the GPL-tagged `open-soundfonts/SGM_V2_01_soundfonts`
mirror are excluded under the existing no-GPL instruction. SC-55 ROM conversions,
Microsoft GM.DLS and Creative factory ROM banks are not cleared hosting candidates.
No additional font binary or GPL component is added to the APK.

## Verification

Release metadata, downloads and checked licenses are recorded under
`temp/soundfont-catalog/`. The checked-in release-download runner exercises
confirmation, cancellation, actual HTTPS transfer, content hash, retained Info,
unchanged default FM selection, and SF2 preview. It accepts different entry
names, hashes and URLs for future catalog additions.

The real download exposed two overly conservative native validation checks.
The expansion bound counted every instrument generator instead of sample-ID
generators, rejecting GeneralUser despite staying within the 65,536-region
allocation limit. Prefix counts now bound sample regions without repeated scans.
The resolved-position check also rejected a sample whose initial offset starts
inside its loop (GeneralUser's Soundtrack preset). Loop endpoints still must be
ordered and inside the resolved sample end and allocated buffer, but the loop
can wrap before the initial playback offset.

Host validation now renders every preset, including Soundtrack, and passes for
both GeneralUser 2.0.3 beta and the bundled bank. Missing, truncated, oversized
and corrupt-preset inputs still fail validation. The 64 MiB input limit remains.

Completed verification:

- Scoped mixed-language formatting/lint passed
- Host SoundFont CMake target built; bundled CTest and GeneralUser all-preset rendering passed
- Debug APK built for arm64-v8a, armeabi-v7a and x86_64
- All 14 SoundFont download/store JVM tests passed
- API 29+ emulator release-download runner passed all 22 steps, including real
  GitHub transfer, retained Info and SF2 preview; downloaded SHA-256 matched
- Emulator manifest and MIDI preferences restored after testing

Artifacts: `temp/soundfont-catalog/build-fixed.log`,
`temp/soundfont-catalog/device-fixed/report.json`, `automation.log`, `native.log`.
