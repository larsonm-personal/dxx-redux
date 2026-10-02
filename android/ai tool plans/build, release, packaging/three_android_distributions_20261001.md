# Three Android distributions

Keep the current UI and engine shared across Play, matching GitHub and legacy
GitHub builds. Play and matching GitHub retain minimum API 24; legacy starts
at API 23. All retain target API 36

- [x] Add a legacy build selection, exclude Play SDKs and provide source stubs
- [x] Enable pinned Java API/NIO backports and repair remaining framework gaps
- [x] Add standard/legacy GitHub release selection and APK-derived SDK labels
- [x] Extend release integration coverage and verify all build selections
- [x] Build both native engines/all ABIs, test API 23 and current Android
- [x] Record outcomes, commands and remaining limitations

Follow-up scope: move current/legacy minimum and target SDK pairs into
`android/distribution_versions.conf`, feed BuildConfig from those choices,
and identify distribution/legacy/SDKs in About, logs, xCrash and native/launcher
fallback reports. Existing version/date fields must remain accurate across
the three builds. Extend device integration coverage for those headers

Release scripts are exercised against fake GitHub services; no real release
will be published as part of implementation

## Results

- `test_android_distributions.ps1` built/inspected Play, matching GitHub and
  legacy debug APKs. All contain D1/D2 for ARM32, ARM64 and x86_64. APK identity,
  actual minimum/target SDKs, NIO backports and Play SDK inclusion/exclusion
  checks passed. The current pair is 24/36; the legacy pair is 23/36
- All 68 GitHub release scenarios passed on PowerShell 5.1 and 7. They cover
  both minimum-SDK badging field names, legacy tags/IDs, exact SDK labels,
  saved metadata and upload recovery. GitHub calls were mocked
- Legacy NewApi/InlinedApi lint found no issues. All 1,091 JVM tests completed
  with zero failures and one existing skip. Two soundfont tests now explicitly
  select the renderer whose preservation they verify, avoiding stale defaults
- About/log metadata passed on API 36 for all three distributions and on a
  Google-services-free API-23 emulator for legacy. Installed VM flags confirmed
  the legacy safeguard is active on API 23 and inactive on API 36
- Native SIGSEGV/backtrace/header checks passed on API 36 for all distributions
  and on API 23 for legacy. Headers identify distribution, version, SDKs and
  build date. New version metadata avoids adding Windows-invalid filename
  characters to xCrash's version-derived report names
- `test_distribution_launch.jsonc` passed D1 (26 steps) and D2 (27 steps) on
  the final legacy APK with the emulator's global compiler filter restored to
  `speed`. Tests cover launcher handoff, native gameplay, automap and objectives
- Scoped mixed-language formatting/lint and `git diff --check` passed. The
  Kotlin runner now includes all three production source directories
- Both signed-release entry points stamp one UTC date/time. About and headers
  retain existing commit/hash/build-type details alongside new identity fields

## Android 6 compatibility notes

The API-23 x86_64 image initially advertised GLES 2 under software graphics.
Host graphics with `-feature GLESDynamicVersion` exposed GLES 3.1 and removed
the shader-creation failure; no rendering/UI source port was needed

An ART native crash in AOT-compiled `RouteMetadataBackground.computeMission`
was reproduced in the same image. Interpreter mode avoided it. The legacy
manifest now requests VM safe mode through an SDK-qualified boolean: enabled
on Android 6, disabled from API 24. This can slow Java/Kotlin work on Android 6;
the C++ engines retain native compilation

The longer lifecycle matrix passed D1 under interpretation, but the old image
lost ADB responsiveness during D2 screen-lock/resume. The dedicated smoke
runner completed both games using the shipped VM safeguard and normal global
compiler settings. Physical ARM device performance and lifecycle testing
remain useful follow-up work; the emulator results do not establish those

References for the compatibility mechanism:
[ART VM safe mode](https://android.googlesource.com/platform/art/+/4489369),
[ApplicationInfo flag](https://developer.android.com/reference/android/content/pm/ApplicationInfo#FLAG_VM_SAFE_MODE),
[emulator GPU configuration](https://developer.android.com/studio/run/emulator-acceleration)

Artifacts and full logs are in ignored `android/temp/minsdk-study/` and
`android/temp/distribution-tests/`. No real tag, release or upload was changed
