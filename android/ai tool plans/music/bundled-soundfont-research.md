# Bundled SoundFont candidates and information pages

Research date: 2026-09-22. Research only; no instrument assets downloaded into
the app, no dependency changes, and no new playback claims based on listening.

## Recommendation

Shortlist GeneralUser GS and FluidR3 GM for a first two-font comparison. FluidR3
has the strongest permissive-license evidence reviewed here. GeneralUser is
compact and explicitly permits software inclusion, but carries an upstream
sample-provenance caveat and needs renderer compatibility testing. Neither is
an authenticated recreation of a named vintage sound card.

Consider MuseScore General as a later third option, not an initial mandatory
Android asset. Its much larger SF2 and Fluid ancestry make its additional
benefit something to establish by listening, rather than assume from size.
FluidR3Mono is another MIT candidate if we prioritize a smaller Fluid variant,
but its commonly advertised small size is for SF3, not a directly loadable SF2.

## Candidates

| Bank | License evidence | Size / fit | Recommendation |
| --- | --- | --- | --- |
| GeneralUser GS 2.0.3, S. Christian Collins | Custom GeneralUser license v2.0 permits software use and modification; discloses uncertain origins of some samples | Upstream SF2 is 32,319,396 bytes, about 30.8 MiB; within current limit | Conditional bundle candidate; assess provenance caveat and TinySoundFont playback |
| FluidR3 GM, Frank Wen | MIT, with original copyright and permission notice retained | Approximately 141 MiB SF2 | Preferred permissive candidate; requires larger-bank memory work or a clearly named reduced derivative |
| FluidR3Mono GM, Michael Cowgill / Frank Wen and contributors | MIT; retain derivative credits | Common distribution is about 13.8 MB SF3; do not treat this as SF2 or runtime-memory size | Alternative to full Fluid, pending exact artifact and decoded-size validation |
| MuseScore General, S. Christian Collins / Michael Cowgill / Frank Wen and contributors | MIT; retain all acknowledgements and notices | Distribution index lists about 206 MiB SF2 and 38 MiB SF3 | Later larger option; not needed alongside every Fluid variant initially |

The GeneralUser byte count was read from the author's GitHub contents API on
the research date (Git blob ID 298b552d2e9d1307e03e5c5c99d2c046aaed9ec3).
This is not a SHA-256 artifact pin. Integration must pin an actual release or
commit and hash its downloaded SF2 and license files together.

