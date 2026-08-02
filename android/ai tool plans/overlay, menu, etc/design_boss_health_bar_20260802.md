# Boss Health Bar Design

## Goal

Design an optional center-top boss health display for D1 and D2. The display should use the native HUD-message visual language, render `boss ` followed by a solid green and red health bar, remain visible from the boss's first hostile activation through its destruction, support cooperative games, and be enabled by default from Game Preferences directly below Robot and hostage counts.

## Work Plan

### Android-owned module refactor

- [x] Move boss state, validation, health scaling, layout, debug data, and rendering into `android/app/src/main/cpp/shared/boss_hud.c/.h`
- [x] Reduce D1 and D2 HUD changes to message-stack integration calls and special-row ordering
- [x] Point AI, collision, multiplayer, and level-transition hooks at the shared Android-owned header
- [x] Add the shared module to both D1 and D2 targets while preserving all desktop builds
- [x] Re-run formatting, native/Windows builds, Android tests/APK assembly, and D1/D2 boss gameplay scenarios

- [x] Trace the general native HUD-message queue, its producers, and its rendering in D1 and D2
- [x] Trace boss identification, combat activation, damage, death, and multiplayer synchronization
- [x] Trace the Android Game Preferences data path from Compose through JNI/native runtime state
- [x] Define display behavior, layout, color, lifecycle, multiplayer semantics, and edge cases
- [x] Pin the requested boss row to the top and count it inside the four-row visible-message budget
- [x] Define concrete file-level implementation phases and verification coverage
- [x] Record the completed design and remaining product decisions in this document
- [x] Implement pilot storage, launcher UI, import/export, and JNI plumbing
- [x] Implement D1 and D2 activation, lifecycle, message-stack layout, and solid bar rendering
- [x] Add structured introspection, focused fill tests, and a maintained D1/D2 gameplay test
- [x] Verify native host tests, Windows builds, Android tests, APK assembly, and D1/D2 gameplay assertions

## Scope

This tranche is implemented in D1, D2, and the Android launcher. It changes native HUD rendering and pilot-backed preferences without changing multiplayer packet formats or binary save formats.

## Implementation Status

- Complete: `bosshealthbar=1` pilot default and read/write behavior in D1 and D2
- Complete: launcher switch directly below Robot and hostage counts, default on, with save/reset/import/export support
- Complete: Android-owned `boss_hud.c/.h` centralizes state, health scaling, layout, rendering, and debug data for both games
- Complete: activation from local shots, replicated shots, proximity drops, melee contact, effective damage, and teleports
- Complete: persistent signature-validated boss tracking through the death roll and reset at level transitions
- Complete: `boss ` native text followed by contiguous solid green and red rectangles
- Complete: boss row pinned at visible slot 0 with timed-message capacity reduced from four to three
- Complete: D2 guided-missile text and cooperative restore status laid out below the boss row
- Complete: introspection reports lifecycle, slot, geometry, fill widths, and queue capacity
- Complete: `test_boss_health_bar.json5` passes on D1 level 27 and D2 level 24
- Not required for implementation sign-off: a dedicated two-emulator boss encounter was not added; existing replicated fire and damage paths are used without a protocol change

## Recommended Player Experience

### Launcher setting

Add a pilot-backed switch immediately below `Robot and hostage counts` on the Game Preferences page:

```text
[ on ] Boss health bar
       Shows a boss health bar after the boss engages a player
```

- Label: `Boss health bar`
- Values: on or off
- Default: on
- Reset to Defaults value: on
- Scope: all D1 and D2 pilot files, matching the adjacent robot and hostage setting
- The switch participates in the page's existing Save button and unsaved-change state

The setting belongs in the `[cockpit]` section of each pilot's `.plx` file:

```ini
bosshealthbar=1
```

Missing values mean on. This is important for existing pilots and for new pilots created before the launcher has written the setting.

### In-game appearance

Use the existing `GAME_FONT` and normal center-top HUD-message green for the label. Honor the requested lowercase text exactly:

```text
boss <solid green remaining region><solid red lost region>
```

