# Plan - object gate review and demo pref cleanup

- [x] Inspect the current D2 object warning path and confirm whether the old behind-screen gate changed behavior
- [x] Verify the current RNG tracing wrapper does not alter AI dispatch semantics
- [x] Move the shared demo-recording preference key out of AdvancedSettingsPage.kt into a dedicated shared prefs file
- [x] Validate Kotlin references and summarize the behavior impact