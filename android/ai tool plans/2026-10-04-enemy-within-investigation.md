# The Enemy Within Android investigation

## Scope

- Reproduce and fix the Level 2 startup/restore crash reported by the Play Store build
- Inspect the Level 2 robot briefing overlap and distinguish mission data from renderer behavior
- Diagnose stalled metadata precompute on the connected Samsung phone using adb evidence
- Correct robot preview projectiles for mission-customized weapons and bosses

## Work plan

1. Preserve crash evidence and collect phone diagnostics before changing device state
2. Trace each issue through mission assets and native/launcher code, with targeted diagnostics where needed
3. Implement focused fixes, checking corresponding D1/D2 paths and preserving unrelated local edits
4. Extend meaningful integration coverage and run relevant host/Android builds and device tests serially
5. Run scoped formatting and test catalog validation if tests change; record findings and remaining limitations

## Initial evidence

- Supplied xCrash report aborts in `d2/main/endlevel.c:1760`, `load_endlevel_data`, assertion `exit_segnum!=-1`
- Samsung SM-S931U is adb serial `RFCY703C48J`; installed Play Store package is not debuggable
- Existing unrelated changes include automation/game files, test catalogs, outstanding bugs, and store-asset work; preserve them

## Findings and changes

- Level 2 has no exterior (`children == -2`) exit. The fly-out loader asserted on this legal mission layout while starting or restoring the level. Both engines now skip exterior-animation setup in that case
- The briefing renderer advanced automatic wraps by the text box's top coordinate and reset overflowing newlines over the retained text stream. Both engines now use the normal line advance and existing page-transition path. Android introspection reports retained briefing text and overlapping character positions for regression coverage
- Samsung Battery Saver was enabled, thermal status was normal, and exported precompute logs repeatedly stopped before discovery completed. Removed Battery Saver from scheduling entirely at the user's explicit request. Game/thermal pauses now report an explicit status instead of leaving a stale scanning display
- Enemy Within Rebirth supplies custom HAM data in `ewithin.dxa`. Metadata and preview requests omitted support archives. They now use the selected mission's scoped DXA archives, mount them before reading assets, and invalidate old metadata results. A unit test caught and fixed source-root selection when the DXA is outside the mission directory
- The installed Play package uses Google's app-signing certificate, different from the local upload key. Device validation uses the existing isolated `com.dxxredux.app.nsdtest` package; the Play app and saves remain intact

## Validation

- Android x86_64 native build and 55 focused Kotlin tests passed
- Scoped mixed-language formatting passed
- Automation catalog validation passed (94 standalone JSON tests, 364 support scripts, 189 standalone PowerShell tests). Master catalog integration passed (287 entries); first attempt timed out under concurrent builds, retry passed
- Original emulator-5580 test was interrupted by the concurrent store-assets task; moved validation to emulator-5582 and the isolated Samsung test package
- Windows D1 and D2 builds passed. D2 initially exposed an existing unguarded Android diagnostic in `state_restore_take_menu_request`; added the missing Android guard before retrying
- Android arm64 diagnostic build passed and was installed over the existing isolated Samsung test app
- `test_enemy_within_level2.ps1` passed on emulator-5582 at 2340x1080 and on Samsung. The strengthened 47-step Samsung run verifies the last robot page includes `shreds.` without retained-character overlap, starts Level 2, saves, changes difficulty, restores, and observes the saved difficulty again
- Samsung Battery Saver remained enabled (`low_power=1`). The test app completed precompute for Enemy Within levels 2, 1, 3, 4, and 5 while visible in the launcher; the original Play app's exported log never reached discovery completion while paused by Battery Saver
- Mission robot preview integration passed on emulator-5582 with the DXA mounted, actual projectile rendering, sound, rotation, aspect preservation, all 50 robot navigation entries in both directions, and clean closure
- Additional Samsung boss checks verified robot 17 rendered smart missile weapon 17/model 134 and robot 64 rendered mega missile weapon 18/model 135. The regression checks both primary and secondary weapon slots, including bosses whose default preview gun shows a different weapon. Normal launcher screen timeout was temporarily extended during this check and restored in `finally`
- Physical preview automation now wakes the device and retries Back only while the preview remains resumed. Samsung logs showed an `android` system window consuming the first Back event, leaving the preview open; this affected test cleanup rather than missile rendering
- Final Samsung preview run passed end to end, including both expected boss missile models, all navigation entries in both directions, and clean Back closure. Final scoped formatting and `git diff --check` passed

## Repeatable checks

```powershell
.\android\tests\test_enemy_within_level2.ps1 -Serial emulator-5554
# Physical tests use the separately installed diagnostic package
$env:DXX_TEST_PACKAGE='com.dxxredux.app.nsdtest'
.\android\tests\test_enemy_within_level2.ps1 -Serial RFCY703C48J
.\android\tests\test_robot_preview.ps1 -Serial RFCY703C48J `
    -MissionZip android/temp/enemy-within-fixtures/ewithin-rebirth.zip `
    -ExpectedAssetArchive ewithin.dxa -ExpectedBossWeaponModels @{17=134;18=135} `
    -TimeoutSeconds 360
```

The existing Play installation still requires a Play-signed update to receive the fixes. No user saves were removed and no store release was published
