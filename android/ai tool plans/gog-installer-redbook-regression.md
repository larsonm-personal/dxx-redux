# GOG D2 Installer Regression Test Plan

## Goal

End-to-end automated test: push the GOG D2 installer .exe to `/sdcard/Download/`
on a blank emulator, import it via the `import_gog` SETUP_COMMAND (with audio),
verify redbook audio was registered with chromaprint-resolved track names, launch
D2, enter level 1, and verify overlay messages (level name + track name) via a new
overlay ring buffer in the introspection API.

### Source file
`game_data/gog installers/setup_descent_2_1.1_(16596).exe`
SHA256: `58ccb37ecd54c73b0ddbde8d9051a0a6498911fbe66cd025179da681e13559cd`
Size: 563 MB

### Expected verification data (from known_discs.json5, disc "d2-gog-v1.2")
- 9 tracks total (1 data + 8 audio)
- Track 6: "Ratzez"
- Level 1 plays track 4 (REDBOOK_FIRST_LEVEL_TRACK = 4 for D2 CD)
- Track 4 name: "Crawl"
- Level 1 name: "Lunar Outpost"
- Expected overlays after entering level 1:
  - Level overlay: "Level 1: Lunar Outpost"
  - Track overlay: "Crawl" (name looked up from audio_playlist.json track_names)

---

## Architecture: Why This Test Is Different

Existing tests use `_deps` to push game data files into the app's private data dir
(`/data/data/com.dxxredux.app/files/sets/default/`) BEFORE the game launches. Then
`run_test.ps1` launches the game and sends the automation broadcast.

