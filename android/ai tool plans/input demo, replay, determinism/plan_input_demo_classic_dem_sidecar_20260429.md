# Input Demo Classic DEM Sidecar Plan 2026 04 29

## Goal

Record and keep a classic `.dem` beside each quick-recorded `.dximdemo`
and `.rngtrace.jsonl` so replay investigation can compare deterministic
input replay against state-recorded playback from the exact same recording
session.

## Current Anchors

- `d1/main/newdemo.c` and `d2/main/newdemo.c` quick recording already starts
	a classic temp demo in `newdemo_start_recording()` before
	`maybe_start_input_demo_recording()`
- the quick-record branch in `newdemo_stop_recording()` currently builds the
	quick-record basename, clears quick-record state, deletes `DEMO_FILENAME`,
	and flushes only the input demo artifacts
- `android/app/src/main/java/com/dxxredux/app/InputDemoManager.kt` currently
	models a staged `.dximdemo` plus optional `.rngtrace.jsonl`
- `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt`
	already routes Save, Share, Add to Game, and Delete through that staged
	demo model

## Working Hypothesis

- the lowest-risk implementation is not a second independent recorder
- keep the classic temp `.dem` that is already being recorded inside the same
	`Newdemo_state == ND_STATE_RECORDING` session
- stage it under the same basename as the `.dximdemo`
- let the launcher treat it as an optional third sibling file
- because both recorders are armed and stopped from the same
	`newdemo_start_recording()` and `newdemo_stop_recording()` transaction,
	preserving the temp `.dem` in that central stop path should keep trigger
	events aligned across F5, Android quick-toggle requests, endlevel auto-stop,
	and window-close cleanup paths

## Proposed Artifact Layout

- staged quick-record files in `input_demo_recordings/new/`:
	- `<base>.dximdemo`
	- `<base>.dximdemo.rngtrace.jsonl`
	- `<base>.dem`
- installed files in the active set `demos/` directory:
	- `<safeName>.dximdemo`
	- `<safeName>.dximdemo.rngtrace.jsonl`
	- `<safeName>.dem`
- keep the existing trace naming so the RNG trace path stays derived from the
	input-demo file path with no shared-code churn

## Work Phases

### Phase 1: Native Staging Changes In D1 And D2

- add a small helper in each `newdemo.c` that resolves the staged `.dem`
	path for a quick-record basename in `INPUT_DEMO_NEW_DIR`
- in the quick-record branch of `newdemo_stop_recording()` preserve
	`DEMO_FILENAME` instead of deleting it:
	- finish classic recording as today via `newdemo_write_end()` and
		`PHYSFS_close(outfile)`
	- build one basename with `input_demo_build_quick_record_name()`
	- flush the input demo to `<base>.dximdemo`
	- rename or move `DEMO_FILENAME` to `<base>.dem`
	- if staging one artifact fails, log the failure and clean up any orphan
		sidecars for that basename
- extend the staged-recording trim path so when the oldest `.dximdemo` is
	evicted its sibling `.dem` is deleted too
- mirror the same change in `d1/main/newdemo.c` and `d2/main/newdemo.c`
	because the quick-record plumbing already exists in both trees

### Phase 2: Launcher Staged-Demo Model

- extend `StagedInputDemo` with `classicDemoFile: File?`
- extend discovery in `InputDemoManager.listStagedDemos()` to look for
	`<demo base>.dem` alongside the `.dximdemo`
- extend `exportFiles()`, `installToSet()`, `deleteStagedDemo()`, and
	`deleteAllStagedDemos()` so Save, Share, Add, and Delete operate on the
	full triplet
- install the classic sidecar as `<safeName>.dem` in the active set
	`demos/` directory
- keep the `.dximdemo` as the primary record for listing, sorting, and
	header parsing so existing UI behavior stays stable

### Phase 3: Advanced Tab UI

- update the section copy to describe the optional classic `.dem` sidecar in
	addition to the RNG trace
- in each row, show whether the classic demo sidecar is present, similar to
	the existing RNG trace line
- keep the button logic thin by routing everything through
	`InputDemoManager.exportFiles()` and the existing install/delete helpers
	instead of adding one-off UI-side file handling

### Phase 4: Alignment And Regression Checks

- confirm the first and last recorded gameplay frames line up by generating
	one short quick-record session and inspecting the `.dximdemo`, `.dem`, and
	shared basename together
- if the first-frame boundary is off by one, adjust the arming point inside
	`newdemo_start_recording()` rather than introducing a second toggle path
- add at least one launcher-side test for `InputDemoManager` that covers
	triplet discovery, export order, install, and delete cleanup
- run code quality and a D1 plus D2 validation build after the native edits
- run one short Android quick-record smoke test that proves the Advanced tab
	can save, share, add, and delete the triplet

## Cheap Check Before Coding

- verify that the current quick-record path already leaves a valid
	`tmpdemo.dem` when stopped manually
- if that temp file is malformed or missing expected content, pause and
	inspect the classic recorder stop path before changing the launcher model

## Risks And Open Questions

- the classic recorder writes through the standard `newdemo` frame path while
	the input demo captures controls in `GameProcessFrame()`
- they should still be session-aligned, but explicit first-frame and last-frame
	verification is required before treating the `.dem` as a forensic baseline
- installing a `.dem` into the active set may collide with an existing demo of
	the same basename, so the install path should deliberately reuse the current
	sanitized base-name flow
- trimming and delete flows must remove orphan `.dem` siblings or the staged
	directory will drift over time

## Status

- [completed] Identify the controlling native stop path and confirm the
	classic temp `.dem` is already recorded during quick recording
- [completed] Identify the launcher staged-demo model and the Advanced tab
	export and install surface

- [completed] Preserve the classic `.dem` sidecar in D1 and D2 quick-record
	stop flows

- [completed] Extend launcher staging, export, install, trim, and delete for
	the third file
- [completed] Validate the native and launcher code paths with Windows D1/D2
	host builds, the focused `InputDemoManager` unit test, and the scoped
	`android\run-code-quality.ps1 -Fix` pass
- [completed] Validate triplet creation and Advanced tab launcher handling on
	Android with emulator smoke tests:
	- `test_quick_record_classic_sidecar_stage.json5` records a fresh staged
		triplet and proves the Advanced page exposes the recorded demo row
	- `test_quick_record_classic_sidecar_install.json5` installs the staged
		triplet into the active set via launcher automation and verifies staged
		cleanup
	- launcher automation now supports native `meta_action` dispatch, exact
		button matching, a less ballistic Advanced-page scroll gesture, and an
		`install_staged_demo` helper for dialog-free smoke validation
