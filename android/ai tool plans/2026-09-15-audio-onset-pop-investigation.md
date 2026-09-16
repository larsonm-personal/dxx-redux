# Audio onset pop investigation

## Report and evidence

After the Vertigo graphics fix, the phone has popping/hissing in Vertigo and
Counterstrike. It persists with Texture logging disabled and through both
Bluetooth headphones and the phone speaker. The clearest trigger is starting
Vulcan firing. This rules out Texture logging as a necessary trigger and makes
a Bluetooth-only fault unlikely.

The previous phone log shows 48 kHz stereo signed 16-bit output, 256-frame
buffers, and one initially queued buffer. Built-in music is selected. Its
early callback-overrun snapshots are zero, but they do not establish what
happens during the newly reported noise. MIDI underrun messages go to logcat;
the exported log alone does not contain a complete music health record.

## Recent regression found

Commit f9387b1f inserted android_sound_trace_play between Mix_PlayChannel and
Mix_SetPanning/Mix_SetDistance in both engines. With Game Logs enabled, this
hashes the original and converted sample on each eligible start and can write
multiple log records. Mix_PlayChannel releases the SDL audio lock before it
returns, so the callback can consume a newly started sample while this work
runs, before the desired volume/panning has been applied. This widens an
existing small setup race and can produce an incorrect initial level.

Move the diagnostic before Mix_PlayChannel, after selecting the channel.
This removes the newly introduced delay without changing sample conversion,
gain, buffering, music playback, or desktop behavior. It does not claim to
eliminate the underlying separate-call setup race, nor prove the cause of
every reported artifact.

## Validation and next discrimination

- Build both Android engines for all configured ABIs and run scoped formatting
- Run the existing stock sound trace integration test to retain source/cache
  validation and exercise playback with Game Logs enabled
- Ask for an A/B on the current phone build with Game Logs disabled, separately
  from Texture logging, and retest the corrected build
- If noise remains, capture the current selected music source, TSF clipping
  and underrun counters, callback duration/overrun counters, and sound-start
  sample identities during repeated Vulcan starts; distinguish effects from
  music by testing them separately before changing buffer sizes or gain

Validation completed: scoped formatting passed, both engines built for
arm64-v8a, armeabi-v7a, and x86_64 without compiler warnings, and the stock
sound trace test passed all 23 steps plus source/cache fingerprint checks.
Logs: `temp/sound-pop-build.log`, `temp/sound-pop-test.log`, and
`temp/sound_trace_logcat.txt`. This smoke test does not measure audible pops.
Phone confirmation of the candidate fix is pending.
