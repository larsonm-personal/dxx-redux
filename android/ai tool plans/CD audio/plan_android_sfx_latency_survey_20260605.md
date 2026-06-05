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
