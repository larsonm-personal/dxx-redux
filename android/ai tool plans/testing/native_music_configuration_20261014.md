# Native music configuration campaign (2026-10-14)

## Scope and observables

- Real native D1/D2 menus and levels, SDL2 offscreen video through SDL 1.2 compatibility, SDL dummy audio
- Public verified D1 and D2 shareware data, plus their combined directory for D1 content in D2
- Native SDL_mixer 1.2 with FluidSynth and TimGM6mb.sf2, built-in HMP converted by the native engine
- Android ymfm/TSF selection, Android per-save music preferences, and physical speaker output are not covered
- Playback position is not available through this SDL_mixer version

The opt-in DXX_HOST_CONFIG_PROBE build records actual mixer playing/paused/volume, requested source, playback return value and engine song index. A postmix observer records PCM sample count, nonzero count, absolute magnitude, trailing-one-second peak, and positive zero crossings. Sound effects are muted for all PCM assertions. A known 440 Hz WAV verifies decoded source frequency and expected signal amplitude at the selected music volume; playlist tests use distinct 440/660 Hz sources.

Production builds compile out these diagnostics. The observer is installed only from the main-thread snapshot, never from a music-completion callback. If a movie replaces SDL_mixer's single postmix callback, stalled counters are a coverage blocker rather than a silent pass.

## Oracles

- At least 950 ms elapsed snapshot time and at least 0.5 s of actual rendered PCM samples per measurement
- Stopped and paused output must have exactly zero nonzero PCM samples
- Unmuted output must exceed one mean-absolute signed-16-bit unit
- FluidSynth gain-zero output may contain its measured dither floor: at most one mean-absolute unit, after a full settled window within a bounded five seconds
- Every unmuted real output window is also checked against the mute oracle and must be rejected
- Known tone frequency must match within 5 Hz; expected mean amplitude must match within 5 percent
- Test execution with Python optimization is refused so assertions cannot disappear
- Executables are frozen per case with matching pre-copy, copied and post-copy SHA-256 hashes
- Each case has isolated HOME, persisted config, command trace, engine logs and a process deadline

## Why muted output is not required to be bitwise zero

The first strict-zero test exposed FluidSynth release/reverb and dither, rather than continued audible music. Direct SDL_mixer playback of the exact MIDI exported through the native D2 HMP converter reproduced the result. Successive half-second means after mute were approximately 59.9, 8.88, 1.31, 0.29 and 0.253 signed-16-bit units. A later tiny transient also reproduced outside the game. Stopping playback yielded exact zero. Raw diagnostic evidence is under temp/music-backend/direct-native-d2-decay.json; the game-side decay trace is temp/music-d2-tail-diagnostic-20261014/decay.json. The PCM floor was not raised to hide the tail; bounded settling retains intermediate observations.

## Completed evidence before the cross-mode fix

- temp/music-d1-final-20261014/music-report.json: 20 passing menu/source/volume observations, all five nonzero volumes 1/2/4/7/8, stop/mute between them, independent WAV waveform verification and clean relaunch persistence
- temp/music-d2-final-20261014/music-report.json: same 20 observations passed
- temp/music-d1-shortcuts-20261014/music-report.json and temp/music-d2-shortcuts-20261014/music-report.json: level entry, real pause/resume, builtin previous/next no-op behavior, save/load with global music-off retained, level-source restoration and title return passed
- temp/music-d1-missing-source-20261014/music-report.json and temp/music-d2-missing-source-20261014/music-report.json: deliberately absent WAV stops playback and built-in source recovers
- android/tests/test_host_music_campaign.py: ten negative-oracle tests, including wrong frequency, wrong gain, absent/stalled callback, false mute, source failure and sound-effect contamination

Native save files do not capture Android's per-save source preferences. The native save/load case deliberately saves with music on, turns global music off, loads, verifies it remains off, then reenables the correct level song.

## Confirmed D1-in-D2 cross-mode failure

The combined public-data case verified engine d2, mission Descent Demo version 1, level 1. Audible playback, stop drain, save/load-global-off, and restoration of the same selected level track succeeded. A subsequent independent archive comparison found that the selected track was game01.hmp from D2, not the correct native D1 game0.hmp; those earlier state/energy observations did not prove correct content identity. Abort Game then failed at d2/main/newmenu.c in nm_draw_background1 with pcx_error == PCX_ERROR_NONE. Evidence: temp/music-d1in2-gameplay-20261014/trace.jsonl and engine-1.log.

The requested path was d1-original/menud.pcx. The original D2 shareware menu retained menud.pcx; presentation remapping selected the active D1 namespace. Independently inspecting the verified HOG directories showed D1 has menu.pcx and not menud.pcx, while D2 shareware has menud.pcx. Both archives and the D1 presentation mount were present. This is a cross-mode resource identity error, not an absent downloaded asset.

Android's Escape / Abort Game also returns to native menus, but adds autosave, free_mission and mission-asset reset steps before restoring menus. A separate return-to-launcher action requests process exit. Android startup content selection can also begin with a D1 menu. Desktop impact is reproduced; Android app impact is unverified and must not be inferred as a device result.

