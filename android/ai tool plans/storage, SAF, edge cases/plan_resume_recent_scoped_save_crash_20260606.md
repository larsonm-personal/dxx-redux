[x] Reproduce/analyze the launcher `Load Last Save` crash path
[x] Identify whether scoped save paths break pending resume launch handling
[x] Patch Android-only resume restore to accept scoped save-set relative paths
[x] Add/adjust tests for scoped resume launch paths
[x] Run formatting and targeted validation

# Resume Recent Scoped Save Crash Plan

## Symptom

Tapping `Load Last Save` from the launcher resume panel can terminate the game process during startup with exit status 102. The breadcrumbs reach normal initialization but do not show a later game-frame crash, which points at pending resume launch handling or direct restore setup.

## Initial Hypothesis

The save explorer/scoped-save work made the launcher scan recursive scoped paths such as:

```text
d2x-redux/Players/save_sets/single/<pilot>/<mission>/<pilot>.sg8
```

The existing resume launcher path was originally built around legacy flat paths such as:

```text
d2x-redux/Players/<pilot>.sg8
```

If the game-side startup resolver only accepts a legacy filename/callsign pair or assumes a flat `Players` path, it may reject the scoped path and deliberately abort.

## Work Plan

1. Inspect pending-resume JSON write/read and the game startup restore code.
2. Confirm the exact path passed by `Load Last Save`.
3. Update Android-only restore code so a scoped relative path can be opened through the same save path used by the save/load menus.
4. Keep non-Android behavior untouched.
5. Run Kotlin/native build checks and host tests affected by save paths.

## Resolution

The launcher was passing resume save paths relative to Android `files/`, such as:

```text
d2x-redux/Players/save_sets/single/test/d2/test.sg8
```

The game process sets the PHYSFS write dir to the active game root before startup resume handling, so D1/D2 restore code expects:

```text
Players/save_sets/single/test/d2/test.sg8
```

`resolveResumeSaveLaunchPath()` now strips the matching `d1x-redux/` or `d2x-redux/` prefix before writing pending resume launch state or intent extras.

## Follow-Up From Build 15330 Logs

The build 15330 logs show the launcher path is now correct:

```text
pending resume launch write: game=d2 path=Players/demo.sg8 callsign=demo
startup resume prep: game=d2 path='Players/demo.sg8' result=ready callsign='demo' source=player_file
startup resume restore begin: game=d2 path='Players/demo.sg8' callsign='demo'
```

The process exits before `restore open`, so the crash is inside D2 `state_restore_all()` before `state_restore_all_sub()`. For direct filename restores D2 sets `filenum = NUM_SAVES + 1`, which makes Android secret companion handling look for slot `bsecret.sgc` before restoring a slot 8 save. The next patch derives the real slot number from `.sgN`/`.mgN` direct restore paths and logs the companion decision.

## Validation

- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ResumeSavePanelTest`
- `.\android\run-code-quality.ps1 -Fix`
- `.\gradlew.bat :app:assembleDebug`
- `.\android\tests\test_native_host_unit_tests.ps1`
