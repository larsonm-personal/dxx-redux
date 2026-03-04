- this project builds a cross-platform 3D video game "descent 2/1", in C and C++, with SDL and a few other dependencies
- the current goal is to create an android port

## principles
- this project, as of now, is largely about porting via build systems, rather than detailed source code changes. try to make as few source changes as possible
- any changes that are made should keep the existing windows/linux/mac builds intact, using #defines or separate files or similar
- any changes should be accompanied by a successful cmake build and test run

## building
- standard cmake commands (`mkdir build`, `cd build`, `cmake ..`, `cmake --build .`)
- see .github\workflows\package-msvc.yml for specific cmake commands
- sometimes on windows cl.exe becomes a zombie. kill it before starting cmake builds
- don't run builds until 100 errors (the msbuild default). stop around 10 (`/errorlimit:10`). later errors are often useless anyway

## automated testing
- use the introspection API (added specifically for AI tool debug access) to find out the current game state such as menu items, current level, ship position, etc. - do *not* rely on printing things to PNG and analyzing images. if you get stuck having to do that, extend the introspection API instead and re-run
- place automated test files into "temp" within this repo so that the file writes don't need to be approved
- when testing with the android emulator, the game will initially load to the main menu. hitting enter opens the "new game" menu and hitting enter again goes to the briefing
- the briefing screens can be skipped by hitting enter about 15 times. then the first level will load and the ship will be ready to move and show "score: 0" in the upper right. it's not possible to get stuck in the briefing screen after hitting enter enough times
- at this point the in-game menu can be accessed with "back"/escape

## introspection API
The game includes a debug introspection system that serializes current game state to JSON. This is the primary way to inspect what the game is doing — **do not screenshot and OCR**.

### quick start
```bash
# one-liner: request a dump and read it
adb shell am broadcast -a com.dxxredux.INTROSPECT && sleep 1 && adb shell run-as com.dxxredux.app cat files/introspect.json
```

Or use the helper script:
```bash
./temp/introspect.sh          # dump + pretty-print (requires python3 or jq)
./temp/introspect.sh raw      # dump without formatting
./temp/introspect.sh menu     # show only the menu section
./temp/introspect.sh player   # show only the player section
./temp/introspect.sh position # show only the position section
```

### how it works
1. **Broadcast** `com.dxxredux.INTROSPECT` — sets a volatile flag via JNI
2. On the **next game frame**, the engine writes `files/introspect.json` from the game thread (thread-safe)
3. **Read** the file with `adb shell run-as com.dxxredux.app cat files/introspect.json`

### JSON fields

| Field | When present | Description |
|---|---|---|
| `screen_mode` | always | `"menu"`, `"game"`, `"movie"`, `"editor"` |
| `game_mode` | always | bitmask: 0=normal, 4=network, 128=game_over |
| `quitting` | always | true if the quit sequence is in progress |
| `difficulty` | always | 0–4 (Trainee to Insane) |
| `current_level_num` | always | 1+ for normal levels, negative for secret, 0 = no level |
| `current_level_name` | always | e.g. `"Lunar Outpost"` |
| `in_game` | always | true when Game_wind is front and screen_mode is game |
| `window_count` | always | number of windows in the stack |
| `game_window_is_front` | always | true if the game window is the topmost window |
| `menu.type` | when a menu is front | `"newmenu"`, `"listbox"`, or `"unknown_window"` |
| `menu.title` | when a menu is front | menu title string |
| `menu.subtitle` | newmenu only | subtitle string |
| `menu.selected_index` | when a menu is front | currently highlighted item index |
| `menu.items[]` | when a menu is front | array of `{index, type, text, value, selected}` |
| `player.*` | when level loaded | callsign, energy, shields, score, lives, level, weapons, ammo, keys, flags |
| `position.*` | when level loaded | x, y, z (floats), segment number, shields |

### extending the API
The introspection code lives in `d2/introspect/game_introspect.c`. To add new fields:
1. Add serialization in `game_introspect_get_state()` using the `jb_*` helper functions
2. Include the relevant game header (most are already included)
3. Rebuild — `INTROSPECT_ON` is defined for debug Android builds only
4. The JSON output updates automatically, no JNI or Kotlin changes needed

Key engine globals available for serialization:
- `Players[Player_num]` — player struct (energy, shields, score, weapons, keys, etc.)
- `ConsoleObject` — player object (position, orientation, segment)
- `Game_wind` — game window pointer (NULL when not in-game)
- `Game_mode` — game mode bitmask
- `Screen_mode` — SCREEN_MENU/SCREEN_GAME/SCREEN_MOVIE/SCREEN_EDITOR
- `Current_level_num`, `Current_level_name` — level info
- `window_get_front()` — topmost window; compare its callback to `newmenu_handler`/`listbox_handler` to identify type
