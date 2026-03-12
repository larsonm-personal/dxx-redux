- this project builds a cross-platform 3D video game "descent 2/1", in C and C++, with SDL and a few other dependencies
- the current goal is to create an android port

## principles
- this project, as of now, is largely about porting via build systems, rather than detailed source code changes. try to make as few source changes (within d1/ or d2/) as possible
  - exceptions:
  - introspection or game control API extensions in order to support automated testing (this project has already started)
  - additional touch features such as adding "OK" to some menus
  - android-specific dependency changes or extensions to work with the android filesystem and ecompass the loader's function as mod manager, etc.
  - maintaining a single source of truth. for example, the details of the player file format should stay in playsave.c and *not* be brought into the kotlin code to support editing player files from the launcher. the launcher code should call into playsave.c functions for every detailed bit of access it needs
  - it's ok to have shared constants that exist in both java and C code in order to make interfaces cleaner. document wherever there are duplicated constants/arrays/structs so both copies can be maintained
- any changes that are made should keep the existing windows/linux/mac builds intact, using #defines or separate files or similar
- any changes should be accompanied by a successful cmake build and test run
- before the first release (as in, currently), do *not* code for backwards compatibility within the launcher or android-specific code. do maintain compatibility in the d1/d2 dirs because it will help upstreaming. virtually every build will be a full apk rebuild with a data reset

# dependencies
- whenever adding dependencies, or build tools, pin them to a specific version using a version string or git commit tag, etc. - see the existing methods for doing that
- keep dependencies lightweight and cross platform. a few things are rebuilt as single file/AI-slop reimplementations because the formats were simple-ish and it was easier than bringing in boost, or whatever

## new features
### launcher
- there *is* a need to build a launcher that encompasses game file management and configuration editing. there will be significant new code for that and it will be mostly kotlin
- this launcher will need to have interfaces to set configurations within the game. in general, the launcher will be operating by changing the game asset files and config files *before* the game launches, and then letting the game read those files in its existing ways, which aren't modified from the base redux game
- when editing config files, attempt to centralize the logic for how the files are laid out within the existing C code,but add clean interfaces so the kotlin can call into helper functions to make edits
- for these interfaces, it's ok to add shared constants in order to make the interfaces clean and minimize line count
- the goal is for the C code to be the source of truth (along with shared constants) so config logic isn't spread unnecessarily between kotlin and C

### touch interfaces and overlays
- there will be new touch interfaces and overlays added. some of these will expose info in an overlay from the base game. interfaces between kotlin and java should be clean and simple. there will be some places where C helper functions are added to expose things the overlay can use, similar to the introspection API, although the full introspection API is probably the wrong answer for this because as it grows it will become more and more inefficient
- the goal for the game is to have the full game be operable through a pure screen touch interface.  in effect, means playing the full game with only a mouse (no keyboard). there are places where keyboard presses are currently required. we're slowly adding ways to skip these with touches

### code quality and testing
- don't use emoji anywhere, not in code, not in comments, not in markdown files
- don't use emdashes anywhere, but especially not in scripts
- keep to printable ascii wherever possible
- always create a plan as step 1 of any block of work. plan files go here: android\ai tool plans\
- attempt to minimize line count to some extent. don't take this to an extreme, but avoid abstractions that are just wrappers, duplicated code, and other verbose things
- add simple, high-level integration tests to catch regressions and document high level functionality. it's not necessary to add tests to cover every little function unless the function has tricky edge cases or is very complex by itself
- add these tests for any code centralized in the android/ directory as the code is added. add test runner scripts or helpers so they're easy to re-run after code changes
- d1/ and d2/ have a huge amount of duplicated code. this means that our hooks and other changes also need to be duplicated in many places. it is a mistake to *only* edit d1/ or d2/ in any set of changes. that said, I want to try to share as much code as possible between the two, *when that code is new*. don't de-duplciate in a way that makes the d1/ or d2/ change set larger which would make upstreaming harder

## building
- standard cmake commands (`mkdir build`, `cd build`, `cmake ..`, `cmake --build .`)
- see .github\workflows\package-msvc.yml for specific cmake commands
- sometimes on windows cl.exe becomes a zombie. kill it before starting cmake builds
- don't run builds until 100 errors (the msbuild default). stop around 10 (`/errorlimit:10`). later errors are often useless anyway

## automated testing
- use the introspection API (added specifically for AI tool debug access) to find out the current game state such as menu items, current level, ship position, etc. - do *not* rely on printing things to PNG and analyzing images. if you get stuck having to do that, extend the introspection API instead and re-run
- use the automation api to drive the game into a desired state for testing. when using the automation api, save new automation scripts to android/game_scripts/*.json5 so they can be maintained and committed to git. eventually they'll be used for regression testing
- place automated test files into "temp" within this repo so that the file writes don't need to be approved
- when testing with the android emulator, the game will initially load to the main menu. there are helper script bits to choose a player, mission, level and skip briefings. see the regression test .json5 files and their attendant scripts
- note that the D1 level set (the base game, not an expansion) is referred to as "first strike" and the d2 level set (the base game, not an expansion) is "counterstrike!". these are used in mission selection during automated tests

## introspection API
The game includes a debug introspection system that serializes current game state to JSON. This is the primary way to inspect what the game is doing — **do not screenshot and OCR**.

### quick start
```bash
# one-liner: request a dump and read it
adb shell am broadcast -a com.dxxredux.INTROSPECT && sleep 1 && adb shell run-as com.dxxredux.app cat files/introspect.json
```

Or use the helper script:
```bash
./android/introspect.sh          # dump + pretty-print (requires python3 or jq)
./android/introspect.sh raw      # dump without formatting
./android/introspect.sh menu     # show only the menu section
./android/introspect.sh player   # show only the player section
./android/introspect.sh position # show only the position section
./android/introspect.sh setup    # dump SetupActivity state (files, readiness, downloads)
./android/introspect.sh setup raw # raw SetupActivity JSON
```

### setup-screen introspection
The SetupActivity has its own introspection system for inspecting file readiness and download state **without screenshots**.

```bash
# one-liner
adb shell am broadcast -a com.dxxredux.SETUP_INTROSPECT && sleep 1 && adb shell run-as com.dxxredux.app cat files/setup_introspect.json
```

#### JSON fields (setup_introspect.json)

| Field | Description |
|---|---|
| `screen` | always `"setup"` |
| `can_launch` | true if either D2 or D1 required files are present |
| `files_on_disk` | sorted array of all filenames in the app's files dir |
| `d2.ready` | true if all D2 required files are present |
| `d2.files[]` | array of `{filename, required, found, found_as?, alternatives?, description}` |
| `d1.ready` | true if all D1 required files are present |
| `d1.files[]` | same format as d2.files, plus `download_url` for downloadable optional files |
| `downloads` | object mapping filename → progress (`"42%"`, `"complete"`, `"error"`) — only present during active downloads |

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
