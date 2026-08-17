# Diagnose missing launcher save UI

- [x] Trace visibility conditions for the resume offer and Save Explorer
- [x] Trace the permanent-dismiss preference and its current settings UI
- [x] Compare recent changes and determine whether save compatibility is involved
- [x] Report the cause without changing product code

## Finding

The matcen save metadata change bumped `ANDROID_SAVE_META_VERSION` from 4 to 5 and enlarged the Android-only metadata trailer. The launcher accepts only the current trailer version, so pre-change saves do not produce a resume candidate. Since the July menu cleanup also removed the independent Save Explorer button and removed the permanent resume-offer setting, a missing candidate makes all save UI disappear. The core D1/D2 save version was not changed.
