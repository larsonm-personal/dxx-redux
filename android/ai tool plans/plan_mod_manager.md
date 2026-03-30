# Plan: Mod Manager (DXA import, toggle, and launcher integration)

## Status: IMPLEMENTED (awaiting emulator test)

---

## 1. DXA Size Reduction

### Problem
Converted .dxa files are 1.7-1.9x larger than the source .7z archives:
- D1: 190.8 MB dxa vs 114.4 MB 7z (1.67x)
- D2: 358.7 MB dxa vs 192.5 MB 7z (1.86x)

Root cause: PNG with deflate compression is less efficient than LZMA on raw pixel data. ZIP on top of PNG adds nothing (PNG is already deflate-compressed).

### Approach: Use JPEG for opaque textures, with honest file extensions

Most game textures are opaque (RGB, no alpha). JPEG at quality 90-92 is 3-5x smaller than PNG for these textures, with negligible visual difference at 512x512 game texture resolution.

Files use their real extensions: `.jpg` for JPEG, `.png` for PNG. The engine's texture lookup in `ogl_loadbmtexture_f()` is extended to try multiple extensions (`.png`, `.jpg`, `.tga`) instead of only `.png`. `pngfile_stb.c` already uses stb_image which detects format by header magic bytes, so `read_png()` loads any of these formats despite the function name.

