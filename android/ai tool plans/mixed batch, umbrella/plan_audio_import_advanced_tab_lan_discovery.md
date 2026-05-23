# Plan: Audio Import Fixes, Advanced Tab, LAN UI, and LAN In-Game Discovery

## Summary
Five items: (1) hide "copy to app storage" for archives, (2) fix SAF CD audio chromaprint identification, (3) add file/SAF link viewers to Advanced tab, (4) fix LAN start dialog portrait UI, (5) implement in-game LAN discovery.

---

## Phase 1: Import Dialog -- Hide "Copy to App Storage" for Archives
- [ ] done

**Files:** `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`

### Problem
The `AddToSetDialog` always shows the "Copy files to app storage" checkbox, even for zip/7z archives that must always be extracted (not merely copied). The option is misleading for archives.

### Steps
1. In `AddToSetDialog`, detect whether selected files are all archives vs contain raw audio files. Use `isArchiveFile()` and `isAudioFile()` helpers already present.
2. If all selected files are archives: hide the checkbox, force `copyToStorage = true`.
3. If mixed: show checkbox but note it only applies to non-archive files (archives always extract).

### Verification
Import a zip file -- checkbox should not appear. Import raw mp3s -- checkbox should appear.

---

## Phase 2: Fix SAF CD Audio Chromaprint Identification
- [ ] done

**Files:** `FingerprintBridge.kt`, `MusicPickerPage.kt`, `AudioSourceManager.kt`

### Problem
`FingerprintBridge.fingerprintAndMatch(path)` requires a filesystem path. SAF files use content URIs. The `/proc/self/fd/<fd>` workaround is used for playback but not fingerprinting. Fingerprinting only runs when `copyToStorage = true`. SAF-referenced CD audio never gets chromaprint identification.

### Root Cause
In the import flow, fingerprinting is gated by `if (copyToStorage)`. For CD audio in AudioSourceManager, `fingerprintDiscTrack()` opens files by filesystem path -- fails for SAF content URIs.

### Steps
1. Add `FingerprintBridge` overload accepting `ContentResolver` + `Uri` -- opens `ParcelFileDescriptor`, uses `/proc/self/fd/${pfd.fd}` as native path.
2. When `copyToStorage = false` in import flow, fingerprint each file via SAF URI approach.
3. For CD audio in `AudioSourceManager`, use `/proc/self/fd` approach for SAF-referenced bin files during chromaprint analysis (same pattern as playback).
4. Verify `nativeFingerprintAudioFile()` works with `/proc/self/fd/<fd>` paths (should, since it's a real FD).

### Verification
Import a CD via SAF reference -- tracks should be chromaprint-identified.

---

## Phase 3: Advanced Tab -- Storage Inspector
- [ ] done

**Files:** `AdvancedSettingsPage.kt`

### Problem
No way to inspect what files are in app storage or what SAF links are active.

### Steps
1. Add "Storage Inspector" section before "Danger Zone" in `AdvancedSettingsPage`.
2. "View App Storage Files" button -- dialog listing all files in `filesDir` recursively with human-readable sizes.
3. "View SAF Links" button -- dialog listing:
   - `referencedUris` from `CustomAudioSetManager`
   - `binContentUri`/`cueContentUri` from `AudioSourceManager`
   - Persistable URI permissions from `contentResolver.persistedUriPermissions`
   - Each entry: URI, associated filename, accessibility status

### Verification
Both buttons should show content. File listing should show sizes. SAF links should show accessibility.

---

## Phase 4: LAN Start Dialog Portrait Fix
- [ ] done

**Files:** `LanDiscoveryTab.kt`

### Problem
5 difficulty buttons unreadable in portrait mode (11.sp, weight(1f) in narrow Row). Level select LazyColumn has no scroll indicators.

### Steps
1. Replace difficulty buttons with dropdown menu.
2. Wrap level LazyColumn in Box, add LazyListScrollArrows (copy from AutoselectEditorPage).
3. Add `rememberLazyListState()` for scroll arrows.

### Verification
Open Start Game dialog in portrait -- difficulty is dropdown, level list has scroll arrows.

---

## Phase 5: LAN In-Game Discovery
- [x] done

**Files:** `LobbyService.kt`, `LobbyProtocol.kt`, `LanDiscoveryTab.kt`

### Architecture
- Pre-game: port 42400 (Kotlin LobbyService), ANNOUNCE packets every 3s
- In-game: port 42424 (C engine), binary protocol, no discovery
- Solution: keep Kotlin broadcasting on 42400 during game. No port conflict. No C changes.

### Steps

#### 5a. Continue ANNOUNCE during game
1. In `startGame()`, keep announce loop running with enriched ANNOUNCE: `"status": "in_game"`, `"level_num"`, `"difficulty"`.
2. Extend `LanLobbyAnnounce` with `status`, `levelNum`, `difficulty`.
3. Ensure coroutine scope/socket survive SetupActivity->MainActivity transition.
4. Audit `stopDiscovery()` call sites -- don't kill broadcasts on game launch.

#### 5b. Handle JOIN for in-progress games
1. When in-game, reject lobby JOINs with clear reason.
2. ANNOUNCE already carries enough info (game, mission, players) for client to show card.

#### 5c. Client-side display
1. Distinguish "lobby" vs "in_game" in discovered lobbies.
2. In-game: show info card + "Join?" button.
3. "Join?" triggers existing join-by-IP flow with correct game type.

#### 5d. Lifecycle
1. LobbyService singleton persists across activities.
2. Guard against stopDiscovery() during activity transition for hosted games.
3. On game exit: stop in-game announces, optionally resume lobby mode.

#### 5e. Protocol
1. Add `status`, `level_num`, `difficulty` to ANNOUNCE (backward compat: old clients ignore unknown fields).
2. No C-side changes.

### Key Decisions
- Entirely Kotlin, no C engine changes
- Port 42400 (discovery) coexists with port 42424 (game)
- Old clients gracefully handle new ANNOUNCE fields
- Does NOT add mid-game join coordination -- only makes games discoverable, uses existing join-by-IP

### Verification
Host LAN game + start. Second device sees in-progress game with info and "Join?" button.

---

## Relevant Files
| File | Purpose |
|------|---------|
| `MusicPickerPage.kt` | Import dialog, archive detection, fingerprint gating |
| `FingerprintBridge.kt` | Native fingerprint bridge |
| `AudioSourceManager.kt` | CD SAF URIs, `/proc/self/fd` for playback |
| `CustomAudioSetManager.kt` | Custom audio referenced URIs |
| `AdvancedSettingsPage.kt` | Advanced tab UI |
| `LanDiscoveryTab.kt` | StartLanGameDialog, discovery view |
| `LobbyService.kt` | LAN discovery lifecycle, announce broadcasts |
| `LobbyProtocol.kt` | Packet types, builders |
| `AutoselectEditorPage.kt` | LazyListScrollArrows reference |
| `jni_fingerprint.c` | Native fingerprint impl |
