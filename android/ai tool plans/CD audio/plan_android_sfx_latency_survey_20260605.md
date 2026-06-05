# Android sound effects latency survey

## Goal
Survey Android app causes of delayed Descent 2 sound effects and identify the most likely latency sources before implementation

## Checklist
- [x] Create this plan
- [x] Trace the active Android sound effects backend and configuration
- [x] Inspect Android SDL, SDL_mixer, and native glue for buffer sizing, format conversion, thread scheduling, and pause behavior
- [x] Inspect Java and manifest settings that can influence audio mode, lifecycle, and performance
- [x] Summarize likely causes, evidence, and recommended next fixes or probes

## Findings
- Highest confidence: Android SDL_mixer uses 2048 sample frames per callback in both D1 and D2. At 48000 Hz this is about 42.7 ms per app buffer before platform buffering. The custom OpenSL ES backend queues two buffers, so a newly started effect commonly waits about 43 to 86 ms before it can reach OpenSL, before AudioFlinger and device output latency
- Highest confidence: the OpenSL ES player explicitly sets `SL_ANDROID_PERFORMANCE_NONE`. Android documentation says latency mode is the low-latency option and `NONE` is for no specific performance requirement and allows effects. This can push playback off the low-latency path
- High confidence: JNI queries `PROPERTY_OUTPUT_FRAMES_PER_BUFFER`, but the mixer ignores it and keeps the hard-coded 2048 frame buffer. Android docs say the low-latency OpenSL path depends on using a sample rate and buffer size compatible with the native output configuration
- Medium confidence: OpenSL ES is deprecated for new low-latency game audio. Android recommends Oboe or AAudio with low-latency performance mode, exclusive sharing when possible, game usage, callbacks, and buffer tuning
- Medium confidence: first play of each effect pays conversion and allocation in `mixdigi_convert_sound`. This can make individual effects late or inconsistent after startup, level load, or cache clearing, but it should not explain constant delay once effects are cached
- Medium confidence: input dispatch adds at most the next game frame under normal load. It does not look like a 300 ms path unless the game thread is blocked or the UI thread is badly delayed
- Lower confidence: music and Redbook callbacks share the SDL_mixer callback, but their Android paths drain pre-rendered ring buffers and should not normally block sound effects

## Recommended Next Pass
- Add debug-log timing probes around effect start, first audio callback after start, and current queued buffer index, then test on device with the existing debug log export
- Change OpenSL ES performance mode from `SL_ANDROID_PERFORMANCE_NONE` to `SL_ANDROID_PERFORMANCE_LATENCY` and log whether `SetConfiguration` succeeds
- Replace the hard-coded 2048 frame SDL_mixer buffer with a value derived from `g_android_native_buffer_frames`, starting conservatively at 2x native frames with a minimum such as 256 or 512
- Preconvert or warm commonly used SFX after `Mix_OpenAudio`, at least weapons, doors, explosions, and menu sounds
- After the OpenSL tuning pass, consider an Oboe or AAudio backend if latency remains above the device path floor

## Tranche 1
- [x] Switch OpenSL ES performance mode to latency mode while keeping the existing 2048 frame SDL_mixer buffer
- [x] Add sound-effect start to callback timing diagnostics
- [x] Expose the new audio diagnostics through introspection
- [x] Build-check the Android native code
- [x] Provide phone test instructions focused on latency and music regressions

### Test Instructions
- Build and install a debug APK, then launch Descent 2 on a real phone
- Confirm music playback first: let a MIDI or Redbook track play for at least 60 seconds and listen for new popping, crackling, or skips
- Enter gameplay, fire primary weapons, open doors, trigger explosions, and compare SFX timing against visible action
- Dump introspection after firing a few effects and inspect `audio.sfx_last_delay_ms`, `audio.sfx_last_cb_delta`, `audio.osl_buf_frames`, `audio.osl_native_buf_frames`, and `audio.osl_perf_mode_result`
- Also check logcat for `DXX-Audio` lines containing `sfx latency probe`
- If music regresses, revert only the OpenSL performance mode change first; this tranche did not reduce the 2048 frame SDL_mixer buffer

## Tranche 2
- [x] Read exported phone logs from the first diagnostic build
- [x] Interpret the SFX callback timing against mixer and native buffer sizes
- [x] Keep the 2048 frame mixer buffer for now, but reduce the OpenSL startup queue from two buffers to one
- [x] Add exportable queue and estimated app latency fields to game logs and introspection
- [x] Build-check the Android native code
- [x] Run Android code quality formatting/linting
- [x] Provide phone test instructions focused on latency improvement and music regressions

### Exported Log Reading
- The phone accepted OpenSL latency mode: `perf_mode_result=0`
- The device native output buffer is small: `native_buf_frames=144` at 48000 Hz, about 3 ms
- The app mixer buffer remains 2048 frames, about 43 ms
- The first callback after SFX start was usually quick: powerup-like sound `71` showed 7 ms around shield and energy pickups in the exported log
- Because the old OpenSL startup path queued two 2048-frame buffers, a sound mixed in the next callback could still sit behind about one full app buffer before reaching the platform output

### Test Instructions
- Install a fresh debug build from this tranche and launch Descent 2 on the real phone
- Turn on exportable Game Logs in the launcher debug settings
- On game start, confirm the exported log later contains `[audio] init:` with `initial_queue_buffers=1`
- Let music play for at least 60 seconds before collecting pickups; listen specifically for new popping, skipping, crackle, or short dropouts
- In gameplay, collect several shield/energy/weapon pickups and compare their timing to the HUD messages and visual pickup moment
- Export the debug logs from the launcher Advanced tab
- Send the exported Game Logs lines containing `[audio] init:` and `[audio] sfx latency:`
- Subjectively note whether SFX feel unchanged, slightly improved, clearly improved, or worse, and whether music regressed

## Tranche 3
- [x] Read exported phone logs from the one-buffer OpenSL queue build
- [x] Confirm the queue-depth change took effect on device
- [x] Compare measured app-side SFX latency against the reported 300 ms subjective delay
- [x] Reduce the Android SDL_mixer buffer from 2048 frames to 1024 frames for a controlled next test
- [x] Add direct exportable `[audio] sfx start:` logs with sample leading-silence timing
- [ ] Build-check the Android native code
- [ ] Run Android code quality formatting/linting
- [ ] Provide phone test instructions focused on 1024-frame buffer stability

### Exported Log Reading
- The phone ran the one-buffer queue build: `initial_queue_buffers=1`
- All exported probes reported `queue_ms=0`, so the extra OpenSL app queue was removed
- SFX app-side timing was low: 133 probes, min 1 ms, average 22.3 ms, median 19 ms, p95 39 ms, max 48 ms
- If audible pickup delay remains around 300 ms, the remaining delay is not explained by the currently measured game-to-callback path
- The next test halves the mixer callback interval from about 43 ms to about 21 ms while keeping the music path otherwise unchanged

### Test Instructions
- Install a fresh debug build from this tranche
- Turn on exportable Game Logs
- Confirm the exported `[audio] init:` line shows `buf_frames=1024` and `initial_queue_buffers=1`
- Let level music play for at least 90 seconds before judging SFX, listening for new pops, skips, crackle, or dropouts
- Collect several pickups and fire/impact weapons; note whether SFX feel unchanged, slightly improved, clearly improved, or worse
- Export logs from the launcher Advanced tab
- Send `[audio] init:`, `[audio] sfx start:`, and `[audio] sfx latency:` lines