Sources for GeneralUser:
- [Author website](https://www.schristiancollins.com/generaluser.php)
- [Author repository and synthesis requirements](https://github.com/mrbumpy409/GeneralUser-GS)
- [Author's complete license](https://github.com/mrbumpy409/GeneralUser-GS/blob/main/documentation/LICENSE.txt)

Sources for Fluid:
- [Maintained Fluid distribution](https://github.com/pianobooster/fluid-soundfont)
- [Original author README and sample-origin statement](https://github.com/pianobooster/fluid-soundfont/blob/main/README)
- [MIT license](https://github.com/pianobooster/fluid-soundfont/blob/main/COPYING)
- [FluidSynth's bank recommendations and sizes](https://www.fluidsynth.org/wiki/SoundFont/)

Sources for the derivatives:
- [FluidR3Mono author discussion](https://musescore.org/en/comment/620926)
- [FluidR3Mono license/credits](https://github.com/musescore/MuseScore/blob/main/share/sound/FluidR3Mono_License.md)
- [MuseScore General distribution, license and sample-source table](https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/)
- [MuseScore General MIT license and credits retained in MS Basic](https://github.com/musescore/MuseScore/blob/main/share/sound/MS%20Basic_License.md)

The MuseScore distribution index and older handbook sizes differ slightly.
Use the exact selected artifact size/version, not a rounded website number,
when preparing an app manifest. Do not assume current MS Basic and every older
MuseScore General release are the same asset.

## Exclusions and unresolved candidates

- TimGM6mb is already bundled, but its GPL-2 license conflicts with the requested
  no-GPL selection policy. Decide on replacing it as part of this integration;
  keeping it as a third bundled option does not resolve that issue.
  [Current pinned asset license](https://github.com/arbruijn/TimGM6mb/blob/master/COPYING.txt)
- FreePats' reviewed General MIDI set is GPL-3-or-later with an exception, is
  incomplete, and is large. It does not meet this task's selection criteria.
  [Project's General MIDI page](https://freepats.zenvoid.org/SoundSets/general-midi.html)
- Arachno documents third-party material, incomplete provenance, and conditions
  around commercial use. Do not approve bundling merely from its freeware label.
  [Author's documentation](https://www.arachnosoft.com/main/soundfont.php?documentation=fullscreen&language=english)
- SGM, Timbres of Heaven, FatBoy, and small banks advertised by third parties as
  CC0/public-domain were not cleared by this review. No verified, complete
  author/rightsholder redistribution chain was established here. This is an
  unresolved status, not a finding that every such bank is prohibited.
- No reviewed evidence grants this project redistribution rights for original
  SC-55, Microsoft GM.DLS, or Creative AWE/Live instrument data. Keep hardware
  banks out of the proposed bundle pending asset-specific permission. A label
  such as GS-compatible establishes a playback convention, not a hardware-bank
  license or an exact hardware emulation claim.

## Current implementation constraints

Checked the repository's actual shared loader and pinned TinySoundFont source:

- `SoundfontStore.MAX_BYTES` and `MUSIC_SOUNDFONT_MAX_BYTES` cap files at 64 MiB
- `music_soundfont_validate` accepts SF2 version 2 and expects PCM sample layout;
  compressed SF3 is not currently an accepted app asset
- Pinned TinySoundFont leaves arbitrary SoundFont modulators and chorus/reverb
  sends unsupported. GeneralUser's upstream specifically relies on modulators
- The native loader reads the bank into memory and TinySoundFont expands samples
  to float storage; download size alone is not a RAM estimate

Consequently, FluidR3 and MuseScore General are licensing candidates, not
drop-in approved app assets. Raising the size constant alone is insufficient:
measure peak memory, loading time and profile-switch overlap on Android first.
A reduced MIT-derived bank is permitted by the reviewed license, but should
have its own name, exact transformation recipe, preserved notices and comparisons
against the untouched original. Do not silently remove instruments or alter mix.

## Information page proposal

Each selectable bundled bank should have an About action showing:

- Display name, exact version, creator and contributors
- Brief sound description; a hardware association only when supported by evidence
- Clickable origin website and upstream project link
- License name, full offline license text, and an upstream license link
- Required copyright/attribution notices and any upstream provenance caveat
- Installed size and any project-made modifications

Keep these records in one bundled catalog, associated with stable bank IDs.
The build record should additionally pin source URL, commit/release, SHA-256,
license snapshot and transformations. Users should not have to inspect hashes
to choose a sound. An information page complements permission; attribution
does not by itself establish a right to redistribute the samples.

AdLib remains the default renderer. SF2 choices apply when SoundFont rendering
is selected and as its documented fallback bank; selecting an SF2 must not
silently replace the original FM banks or switch the HMQ/FM arrangement.

## Next validation increment

1. Pin the proposed banks and their notices, resolving GeneralUser's documented
   provenance issue before treating it as an unconditional release choice
2. Render D1 title/game01/game07/game08 and representative D2 music through the
   current production pipeline; compare GeneralUser with a compliant reference
   renderer to identify modulator-dependent differences
3. Check program/drum coverage, clipping, loading, peak RAM, repeat and seek;
   provide level-matched listening samples without changing saved playback gain
4. Decide full-size versus reduced Fluid based on measurements, then implement
   the bundled catalog and About pages with existing preference/reset behavior

No new GPL synth, bank, or tool is proposed for this work. A non-GPL asset license
is separate from the license of software used to render it.
