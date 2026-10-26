# Native music PCM recovery

Selectively recover the useful audio oracle from `test-overnight-config-fuzz`
(`9273bbc011f8e66f65b50f4dcf706acd6389bcfc`) onto current `cmake`
(`a6036eacf4725dfaaab56630baca813f80c99dba`). Reuse `test_upstream_compat`
and its real SDL_mixer linkage; no production observation bridge is required.

The new test generates two stereo PCM WAV tones, calls the production music
load/volume/stop/free functions, and observes actual postmix callbacks. It checks
frequency and gain, full/half/muted/unmuted output, A-B-A source replacement,
and silence after stop and teardown. Wrong frequency/gain and playing-but-muted
negative controls must fail the audible oracle. Callback progress is mandatory;
an absent device or stalled callback cannot pass a silence check. Audio counters
are read/reset under the SDL audio lock, with a discarded transition window.

Both D1 and D2 Debug native tests passed locally using dummy audio. This proves
decoded mixer output, not speakers, Android audio, HMP synthesis, mission routing,
or native-D1 versus imported-D1 soundtrack identity. Those require separate
fixtures rather than treating synthetic file replacement as mission admission.
Existing song-list mount-priority tests retain their independent routing scope.

The source branch remains intact. No settings, toolchain pins, private assets,
or production code change. Hosted native CI remains required.
