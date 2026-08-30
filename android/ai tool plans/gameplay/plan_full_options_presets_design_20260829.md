# Full options preset system design

## Goal

- Design a preset system that keeps the current reset-to-defaults behavior and adds an Original Game preset
- Inventory existing D1, D2, launcher, touch, gameplay, HUD, graphics, audio, control, and cheat-related options
- Identify remaster behavior that needs a new switch for faithful original-game behavior

## Plan

- [x] Read repository instructions and establish read-only design scope
- [x] Trace option storage, defaults, menus, and launcher-facing configuration interfaces
- [x] Classify options as preset-owned, intentionally preserved, game-specific, or missing
- [x] Propose preset semantics, UI behavior, persistence, and implementation boundaries
- [x] Record the resulting option matrix and mark this design pass complete

## Scope notes

- This pass does not change runtime behavior
- Existing unrelated worktree changes are preserved
- D1 and D2 differences will remain explicit where the original games differ

## Recommended product model

Use three displayed states:

1. `Current Defaults`
   - The curated Android/Redux defaults already used today
   - This replaces or expands the current `Reset to Defaults` action without changing its values
2. `Original Game`
   - A one-shot, atomic bulk assignment of all fidelity-owned settings below
   - D1 and D2 receive separate values where their retail behavior differs
3. `Custom`
   - Derived whenever the current values do not exactly match either built-in preset

Do not make a preset an enforcement mode. Applying it writes concrete settings. A later manual change moves the displayed state to `Custom` and is not silently reverted at launch.

The UI should show a short summary and an expandable change list before applying `Original Game`. It should also state that the operation affects both games and all existing pilots. If per-pilot editing is added later, scope should become an explicit choice rather than being inferred from the selected launcher tab.

Applying a preset must be one transaction across:

- D1 and D2 `descent.cfg`
- all D1 and D2 pilot `.plr` and `.plx` files
- Android `dxx_prefs`
- D1 and D2 multiplayer host defaults
- a stored default template used when a new pilot is created later

The last item fixes a current limitation: launcher pilot preferences cannot be written when no pilot file exists.

## Original Game preset matrix

### Gameplay and pilot settings

| Setting | Original Game value | Reason |
| --- | --- | --- |
| Cockpit/HUD | Full cockpit, normal HUD, classic reticle and size | Stock initial presentation |
| Auto-level | On | Retail D1 and D2 player default |
| Original homing | On in single-player and coop | Existing retail-semantics option |
| Headlight on when picked up (D2) | On | Retail D2 behavior; set `headlight_off_by_default=false` |
| Missile view (D2) | On | Retail option default |
| Guided missile in main display (D2) | Off | Stock initial value |
| Escort hotkeys (D2) | On | Stock Guide-Bot controls remain available |
| Classic no-ammo autoselect | On | Uses the existing classic selection routine |
| No autoselect while firing | Off | Do not suppress stock pickup autoselection |
| Delayed autoselect after firing | Off | Redux extension |
| Cycle only autoselect weapons | Off | Redux extension |
| Weapon ordering | Retail D1/D2 ordering | Game-specific native source of truth |
| Ammo warnings | Off | Redux HUD helper |
| Shield warnings | Off | Redux HUD helper |
| Persistent debris | Off | Redux visual/gameplay extension |
| Free-flight automap | Off | Stock automap controls; warn that touch translation becomes unavailable |
| Sticky rear view | Off | Redux input option |
| D1 D2-style proximity bomb gauge | Off | Not present in retail D1 |
| No redundant pickup messages | Off | Preserve stock messages |
| Robot and hostage counts | Off | Remaster HUD helper |
| Boss health bar | Off | Remaster HUD helper |
| Map cheat controls accessible | Off | Removes secret reveal, objectives, reactor, and matcen controls |
| Auto demo recording and recording indicator | Off | Not part of normal retail play |
| Custom ship, missile, and team colors | Off/default slot colors | Preserve stock multiplayer colors |
| Transparency effects | Off | Existing original-visual value |
| Colored dynamic light | Off | Existing original-visual value |
| Disable cockpit | Off | Keep stock cockpit modes available |

Values not affecting fidelity, such as lifetime stats, level progress, difficulty choice, callsign, macros, and save data, must be preserved.

### Graphics and presentation

| Setting | Original Game value | Reason |
| --- | --- | --- |
| Main view FOV | Base (`0`) | Existing stock projection path |
| Texture filtering | Nearest (`0`) | Pixel-preserving sampling |
| Menu filtering | Off | Pixel-preserving menus and briefings |
| HUD filtering | Off | Current default is on, but this is not the stock appearance |
| D2 movie filter | Off | Stock pixels |
| MSAA | Off | Remaster feature |
| Anisotropic filtering | Off | Remaster feature |
| Color depth | 16-bit compatibility path | Closest existing option; see missing palette option below |
| Classic depth ordering | On | Existing option explicitly intended for classic ordering |
| FPS indicator and Video Info overlay | Off/hidden | Not stock HUD content |
| External replacement textures | Off | New switch needed; base game art remains usable at its shipped resolution |
| Internal render mode | Classic 4:3 | New option needed; use D1 320x200 and D2 640x480 where supported, integer-scaled and letterboxed |
| Intro skip | Off | Play retail intro sequence |

Preserve display orientation, rounded-corner safe insets, physical output resolution, brightness/gamma, VSync, and window/surface behavior. They are device or accessibility concerns. A classic internal framebuffer can be scaled into the safe physical output without forcing the device itself to 320x200.

Movie subtitles should also be preserved rather than forcibly disabled. They are an accessibility option even though the initial retail value was off.

### Android helpers and extra actions

