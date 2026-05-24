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
- long term, the android launcher will be extracted to a library to support multiple 90s era games (using the same controls picker, CD extraction, and so on). try to plan for clean separation between the game code and launcher code, as well as details like the multiplayer packet format having a game type tag so the server can support multiple games

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

### matchmaking server
- this is a freshly-built matchmaking and relay server in rust. it's in `/server/`
- build with `cargo build` and test with `cargo test`
- any changes should include a run of `rust_lint.sh` (it does a build+test call at the end to verify)
- tests should complete within 60s, make sure to wrap them with a way to fail if it takes longer
- server notes: the code will be open-source, so any possible flaw might be exploited. guard against malicious clients, dos, attempts to deadlock or crash the server, etc.  Clean up stale connections and clients such that the server doesn't leak system resources (for example, socket handles)

### code quality and testing
- don't use emoji anywhere, not in code, not in comments, not in markdown files
- don't use emdashes anywhere, but especially not in scripts
- in printlines/comments, don't end self-contained sentences, especially those that already end with a newline, with a period. for example, `Write-Host "No lobbies to join"` instead of `Write-Host "No lobbies to join.`
- keep to printable ascii wherever possible
- avoid utf8-with-bom files; use plain ascii or utf8 without bom
- always create a plan as step 1 of any block of work. plan files go here: android\ai tool plans\
  - when a given tranche of work is done, always mark the finished parts in the plan file so the next phase can start at the right place
  - attempt to categorize the plan file into an existing subdir, or propose a new subdir
- attempt to minimize line count to some extent. don't take this to an extreme, but avoid abstractions that are just wrappers, duplicated code, and other verbose things
- mimic the style of the existing code. by and large, this is a "C in C++" codebase without classes or templates. it's ok to use things like std::array<> or simple RAII classes within the android/ dir, but don't get crazy. I *do* want you to use C++ patterns (within the android/ dir) that can avoid things like null pointer access and array bounds problems, the base game is highly susceptible to these things and it's sometimes a problem
- add simple, high-level integration tests to catch regressions and document high level functionality. it's not necessary to add tests to cover every little function unless the function has tricky edge cases or is very complex by itself
  - make every effort to include at least one integration test or an extension of an existing one with each major set of changes, and include it as a final phase of work. by-hand verification is ok, but I'd like to build up the high level test suite
  - don't just write the test, include in this phase a run of the actual as-written test and fixes until it passes
  - add these tests for any code centralized in the android/ directory as the code is added. add test runner scripts or helpers so they're easy to re-run after code changes
- d1/ and d2/ have a huge amount of duplicated code. this means that our hooks and other changes also need to be duplicated in many places. it is a mistake to edit *only one of* d1/ or d2/ in any set of changes, typically they both need the same hooks. that said, I want to try to share as much code as possible between the two, *when that code is new*. don't de-duplciate in a way that makes the d1/ or d2/ change set larger which would make upstreaming harder
- for the few edits needed in d1/ or d2/, generally they're going to be #idef __ANDROID__. if they're something like a test debug line, or additional log line, make sure to mark it as being related to android port work
- for d1/ and d2/ edits, take extra care to match existing style in detail. for example, many string constants are in headers. don't add a new string constant in a source file right next to an existing header-included string constant - instead, add a header constant in the same style
- new code should be as free of compiler warnings as possible. -werror isn't enabled, but do a 2nd pass to remove warnings when building to check
- new code should have formatting linters run on it: `android\run-code-quality.ps1 --fix`
- `android\run-code-quality.ps1 --fix` is file-mutating and can take several minutes. do not treat it as done until the process has fully exited in its terminal
- before starting another cleanup or validation pass after any timeout, interrupted agent run, or "file is newer" popup, list stale formatter tasks with `android\stop-stale-formatters.ps1` and kill them with `android\stop-stale-formatters.ps1 -Kill` if needed
- `android\run-code-quality.ps1` now uses `android\temp\run-code-quality.lock.json` to fail fast if another cleanup pass is still active. if that lock is stale, kill the old formatter task and rerun instead of forcing saves over newer edits
- cmake files added by this branch (android/, cmake/, android/tools/etc2tool/) are formatted with cmake-format and linted with cmake-lint (cheshirekow/cmakelang). run individually with `android\run-cmake-format.ps1` (auto-format) or `android\run-cmake-lint.ps1` (lint only). both are also included in the full `run-code-quality.ps1` pass. upstream files in d1/ and d2/ are excluded. config: `.cmake-format.yaml` at repo root

## building
- standard cmake commands (`mkdir build`, `cd build`, `cmake ..`, `cmake --build .`)
- see .github\workflows\package-msvc.yml for specific cmake commands
- for Windows host-build verification, run `run-windows-build.ps1` from repo root instead of calling existing `buildd1`/`buildd2` directories directly. it imports the MSVC environment and finds cmake/ninja for you
- sometimes on windows cl.exe becomes a zombie. kill it before starting cmake builds
- don't run builds until 100 errors (the msbuild default). stop around 10 (`/errorlimit:10`). later errors are often useless anyway

