# Owned named-sound playback

Action 30 now has a separate XL runtime for wall and object actions. Static
inspection counts the requirement; loading additionally requires a complete,
bounded SND/WAV resource set owned by the selected mission HOG. External sound
directories and ambient sound objects are separate requirements. This work does
not qualify any original Pumo level or the campaign.

## Runtime and source behavior

The [pinned source contracts](named-audio-source.md) remain the event reference.
Positive counts play that many complete repetitions; zero and negative counts
loop indefinitely. The source's converted-duration plus half-second cooldown,
strict boundary, wall completion flags, object asymmetry, autoplay, master,
one-shot and disabled behavior remain distinct. Inert sound links do not become
geometry dependencies or repair warnings. Repetition counts do not become
scheduler delays.

Resources are decoded and converted before installation. Both stereo channels
are retained; mono duplicates into stereo. Numeric and prefix addon aliases
resolve before filename lookup. Missing, malformed, oversized or foreign
resources reject the level with a resource-specific error. Resource identities
cover SND bytes and selected WAV names/bytes, including otherwise inert table
changes. Failed preparation leaves the active bank and voices unchanged.

An XL-owned, bounded 128-voice mixer adds samples through SDL_mixer's post-channel
effect, preserving the existing movie post-mix hook and Redux conversion path.
Its callback allocates no memory, reads no files and touches no game objects.
Output is signed 16-bit stereo; rate conversion uses bounded linear
interpolation. Center pan, integer volume and saturation have actual SDL_mixer
output comparisons. Android retains the existing SFX volume and headroom scales.

Snapshots retain owner identities, each active playhead and remaining repetition
count, cooldowns and exact resource identity. They validate before application.
Restoration resumes the saved voice directly, avoiding the source's finite-sound
restart failure through its cooldown check. Output-rate changes map the saved
phase while retaining the event's original cooldown calculation rate.

Pause survives bank replacement and stop operations. Window reactivation only
requests a resume; restored audio waits for the first unpaused gameplay frame.
Android's three consecutive rewind restores reproduced unwanted playback during
window reactivation, and this rule addresses that interval in the runtime.

## Controls and boundaries

- `xl_audio_voice_contract` covers stereo and signed interpolation, mono,
  partial callbacks, repeat boundaries, pause, overlap, clipping, zero volume,
  capacity and aliases.
- `xl_audio_mixer_reference` compares 54 cases sample-for-sample against actual
  SDL_mixer centered channels: boundary-valued stereo samples, one/two/four
  simultaneous voices, one/two/three repetitions and six volume settings.
- `xl_named_audio_runtime` uses labeled derived 23/40 and 27/40 fixtures on
  unchanged Neptune geometry. It exercises actual output, event dispatch,
  duration/repetitions, ownership, 24 malformed trailers, real disk restoration
  through stock, actual mixer reopen and cross-rate playhead restoration.
- `test_xl_named_audio.ps1` owns the normal Android fixture. Its 73 steps cover
  actual device-output PCM, mono/stereo resampling, pause, repeated rewind,
  disk-through-stock restoration and level transitions. Every Android SFX slider
  position has an actual output comparison, including mute; native gain checks
  compare against SDL's existing channel settings. The driver also captures
  music before SFX, so a music restart cannot masquerade as a named-sound error
  or satisfy its expected PCM contribution. Protected app files are backed up and
  restored byte-for-byte; no campaign navigation is attempted.

These tests qualify named voices within the recorded profile. They do not prove
acoustic device/speaker behavior, exact resampling equivalence across SDL
versions, the source's shared priority/stealing policy under mixed classic and
named channel exhaustion, ambient spatialization, multiplayer, or arbitrary
external sound-provider precedence. The browser retains resource-dependent
partial status. Unsupported independent gameplay requirements still reject.

## Original resources

The unchanged Pumo HOG has SHA-256
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.
The production preparation path accepts all 60 actions across its fourteen
levels, resolving 22 distinct owned WAVs at both 44.1 and 48 kHz on MSVC and GCC.
This component audit does not load those levels into gameplay or play their
original events. The independent decoder/source audits cover all 25 WAVs,
including the three not referenced by these actions.

## Reproduction and required selection

The public owner requires 43 tests on Windows and 46 on Linux when both `--gpu`
and `--audio` are selected. `XL_AUDIO_MIXER_TESTS=ON` requires the actual
SDL_mixer output comparison; it cannot silently disappear. The private fast
owner requires 103 tests, including named runtime, source and output controls.

```text
ctest --test-dir <contracts-build> -R "^(xl_audio_.*|xl_named_audio_.*|xl_level_messages)$" --output-on-failure
pwsh -NoProfile -File android/tests/test_xl_named_audio.ps1 -Serial emulator-5554 -Hog <xsound.hog> -Descriptor <xsound.mn2>
xl-preview-lifecycle -named-audio-original pmines -audio-levels 14 -audio-actions 60 -audio-waves 22 -hogdir <retail-data> -extra-dir <isolated-package-root> -userdir <isolated-package-root>/preview-user -nomusic
```

The Android owner requires the exact derived fixture hashes, provisioned retail
D2 and the existing `player` profile. The native resource audit accepts a mission
argument and exact expected counts; no mission-name exception enters admission.
Original hashes must match before and after the optional audit. Pumo is not
present in the private CI asset store, so its resource audit remains local.

## Validation status

The Windows public owner passes all 43 cases in 12.203 seconds of tests and
22.438 seconds overall. The Linux public owner passes all 46 cases in 6.242
seconds of tests and 67.915 seconds overall, including the real SDL comparison.
Both native runtime formats pass on MSVC. Each uses a fresh headless process;
the normal Android application checks the same-process level transition.

The final Android run passes 73/73 steps in 126.377 seconds. Its 33 actual output
comparisons check 749,056 samples at 48 kHz, including all nine SFX slider
positions. An earlier complete run also verifies the independently captured
music contribution while restoring voices. Protected files match their before
hashes after both runs. Source/converted PCM equality does not establish
speaker behavior or original campaign acceptance.

Both automation catalogs and scoped mixed formatting pass. Because the
PowerShell helper excludes absolute paths containing `temp`, the three changed
PowerShell scripts also pass direct pinned PSScriptAnalyzer 1.25.0 formatting
and lint checks in this worktree. Original Pumo HOG/descriptor hashes and their
isolated copies match before and after the preparation audit.

The final Linux private owner passes all 103 required tests, sixteen original
format outcomes and three modern source comparisons. CTest takes 320.16 seconds;
the test/inspection phase takes 323.479 seconds. No test is omitted or skipped,
and the receipt retains `level_two_completion_qualified: false`.

The existing stock D2 sound-trace owner passes 23/23 steps, checking sample-bank
selection, conversion fingerprints and calibrated effects gain. Its surrounding
state backup is restored byte-for-byte. The separate 42-step Dragon Pit sound
scenario could not run because that package was absent from this emulator; it
receives no new passing claim. Local artifacts are in
`temp/xl-named-audio-20261029`.