- `boss ` is text, including the visual separation before the bar
- The bar itself contains no text glyphs, dashes, brackets, outline, or segment dividers
- The remaining region is a solid green rectangle on the left using `BM_XRGB(0, 28, 0)`
- The lost region is a solid red rectangle on the right using `BM_XRGB(28, 0, 0)`
- The green and red rectangles are contiguous and always fill the same total bar width
- At full health the entire bar is green
- At zero health the entire bar is red
- Use at least one green pixel while shields are greater than zero, so a nearly defeated living boss does not look dead
- Use a 64-bit intermediate when converting fixed-point shields to pixel width

Match the scale of the existing native observer shield bars:

```text
bar_width  = max(1, int(100 * FNTScaleX) - 2)
bar_height = max(1, int(4 * FNTScaleY))
```

Recommended fill calculation after clamping shields to `0..maximum`:

```text
if shields <= 0: green_width = 0
else:            green_width = max(1, floor(shields * bar_width / maximum))
red_width = bar_width - green_width
```

This gives the bar continuous pixel-level resolution without adding an art asset. Draw the two regions only when their widths are positive.

### Interaction with the general HUD-message queue

The existing center-top message system is not Guide-Bot-specific. `HUD_messages[]` stores up to 20 timed messages and displays the newest four. `HUD_init_message()` and `HUD_init_message_literal()` feed this shared queue using the `HM_DEFAULT`, `HM_MULTI`, `HM_REDUNDANT`, `HM_MAYDUPL`, and `HM_KILLFEED` classifications.

Current producers include powerup and hostage events, shield and ammunition warnings, marker text, multiplayer joins and rejoins, kill feed entries, mode changes, cheats, screenshots, control-center notices, and D2 Guide-Bot task text. The boss bar must interact with this whole queue in exactly the same way, regardless of which subsystem produced a row.

The boss bar is a persistent row in the general HUD-message layout, but it is not physically inserted into `HUD_messages[]` and is not refreshed through `HUD_init_message()`. Instead, it is pinned into visible slot 0 whenever the preference is enabled and the encounter is active.

The center-top stack is:

1. Boss health row, when enabled and active
2. Existing special rows, such as the D2 guided-missile label or cooperative restore status line
3. The newest timed rows selected from the shared `HUD_messages[]` queue

The boss row always owns the first row of the message stack, using `FSPACY(1)` normally or `Observer_message_y_start` while observing. No queued message or special center-top status may render above it. All other rows advance downward from its Y coordinate by `LINE_SPACING`.

The four-row timed-message display budget includes the boss row:

```text
boss hidden: 4 newest HUD_messages[] rows are eligible to draw
boss shown:  boss occupies slot 0, then 3 newest HUD_messages[] rows draw
```

The boss does not consume one of the 20 stored queue entries. A fourth timed message can remain stored and continue aging while it is outside the three currently visible queued slots. When the boss disappears, the normal four-message selection resumes.

Special status rows are not members of `HUD_messages[]` and historically do not consume its four timed-message slots. Preserve that behavior, but assign them below the boss before laying out timed messages. In D2, move `Guided Missile View` out of its fixed `FSPACY(1)` draw call and into the shared prepared row layout, preserving its red font color. Likewise, stop drawing the cooperative restore status at a hard-coded first-row Y. This prevents either special row from overlapping or appearing above the boss.

The boss row's padded composite rectangle is added first to `HUD_message_rects`, so score, progress counters, names, and other collision-aware elements treat it exactly like the top message line. The later special and queued rows add their rectangles in actual visual order.

The existing rectangle storage must be large enough for four budgeted rows total, plus the cooperative restore-status and D2 guided-missile special rows. `HUD_MAX_NUM_DISP + 2` is sufficient because the boss replaces one timed row rather than adding a fifth budgeted row. Prefer named special-row capacity and bounds checks so future persistent rows cannot overrun it.

The row should follow the existing HUD-message visibility rules for cockpit, rear view, guided view, observer mode, and immersion mode. It must not create a separate Android overlay.

## Boss Encounter State

Keep display state separate from the general message queue:

```c
typedef struct boss_health_hud_state {
    int active_objnum;
    int active_signature;
    fix maximum_shields;
    int activated;
} boss_health_hud_state;
```