This test is fundamentally different:
1. The GOG .exe must go to `/sdcard/Download/` (NOT the app's private dir)
2. The import must happen on the **SetupActivity** screen, BEFORE game launch
3. Import runs on a background thread -- we must wait for it and verify results
4. Only THEN do we launch the game and run the in-game automation script

This means we need a **custom PowerShell test runner** (like `run_lan_test.ps1`)
that orchestrates the pre-game import phase, then hands off to the standard
automation engine for the in-game phase.

---

## Phase 1: Extend _deps Infrastructure for /sdcard/Download/

### 1a. Add .exe to game_data_index.ps1
- [ ] Edit `game_data/generate_game_data_index.ps1`: add `.exe` to `$GameExtensions`
- [ ] Add `game_data/gog installers` to `$SearchDirs` (new priority entry)
- [ ] Regenerate index: `.\game_data\generate_game_data_index.ps1`
- [ ] Verify the .exe hash appears in `game_data_index.txt`

### 1b. Extend Resolve-GameDataDeps for external storage targets
The current push logic (test_helpers.ps1 lines ~376-380) always uses `run-as` which
only works for the app's private data dir. For targets like `/sdcard/Download`:

- [ ] Edit `Resolve-GameDataDeps` in `android/test_helpers.ps1`:
  - Detect absolute paths (starting with `/sdcard/` or `/storage/`)
  - For those: use `adb push` directly to the target path (no `run-as`)
  - For those: use `adb shell ls -la` without `run-as` for device inventory
  - For those: use `adb shell mkdir -p` without `run-as` to create target dir
- [ ] The `_deps` entry in the test script will be:
  ```json5
  {"file": "setup_descent_2_1.1_(16596).exe",
   "sha256": "58ccb37ecd54c73b0ddbde8d9051a0a6498911fbe66cd025179da681e13559cd",
   "target": "/sdcard/Download"}
  ```

### 1c. Verify file placement
- [ ] Run the deps resolution manually and confirm via:
  `adb shell ls -la /sdcard/Download/setup_descent_2_1.1_\(16596\).exe`
- [ ] Confirm the file is NOT in the app's private data dir
- [ ] Note: 563 MB file -- push timeout needs to be generous (10+ minutes)

---

## Phase 2: Blank Emulator Setup

The test requires a blank emulator (no pre-existing game data or audio sources).
`Reset-GameState` only clears pilots/config, not game data files.

- [ ] Add a `Reset-FullGameData` helper function to `test_helpers.ps1`:
  - Call `clear_set` SETUP_COMMAND for the "default" set
  - Delete audio_playlist.json, audio_sources.json
  - Delete /sdcard/Download/*.exe (cleanup from prior runs)
  - Delete descent.cfg
  - Wait for SetupActivity to re-introspect and confirm can_launch=false
- [ ] OR: use existing `clear_set` broadcast + manual cleanup in the test runner

---

## Phase 3: Extend Setup Introspection for Track Names + Chromaprint

The `setup_introspect.json` already includes `audio_sources` but does NOT expose
`trackNames`, `discId`, or chromaprint match status.

### 3a. Add track names to setup_introspect.json audio_sources
- [ ] Edit SetupActivity.kt introspection writer (around line 878)
- [ ] For each audio source, add:
  - `disc_id`: the known_discs ID string (e.g. "d2-gog-v1.2")
  - `track_names`: the Map<Int, String> from AudioSource.trackNames
  - This tells us whether chromaprint/known_discs lookup succeeded
- [ ] Example output after GOG import:
  ```json
  "audio_sources": [{
    "id": "d2-gog-v1.2",
    "label": "Descent II (GOG)",
    "track_count": 9,
    "audio_track_count": 8,
    "disc_id": "d2-gog-v1.2",
    "track_names": {
      "2": "Title", "3": "Base Return", "4": "Crawl",
      "5": "Gunner Down", "6": "Ratzez", "7": "Techno Industry",
      "8": "Are You Descent", "9": "Robot Jungle"
    }
  }]
  ```

### 3b. Verify via introspection after import
- [ ] The test runner will:
  1. Trigger `SETUP_INTROSPECT` after import completes
  2. Parse the JSON and assert:
     - `audio_sources` array length >= 1
     - First source has `disc_id == "d2-gog-v1.2"`
     - Track 6 name == "Ratzez"
     - `d2.ready == true`

---

## Phase 4: Overlay Ring Buffer for Introspection

The user specifically wants to record the last N overlay popup messages into an
array accessible asynchronously -- NOT catch them as they appear.

### 4a. Create overlay_ringbuf.h/cpp
- [ ] Follow the `console_ringbuf.h/cpp` pattern exactly
- [ ] Header: `overlay_ringbuf.h`
  - `overlay_ringbuf_add(const char *type, const char *text)` -- type is "level", "track", "jukebox"
  - `overlay_ringbuf_get_json(uint64_t since_seq, int max_lines)` -- returns JSON string
  - Guarded by `#ifdef INTROSPECT_ON`
- [ ] Implementation: `overlay_ringbuf.cpp`
  - Ring buffer of 32 entries (overlays are rare, 32 is plenty)
  - Each entry: `{seq, type, text}`
  - Mutex-protected (game thread writes, introspection reads)
  - JSON output format:
    ```json
    {"next_seq": 5, "lines": [
      {"seq": 3, "type": "level", "text": "Level 1: Lunar Outpost"},
      {"seq": 4, "type": "track", "text": "Crawl"}
    ]}
    ```
- [ ] Place in `android/app/src/main/cpp/shared/`
- [ ] Add to CMakeLists.txt (shared sources)

### 4b. Hook into existing overlay calls
- [ ] In `track_names.c`:
  - After `android_send_track_name(s_overlay_text)`: add `overlay_ringbuf_add("track", s_overlay_text)`
  - After `android_send_level_name(buf)`: add `overlay_ringbuf_add("level", buf)`
  - After jukebox overlay: add `overlay_ringbuf_add("jukebox", buf)`
  - Guard all calls with `#ifdef INTROSPECT_ON`

### 4c. Expose in game introspection
- [ ] In `game_introspect.cpp`, add an `overlays` section (like `console`):
  ```cpp
  char *overlay_json = overlay_ringbuf_get_json(0, 32);
  if (overlay_json) {
      jb_raw(&jb, "overlays", overlay_json);
      free(overlay_json);
  }
  ```

### 4d. Test verification
- [ ] After entering level 1, wait a few seconds, then introspect
- [ ] The overlay ring buffer should contain:
  - `{"type": "level", "text": "Level 1: Lunar Outpost"}`
  - `{"type": "track", "text": "Crawl"}`
- [ ] Assert on both. No timing concerns -- they persist in the ring buffer.

---

## Phase 5: Custom Test Runner Script

Create `android/tests/test_gog_installer_redbook.ps1` (or similar name).

### 5a. Pre-game phase (PowerShell orchestration)
- [ ] Source test_helpers.ps1
- [ ] Ensure emulator healthy
- [ ] Push GOG .exe to /sdcard/Download via _deps resolution
- [ ] Force-stop app, clear game data (blank slate)
- [ ] Launch SetupActivity
- [ ] Wait for SetupActivity ready (Wait-SetupActivityReady)
- [ ] Verify via setup_introspect: `can_launch == false`, no audio_sources
- [ ] Send `import_gog` SETUP_COMMAND:
  ```
  adb shell am broadcast -a com.dxxredux.SETUP_COMMAND
    --es command import_gog
    --es path "/sdcard/Download/setup_descent_2_1.1_(16596).exe"
    --ez include_audio true
  ```
- [ ] Poll setup_introspect until:
  - `d2.ready == true`
  - `audio_sources` contains entry with disc_id "d2-gog-v1.2"
  - `audio_sources[0].track_names` is populated
  - Timeout: 120s (extraction of 563 MB can be slow)
- [ ] Assert track names: track 6 == "Ratzez"
- [ ] Assert descent.cfg has MusicType=2:
  `adb shell run-as com.dxxredux.app cat files/descent.cfg | grep MusicType`

### 5b. Game launch + automation phase
- [ ] Send `launch` SETUP_COMMAND with game=d2
- [ ] Wait for game started
- [ ] Push automation script to device
- [ ] Send automation broadcast
- [ ] Watch-AutomationResult with timeout

### 5c. Game automation script (test_gog_installer_redbook.json5)
The in-game portion. NO _deps needed (files are already imported).
- [ ] `_standalone: false` (run via the custom PS1 runner, not run_test.ps1)
- [ ] Accept pilot name (Ok)
- [ ] New game -> Counterstrike -> Ok -> Rookie
- [ ] Skip briefing
- [ ] Wait for in_game == true, game_window_is_front == true
- [ ] Wait 3-5 seconds (for overlays to fire)
- [ ] Introspect and assert:
  - `overlays` ring buffer contains level entry matching "Level 1: Lunar Outpost"
  - `overlays` ring buffer contains track entry matching "Crawl"
  - `redbook.current_track` == 4 (or similar)
  - `redbook.play_status` == "playing"
- [ ] PASS

---

## Phase 6: Likely Bugs & Investigation Plan

These are the areas most likely to have existing bugs that will surface during
testing. For each, the plan is: run the test, see if it fails at that point,
diagnose, fix, re-run.

### 6a. GOG extraction on blank install
- **Risk**: `enableRedbookInConfig()` reads/modifies descent.cfg, but on a blank
  install with no prior game launch, descent.cfg may not exist yet.
  - The function has `if (!cfgFile.exists()) return` -- this means it silently
    does nothing on a fresh install, so MusicType won't be set to 2.
  - **Fix**: create a minimal descent.cfg if it doesn't exist, or write the
    keys unconditionally.
- **Risk**: `registerGogAudioSource()` calls `FingerprintBridge.lookupTrackNames()`
  which does a direct DB lookup by disc ID "d2-gog-v1.2". This should work if
  known_discs.json5 has that entry. But exceptions are swallowed silently.
  - **Investigation**: add logging, verify the lookup succeeds

### 6b. findGogPair() file detection
- **Risk**: After extraction, the .gog/.inst files land in the set dir with
  whatever case InnoSetup used. `findGogPair()` uses `.lowercase()` comparison
  so should be ok, but need to verify the actual extracted filenames.
- **Investigation**: check what names come out of inno_reader.c extraction

### 6c. audio_playlist.json never written before game launch
- **Risk**: `AudioSourceManager.writePlaylist()` is called somewhere before game
  launch but may not be called after `import_gog` SETUP_COMMAND completes
  on its background thread. The game reads `audio_playlist.json` at startup via
  `RBAInit()`. If the playlist isn't written, the game won't see the audio source.
- **Investigation**: check if `writePlaylist()` is called during game launch
  sequence in SetupActivity, or only when the music picker is explicitly used.
  If it's only in the music picker, we need to either:
  - Add `writePlaylist()` call in the `import_gog` handler after registration
  - OR: ensure the game launch path calls it (check `writeInitialGameConfig()`)
- **This is probably a real bug** -- the import_gog command registers the source
  but may not persist it to the playlist JSON that the engine reads.

### 6d. songs_haved2_cd() detection
- **Risk**: This function checks `GameCfg.OrigTrackOrder` first (quick path,
  returns 1 if set). `enableRedbookInConfig()` sets `OrigTrackOrder=1`. But if
  descent.cfg doesn't exist (see 6a), this won't be set.
- **Risk**: Fallback path checks `RBAGetDiscID()` against known disc IDs.
  `RBAGetDiscID()` returns the `legacy_disc_id` from audio_playlist.json.
  The GOG source uses `0x7d0ff809L`. Need to verify this is in the switch
  statement in songs_haved2_cd().

### 6e. Track names not propagating to engine
- **Risk**: Track names flow: AudioSourceManager.trackNames -> audio_playlist.json
  -> rbaudio_bin.c parse -> track_names_set_cue_title() -> track_names_lookup()
  -> track_overlay_notify(). If any link breaks, overlay shows "Track 4" instead
  of "Crawl".
- **Verification**: after game start, introspect and check if the track name
  is the chromaprint-resolved name or a fallback

### 6f. Case sensitivity in _deps for .exe filename
- **Risk**: `Resolve-GameDataDeps` lowercases filenames (`$dep.file.ToLower()`).
  The actual file has parentheses and mixed case. The `game_data_index.txt` stores
  the original path. When pushing to /sdcard/Download/, we need the original
  filename (or at least the lowercased version), and the import_gog command needs
  to reference the correct path.
- **Investigation**: check if `adb push` preserves the original name, and document
  what the actual filename on /sdcard/Download/ will be after push

---

## Phase 7: Build, Test, Iterate Methodology

The test will likely fail multiple times as real bugs surface. Process:

1. **Build**: After each code change, build the APK:
   `cd android && ./gradlew assembleDebug`
2. **Deploy**: Install to emulator:
   `adb install -r app/build/outputs/apk/debug/app-debug.apk`
3. **Run**: Execute the test runner script
4. **Diagnose**: On failure:
   - Read automation_log.jsonl for step-level diagnostics
   - Read setup_introspect.json to check import state
   - Read introspect.json for game state
   - Read descent.cfg for config state
   - Check `adb logcat -s DXX-Setup DXX-Automate` for errors
5. **Fix**: Make targeted fix to the identified issue
6. **Repeat**: from step 1

**Critical rule**: When the test fails, assume the bug is real and in the
application code (not the test infrastructure), until proven otherwise. The
most likely failure points are:
- descent.cfg not created (Phase 6a)
- audio_playlist.json not written (Phase 6c)  
- Track names not propagating (Phase 6e)

---

## File Change Summary

| File | Change |
|------|--------|
| `game_data/generate_game_data_index.ps1` | Add .exe extension, add gog installers dir |
| `android/test_helpers.ps1` | Extend Resolve-GameDataDeps for /sdcard/ targets |
| `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | Add trackNames/discId to setup introspect; fix enableRedbookInConfig on blank install; add writePlaylist+writeMusicConfigForLaunch to launch SETUP_COMMAND |
| `android/app/src/main/cpp/shared/overlay_ringbuf.h` | New: overlay ring buffer header |
| `android/app/src/main/cpp/shared/overlay_ringbuf.cpp` | New: overlay ring buffer impl |
| `android/app/src/main/cpp/shared/track_names.c` | Hook overlay_ringbuf_add calls |
| `android/app/src/main/cpp/shared/game_introspect.cpp` | Add overlays section, include overlay_ringbuf.h |
| `android/app/src/main/cpp/shared/game_automate.cpp` | New assert_overlay action type |
| `android/app/src/main/cpp/CMakeLists.txt` | Add overlay_ringbuf.cpp to D1 and D2 builds |
| `android/tests/test_gog_installer_redbook.ps1` | New: custom test runner |
| `android/game_scripts/test_gog_installer_redbook.json5` | Rewrite: in-game automation script |

---

## Progress Tracking

- [x] Phase 1a: Add .exe to game_data_index
- [x] Phase 1b: Extend Resolve-GameDataDeps for /sdcard/
- [ ] Phase 1c: Verify file placement on emulator
- [ ] Phase 2: Blank emulator setup (handled by custom test runner inline)
- [x] Phase 3a: Add track names + disc_id to setup introspection
- [ ] Phase 3b: Verify introspection after import
- [x] Phase 4a: Create overlay_ringbuf.h/cpp
- [x] Phase 4b: Hook overlay calls in track_names.c
- [x] Phase 4c: Expose overlays in game_introspect.cpp
- [x] Phase 4d: Add assert_overlay action type to game_automate.cpp
- [x] Phase 5a: Custom test runner script (test_gog_installer_redbook.ps1)
- [x] Phase 5c: In-game automation script (test_gog_installer_redbook.json5)
- [x] Phase 6a FIX: enableRedbookInConfig now creates descent.cfg if missing
- [x] Phase 6c FIX: launch SETUP_COMMAND now calls writePlaylist() + writeMusicConfigForLaunch()
- [ ] Phase 6: Remaining bug investigation (run test, iterate)
- [ ] Phase 7: Final passing test run
- [x] Code quality: clang-format applied, build passes, PSScriptAnalyzer+shellcheck pass
