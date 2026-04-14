# Plan: LAN Games and Weapon Autoselect Layout Cleanup

## Scope

- Small launcher-only Compose cleanup for the LAN Games and Weapon Autoselect screens
- No d1 or d2 engine changes expected
- Keep changes limited to layout ownership, row order, spacing, and button visibility

## Code Study Summary

- [android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt](android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt#L823) currently owns the LAN Games title and Back button in `LanContent()`
- [android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt](android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt#L322) currently owns the local IP text, scan/host/join action buttons, hosted-lobby block, and discovered-lobbies heading
- [android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt](android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt#L151) currently owns the top app bar, portrait helper text, and the Descent 1 / Descent 2 selector row
- The LAN cleanup is not just row shuffling. The local-IP label must move or be hoisted so the LAN header can lay out title, IP, and Back consistently by orientation
- The missing Host LAN Game button when not scanning is caused by the current `!isHosting && isDiscovering` gate in `LanDiscoveryTab`, not by button sizing alone

## Requested End State

### 1. LAN Games, portrait

- Put `LAN Games`, `Your IP: ...`, and `Back` on the same top row
- Make the action row keep `Host LAN Game` as the leftmost button
- Keep the scan toggle on that same action row
- Keep `Join by IP` as the last button on that action row
- Do not hide `Host LAN Game` just because scanning is off
- Move `Local Network Games (N)` below `Scanning for LAN games...`

### 2. LAN Games, landscape

- Put `LAN Games` and `Back` on the first row
- Put `Your IP: ...` on the second row
- Put `Host LAN Game` first and the scan toggle second on the third row
- Move `Join by IP` to its own fourth row so it remains visible
- Move `Local Network Games (N)` below `Scanning for LAN games...`

### 3. Weapon Autoselect, portrait

- Reduce the visual gap between the title/helper area and the Descent 1 / Descent 2 selector row
- Keep the cleanup limited to spacing unless a very small title/helper regrouping is needed to match the intended look

## Implementation Plan

### Phase 1: Re-center LAN header ownership

- Move LAN header-related IP rendering out of `LanDiscoveryTab` and into `LanContent`, or pass a header composable/data model so one place controls the top rows
- Add a small helper for formatting `Your IP` vs `Your IPs` so portrait and landscape share the same text logic
- Keep `LanDiscoveryTab` focused on action rows, hosted-lobby content, discovery results, and diagnostics

### Phase 2: Rebuild LAN top rows by orientation

- In `LanContent`, add explicit portrait and landscape header layouts instead of relying on one generic column
- Portrait target:
  - Row 1: `LAN Games`, `Your IP: ...`, `Back`
- Landscape target:
  - Row 1: `LAN Games`, `Back`
  - Row 2: `Your IP: ...`
- Use compact typography for the IP text if needed so the portrait top row stays readable

### Phase 3: Fix LAN action-row composition and visibility

- In `LanDiscoveryTab`, split scan-state handling from host/join button visibility
- Keep `Host LAN Game` visible when not scanning
- Keep `Join by IP` in the action controls instead of letting it disappear with the scan state
- Portrait target:
  - Row 1: `Host LAN Game`, `Start Scanning` or `Stop Scanning`, `Join by IP`
- Landscape target:
  - Row 3: `Host LAN Game`, `Start Scanning` or `Stop Scanning`
  - Row 4: `Join by IP`
- Audit the `isHosting` branch so `Stop Hosting` still has a sensible place and does not undo the requested layout when a lobby is already hosted

### Phase 4: Reorder LAN discovery status and heading

- In the discovery block, place `Scanning for LAN games...` before `Local Network Games (N)`
- Keep the results list below the heading
- Prefer a stable status line while discovery is active so the heading order does not flip when lobbies appear

### Phase 5: Tighten Weapon Autoselect portrait spacing

- Reduce the vertical padding on the Descent 1 / Descent 2 selector row
- Reduce the extra spacing around the portrait helper text
- If the spacing still feels loose, compact the portrait title/helper area with a minimal regrouping, but do not touch the reorder-list logic or the landscape-specific layout unless needed

## Verification Plan

### Code quality

- Run `android\run-code-quality.ps1 -Fix`
- Run a targeted Android build such as `android\gradlew.bat :app:compileDebugKotlin` or `android\gradlew.bat assembleDebug`

### Validation result

- [x] `android\run-code-quality.ps1 -Fix`
- [x] `android\gradlew.bat :app:testDebugUnitTest :app:assembleDebug` with `JAVA_HOME=c:\local\jdk-21`
- [ ] Manual portrait and landscape validation still pending

### Manual validation

- Open LAN Games in portrait and verify the header row, action-row order, and discovered-list heading order
- Rotate to landscape and verify the four-row LAN layout and that `Join by IP` remains visible
- Verify the non-scanning state still shows `Host LAN Game` and `Join by IP`
- Open Weapon Autoselect in portrait and confirm the Descent 1 / Descent 2 buttons sit closer to the title/helper area without crowding

## Assumptions To Carry Into Implementation

- `Join by IP` should remain visible even when scanning is off, because the requested row order treats it as a stable control rather than a scan-dependent one
- `Scanning for LAN games...` should stay visible while discovery is active, even if lobbies are already listed, so `Local Network Games (N)` can reliably stay below it
- This cleanup stays inside the Android launcher Compose code and should not require native or protocol changes

## Status

- [x] Studied current layout ownership and button gating
- [x] Mapped the requested portrait and landscape row targets to existing Compose code
- [x] Implement the LAN header and action-row cleanup
- [x] Implement the Weapon Autoselect portrait spacing cleanup
- [ ] Run manual orientation checks