The object signature protects against an object slot being reused after a boss is removed. Do not use global `Boss_dying` alone to identify the displayed object because custom levels may contain more than one boss.

### State transitions

| State | Event | Result |
|---|---|---|
| Hidden | Level starts | Clear tracked object and activation state |
| Hidden | Valid boss becomes hostile | Capture object number, signature, and maximum shields; show row |
| Hidden | Boss takes effective damage | Treat as engagement and show row |
| Visible | Same boss fires, sounds, teleports, or takes damage again | Idempotent; keep current row |
| Visible | Boss shields change | Recalculate green and red pixel widths directly from live object shields |
| Visible | Boss enters death roll | Keep row visible at zero health during the death roll |
| Visible | Boss object is removed, changes type, loses its boss flag, or its signature changes | Hide row |
| Visible | A second boss activates while the tracked boss lives | Keep the first boss stable; remember the second as a candidate if practical |
| Hidden after first boss | Another already engaged boss remains | Select that candidate, otherwise wait for its next hostile event |

Keeping the zero bar through the existing boss death roll gives the lethal hit visible confirmation. The row disappears when the explosion removes or repurposes the boss object, which is the clearest definition of `vanquished` in both games.

### Activation signals

Expose one idempotent shared entry point to each game:

```c
void boss_hud_note_active(int objnum);
void boss_hud_reset(void);
```

`boss_hud_note_active()` validates object range, type, boss flag, signature, and positive maximum health before changing display state. Call it at these existing hostile events:

1. After a boss creates a normal weapon in `ai_fire_laser_at_player()`
2. When `multi_do_robot_fire()` receives a replicated shot from a boss
3. When a boss performs a contact attack against a player
4. When `apply_damage_to_robot()` applies positive, effective damage to a boss
5. When a boss teleports and plays its encounter sound, locally and in the replicated boss-action handler
6. Optionally, at the existing sight or attack sound gate when a boss first has full player visibility

The first two calls are the required baseline. Together they mean that when a boss shoots at any player in a cooperative game, the controlling peer activates locally and every other peer activates while processing the existing `MULTI_ROBOT_FIRE` packet. The damage and teleport hooks cover melee bosses, player-initiated engagements, and bosses whose first obvious action is not a normal laser.

The sight-sound hook improves immediacy in single player. In cooperative play it may occur on one peer slightly before the first replicated hostile action, but the first shot or teleport makes every peer converge. Do not add a new multiplayer packet for this first implementation.

### Maximum health

Do not use health after damage as the denominator because the player may damage a boss before it fires. Capture activation before subtracting effective damage and calculate the same full-health value used by each game's `copy_defaults_to_robot()`.

D1 uses robot strength directly:

```text
maximum = Robot_info[boss->id].strength
```

D2 applies its existing boss difficulty scaling:

```text
maximum = Robot_info[boss->id].strength / (NDL + 3) * (Difficulty_level + 4)
if Difficulty_level == 0: maximum /= 2
```

Preserve D2's existing operation order so fixed-point truncation matches the live object initialization exactly. In either game, if a custom or restored boss currently has shields above the calculated value, raise the captured maximum to the current shield value rather than drawing more than 100 percent.

On save restore, if no explicit activation state exists but a live boss has less than its calculated maximum, treat it as active. This recovers the useful case without changing the classic save-game format. A full-health boss that was activated, saved, and restored will remain hidden only until its next hostile event. That narrow case is preferable to adding a HUD-only field to the D1 and D2 binary save layouts.

## Rendering Design

Implement the row in the Android-owned `boss_hud.c/.h` module. Keep `HUD_prepare_message_frame()` and `HUD_render_message_frame()` in both games responsible only for reserving the first row, inserting its collision rectangle, applying the reduced message budget, and calling the shared renderer.

Preparation should:

