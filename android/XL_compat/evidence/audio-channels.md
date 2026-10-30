# Owned XL named-channel allocation

The owned 128-voice pool now follows the pinned XL channel-selection policy.
It scans from a retained circular cursor. When every channel is occupied, it
restricts replacement to the requested class if that class occupies at least
half the pool, then prefers a nonpersistent voice with the lowest recorded
volume. If only persistent candidates remain, it chooses the lowest-volume
persistent voice. Equal volumes retain circular scan order.

This is the fixed 128-channel named-voice profile. Redux's ordinary channel pool
is still separate. Ordinary replacements, ambient owners and their shared
exhaustion behavior remain unqualified and gated where required. No original
Pumo level or campaign is admitted by this change.

## Source and state

The existing source owner executes actual pinned `CAudio::SoundClassCount`,
`FindFreeChannel`, `StartSound` and `StopAllChannels`. It also extracts the
channel-persistence expression from `CAudioChannel::Start`. Independent channel
storage and playback adapters supply the environment; source owner cleanup is
called but has no linked objects in the named-only profile.

The 5,723 comparisons cover class normalization, the 63/64/65 class boundary,
free slots, both persistence classes, zero and unequal volumes, ties, cursor
wrap, stopped pools and deterministic varied occupancy. Source failed-start
and unavailable-backend controls additionally record their cursor behavior.
The source runs before original-resource audits in the same existing test
executable; there is no additional CI test owner.

Zero and negative repeat counts both loop, but only negative repeats make a
named channel persistent and remap generic class zero to class minus one.
Source volume is captured at start for replacement priority. The owned runtime
retains that value, its allocation cursor and serial identities through saves.
Class and persistence are reconstructed from the owned authored action; a
reused slot receives a new serial and playhead. Stopping or replacing a bank
preserves the allocation cursor, as the source stop-all path does.

The pre-release trigger trailer is schema 17. Audio state adds a bounded cursor
and each voice adds its captured priority volume. Missing, nonintegral,
out-of-range and extra fields reject atomically; no compatibility reader is
introduced. Restoration preserves the next replacement decision, not only the
voices audible at the instant of saving.

The existing immediate global SFX output control remains in place, including
Android headroom and mute. This differs from the reference's retained volumes
for already-started unlinked channels; the reference's `SetFxVolume` synchronizes
linked sound objects. Captured priority follows the source while output retains
the existing application slider behavior. Actual-output comparisons hold that
slider fixed during playback. This is an explicit output-policy difference,
not a claim of identical live volume history across the two engines.

## Integration controls

Native 23/40 and 27/40 fixtures fill all 128 channels with 63 negative-repeat and
65 zero-repeat requests. Further real actions prove the change at the half-pool
class threshold and circular replacement order. Actual SDL output is compared
against independently evaluated stereo and mono fixture waveforms, including
clipping and nonzero playheads. A restored full pool must make the same next
replacement decisions. Another earned pool captures different start-time
volume settings and preserves the lowest-volume choices after restoration.

Thirty-three malformed trailer cases run against full, varied-volume and
single-voice earned states. Existing event, source-rate reopen, disk-through-stock
and Redux controls remain. The same `xl_named_audio_runtime` owner includes
these controls without another process or increased timeout.

Android extends the existing 73-step owner: the 23/40 fixture earns and restores
the full pool through repeated rewind, ordinary disk saves and stock; the 27/40
fixture retains the previous single-voice output/persistence checks. Every full
pool verification also starts further real actions and checks which slots are
replaced before restoring the earned state. Its output checker evaluates the
authored waveform and recorded music contribution independently of the mixer.

## Validation

Windows source and both native layout controls pass. The complete public owners
pass 43 Windows and 46 Linux controls. The required private owner passes all
103 controls in 351.339 seconds. After strengthening the continuation assertion
to distinguish a reset cursor by its next replacement, both Windows layouts
pass again and the focused Linux owner passes in 36.27 seconds.

The final Android named-audio owner passes all 73 steps in 128,889 ms, run
`5c266878ac40457896086b81b16b3f0b`. Its 33 actual-output checks compare 743,424
PCM samples at 48 kHz, with 23 full-pool continuation checks and 1,534 nonzero
music samples. Protected application data has identical before/after hashes.

The Android folder owner passes 65 steps in 98,036 ms, run
`108329e4cb504ff18d0bf4177d02962e`. Its ordinary-replacement rejection control
passes 27 steps in 37,488 ms, run `9042e08a99c44e80a67e12fa1523d2a1`.
These retain normal/secret scope and unapplied-resource gates with the new
trailer; their protected-data hashes also match. Scoped mixed formatting and
both automation catalogs pass.

Artifacts are under `temp/xl-audio-channels-20261029`. The existing reproduction
owners are `xl_named_audio_source`, `xl_named_audio_runtime`, and
`android/tests/test_xl_named_audio.ps1`. No autonomous level navigation is used.
