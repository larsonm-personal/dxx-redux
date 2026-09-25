# FluidSynth runtime and build audit

Fix the MIDI playback cost caused by the deprecated seventh-order enum mapping
to 25-point sinc in FluidSynth 2.6.1, and audit the remaining integration choices

1. Select explicit fourth-order interpolation on load and DSP recreation
2. Review runtime settings, HMI adaptation, rendering/reset ownership, pinned
   dependencies, source patches and actual compiler commands for Android ABIs
3. Keep historical listening experiments identified accurately; update current
   guidance and add regression coverage through the existing synth integration test
4. Run scoped formatting, host native contracts, Android builds and sustained
   installed-app playback; record measured results and outstanding limitations

Initial evidence: two Windows Release offline game01 comparisons (45 seconds,
SC-55, 128 voices, reverb and chorus) measured 13.61/13.39 seconds with enum 7
versus 0.396/0.398 seconds with enum 4. These are not phone or gameplay timings

## Implementation and audit

- Production pins explicit fourth-order interpolation on both creation paths
- Host integration compares exact PCM against an explicit fourth-order reference
  after load, reset and rate change; existing seek/reset/loop contracts pass
- Corrected historical seventh-order descriptions; offline experiments retain
  enum 7 under its unambiguous HIGHEST name for reproduction
- Audited runtime settings, thread ownership, sample loading/reset behavior,
  dependencies, source changes, and actual three-ABI Debug compiler commands
- No further runtime/build changes justified. Upstream's filter-only fast-math,
  deliberate FDN/chorus profile, gain and double precision are documented
- Added optional elapsed-render headroom budget to the sustained preview probe
- Five native CTest contracts pass, scoped formatting passes, Debug APK builds
  all three ABIs. Initial Gradle attempt hit a locked Kotlin output; retry passed
- Detailed findings: `android/tests/fluidsynth_quality/AUDIT.md`
- Repackaged APK verified for primary DEX/ZIP integrity and installed on emulator-5556
- Installed-app probe blocked before playback: launcher introspection broadcast
  timed out after 60 seconds amid system-wide emulator delays. Cleanup completed;
  no new Android performance number or hosting/phone validation is claimed