The initial one-line production fix refreshes Menu_pcx_name when the D2 main menu activates, updating its stable background path for the current content profile before redraw. After that change, temp/music-d1in2-fixed-20261014/music-report.json passed its eight state/energy observations, including pause/resume, save/load restoration of the selected source, Abort to title music and clean process exit. This initial oracle did not compare D1 reference source bytes and therefore did not catch the separate wrong-soundtrack issue described above. Its immutable D2 binary SHA-256 is 27f5817ec37ac7034affa5a09108d7997934ead41a7460d4479fa275aaa9cc87 (see the adjacent binary.json for the authoritative full digest). This verifies only the forward D2-title to D1-mission to title route. The broader configuration campaign subsequently found a reverse D1-to-D2 return failure; the completed namespace-aware candidate-existence fix subsequently passed the full D1-to-D2-to-D1 route. The later source-routing gate below also independently repeats that complete route. No music behavior fix was made based on the FluidSynth tail observations.

## Reuse

Run android/helpers/host_music_campaign.py with --executable, --data, --output and --game. Use --rounds 5 for the complete seeded nonzero-volume set; --gameplay for level/save/load/shortcut transitions; --missing-source for expected-negative source recovery; --playlist for continuous/per-level/random playlist and previous/next tests. D1-in-D2 gameplay additionally takes --mission "Descent Demo" --mission-version 1 with the combined data directory. Use an immutable completed test build with DXX_HOST_CONFIG_PROBE enabled.

## Confirmed builtin source identity issue and fix

The combined-data D1 mission reports version 1 but selects game01.hmp, which exists only in the verified D2 shareware HOG. Native D1 selects game0.hmp from its verified archive. The songs loader consults root descent.sng and unqualified HMP names without content-profile routing. Actual converted-MIDI payload length and FNV-1a-64 fingerprints were added to the explicit test build and compared against independently captured native D1 and native D2 references. Earlier references to correct game0.hmp in the imported-mode results were inaccurate; only native D1 had that track.

The reduced before-fix case failed with `Wrong builtin MIDI source payload`: native D1 level one was 58,378 bytes / `f364d03c4f51172c`, while imported D1 loaded 28,508 bytes / `c7eef94ee5d81281`. The latter exactly matched the independently exported D2 `game01.hmp` conversion. These are payload fingerprints and lengths, not merely a filename or configuration assertion.

The focused `d2/main/songs.c` fix uses the existing content namespace resolver for song lists, default-track candidate existence and built-in playback. It preserves authored overrides and jukebox filesystem paths. A successful scoped lookup also permits Android-only scoped assets; the root-existence fallback prevents accidentally playing another game's base song when the correct scoped song is absent.

Final snapshot `396d5a3ff83cb2a452f6341f03814e653c43e12a80a0a4a68098e68358fc206c` passed:

- D2 initial title: 68,249 bytes / `e1cefa46c9e64599`
- D1 level: `d1-original/game0.hmp`, 58,378 bytes / `f364d03c4f51172c`
- D1 title return: `d1-original/descent.hmp`, 73,264 bytes / `f1a3e5f9b6584244`
- D2 level after D1: `d2-original/game01.hmp`, 28,508 bytes / `c7eef94ee5d81281`
- D2 title return: `d2-original/descent.hmp`, 68,249 bytes / `e1cefa46c9e64599`
- Second D1 level and title return matched the same independent native D1 fingerprints again
- Loose authored `dxx-r.sng` / WAV overrides remained active for title, imported D1 level and title return, independently verified at 440 Hz and expected PCM gain
- The nine-window native jukebox next/previous/completion/per-level/random/stop/recovery regression still passed after the source fix

Primary final reports are `temp/music-source-routing-final-20261014/source-routing-report.json`, `temp/music-d2-playlist-after-source-20261014/music-report.json`, and the reduced before/after reports under `temp/music-d1in2-source-before-20261014` and `temp/music-d1in2-source-after-20261014`. The source restoration/override runner is `android/tests/test_host_music_source_routing.py`; supply the independently captured native D1 and D2 music reports with `--native-d1-report` and `--native-d2-report`. All scoped mixed-language quality checks and ten negative-oracle tests passed. Probe-off native production builds also passed.

### Remaining Android scope

Android runtime was not exercised. In particular, `track_names.c` still passes unscoped BIMSongs names to embedded metadata readers, so colliding base-game title or briefing names can consult metadata from a different archive even though playback is now routed correctly. This is a source-reviewed metadata risk, not a reproduced Android display failure. Android ymfm/TSF backends, app music-picker controls, per-save source preferences and actual device output remain genuine coverage gaps. No claim is made that this native playback fix resolves those paths.


## Final local disposition

- `d34e08b04`: namespace-aware menu-background fix, committed separately by the menu campaign owner
- `f9918585f`: built-in soundtrack routing fix; production-only commit changes `d2/main/songs.c` (15 additions, 5 deletions)
- Final independent review found no blocker: seven cross-mode source observations, three loose authored override observations and nine post-fix playlist observations agree with the final immutable binary identity above
- Test infrastructure and this report remain unstaged for root integration; neither fix was merged or published

The native audio campaign is complete at this scope. Missing standalone source recovery was tested, but malformed or partially missing playlist entries were not. Loose authored song-list/WAV precedence was verified at runtime; arbitrary custom archive precedence was source-reviewed rather than exercised. Movie callback replacement, unavailable playback-position observability and the Android gaps above remain explicit limitations. No further duplicate audio matrices are planned.