| Setting | Original Game value |
| --- | --- |
| Guidebot helper line | Off |
| Nearest-player line | Off |
| Rewind support and rewind binding visibility | Off |
| Touch cheat catalog | Hidden; typed retail cheat codes still work |
| Automap secret reveal | Hidden and cleared |
| Automap objective overlay | Hidden and set to Off |
| Reactor countdown pause/extend | Hidden and cleared |
| Matcen one-round/pause controls | Hidden and reset to Default |
| Mid-level difficulty action | Hidden; normal new-game difficulty selection remains |
| Coop level restart helper | Hidden |
| Multiplayer warp helper | Hidden |
| Guide-Bot abdication helper | Hidden when classic coop rules do not create a shared Guide-Bot |

Keep touch controls, controller bindings, menu zoom, tap-to-continue, touch `OK`, touch `Exit`, launcher save/load access, and the admin route back to the launcher. Those are platform input affordances, not gameplay assistance. Do not reset a player's touch or controller layout when applying the preset.

### Multiplayer host defaults

| Setting | Original Game value |
| --- | --- |
| Coop QoL (`guidebot, arrows, warp`) | Off |
| Per-player duplicated energy/shields | Off |
| Client rewind requests | Off |
| 100 percent death spew | Off |
| Player spew never expires | Off |
| Original homing | On for coop |
| Non-coop FOV | Base |

Host migration, reconnect support, relay/matchmaking transport, autosave integrity, anti-abuse checks, and deterministic simulation fixes stay enabled. They make the port reliable but do not grant an in-game advantage under normal connected play.

### Music

Do not change sound and music volume, stereo reversal, or accessibility-related audio controls. For source selection:

- D1 prefers its shipped MIDI soundtrack
- D2 prefers Redbook/CD audio when valid disc tracks are installed, otherwise shipped MIDI
- Mission-provided or imported replacement soundtracks are not preferred in Original Game mode

This should be listed in the confirmation because changing a user's selected soundtrack is conspicuous. An alternative is to leave music source outside the preset while still disabling `prefer mission soundtrack`; the product decision should be explicit.

## New settings needed for a credible preset

1. `Extended cheat and admin helpers`
   - Controls cheat catalog visibility plus the automap secret, objective, reactor, matcen, live difficulty, warp, and restart helpers
   - Typed engine cheat codes remain unchanged
2. `Guide-Bot behavior: Original / Redux`
   - Original uses retail D2 destination selection and path behavior
   - It disables the metadata-driven exit/unexplored route planner, automatic semantic replanning, stall recovery, new route target modes, and related route notifications
   - Coop-only ownership/network replication remains separately controlled by Coop QoL
3. `External replacement textures`
   - When off, ignore installed `.dxa`, PNG, KTX2, and similar replacement art but keep base-game low/high-resolution assets
4. `Classic internal framebuffer`
   - A fixed 4:3 game canvas with integer scaling and letterboxing into the Android surface
   - Avoid coupling this to device output resolution or touch coordinate space
5. `Classic gameplay extensions`
   - A native, pilot-backed group for classic autoselect, warning HUDs, D1 bomb gauge, free-flight automap, and other fields the launcher cannot currently patch
6. `Show remaster action catalog`
   - Controls visibility of rewind and other non-stock meta actions in touch/controller pickers and the More menu
   - Existing user bindings may remain stored but should report unavailable while disabled

A future optional `Palette-accurate rendering` switch could reproduce 8-bit palette quantization and palette effects more closely. `ColorDepth=0` is only a 16-bit approximation, so the preset should not claim pixel-identical DOS output until such a renderer exists.

## Explicit exclusions

`Original Game` should mean original rules and presentation, not a wholesale historical-bug mode. It should not disable:

- crash, bounds, file-format, security, and networking fixes
- deterministic RNG and save-state completeness fixes
- Android lifecycle, storage, audio backend, and touch interoperability changes
- broad Redux compatibility for modern missions and mods
- user accessibility settings such as subtitles and display-safe insets
- callsign, progression, saves, stats, volumes, control mappings, or imported content

If exact executable compatibility is desired later, that should be a separately named `DOS Compatibility` mode with its own tested behavioral contract. It should not be silently folded into this preset.

## Implementation shape

- Put preset definitions and validation in shared native code, with separate D1 and D2 tables where fields differ
- Keep C as the source of truth for pilot/config formats; Kotlin should request a preset ID and display a native-produced diff/result
- Apply all file changes through one staged transaction and restore every touched file if any write fails
- Store a small preset schema version and desired future-pilot preset in `dxx_prefs`; do not store a duplicate Kotlin copy of every native field
- Detect `Current Defaults`, `Original Game`, or `Custom` from actual values after every load
- Settings that need restart should be marked in the diff; Android-only live preferences can refresh immediately
- Refuse to apply while a game process is actively writing config/pilot files, or coordinate a clean save-and-reload boundary

## Verification expected for implementation

- Native D1 and D2 tests for every preset-owned field and the game-specific differences
- Transaction rollback tests with a forced failure in each storage layer
- Tests for existing pilots plus future pilot creation with no pilot files present at apply time
- Launcher tests for preset detection, confirmation diff, Custom transition, and failed apply messaging
- Admin tray/action catalog tests proving all helper and cheat paths are absent or unavailable
- Single-player D1/D2 automation checking cockpit, HUD helpers, homing policy, automap policy, rewind, and intro behavior
- Two-player coop automation checking QoL, duplicated pickups, spew rules, lines, warp, Guide-Bot policy, and client rewind
- Render configuration tests for base FOV, nearest filtering, classic depth, stock assets, and classic framebuffer