## debugging
- for the *vast majority* of debugging that takes place on-device, it's because it can't be isolated to an emulator or a local test. these are generally going to be things that are too complex for AI tools to guess at from code analysis. step 1 for these tasks is to *add printlines* to the code to confirm or deny initial guesses, or at least cover the area of interest. don't skip this step if you're responding to an on-device problem
- on non-Android platforms, the d1/d2 source logs to "gamelog.txt" or the console. on Android, con_printf output is routed through the debug_log() system under the "Game Logs" category (DLOG_GAME) instead of writing gamelog.txt. for manual debugging on a phone, use the debug log system that writes to files exportable from the "advanced" tab in the launcher. all debug logging is centralized in `android_log.c` / `android_log.h`, which provides `debug_log(int category, const char *fmt, ...)`. categories are defined in `debug_log_categories.h` (DLOG_NETWORK, DLOG_GRAPHICS, DLOG_TEXTURE, DLOG_GAME). convenience macros like COOPLOG are in `android_log.h`. the C code calls through JNI to the kotlin DebugLog class, which writes to files in the app's private storage. these files can be accessed via adb or exported through the launcher UI. for automated testing, we have an introspection API that dumps game state to JSON on demand, which is more efficient than parsing log files or screenshots
- do *not* rely on gamelog.txt for by-hand debugging, or suggest using it, it does not exist on Android. for any log entries that are needed to solve some problem, add them to the debug_log() system under a new category if needed, and then read them from the debug log files

## the input-based demo replay system
- the original game's demo system simply stored position data for all on-screen objects at each frame, like a video
- in order to create a set of regression tests that cover game simulation logic, a new demo system has been created: starting from an initial state, either level start or a saved game (using the existing save game system), the player's inputs are recorded and then replayed. when the demo ends, the final state of the game is checked against the final state encoded at the end of the demo file
- a series of demo files has been created to cover various parts of the game, and are used as regression tests. these files are in `android/regression_demos/` and have the extension `.dximdemo`. there are also ".rngtrace.jsonl" files that are used to check RNG state at each frame when investigating desyncs. the `.dximdemo` files optionally encoded game state with each frame to further help with investigations
- a high level of effort has gone into reducing game engine non-determinism: for example, the rendering and sound code have had their RNG calls moved to a non-sim RNG instance; android vs. PC floating point settings have been tightened up; state that was missing from save games has been added. there is probably remaining work to be done here
- in case of a mismatch, this can indicate either a regression, or it's because the demo found some remaining engine non-determinism
- the immediate goal is to remove all non-determinism from the game engine
- when a failing demo is discovered that highlights non-determinism, your instinct will be to go into forensic mode and try to *describe* what happened in the demo vs. what happens in the replay. this is a flawed instinct. the goal is to *use the demo* to *fix* the non-determinism. you will need to examine the divergence, but only enough to generate theories about non-determinism
- when proposing non-determinism theories, then your next instinct will be to add special handling to the demo recording and replay system. again, this is a flawed instinct. the goal is to fix the game engine, not to patch up the demo system. the demo system is just a tool to expose the game engine's non-determinism, and it should be as simple and transparent as possible. if you find yourself wanting to add special handling to the demo system, that's a strong signal that you should be looking for a game engine fix instead
- a third flawed intinct: the idea that an existing desyncing demo file should be handled and the game sim/demo corrected so the *existing* demo file replays correctly. you shouldn't care about fixing existing demos, they were only presented as examples of non-determinism. new demo files will be recorded after the engine is fixed

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

### two-emulator multiplayer testing
- **emulators crash under load**: long-running adb/logcat sessions and repeated test runs eventually cause emulators to go offline. check `adb devices` before each test run and restart if needed. launch with `-no-snapshot-save -gpu swiftshader_indirect`. kill zombie emulator/qemu processes before restarting
- **fresh emulators lose app data**: `-no-snapshot-save` does NOT wipe app data (it skips saving emulator state on exit). app data on /data persists across restarts. however, if you wipe the AVD or delete its data folder, you must re-push game data and re-install the APK
- **terminal buffer pollution**: the VS Code integrated terminal accumulates stale output from adb/logcat over time. for long-running operations, write helper .ps1 scripts and run them via `Start-Process powershell -ArgumentList "-File","script.ps1" -Wait -WindowStyle Hidden`, reading results from output files. never trust terminal scrollback for correctness
- **kill stale powershell processes**: `Get-Process powershell | Where-Object { $_.Id -ne $PID -and $_.StartTime -lt (Get-Date).AddMinutes(-10) } | Stop-Process -Force` before starting tests

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
- On test timeout/failure, the runner automatically dumps `automation_log.jsonl` and debug log files for diagnostics

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
