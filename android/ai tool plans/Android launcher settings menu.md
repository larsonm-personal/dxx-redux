# Android launcher settings menu plan

Plan only. No implementation, branch, commit, push, or local checkout changes.

## Recommendation

Give the launcher one Settings gear in its header. Keep the home screen focused on **Start game**, **Multiplayer**, and **Import files**, plus only the context needed to use those actions. Put a searchable Settings home behind the gear, with a short list of destinations that reuse existing pages.

Ship the search field at the top of Settings home first. Do not put a permanent search field over every child page, particularly the full-screen touch editor. Child pages get a clear title and Back. A later search icon can reopen the same search surface if actual use justifies it.

This is a launcher navigation and presentation change. It should not become an engine-settings rewrite, preference-format migration, content-manager replacement, or new navigation-framework project.

## Verified baseline and independence

- Inspected current `cmake` at `49e92bbcdf92b31f77c705f65587d3dfe81f299c`, including the actual Compose UI builders, page implementations, persistence calls, input dispatch, and existing launcher tests.
- Compared repository trees against open stack tip [#461](https://github.com/larsonm-personal/dxx-redux/pull/461), `bf388fc1064ea3ce70bfe88daceff3348d359608`. All files under `android/app/src/main/java` and `android/app/src/test/java` are identical. Independent open [#454](https://github.com/larsonm-personal/dxx-redux/pull/454) changes only nightly tooling workflow/test/plan files.
- There is no observed dependency on the current gameplay stack. If implemented later, use then-current `cmake` in a separate worktree/branch after checking for newly arrived launcher changes. Do not base this on or change the local agent's stack.
- Research used GitHub reads and isolated source snapshots. No local working tree was inspected or modified.
- This is a source-verified UI plan, not a claim of visual/device testing. No APK was run.

## 1. Home screen

Header: app name and one gear with accessible label **Settings**.

Main body:

1. **Start game** area retaining the existing Descent 1 / Descent 2 choices and their launch-readiness logic. The smallest change is to keep two clearly grouped launch buttons; do not add a mandatory selection dialog just to achieve a single button.
2. **Multiplayer**, still a first-class action.
3. **Import files**, retaining the existing file/folder chooser, archive/disc/GOG import flows, and progress/results.

Contextual exceptions:

- When a game is running, retain **Return to game**.
- Keep a compact **Continue** offer when a valid save exists. **Saved games** belongs to the Start game flow, not buried in Advanced.
- Show missing-files guidance when launch is blocked. Put file-format help inside the import chooser or empty state rather than permanently under the button.
- Keep active import progress, errors, recovery warnings, and actionable asset problems visible. Routine stale-file cleanup detail can live on Files & content.
- Keep demo installation as an empty-state/Import files option, respecting the existing offer preference, instead of permanent marketing cards on a configured home.
- Move routine About/build/store/release links to Settings > About. Preserve actionable update behavior rather than silently dropping it.
- Remove the controller panel, touch switch, orientation buttons, file inventories, Mods/Levels, Music settings link, and separate settings shortcuts.
- If the active file-set name is useful, show it as one compact status line near Start game, not another full control panel. Its management action lives under Settings > Files & content.

The home should fit comfortably without a second settings column. Use a compact centered layout on wide screens, not the current left files/right controls arrangement.

Source: [home, import and launch actions](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L4534-L6121).

## 2. Settings home and complete placement

Top: Back, **Settings**, then a full-width **Search settings** field. Empty query shows these destinations:

- **Controller**: connected-device status, current Test/raw-input view, **Controller mapping** and its existing slots, presets, import/export, response and assignment editors. Reuse ControllerConfigPage. Do not show “not detected” as an alarming home warning; mapping should remain reachable without a connected pad.
- **Touch controls**: touch-overlay enabled switch and **Edit touch layout**, opening the existing full-screen TouchEditorPage. Keep slots, presets, add-control tools, global settings and gyro within that editor. Search can expose those destinations without moving their setting ownership.
- **Game preferences**: existing EnginePreferencesPage, including presets, launcher behavior, pilot-backed HUD/auto-level/homing/counts/boss/map settings, new-server Coop QoL, intro/progress, rewind, headlight, screen saver, Guidebot and visual helpers. Do not split its mixed transactional settings into separate pages in the initial change.
- **Weapon autoselect**: existing AutoselectEditorPage, preserving D1/D2 selection, ordering, reset and Save behavior.
- **Graphics & display**: existing GraphicsSettingsPage plus the moved in-game Landscape/Portrait choice. Retain resolution, FOV, filters, color depth, MSAA/AF, corner insets, original visuals and graphics debugging.
- **Music**: existing MusicPickerPage with MIDI, CD audio and audio-file controls, source management and previews. Move any unique file/source information from MusicInfoSection here or into Files & content; avoid retaining a second “Music Settings” doorway.
- **Files & content**: active file-set selection/management; D1/D2 required/optional file inventories and details; optional downloads; existing Mods/Levels content, enable/order controls, recommended mods, demos, package documents, metadata and previews; import/location status and cleanup detail. Preserve the existing content-management components instead of recreating them. Import files remains a main-home action; a contextual import shortcut here can reuse it.
- **Advanced**: existing AdvancedSettingsPage: config import/export, online metadata permission, route/chromaprint work, logging, crash reports, newly recorded demos, storage/SAF inspection, imported-files location and reset/restart operations. Keep destructive actions separated and their existing confirmations intact.
- **About**: app version/build/architecture/renderer, store link and GitHub releases where applicable, plus existing update information.

Controller and Touch controls need only thin submenu wrappers around existing components. Do not add an unnecessary “Controls” category above both.

First release does not need to break Advanced into many new pages. Search should jump to its existing sections. If Advanced later becomes unwieldy, split sections in a separate focused change.

Sources: [current scattered controls](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L6132-L6376), [content sections](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/SetupSections.kt#L190-L651), [Advanced](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt#L558-L985).

## 3. Navigation model

Replace the eight independent `show...Page` booleans with one small typed destination/back-stack model. A local sealed route or enum plus route arguments is sufficient; the build does not currently use Navigation Compose, and this task does not justify introducing it.

Suggested destinations: Home, SettingsHome, Controller, ControllerMapping, TouchControls, TouchEditor, GamePreferences, Autoselect, Graphics, Music, FilesContent, Advanced, About, Multiplayer.

Route arguments should be limited to navigation concerns: section ID, passive tab/target when supported, return origin, and entry identity. Never use display labels as route keys.

- Regular path: Home > Settings > page > child editor/dialog.
- Search path: Home > Settings(search results) > target page/section.
- Back from a search result restores query, list scroll and focused result, not an unrelated category or Home.
- Back from Settings returns to Home and restores focus to the gear.
- Reopening Settings on a new visit starts at its top level. Retain a query within a visit, not as a new permanent user preference.
- System Back, gesture Back, visible Back and controller B use the same page exit policy.
- Dialogs, IME and editor-local modes get first chance to consume Back.
- Do not render inactive pages invisibly to keep them alive: several scan files, acquire resources or poll controllers. Preserve only their necessary state.
- Keep activity-result launchers and ongoing import ownership above page switching. Moving import UI must not cancel a running import or lose a picker result. Existing SetupScreen work already survives child-page early returns; preserve that boundary.
- LAN invitation handling must still open Multiplayer and respect the existing leave-game/lobby confirmation. Never save/discard an editor draft silently when an external join interrupts it.

Source: [existing page state, LAN routing and focus restoration](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L4343-L4534).

## 4. Search behavior

Use a small in-process catalog of real destinations/sections, not runtime scraping of Compose text or a new search service.

Each entry needs a stable ID, visible title, short category path, a few aliases, destination and optional section target. Derive menu rows and page-level search entries from the same destination definitions. Share section IDs between pages and search; avoid parallel unrelated lists of route strings.

Initial coverage:

- All Settings destinations and visible static setting names.
- Named existing subsections such as controller response/raw inputs, touch gyro, MSAA, field of view, rewind amount, Guidebot, config export, storage, crash reports and reset controls.
- Do not index arbitrary mission filenames, log contents, individual dynamic controller bindings or every touch object. That would turn settings search into a different product.

Matching:

- Trim/normalize case and whitespace; treat punctuation sensibly.
- Rank exact title, title prefix/token matches, then aliases/category matches.
- Include aliases users actually use: “FOV/field of view”, “antialiasing/MSAA”, “gamepad/controller”, “gyro/tilt”, “redbook/CD audio”, “bindings/mapping”.
- Show title plus path, e.g. **Anti-aliasing (MSAA)** / Graphics & display.
- Clear button, empty-state message and a stable deterministic result order.
- Query filtering must be local and immediate, with no file scan, network call or preference write.

Selection opens the target and brings the relevant control/section into view, with a brief highlight and appropriate controller/accessibility focus. Use stable section IDs and measured bring-into-view/lazy-list positions, not hard-coded pixel offsets. Apply the target once after the destination is ready; recomposition must not continually pull the user back.

Critical rule: searching or opening a result must never apply a setting, start playback, launch a download, export files, or execute a reset.

**Music exception:** current MIDI/CD Audio/Audio Files chips immediately call `saveMusicMode`, including pilot-file writes. They are not passive tabs. For the first release, a CD-audio result can open Music and highlight the CD Audio choice without selecting it. If direct section opening is required, separate the viewed section from the selected playback mode and make changing playback explicit. Do not invoke the chip callback as a deep link.

Likewise, destructive results open the relevant Advanced section; the existing action and confirmation remain separate user actions. Disabled/game-specific controls should show why unavailable, not vanish from search unexpectedly.

Source: [music writes and mode selector](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt#L194-L310).

## 5. Persistence, scope and safe exits

Navigation does not change setting scope. Keep existing SharedPreferences keys, config paths, slot repositories, native bridges and write transactions. This reorganization needs no data migration, new compatibility reader or migration fixture.

Make existing scope visible where relevant:

- Device/app-wide preferences, such as orientation and touch-overlay visibility.
- Active file-set scoped content and audio sources. Recreate/refresh those views when the set changes.
- Active controller/touch slot.
- Pilot-backed settings. EnginePreferencesPage explicitly writes to all D1/D2 pilot files, not merely the last selected launcher game.
- Weapon autoselect has separate D1/D2 orderings and can save changes for both.
- New-server Coop QoL is a default for future servers, not a live-session change.
- Graphics settings may use protected config writes/confirmation behavior. Reuse it.

Do not add a global Save button. Existing pages have different save contracts:

- Graphics and many toggles save immediately.
- Controller mapping has Save and Cancel.
- Game preferences mixes immediate settings with pending pilot-backed changes and presets.
- Autoselect has pending ordering changes and Save.
- Touch editor has explicit Save and saves through Close Editor, but its current system-Back callback does not use that same save path.

Resolve exit consistency deliberately, not as an accidental side effect of the router:

- Immediate-save pages simply leave.
- Keep Controller Cancel as discard and Controller Save as save-then-leave. If dirty, general Back offers Save / Discard / Keep editing.
- For Game preferences and Autoselect, dirty Back offers Save / Discard / Keep editing. Save leaves only after success; failure stays with a useful error.
- For Touch editor, preserve the existing explicit Close Editor save behavior and route system/controller Back through the same intended exit policy. Prefer an explicit dirty-state prompt for Back rather than silently changing it to autosave.
- Do not mistake existing mismatched pilot values for edits made during the visit. Autoselect's current `hasChanges` includes mismatch counts, so track actual draft edits separately for an exit prompt.
- Explain that Discard affects unsaved changes only on mixed-save pages. Do not roll back already-saved toggles.
- Preserve draft state across configuration changes and temporary external activities. Save only small navigation/query/scroll state with a saver. Do not serialize file managers, native handles or full import jobs into a navigation Bundle.
- If process recreation cannot safely restore an editor draft, recover to a valid parent with saved data, never partially apply it. No promise of cross-process unsaved-draft persistence is needed for this task.

Sources: [pilot scope/save behavior](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt#L81-L250), [pilot scope UI](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt#L432-L604), [touch lifecycle and save](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt#L261-L529), [controller save/cancel](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L469-L481).

## 6. Touch, controller and accessibility

- Use at least 48 dp targets for the new gear, menu rows, search clear and Back controls. Do not propagate the old 24–32 dp settings-button density into the new menu.
- Keep title/search outside the scrolling Settings list; handle IME insets, large fonts, portrait/landscape and TV safely.
- Do not open the keyboard automatically on entry. On a controller/TV, focus the first menu item; Up reaches search. Search opens the IME only when activated.
- On Home, seed controller focus to Return/Continue/Start when available, otherwise Import. On touch-only phones retain the existing no-forced-controller-focus behavior.
- Restore focus to the exact invoking menu row or search result on return.
- Reuse existing TV focus borders, navigation repeat and A-to-activate/B-to-Back dispatch. The controller-mapping page's raw-input capture flags must only be active while that editor is visible.
- Preserve the touch editor's orientation/system-bar lifecycle and nested picker focus.
- Search accessibility announces result count without excessive per-keystroke chatter; labels and breadcrumbs are readable, and highlighted targets are not identified by color alone.

Source: [gamepad dispatch](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L2590-L2669).

## 7. Implementation sequence

### Phase 1: navigation and move, behavior preserved

- Add a lightweight destination model and SettingsHomePage.
- Introduce a reusable settings header/menu-row style only where it removes actual duplication.
- Replace SetupScreen page booleans with one router.
- Extract home content, Files & content, and the thin Controller/Touch menus from existing code; keep import/launch state ownership intact.
- Move the scattered controls and About link. Keep existing child page bodies and persistence calls.
- Change initial and return-focus targets.
- Update existing launcher automation to enter pages through Settings.

Deliverable: the cleaner launcher and working menu/back stack, with no dropped actions.

### Phase 2: search and safe targeted entry

- Add the catalog and search result UI.
- Add optional section-target arguments only to pages that need them.
- Implement one-shot scroll/highlight/focus targeting.
- Preserve query/result state on return.
- Apply the passive Music fallback or explicitly separate passive browsing from mode changes.
- Finish dirty-exit consistency at affected editors, using page-owned save functions and truthful dirty tracking.

Deliverable: settings are discoverable by name and search does not mutate anything.

### Phase 3: focused device validation and polish

- Validate portrait, landscape and controller/TV flows.
- Check import chooser/activity return, a save failure, LAN invitation interruption, orientation changes and reopening settings.
- Check TalkBack/large text, empty-state onboarding and no-data search.
- Remove obsolete home-only wiring and update user-facing screenshots/docs that actually show the old navigation.

These can be sequential commits or reviewable draft PRs in one independent feature line. Do not require a giant cross-platform or gameplay refactor. Do not ship a temporary second settings entry forever.

## 8. Proportionate verification

Extend existing coverage rather than cloning fixtures.

1. **Existing launcher D-pad integration:** update `android/tests/test_launcher_dpad.ps1`. It currently expects Controller Mapping on Home and initial focus there. Make it prove Home > gear > Settings > Controller > mapping, Back restoration, another destination, search entry/result navigation, and no duplicate home settings buttons.
2. **Small catalog/navigation unit coverage:** one table-driven suite for real destination validity, meaningful aliases/ranking, blank/no-result query, search return state and passive result selection. A second tiny exit-policy suite is justified only if exit decisions are extracted as real production logic. Do not unit-test every label or trivial getter.
3. **Reuse existing regression tests:** LauncherFocusPolicyTest, SetupLaunchReadinessTest, existing config/slot and graphics-confirmation tests. Only extend tests whose behavior actually changes. SettingsChildOverlayControllerTest is mostly in-game overlay coverage; do not create redundant launcher tests there merely because of its name.
4. **Device integration:** cover one immediate-save preference, one deferred-save editor's Save/Discard/failure, touch-editor Back/orientation, Music search leaving `music_mode` unchanged, import picker cancellation/result and existing content readiness. Reuse current small fixtures and native-backed test infrastructure.
5. **No unrelated corpus expansion:** no new missions, gameplay replay baselines, duplicated native fixtures, migration matrices or full campaign qualification for moving launcher navigation.

Run scoped mixed-language formatting, relevant JVM tests, an Android compile/package check and the updated launcher integration serially on an emulator. Inspect actual screenshots for compactness and clipping, but use existing introspection for behavior assertions.

When extending automation, register top-level tests in the existing coverage catalog and run its two required validators. Prefer extending the existing top-level launcher test so no new runner/catalog entry is needed.

Source: [existing launcher integration](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/android/tests/test_launcher_dpad.ps1#L228-L310), [repository implementation/test rules](https://github.com/larsonm-personal/dxx-redux/blob/49e92bbcdf92b31f77c705f65587d3dfe81f299c/.github/copilot-instructions.md).

## Acceptance criteria

- A configured launcher's persistent interactive surface is Start game/Return, Multiplayer, Import files, and one Settings gear, with only justified Continue/status exceptions.
- Every existing settings/content/admin action remains reachable and has one canonical home.
- Settings home has a top search field and a short, comprehensible destination list.
- Search finds real named settings and opens the right page/section without changing values.
- Back returns one level and restores query/scroll/focus; editor changes are not silently lost.
- Launch readiness, imports, file-set scoping, pilot write scope, controller capture and touch editor lifecycle retain their behavior.
- No change to engine/save/content formats and no dependency on the separate gameplay stack.
- The updated existing integration runs successfully; visual/device limits are reported honestly.
