# Fan mission fly-out hardening

## Intended behavior

Invalid optional scenery or tunnel geometry skips the animation and preserves normal level completion. Valid secret-level fly-outs, including The Grand Finale, play before the existing secret return or campaign ending. Mission files remain unchanged

## Work

1. Share bounded tunnel and scenery validation across both engines and metadata
2. Handle missing/corrupt terrain without fatal errors; select the exterior reached by the actual route
3. Preserve D2 secret-level completion after a valid animation
4. Exercise malformed fixtures and valid fly-outs through the real engines, including secret exits; build Windows and Android, run scoped formatting and catalog checks

## Evidence

- Runtime tunnel walks lack index validation and cycle bounds
- Scenery parsing relies on an assertion for field count and unchecked numeric conversion
- Terrain loading calls Error on missing/corrupt height maps and only asserts dimension limits
- D2 secret exit triggers call ExitSecretLevel directly even when complete fly-out geometry is present

## Implementation

- Shared END/TXB parser checks field count, numeric ranges, trailing garbage, and bounded lines before modifying either engine's presentation state
- Shared tunnel traversal checks segment/side bounds, reciprocal links, and a finite walk limit. Runtime actor movement also rejects invalid connections
- Scenery is positioned at the opening reached by the validated route, rather than asserting that the first exterior found during loading is the right one
- Optional IFF bitmap validation checks chunk lengths, dimensions, palettes, compressed rows, and height-map coordinates before the legacy decoder runs. Terrain failures skip the animation instead of calling Error
- D2 secret exit triggers can start authored fly-outs. Completion preserves the original secret return/campaign-ending decision. Revisitable secrets save before cinematic movement, and that checkpoint is not overwritten afterward. Multiplayer retains its existing completion coordinator
- Metadata uses the same parser, route checks, and bitmap validation. Result cache schema advances to v7
- The D2 IFF decoder now consumes the complete BODY chunk as its existing comment intended, including a final mask row. A valid masked height-map fixture guards against rejecting supported artwork while handling corrupt files

## Validation

- Windows D1/D2 builds passed; Android arm64 diagnostic build and eight metadata result-cache unit tests passed
- Existing seven-case fly-out metadata integration passed
- New actual-engine regression passed malformed scenery/numbers/assets, terrain dimensions/coordinates, corrupt compressed data, solid/out-of-range/disconnected/cyclic tunnels, valid fly-outs, and multiple exterior openings in both engines
- The Grand Finale completed every fly-out phase through its actual exit trigger, both returning to surviving Level 26 with its pre-animation secret checkpoint intact and ending the campaign when the parent level was destroyed
- Samsung Level 2 integration passed all 47 steps, including briefing overlap, fresh start, save, and restore
- Automation catalog and master catalog passed: 190 standalone PowerShell tests, 288 top-level entries, 364 support scripts
- Final masked-bitmap regression and complete fly-out suite passed in both engines, including both Grand Finale outcomes. Evidence: `android/temp/flyout-safety/run_d83e88eaf2214d3f9994409690c3d36c/`
- Final Windows D1/D2 and Android arm64 builds passed. Scoped formatting and `git diff --check` passed. Final APK installed successfully on Samsung RFCY703C48J in the isolated NSD Test app

No mission assets or Play-signed installation were modified. Phone validation uses com.dxxredux.app.nsdtest

Repeat with `android/tests/test_flyout_safety.ps1` (or `-NoBuild` after building both engines), `android/tests/test_mission_metadata_flyouts.ps1 -NoBuild`, and the existing Samsung Level 2 runner