Implementation in `convert_d2xxl_textures.ps1`:
1. Check if TGA is 24-bit (RGB) or 32-bit (RGBA)
2. RGB textures: save as JPEG quality 92 with `.jpg` extension
3. RGBA textures: save as PNG with `.png` extension (JPEG doesn't support alpha)
4. Use `CompressionLevel::NoCompression` for zip entries since JPEG/PNG are already compressed

Engine change in `ogl.c` (both d1 and d2, guarded by `#ifdef ANDROID`):
- Replace single `sprintf(filename, "%s.png", bitmapname)` + `read_png()` call
- With a loop over `{".png", ".jpg", ".tga"}` trying each extension until one succeeds
- About 5 lines of change, minimal impact

Expected size: ~100-150 MB per game (D2), down from 358 MB. Roughly matching .7z size.

Desktop note: desktop builds continue to try only `.png` (the `#ifdef ANDROID` guard keeps the multi-extension loop Android-only). If desktop ever uses stb_image, the guard can be broadened.

---

## 2. Mod Manager -- Overview

### User-facing concept

A new collapsible section in SetupActivity below "Music" called **"Mods"**. Users can:
- Import `.dxa` files via the existing SAF file picker ("Select Game Files or Archive to Import")
- See a list of imported mods with on/off toggles
- Enabled mods are made available to the game engine on next launch
- Disabled mods stay in the list but are not loaded

### File organization

```
filesDir/
  mods/                         <-- new directory, shared across all file sets
    mod_manifest.json           <-- mod list with enabled/disabled state
    hires-textures-d2.dxa       <-- imported mod files (copied from SAF)
    hires-textures-d1.dxa
    hires-sounds-d2.dxa
    custom-levels.dxa
```

Mods are stored in `filesDir/mods/` (not per-set). This is appropriate because:
- Mods are game-content overlays, not base-game-file alternatives
- A user with multiple file sets (GOG vs retail) probably wants the same hires textures on both
- Keeps the mod directory simple and avoids duplicating large files across sets

### mod_manifest.json format

```json
{
  "mods": [
    {
      "filename": "hires-textures-d2.dxa",
      "displayName": "Hi-Res Textures (D2)",
      "enabled": true,
      "addedAt": 1711234567890,
      "sizeBytes": 150000000,
      "game": "d2"
    },
    {
      "filename": "hires-textures-d1.dxa",
      "displayName": "Hi-Res Textures (D1)",
      "enabled": true,
      "addedAt": 1711234567891,
      "sizeBytes": 100000000,
      "game": "d1"
    },
    {
      "filename": "hires-sounds-d2.dxa",
      "displayName": "Hi-Res Sounds (D2)",
      "enabled": false,
      "addedAt": 1711234567892,
      "sizeBytes": 3500000,
      "game": "d2"
    }
  ]
}
```

Fields:
- `filename`: the .dxa file in `filesDir/mods/`
- `displayName`: shown in the UI; derived from filename on import (strip extension, replace hyphens/underscores with spaces, title-case)
- `enabled`: whether the mod is active
- `addedAt`: import timestamp
- `sizeBytes`: file size for display
- `game`: "d1", "d2", or "both" -- which game the mod applies to (auto-detected or user-chosen)

### Game detection

When a .dxa is imported, try to auto-detect which game it's for:
1. If filename contains "d1" (case-insensitive) -> "d1"
2. If filename contains "d2" (case-insensitive) -> "d2"
3. Otherwise -> "both" (applied to whichever game is launched)

This is a heuristic. The user can change it in the UI if wrong.

---

## 3. Import Flow

### SAF integration

The existing `filePickerLauncher` in SetupActivity already handles multiple file types. Add `.dxa` detection:

```kotlin
// In the file picker result handler, inside the for-uri loop:
lname.endsWith(".dxa") -> dxaImportUris.add(name to uri)
```

After the picker returns, for each `.dxa` URI:
1. Copy the file from the SAF URI to `filesDir/mods/{filename}`
   - Use streaming copy (not loading into memory -- these files can be 100+ MB)
   - Show a progress indicator during copy
2. Add an entry to `mod_manifest.json` with `enabled = true`
3. Refresh the Mods section UI

### Why copy instead of leave-in-place?

The SAF archiver provides fd-based access for individual files. A .dxa is a ZIP archive that PhysFS needs to mount as a directory. PhysFS's ZIP archiver requires a filesystem path (not an fd) to open the archive. It uses `fopen()` internally.

Options:
1. **Copy to app storage** (recommended): simple, fast lookup. DXA files are self-contained. The user explicitly picked them for import.
2. **SAF leave-in-place**: would require a custom PhysFS archiver that can open ZIPs from fds. Significant complexity for minimal benefit (the user is already choosing to import these files).
3. **Symlink /proc/self/fd/N**: fragile, fd must stay open for the entire game session.

Copy is the right choice. Mod files are imported once and used many times.

---

## 4. Engine Integration (making mods available on launch)

### Approach: write a mods path file, mount in physfsx.c

Similar to `.active_set_path`, the launcher writes a file listing all enabled mod paths before engine launch. The C engine reads this at init and adds each mod directory to the PhysFS search path.

**Kotlin side** (`ModManager` class):
```kotlin
fun writeEnabledModPaths(game: String) {
    val manifest = loadManifest()
    val enabledMods = manifest.mods
        .filter { it.enabled && (it.game == game || it.game == "both") }
    // Write list of absolute paths, one per line
    val modsDir = File(filesDir, "mods")
    for (gameDir in arrayOf("d2x-redux", "d1x-redux")) {
        val pathFile = File(filesDir, "$gameDir/.active_mod_paths")
        pathFile.writeText(
            enabledMods.joinToString("\n") { File(modsDir, it.filename).absolutePath }
        )
    }
}
```

Called from `onLaunchGame` alongside `writeActiveSetPath()` and `writeMusicConfigForLaunch()`.

**C side** (in `physfsx.c`, `#ifdef ANDROID` block, after `.active_set_path` handling):
```c
/* Mod support: mount enabled .dxa files from .active_mod_paths */
{
    char modpath[512];
    snprintf(modpath, sizeof(modpath), "%s.active_mod_paths", gd);
    FILE *mf = fopen(modpath, "r");
    if (mf) {
        char line[512];
        while (fgets(line, sizeof(line), mf)) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            if (strlen(line) > 0) {
                if (PHYSFS_mount(line, NULL, 0))  /* prepend */
                    con_printf(CON_DEBUG, "PHYSFS: Mounted mod %s\n", line);
            }
        }
        fclose(mf);
    }
}
```

Note: using `PHYSFS_mount` with `append=0` (prepend) so mod content takes priority over base game files. This is the same behavior as `PHYSFSX_addArchiveContent()` which uses `PHYSFS_addToSearchPath(file[1], 0)`.

With this approach, `PHYSFSX_addArchiveContent()` (called later in inferno.c) will also discover any .dxa files in the search path. But since the mod .dxa files are in `filesDir/mods/` which is NOT on the search path unless explicitly mounted, there's no double-mount. The explicit mounting via `.active_mod_paths` is the sole path for mods.

### Search path order with mods

```
0. [enabled mod .dxa files]  <-- NEW: highest priority (prepended)
1. sets/<active>/            <-- game data for active file set
2. filesDir/                 <-- write dir + configs/saves/music
3. [SAF manifest]            <-- leave-in-place files
4. APK base dir              <-- bundled assets
5. data/                     <-- relative subdir
```

This means mod textures/sounds override base game assets, which is exactly the desired behavior.

---

## 5. Launcher UI

### Mods section (collapsible, below Music)

```
[v] Mods                                    (2 of 3 enabled)

  [X] Hi-Res Textures (D2)      150.0 MB    [^] [v] [trash]
  [X] Hi-Res Textures (D1)      100.0 MB    [^] [v] [trash]
  [ ] Hi-Res Sounds (D2)          3.5 MB    [^] [v] [trash]
```

Components:
- `GameSectionHeader` with title "Mods" and summary "(N of M enabled)" or "(none)" 
- Each mod is a row with:
  - Checkbox (toggle enabled/disabled)
  - Display name
  - Size in MB
  - Up/down arrow buttons for reordering (matching AudioSourceRow in MusicPickerPage.kt)
  - Delete button (trash icon) with confirmation dialog
- No separate import button -- the existing "Select Game Files or Archive to Import" button handles .dxa files
- Reorder uses the same pattern as audio source rows: up/down IconButtons that swap adjacent items in the ordered list

### ModsSection composable

```kotlin
@Composable
private fun ModsSection(
    filesDir: File,
    refreshTrigger: Int,
    onRefresh: () -> Unit,
) {
    val modManager = remember { ModManager(filesDir) }
    var mods by remember { mutableStateOf(modManager.listMods()) }
    var expanded by remember { mutableStateOf(false) }
    
    LaunchedEffect(refreshTrigger) { mods = modManager.listMods() }
    
    val enabledCount = mods.count { it.enabled }
    val totalCount = mods.size
    val summary = if (totalCount == 0) "(none)" else "($enabledCount of $totalCount enabled)"
    
    GameSectionHeader(
        title = "Mods",
        ready = true,  // mods are always optional
        expanded = expanded,
        onToggle = { expanded = !expanded },
    )
    
    if (expanded) {
        if (mods.isEmpty()) {
            Text(
                "No mods installed. Use the file picker above to import .dxa files",
                fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 16.dp)
            )
        } else {
            mods.forEach { mod ->
                ModRow(mod, onToggle = { ... }, onDelete = { ... })
            }
        }
    }
}
```

### Interaction with GameSectionHeader

The existing `GameSectionHeader` takes a `ready` boolean that shows a green/red indicator. For mods, `ready` is always true since mods are entirely optional. The summary text replaces the status dot -- we can add an optional `statusText` parameter or just always show the count next to "Mods".

---

## 6. ModManager class

New file: `android/app/src/main/java/com/dxxredux/app/ModManager.kt`

```kotlin
class ModManager(private val filesDir: File) {
    
    data class ModInfo(
        val filename: String,
        val displayName: String,
        val enabled: Boolean,
        val addedAt: Long,
        val sizeBytes: Long,
        val game: String,  // "d1", "d2", or "both"
    )
    
    private val modsDir get() = File(filesDir, "mods").also { it.mkdirs() }
    private val manifestFile get() = File(modsDir, "mod_manifest.json")
    
    fun listMods(): List<ModInfo> { ... }
    fun setEnabled(filename: String, enabled: Boolean) { ... }
    fun deleteMod(filename: String) { ... }
    fun importMod(sourceUri: Uri, displayName: String, contentResolver: ContentResolver): ModInfo { ... }
    fun writeEnabledModPaths(game: String) { ... }
    
    // Auto-detect game from filename
    private fun detectGame(filename: String): String { ... }
    
    // Generate display name from filename
    private fun generateDisplayName(filename: String): String { ... }
}
```

---

## 7. Changes Summary

### New files
| File | Purpose |
|------|---------|
| `ModManager.kt` | Mod list management, manifest I/O, path file writer |

### Modified files
| File | Change |
|------|--------|
| `d2/arch/ogl/ogl.c` | Try `.png`, `.jpg`, `.tga` extensions in texture lookup (Android only) |
| `d1/arch/ogl/ogl.c` | Same as d2 |
| `SetupActivity.kt` | Add .dxa handling to file picker, add ModsSection below MusicInfoSection, call writeEnabledModPaths before launch |
| `d2/misc/physfsx.c` | Read `.active_mod_paths` and mount each listed .dxa |
| `d1/misc/physfsx.c` | Same as d2 |
| `convert_d2xxl_textures.ps1` | Use JPEG for RGB textures with .jpg extension, NoCompression zip entries |

### No changes needed
| Component | Why |
|-----------|-----|
| `PHYSFSX_addArchiveContent()` | Already mounts .dxa files from search path; mods are mounted separately |
| `pngfile_stb.c` | Already handles JPG/TGA/PNG via stb_image |
| `FileSetManager` | Mods are orthogonal to file sets |
| `SafManifest` / SAF archiver | Mods are copied, not left in place |

---

## 8. Implementation Phases

### Phase 1: DXA compression improvement + engine extension lookup
1. Update `convert_d2xxl_textures.ps1` to use JPEG for RGB textures with `.jpg` extension
2. Add multi-extension lookup (`.png`, `.jpg`, `.tga`) in `ogl_loadbmtexture_f()` in d1 and d2 ogl.c
3. Test conversion, verify file sizes
4. Verify engine loads `.jpg` textures from DXA on emulator

### Phase 2: ModManager + manifest
1. Create `ModManager.kt` with manifest I/O
2. Add `filesDir/mods/` directory creation
3. Implement import, list, toggle, delete

### Phase 3: Launcher UI
1. Add ModsSection composable below MusicInfoSection
2. Add .dxa routing in file picker handler
3. Wire up import progress, toggle, delete

### Phase 4: Engine integration
1. Add `.active_mod_paths` reading to `d2/misc/physfsx.c` and `d1/misc/physfsx.c`
2. Call `writeEnabledModPaths()` before game launch in SetupActivity
3. Test with hires texture .dxa on emulator

### Phase 5: Testing
1. Import a .dxa via the file picker, verify it appears in the mod list
2. Toggle mod on/off, launch game, verify textures load/don't load
3. Delete a mod, verify cleanup
4. Test with D1 and D2 mods simultaneously

---

## 9. Open Questions

1. **Per-set vs shared mods**: Current design is shared (all sets see all mods). Should mods be per-set? Leaning no -- mods are content overlays, not base game alternatives.

2. **Mod ordering**: ~~If multiple mods provide the same file (e.g., two texture packs), which wins?~~ **RESOLVED**: Mods are mounted in manifest order (top-to-bottom), with up/down arrow buttons for reordering matching the music picker pattern (AudioSourceRow in MusicPickerPage.kt).

3. **Game-specific filtering**: Should the UI show only mods relevant to the currently selected game? Or show all mods with a game tag? Showing all with a tag is simpler and less confusing.

4. **Large file import UX**: .dxa files can be 100+ MB. The copy from SAF to app storage needs a progress bar and should run on a background coroutine. Match the pattern used for GOG installer extraction.

5. **Disk space**: Should the UI show total mod disk usage and warn if space is low? Nice to have, not critical for initial release.
