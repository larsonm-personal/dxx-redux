# Music editor sources

- [x] Put built-in Descent 1 before Descent 2 in the editor dropdown
- [x] Include imported level/mod MIDI sources using the existing archive music catalog and preview reader
- [x] Keep saved source IDs stable and omit sources without playable MIDI
- [x] Verify source ordering/filtering and archive reads with JVM tests, compile the launcher, and run scoped code quality

Validation: launcher Kotlin compilation and 55 targeted JVM tests passed (source preferences, editor archive integration, and MissionZipMusic suites). Scoped code quality passed; test Kotlin was formatted directly because the scoped runner only includes main sources. No device UI/audio check was performed. No native code changed.
