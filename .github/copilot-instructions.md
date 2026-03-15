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
- mimic the style of the existing code. by and large, this is a "C in C++" codebase without classes or templates. it's ok to use things like std::array<> or simple RAII classes within the android/ dir, but don't get crazy. I *do* want you to use C++ patterns (within the android/ dir) that can avoid things like null pointer access and array bounds problems, the base game is highly susceptible to these things and it's sometimes a problem
- add simple, high-level integration tests to catch regressions and document high level functionality. it's not necessary to add tests to cover every little function unless the function has tricky edge cases or is very complex by itself
  - make every effort to include at least one integration test or an extension of an existing one with each major set of changes, and include it as a final phase of work. by-hand verification is ok, but I'd like to build up the high level test suite
  - don't just write the test, include in this phase a run of the actual as-written test and fixes until it passes
  - add these tests for any code centralized in the android/ directory as the code is added. add test runner scripts or helpers so they're easy to re-run after code changes
- d1/ and d2/ have a huge amount of duplicated code. this means that our hooks and other changes also need to be duplicated in many places. it is a mistake to edit *only one of* d1/ or d2/ in any set of changes, typically they both need the same hooks. that said, I want to try to share as much code as possible between the two, *when that code is new*. don't de-duplciate in a way that makes the d1/ or d2/ change set larger which would make upstreaming harder
- new code should be as free of compiler warnings as possible. -werror isn't enabled, but do a 2nd pass to remove warnings when building to check
- new code should have formatting linters run on it: `android\run-code-quality.ps1 --fix`

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

## running integration tests from an AI tool session
### emulator
- the emulator should already be running. check with `adb devices`. if it's not listed, start it with `android\run_emulator.sh` or the emulator GUI -- don't try to start it from powershell
- if `adb devices` shows "offline" or "unauthorized", kill and restart the emulator

### running a test
```powershell
# clear logcat first, then pipe output to a file to avoid terminal buffer issues
cd d:\local\dxx-redux
adb logcat -c; .\android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2 2>&1 | Out-File temp\test_output.txt -Encoding utf8; Write-Output "EXIT: $LASTEXITCODE"
# then read the tail of the output file
Get-Content temp\test_output.txt | Select-Object -Last 30
```

### key pitfalls
- **always pipe to a file** (`Out-File`). do not rely on reading terminal output directly -- the terminal buffer accumulates garbage from adb/logcat across runs and you'll see stale output or nothing at all
- **always `adb logcat -c` before the test**. stale logcat lines from previous runs will confuse the test runner's pattern matching
- **check exit code via `$LASTEXITCODE`** after the piped command, not via terminal scrollback
- **don't run multiple test commands in parallel** -- they share the emulator and will interfere
- **app resume after HOME**: the test runner uses `monkey -p com.dxxredux.app -c android.intent.category.LAUNCHER 1` followed by a BACK keypress to resume the app. `am start -n` does not work because MainActivity has no launch mode and no intent filter -- it creates a new instance instead of resuming. the monkey command brings the task to foreground but lands on SetupActivity; BACK dismisses it to reveal the running game's MainActivity
- **`ogl_start_frame`/`ogl_end_frame` are 3D-only**: menus render via `event_process()` -> `EVENT_WINDOW_DRAW` -> `newmenu_draw()` -> `gr_flip()` without ever calling `ogl_start_frame`/`ogl_end_frame`. Any per-frame OpenGL state (e.g. `glViewport` offset) that should affect menus must be applied in `gr_flip()`, not in the 3D frame helpers

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
./android/introspect.sh console  # show recent con_printf output (ring buffer, last 50 lines)
./android/introspect.sh setup    # dump SetupActivity state (files, readiness, downloads)
./android/introspect.sh setup raw # raw SetupActivity JSON
./android/introspect.sh autolog    # dump automation step log (automation_log.jsonl)
./android/introspect.sh autoresult # dump automation result (automation_result.json)
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
| `console` | always | `{next_seq, lines: [{seq, text}]}` -- last 50 con_printf lines from the engine ring buffer |

### automation result files (debug builds only)
The automation system writes durable files alongside logcat. These survive logcat buffer overflow, emulator restarts, and timing races. The test runner (`run_test.ps1`) uses these as the primary pass/fail source.

- `files/automation_result.json` -- written on PASS/FAIL: `{result, steps_completed, total_steps, reason?, elapsed_ms}`
- `files/automation_log.jsonl` -- one JSON line per step event: `{seq, step, total, action, status, elapsed_ms, detail}`
- Read directly: `adb shell run-as com.dxxredux.app cat files/automation_result.json`
- Or via helper: `./android/introspect.sh autoresult` / `./android/introspect.sh autolog`
- On test timeout/failure, the runner automatically dumps `automation_log.jsonl` and `gamelog.txt` for diagnostics

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