- Validate the tracked object and clear stale state
- Measure the literal `boss ` using `GAME_FONT`
- Determine the composite row width as label width plus the fixed scaled bar width
- Center that composite width as one unit, rather than centering the label and bar separately
- Start a single row cursor at `FSPACY(1)` or `Observer_message_y_start`
- If the boss is visible, assign it the current cursor, add its rectangle first, and advance the cursor
- Lay out the guided-missile and restore-status special rows below the boss, advancing the same cursor
- Set queued capacity to `HUD_MAX_NUM_DISP - 1` when the boss is visible, otherwise `HUD_MAX_NUM_DISP`
- Select only the newest `queued capacity` entries and lay them out below all assigned special rows
- Save every assigned Y coordinate for the render pass instead of recomputing from hard-coded origins
- Increase rectangle capacity for the two possible special rows while keeping the boss inside the four-row budget

Use the same dynamic queued capacity in the message timer and duplicate-refresh calculations that currently assume `HUD_MAX_NUM_DISP`. This keeps expiry ordering internally consistent with the three queued rows that are actually visible while the boss is pinned.

Rendering should:

- Draw `boss ` at the composite row's left X using normal message green
- Place the bar immediately after the measured label width
- Vertically center the solid bar within the measured text height
- Draw the green region first with `gr_setcolor(BM_XRGB(0, 28, 0))`
- Draw the red region second with `gr_setcolor(BM_XRGB(28, 0, 0))`
- Use `gr_rect()` rather than `gr_urect()`; its software path clips, and its OpenGL path uses the standard filled-rectangle backend already used by other gauges
- Account for `gr_rect()` having inclusive right and bottom coordinates: green is `x..x + green_width - 1`, and red is `x + green_width..x + bar_width - 1`
- Skip either rectangle when its width is zero, preventing invalid or overlapping calls at full and zero health
- Restore normal HUD font color before drawing subsequent messages

Define the `boss ` label constant in a header beside the HUD declarations, in the same style as other source-level text constants, rather than burying the user-visible string in the renderer.

The bar should not:

- Consume a stored `HUD_messages[]` entry
- Allow four timed rows to draw in addition to the boss; the boss is one of the four budgeted visible rows
- Reset or refresh queued-message expiration every frame merely to keep the boss visible
- Write a console message every frame
- Produce demo HUD-message events every frame
- Depend on Android canvas or Compose rendering
- Render the health portion from font glyphs
- Use screenshots or OCR for verification

## Preference Data Path

Use the same path as `ShowRobotHostageCounts` and extend it as one atomic engine preference group.

### Native player configuration

In both games:

- Add `ubyte ShowBossHealthBar` beside `ShowRobotHostageCounts` in `player_config` in `playsave.h`
- Initialize it to `1` in the player defaults function
- Read `BOSSHEALTHBAR` from the `[cockpit]` section in `playsave.c`
- Write `bosshealthbar=%i` beside `robothostagecounts=%i`

This adds only a text `.plx` key and does not change the binary `.plr` layout.

### Shared launcher-side file helpers

In `playsave_android_shared.h/.c`:

- Replace the one-field count helper with a cockpit HUD preference read/write helper that handles both `robothostagecounts=` and `bosshealthbar=` in one section pass and one transactional update
- Default `show_counts` to off and `show_boss_health` to on before parsing
- Preserve unknown lines and existing comments through the current text-update mechanism

Keeping both keys in a single update avoids rewriting the same `.plx` file twice from one Save click.

### JNI and Kotlin model

Extend `NativePilotPreferences.EnginePrefs` and the D1/D2 `IntArray` contract with `showBossHealthBar`. Document the array indexes next to the decoder because the C++ and Kotlin copies must remain synchronized.

Update:

- `android_pilot_prefs.cpp` read state, write context, visitors, logging, JNI signatures, and result array
- `NativePilotPreferences.kt` external signatures, decoder, per-game writer, and all-games writer
- `EnginePreferencesPage.kt` current state, saved state, change detection, loading, Save, Reset to Defaults, and the new switch directly below Robot and hostage counts
- `SetupActivity.kt` launcher automation command, with a default of true
- `ConfigImportExport.kt` engine preference export and import, with missing imported `show_boss_health_bar` treated as true

Because the option is pilot-backed, do not also create a duplicate `SharedPreferences` key. The native `.plx` setting remains the source of truth.

## Cooperative Multiplayer Behavior

The initial implementation should use existing simulation and replication:

- The boss controller calls the activation helper when it fires
- `MULTI_ROBOT_FIRE` activates the mapped local boss on every receiver
- Replicated player weapons and the existing robot damage path update each peer's boss shields
- Existing boss explosion packets and object removal hide the row

No packet format change is needed for activation. Health itself is currently not carried in robot position packets, so two-emulator testing must compare the reported current and maximum shields after several hits from each player. If measurable divergence appears, handle that as a follow-up authoritative boss-health synchronization design instead of sending unordered absolute health values from every peer, which could make a boss appear to heal when packets cross.

Competitive multiplayer with robots may also show the bar if a boss exists and becomes hostile. This is harmless and follows the native boss state. The preference remains local to each player, so one player may disable the bar without affecting peers.

## Demo, Save, and Level Lifecycle

- Reset the HUD boss state from explicit new-level and level-transition paths, not from `HUD_clear_messages()`. Cockpit toggles call `HUD_clear_messages()` mid-level and must not dismiss the boss bar.
- Classic and input demo playback should derive activation from the same replayed boss actions and damage. The bar is presentation state and must not consume simulation RNG or alter robot AI state.
- Do not serialize the HUD row as a normal demo message.
- Do not change classic save-game structures. Recover an engaged damaged boss by scanning after restore, then rely on the next hostile event for a previously engaged full-health boss.
- Disabling the preference hides the row but does not discard encounter state. This distinction keeps behavior correct if a future in-game preference editor changes the value while a level is running.

## Introspection and Automated Verification

Extend the existing `hud_layout` introspection object with:

```json
{
  "boss_health": {
    "enabled": true,
    "active": true,
    "drawn": true,
    "visible_slot": 0,
    "objnum": 123,
    "signature": 456,
    "shields_raw": 6553600,
    "maximum_shields_raw": 13107200,
    "label_width": 24,
    "bar_width": 98,
    "bar_height": 4,
    "green_width": 49,
    "red_width": 49,
    "bar_rect": {"x": 123, "y": 3, "w": 98, "h": 4},
    "rect": {"x": 98, "y": 0, "w": 124, "h": 8}
  },
  "queued_message_capacity": 3
}
```

This permits exact assertions without image analysis.

### Focused unit coverage

- Bar pixel-width conversion at full, first damage, half, one positive shield, zero, negative, and above maximum
- Inclusive rectangle coordinates at full, partial, and zero health, proving there is no gap, overlap, or out-of-range endpoint
- Invalid object number, non-robot, non-boss, and stale-signature validation
- Missing `.plx` key defaults to on
- Read and write both cockpit HUD keys without losing unrelated lines or handmade comments
- Kotlin decoder defaults boss health to on for a short JNI result
- Engine Preferences dirty-state, reset default, export, and import behavior
- HUD rectangle intersection includes the boss row and temporary rows start one line lower
- With four live timed messages, an active boss is slot 0 and only the newest three timed messages are marked drawn
- With the boss hidden or disabled, the newest four timed messages are eligible to draw
- Restore status and D2 guided-missile text are assigned below the boss and cannot retain a hard-coded top-row Y

### Single-player integration coverage

Add a maintained `android/game_scripts/test_boss_health_bar.json5` for both games, using a boss level or a compact checked-in test level:

1. Start with the preference on and assert `active=false`
2. Trigger a real boss hostile action and wait for `active=true`
3. Queue four distinguishable HUD messages and assert the boss is visible slot 0, only the newest three messages draw, and all rectangles have increasing Y coordinates
4. Apply nonlethal damage through the normal `apply_damage_to_robot()` path and assert green decreases and red increases
5. Lethally damage the boss, assert zero green during the death roll, then assert the row disappears after object removal
6. Repeat with the preference off and assert encounter state may be active but `drawn=false`
7. Toggle cockpit modes during the encounter and assert the row remains active

### Cooperative integration coverage

Use the existing two-emulator harness:

1. Let the boss first acquire and shoot player two
2. Assert both peers report the same active boss signature mapping and fill widths
3. Damage the boss from each peer and compare shields and fill widths after packet settling
4. Kill the boss and assert both rows disappear
5. Repeat with one peer's local preference off to verify preferences are not network-coupled

