## Overlay settings close unpause

### Goal
- [x] Make closing the overlay settings tray also resume gameplay
- [x] Keep the paused icon as a status indicator unless a specific overlay owns a pause guard
- [x] Add focused regression coverage for tray, music panel, and quick-load pause policy
- [x] Run scoped code quality, targeted JVM tests, and an Android debug build

### Notes
- Preserve pause ownership while the music panel or quick-load prompt remains open
- Do not change lifecycle or unrelated engine menu pause behavior
- The tray animation now marks the tray closed before its callback, so the existing pause
  synchronizer releases the tray-owned pause when no other overlay guard remains
- `QuickSaveLoadActionTest` and `:app:assembleDebug` passed with JDK 21