### Build and quality gates for implementation

- Run scoped `android/run-code-quality.ps1 -Fix` over all touched D1, D2, C/C++, and Kotlin files
- Run the focused native unit targets, including `test_hud_layout`
- Run Android unit tests and `:app:assembleDebug` with JDK 21
- Run `run-windows-build.ps1` for D1 and D2
- Run the single-player boss automation for both games
- Run the two-emulator cooperative scenario once the single-player test is stable

## File-Level Implementation Plan

### Phase 1: Preference storage and launcher UI

- `d1/main/playsave.h`, `d2/main/playsave.h`
- `d1/main/playsave.c`, `d2/main/playsave.c`
- `android/app/src/main/cpp/shared/playsave_android_shared.h/.c`
- `android/app/src/main/cpp/android_pilot_prefs.cpp`
- `android/app/src/main/java/com/dxxredux/app/NativePilotPreferences.kt`
- `android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`

### Phase 2: Boss lifecycle and rendering

- `android/app/src/main/cpp/shared/boss_hud.c/.h` as the single source of truth for boss state, scaling, geometry, colors, rendering, and debug data
- Narrow integration calls in `d1/main/hud.c` and `d2/main/hud.c`
- `d2/main/gamerend.c` to move the guided-missile label into the prepared row ordering
- `d1/main/ai.c`
- `d2/main/ai.c`, `d2/main/ai2.c`
- `d1/main/collide.c`, `d2/main/collide.c`
- `d1/main/multibot.c`, `d2/main/multibot.c`
- Explicit level-start call sites in `d1/main/gameseq.c` and `d2/main/gameseq.c`

Keep D1 and D2 changes mechanically parallel where their upstream structures match. Do not introduce Android-only rendering branches for the native bar.

### Phase 3: Introspection and tests

- `android/app/src/main/cpp/shared/game_introspect.cpp`
- Shared fill math in `android/app/src/main/cpp/shared/boss_hud.h`, exercised by `android/tests/test_hud_layout.c`
- `android/tests/test_hud_layout.c` or a focused boss HUD test target
- Relevant Kotlin preference and import/export tests
- `android/game_scripts/test_boss_health_bar.json5`
- A two-emulator cooperative runner or extension to the current multiplayer test harness

## Acceptance Criteria

- New and existing pilots have the setting on unless they explicitly turn it off
- The launcher switch is directly below Robot and hostage counts and saves to all D1 and D2 pilots
- No bar is present merely because a dormant boss exists in the level
- The first replicated boss shot at any co-op player makes the bar visible to all peers
- Whenever visible, the boss is the top center-stack row and occupies slot 0 of the four-row display budget
- An active boss leaves room for only the newest three timed `HUD_messages[]` rows; hiding or disabling it restores all four
- Guided-missile and restore-status special rows never appear above or overlap the boss
- The bar stays visible through cloaking, teleporting, temporary loss of sight, and ordinary popup expiration
- `boss ` uses the same native font, position system, and green as center-top HUD messages
- The bar after `boss ` consists of contiguous solid green and red rectangles, with no text glyphs or segment lines
- Remaining health is green on the left and lost health is red on the right
- Every class of queued HUD message stacks without overlap or lost message slots
- The bar reaches zero for the death roll and disappears when the boss object is gone
- Disabling the preference hides only the row and does not change simulation or network behavior
- D1, D2, Android, Windows, Linux, and macOS builds remain source-compatible

## Product Decisions Resolved by This Design

- Use solid, continuously filled green and red regions rather than text characters
- Treat the boss row as part of the general `HUD_messages[]` layout; Guide-Bot text is only one producer of that queue
- Pin the boss to visible slot 0 and count it against the four-row display budget without inserting it into the stored queue
- Keep the label lowercase as requested: `boss `
- Treat a replicated first shot as the definitive cooperative activation event
- Also accept effective damage, contact attack, teleport, and encounter sound as similar activation signals
- Keep zero health visible during the death roll and hide after object removal
- Store the option in pilot `.plx` data, not duplicate launcher preferences
- Avoid a multiplayer protocol or classic save-format change unless integration testing proves one necessary
