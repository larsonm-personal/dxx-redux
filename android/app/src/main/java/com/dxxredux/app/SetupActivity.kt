package com.dxxredux.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.DocumentsContract
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import android.content.res.Configuration
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableFloatStateOf
import androidx.core.view.WindowCompat
import android.content.SharedPreferences
import java.io.File
import java.io.FileOutputStream
import java.io.FileWriter
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipInputStream
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Pre-game setup screen built with Jetpack Compose.
 *
 * Shows the readiness status of required and optional game data files
 * for both Descent 2 and Descent 1, instructions for installing
 * missing files, and a button to launch (or return to) the game.
 *
 * This is the launcher activity.
 */
class SetupActivity : ComponentActivity() {

    /** Incremented in onResume so Compose re-checks file status. */
    private val refreshTrigger = mutableIntStateOf(0)

    // ── Setup-screen introspection ──────────────────────────────────────
    //   adb shell am broadcast -a com.dxxredux.SETUP_INTROSPECT
    //   adb shell run-as com.dxxredux.app cat files/setup_introspect.json
    private val introspectReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context?, intent: Intent?) {
            writeIntrospectJson()
        }
    }

    // ── Setup-screen command API ────────────────────────────────────────
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch
    private var gameRunningFlag = false
    private val commandReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context?, intent: Intent?) {
            val cmd = intent?.getStringExtra("command") ?: return
            when (cmd) {
                "launch" -> {
                    if (gameRunningFlag) {
                        finish()
                    } else {
                        FileSetManager(filesDir).writeActiveSetPath()
                        writeInitialGameConfig()
                        startActivity(Intent(this@SetupActivity, MainActivity::class.java))
                    }
                }
                else -> Log.w("DXX-Setup", "Unknown command: $cmd")
            }
        }
    }

    /** Active download progress visible to introspection. */
    internal val downloadStates = mutableMapOf<String, Int>()

    // ── Controller live-state ───────────────────────────────────────────
    /** Axis values observable by Compose (LX, LY, RX, RY, LT, RT). */
    internal val controllerAxes = FloatArray(6)
    /** D-Pad HAT axis values (hatX, hatY). */
    internal val dpadAxes = FloatArray(2)
    /** Compose-observable axis update counter (increment triggers recompose). */
    internal val axisGeneration = mutableIntStateOf(0)
    /** Currently pressed gamepad buttons (name strings). */
    internal val pressedButtons = mutableStateListOf<String>()

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
            && event.action == MotionEvent.ACTION_MOVE
        ) {
            controllerAxes[0] = event.getAxisValue(MotionEvent.AXIS_X)
            controllerAxes[1] = event.getAxisValue(MotionEvent.AXIS_Y)
            controllerAxes[2] = event.getAxisValue(MotionEvent.AXIS_Z)
            controllerAxes[3] = event.getAxisValue(MotionEvent.AXIS_RZ)
            controllerAxes[4] = event.getAxisValue(MotionEvent.AXIS_LTRIGGER)
            controllerAxes[5] = event.getAxisValue(MotionEvent.AXIS_RTRIGGER)
            dpadAxes[0] = event.getAxisValue(MotionEvent.AXIS_HAT_X)
            dpadAxes[1] = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
            axisGeneration.intValue++
            return true
        }
        return super.dispatchGenericMotionEvent(event)
    }

    private fun gamepadButtonName(keyCode: Int): String? = when (keyCode) {
        KeyEvent.KEYCODE_BUTTON_A      -> "A"
        KeyEvent.KEYCODE_BUTTON_B      -> "B"
        KeyEvent.KEYCODE_BUTTON_X      -> "X"
        KeyEvent.KEYCODE_BUTTON_Y      -> "Y"
        KeyEvent.KEYCODE_BUTTON_L1     -> "L1"
        KeyEvent.KEYCODE_BUTTON_R1     -> "R1"
        KeyEvent.KEYCODE_BUTTON_L2     -> "L2"
        KeyEvent.KEYCODE_BUTTON_R2     -> "R2"
        KeyEvent.KEYCODE_BUTTON_SELECT -> "Select"
        KeyEvent.KEYCODE_BUTTON_START  -> "Start"
        KeyEvent.KEYCODE_BUTTON_THUMBL -> "L3"
        KeyEvent.KEYCODE_BUTTON_THUMBR -> "R3"
        KeyEvent.KEYCODE_DPAD_UP       -> "D-Up"
        KeyEvent.KEYCODE_DPAD_DOWN     -> "D-Down"
        KeyEvent.KEYCODE_DPAD_LEFT     -> "D-Left"
        KeyEvent.KEYCODE_DPAD_RIGHT    -> "D-Right"
        else -> null
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val name = gamepadButtonName(event.keyCode)
        if (name != null) {
            if (event.action == KeyEvent.ACTION_DOWN) {
                if (name !in pressedButtons) pressedButtons.add(name)
            } else if (event.action == KeyEvent.ACTION_UP) {
                pressedButtons.remove(name)
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    private fun writeIntrospectJson() {
        try {
            val dir = filesDir
            val fsm = FileSetManager(dir)
            val activeSet = fsm.getActive()
            val setDir = fsm.getSetDir(activeSet)
            val manifest = AssetManifest(setDir)
            val safManifest = fsm.safManifestForSet(activeSet)
            val d2FileList = detectD2FileList(setDir, safManifest)
            val d2Statuses = checkFiles(setDir, d2FileList, manifest, safManifest)
            val d1Statuses = checkFiles(setDir, D1_FILES, manifest, safManifest)
            val d2Ready = d2Statuses.filter { it.info.required }.all { it.found }
            val d1Ready = d1Statuses.filter { it.info.required }.all { it.found }

            val root = JSONObject()
            root.put("screen", "setup")
            root.put("can_launch", d2Ready || d1Ready)
            root.put("active_set", activeSet)

            // All files on disk
            val allFiles = dir.listFiles()?.map { it.name }?.sorted() ?: emptyList()
            root.put("files_on_disk", JSONArray(allFiles))

            // D2 section
            val d2 = JSONObject()
            d2.put("ready", d2Ready)
            d2.put("files", fileStatusArray(d2Statuses))
            root.put("d2", d2)

            // D1 section
            val d1 = JSONObject()
            d1.put("ready", d1Ready)
            d1.put("files", fileStatusArray(d1Statuses))
            root.put("d1", d1)

            // Active downloads
            if (downloadStates.isNotEmpty()) {
                val dl = JSONObject()
                for ((name, progress) in downloadStates) {
                    dl.put(name, when (progress) {
                        -2 -> "complete"
                        -1 -> "error"
                        else -> "${progress}%"
                    })
                }
                root.put("downloads", dl)
            }

            val outFile = File(dir, "setup_introspect.json")
            FileWriter(outFile).use { it.write(root.toString()) }
            Log.i("DXX-Setup", "Introspect written: ${outFile.absolutePath}")
        } catch (e: Exception) {
            Log.e("DXX-Setup", "Failed to write introspect JSON", e)
        }
    }

    private fun fileStatusArray(statuses: List<FileStatus>): JSONArray {
        val arr = JSONArray()
        for (s in statuses) {
            val obj = JSONObject()
            obj.put("filename", s.info.filename)
            obj.put("required", s.info.required)
            obj.put("found", s.found)
            if (s.foundName != null) obj.put("found_as", s.foundName)
            if (s.info.alternatives.isNotEmpty())
                obj.put("alternatives", JSONArray(s.info.alternatives))
            if (s.info.downloadUrl != null)
                obj.put("download_url", s.info.downloadUrl)
            obj.put("description", s.info.description)
            if (s.safUri != null) {
                obj.put("saf_linked", true)
                obj.put("saf_uri", s.safUri)
            }
            if (s.manifestEntry != null) {
                obj.put("sha256", s.manifestEntry.sha256)
                obj.put("version", s.manifestEntry.versionDisplay)
                if (s.manifestEntry.isExternal) {
                    obj.put("source_uri", s.manifestEntry.sourceUri)
                    obj.put("external", true)
                }
                if (!s.found) {
                    obj.put("missing_from_disk", true)
                }
            }
            arr.put(obj)
        }
        return arr
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Edge-to-edge: draw behind system bars, Compose handles insets
        WindowCompat.setDecorFitsSystemWindows(window, false)

        // Register introspection receiver
        val filter = IntentFilter("com.dxxredux.SETUP_INTROSPECT")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(introspectReceiver, filter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(introspectReceiver, filter)
        }

        // Register command receiver
        val cmdFilter = IntentFilter("com.dxxredux.SETUP_COMMAND")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(commandReceiver, cmdFilter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(commandReceiver, cmdFilter)
        }

        val gameRunning = intent.getBooleanExtra("gameRunning", false)
        gameRunningFlag = gameRunning
        val filesDir = filesDir

        setContent {
            SetupScreen(
                filesDir = filesDir,
                gameRunning = gameRunning,
                refreshTrigger = refreshTrigger.intValue,
                controllerAxes = controllerAxes,
                dpadAxes = dpadAxes,
                axisGeneration = axisGeneration.intValue,
                pressedButtons = pressedButtons,
                onLaunchGame = {
                    if (gameRunning) {
                        finish()   // return to the already-running game
                    } else {
                        FileSetManager(filesDir).writeActiveSetPath()
                        writeInitialGameConfig()
                        startActivity(Intent(this, MainActivity::class.java))
                        // Don't finish() — stay in back stack so quitting
                        // the game returns here instead of the launcher.
                    }
                },
                onRefresh = { refreshTrigger.intValue++ },
                onDownloadStateChanged = { name, progress ->
                    if (progress == -2) downloadStates.remove(name)
                    else downloadStates[name] = progress
                }
            )
        }
    }

    /**
     * Write initial descent.cfg with Android-appropriate defaults if the file
     * doesn't exist yet (first launch).  Once the user changes settings in-game,
     * the engine overwrites this file and their preferences stick.
     *
     * Settings that live in binary .plr files (like ControlType) can't be
     * handled here — those are set in config.c's android_apply_initial_defaults().
     */
    private fun writeInitialGameConfig() {
        val cfgFile = File(filesDir, "descent.cfg")
        if (cfgFile.exists()) return   // user already has a config — don't overwrite

        // Determine the device's real screen dimensions (including system bars)
        val (screenW, screenH) = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val bounds = windowManager.currentWindowMetrics.bounds
            Pair(bounds.width(), bounds.height())
        } else {
            @Suppress("DEPRECATION")
            val size = android.graphics.Point()
            @Suppress("DEPRECATION")
            windowManager.defaultDisplay.getRealSize(size)
            Pair(size.x, size.y)
        }

        // Ensure wider dimension is treated as width (game is landscape)
        val w = maxOf(screenW, screenH)
        val h = minOf(screenW, screenH)

        // Reduce to simplest fraction via GCD
        fun gcd(a: Int, b: Int): Int = if (b == 0) a else gcd(b, a % b)
        val g = gcd(w, h)
        val aspectY = w / g   // width component  (game naming: Y = wider)
        val aspectX = h / g   // height component (game naming: X = narrower)

        Log.i("DXX-Setup", "First launch: writing descent.cfg with aspect ${aspectY}x${aspectX} (from ${w}x${h})")

        cfgFile.writeText(
            "AspectX=$aspectX\n" +
            "AspectY=$aspectY\n"
        )
    }

    override fun onResume() {
        super.onResume()
        refreshTrigger.intValue++
    }

    override fun onDestroy() {
        try { unregisterReceiver(introspectReceiver) } catch (_: Exception) {}
        try { unregisterReceiver(commandReceiver) } catch (_: Exception) {}
        super.onDestroy()
    }
}

// ── Data model ──────────────────────────────────────────────────────────────

private data class GameFileInfo(
    val filename: String,
    val description: String,
    val required: Boolean,
    val alternatives: List<String> = emptyList(),
    val downloadUrl: String? = null       // non-null = show [Download] button
)

private data class FileStatus(
    val info: GameFileInfo,
    val found: Boolean,
    val foundName: String?,
    val manifestEntry: AssetManifest.AssetEntry? = null,
    val safUri: String? = null,
    val safSizeBytes: Long = 0
)

// ── Helpers ─────────────────────────────────────────────────────────────────

/** Case-insensitive file lookup (Android ext4 is case-sensitive). */
private fun findFile(dir: File, name: String): String? {
    val files = dir.listFiles() ?: return null
    return files.firstOrNull { it.name.equals(name, ignoreCase = true) }?.name
}

private fun checkFiles(dir: File, fileList: List<GameFileInfo>, manifest: AssetManifest? = null,
                       safManifest: SafManifest? = null): List<FileStatus> {
    val safEntries = safManifest?.read() ?: emptyList()
    return fileList.map { info ->
        val primaryMatch = findFile(dir, info.filename)
        val altMatch = if (primaryMatch == null)
            info.alternatives.firstNotNullOfOrNull { findFile(dir, it) }
        else null
        val foundName = primaryMatch ?: altMatch
        // SAF leave-in-place: if the file isn't on disk, check the SAF manifest.
        val safEntry = if (foundName == null) {
            safEntries.firstOrNull { it.filename.equals(info.filename, ignoreCase = true) }
                ?: info.alternatives.firstNotNullOfOrNull { alt ->
                    safEntries.firstOrNull { it.filename.equals(alt, ignoreCase = true) }
                }
        } else null
        val entry = if (foundName != null) manifest?.getEntry(foundName)
                    else manifest?.getEntry(info.filename)
        FileStatus(info, found = foundName != null || safEntry != null,
                   foundName = foundName ?: if (safEntry != null) info.filename else null,
                   manifestEntry = entry,
                   safUri = safEntry?.contentUri,
                   safSizeBytes = safEntry?.sizeBytes ?: 0)
    }
}

/** Look up the description for a filename from the known file lists. */
private fun descriptionForFile(filename: String): String {
    val lower = filename.lowercase()
    val allFiles = D2_FILES + D2_DEMO_FILES + D1_FILES + MUSIC_FILES
    return allFiles.firstOrNull { info ->
        info.filename.equals(lower, ignoreCase = true) ||
        info.alternatives.any { it.equals(lower, ignoreCase = true) }
    }?.description ?: "Unknown file"
}

/** Describe a file's type based on its extension. */
private fun describeExtension(filename: String): String {
    val ext = filename.substringAfterLast('.', "").lowercase()
    return EXTENSION_TYPES[ext] ?: "[.$ext] \u2014 unknown type"
}

private val EXTENSION_TYPES = mapOf(
    "hog" to ".hog \u2014 mission archive",
    "mn2" to ".mn2 \u2014 Descent II mission descriptor",
    "msn" to ".msn \u2014 Descent I mission descriptor",
    "ham" to ".ham \u2014 global robot/weapon data",
    "vham" to ".vham \u2014 variant HAM (D2X-XL)",
    "pig" to ".pig \u2014 texture/sound container",
    "pog" to ".pog \u2014 texture override pack",
    "pcx" to ".pcx \u2014 briefing/cutscene image",
    "s11" to ".s11 \u2014 11 kHz PCM sound",
    "s22" to ".s22 \u2014 22 kHz PCM sound",
    "hmp" to ".hmp \u2014 HMI-format MIDI music",
    "raw" to ".raw \u2014 raw PCM audio",
    "rl2" to ".rl2 \u2014 Descent II level",
    "rdl" to ".rdl \u2014 Descent I level",
    "mvl" to ".mvl \u2014 movie library archive",
    "dxa" to ".dxa \u2014 Rebirth zip addon file",
    "dtx" to ".dtx \u2014 D2X-XL texture pack",
    "gog" to ".gog \u2014 GOG CD image (Redbook audio)",
    "inst" to ".inst \u2014 GOG CD cue sheet",
)

// ── File definitions ────────────────────────────────────────────────────────

private val D2_FILES = listOf(
    // Required – core engine files
    GameFileInfo("descent2.hog", "Main game data",
        required = true, alternatives = listOf("d2demo.hog")),
    GameFileInfo("descent2.ham", "Models & objects",
        required = true, alternatives = listOf("d2demo.ham")),
    GameFileInfo("groupa.pig", "Main textures",
        required = true, alternatives = listOf("d2demo.pig")),
    GameFileInfo("descent2.s22", "Sound effects (22 kHz)",
        required = true, alternatives = listOf("descent2.s11")),

    // Required – level texture packs
    GameFileInfo("alien1.pig", "Alien 1 level textures", required = true),
    GameFileInfo("alien2.pig", "Alien 2 level textures", required = true),
    GameFileInfo("fire.pig", "Fire level textures", required = true),
    GameFileInfo("ice.pig", "Ice level textures", required = true),
    GameFileInfo("water.pig", "Water level textures", required = true),

    // Optional – movies & extras
    GameFileInfo("intro-h.mvl", "Intro movie",
        required = false, alternatives = listOf("intro-l.mvl")),
    GameFileInfo("other-h.mvl", "Cutscene movies",
        required = false, alternatives = listOf("other-l.mvl")),
    GameFileInfo("robots-h.mvl", "Robot movies",
        required = false, alternatives = listOf("robots-l.mvl")),
    GameFileInfo("d2x.hog", "Vertigo expansion", required = false),
    GameFileInfo("hoard.ham", "Hoard multiplayer mode", required = false),
)

private val D2_DEMO_FILES = listOf(
    GameFileInfo("d2demo.hog", "Demo game data", required = true),
    GameFileInfo("d2demo.ham", "Demo models & objects", required = true),
    GameFileInfo("d2demo.pig", "Demo textures", required = true),
)

/**
 * Detect whether the files on disk (and in SAF manifest) correspond to the
 * D2 demo or the full game, and return the appropriate file list.
 */
private fun detectD2FileList(dir: File, safManifest: SafManifest? = null): List<GameFileInfo> {
    val demoFiles = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig")
    val hasDemoOnDisk = demoFiles.any { findFile(dir, it) != null }
    val hasDemoInSaf = safManifest?.let { sm ->
        val entries = sm.read()
        demoFiles.any { demo -> entries.any { it.filename.equals(demo, ignoreCase = true) } }
    } ?: false
    return if (hasDemoOnDisk || hasDemoInSaf) D2_DEMO_FILES else D2_FILES
}

private val D1_FILES = listOf(
    // Required – core D1 files
    GameFileInfo("descent.hog", "D1 game data", required = true),
    GameFileInfo("descent.pig", "D1 textures", required = true),

    // Optional – downloadable extras
    GameFileInfo("d1xr-mac-demo-sounds.dxa", "Optional sound file",
        required = false,
        downloadUrl = "https://dxx-redux.com/dl/d1xr-mac-demo-sounds.dxa"),
    GameFileInfo("d1xr-hires.dxa", "Optional D1 high-res file",
        required = false,
        downloadUrl = "https://dxx-redux.com/dl/d1xr-hires.dxa"),
)

private val MUSIC_FILES = listOf(
    GameFileInfo("descent_ii.gog", "GOG CD image (Redbook audio)",
        required = false, alternatives = listOf("DESCENT_II.gog")),
    GameFileInfo("descent_ii.inst", "GOG CD cue sheet (Redbook audio)",
        required = false, alternatives = listOf("DESCENT_II.inst")),
)

// ── Demo downloads ──────────────────────────────────────────────────────────

private data class DemoPackage(
    val name: String,
    val url: String,
    val description: String,
    val sizeBytes: Long,
    val files: List<String>,   // expected extracted filenames (lowercase)
)

private val DEMO_DOWNLOADS = listOf(
    DemoPackage(
        name = "D2 Demo",
        url = "https://dxx-redux.com/dl/d2demo.zip",
        description = "Official Descent 2 Demo (3 levels)",
        sizeBytes = 5_500_000L,
        files = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig"),
    ),
)

// ── SAF directory scanning ───────────────────────────────────────────────────

/** All filenames we care about (D2 + D2 Demo + D1 + Music), lowercase for matching. */
private val ALL_GAME_FILENAMES: Set<String> by lazy {
    (D2_FILES + D2_DEMO_FILES + D1_FILES + MUSIC_FILES).flatMap { info ->
        listOf(info.filename) + info.alternatives
    }.map { it.lowercase() }.toSet()
}

/** Result of scanning a user-chosen directory tree. */
private data class FoundFile(
    val name: String,        // original filename (preserving case)
    val uri: Uri             // content:// URI to read from
)

/** Result of extracting a game file from a ZIP archive. */
private data class ExtractedFile(
    val name: String,        // lowercase canonical filename
    val tmpFile: File,       // temp location
    val sha256: String,      // SHA-256 of extracted file
    val sizeBytes: Long      // file size
)

/**
 * Recursively walk a SAF document tree and return game files found.
 * Uses DocumentsContract for efficiency (no MediaStore needed).
 */
private fun scanTreeForGameFiles(context: Context, treeUri: Uri): List<FoundFile> {
    val results = mutableListOf<FoundFile>()
    val docId = DocumentsContract.getTreeDocumentId(treeUri)
    val queue = ArrayDeque<String>()
    queue.add(docId)

    while (queue.isNotEmpty()) {
        val parentId = queue.removeFirst()
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentId)
        val cursor = context.contentResolver.query(
            childrenUri,
            arrayOf(
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE
            ),
            null, null, null
        ) ?: continue

        cursor.use {
            while (it.moveToNext()) {
                val childId = it.getString(0)
                val displayName = it.getString(1) ?: continue
                val mimeType = it.getString(2) ?: ""

                if (mimeType == DocumentsContract.Document.MIME_TYPE_DIR) {
                    queue.add(childId)
                } else if (displayName.lowercase() in ALL_GAME_FILENAMES) {
                    val fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId)
                    results.add(FoundFile(displayName, fileUri))
                }
            }
        }
    }
    return results
}

/**
 * Copy a SAF document to the app's files directory.
 * Returns true on success.
 */
private fun importFile(context: Context, source: FoundFile, destDir: File): Boolean {
    return try {
        // Use lowercase canonical name so the engine finds it
        val canonicalName = source.name.lowercase()
        val destFile = File(destDir, canonicalName)
        context.contentResolver.openInputStream(source.uri)?.use { input ->
            FileOutputStream(destFile).use { output ->
                input.copyTo(output, bufferSize = 8192)
            }
        }
        Log.i("DXX-Setup", "Imported ${source.name} → $canonicalName (${destFile.length()} bytes)")
        true
    } catch (e: Exception) {
        Log.e("DXX-Setup", "Failed to import ${source.name}", e)
        false
    }
}

/** Get the display name (filename) for a content:// URI. */
private fun getDisplayName(context: Context, uri: Uri): String? {
    return try {
        context.contentResolver.query(
            uri,
            arrayOf(android.provider.OpenableColumns.DISPLAY_NAME),
            null, null, null
        )?.use { cursor ->
            if (cursor.moveToFirst()) cursor.getString(0) else null
        }
    } catch (e: Exception) {
        null
    }
}

/**
 * Extract game files from a ZIP archive. Streams one entry at a time to tmpDir.
 * Returns list of extracted files with SHA-256 hashes.
 */
private suspend fun extractZipContents(
    context: Context,
    zipUri: Uri,
    tmpDir: File,
    onProgress: (String) -> Unit
): List<ExtractedFile> = kotlinx.coroutines.withContext(Dispatchers.IO) {
    tmpDir.mkdirs()
    val results = mutableListOf<ExtractedFile>()
    try {
        context.contentResolver.openInputStream(zipUri)?.use { raw ->
            ZipInputStream(raw).use { zis ->
                var entry = zis.nextEntry
                while (entry != null) {
                    val name = entry.name.substringAfterLast('/').lowercase()
                    if (!entry.isDirectory && name in ALL_GAME_FILENAMES) {
                        kotlinx.coroutines.withContext(Dispatchers.Main) {
                            onProgress(name)
                        }
                        val tmpFile = File(tmpDir, name)
                        val digest = java.security.MessageDigest.getInstance("SHA-256")
                        var size = 0L
                        FileOutputStream(tmpFile).use { out ->
                            val buf = ByteArray(8192)
                            while (true) {
                                val n = zis.read(buf)
                                if (n <= 0) break
                                out.write(buf, 0, n)
                                digest.update(buf, 0, n)
                                size += n
                            }
                        }
                        val sha256 = digest.digest().joinToString("") { "%02x".format(it) }
                        results.add(ExtractedFile(name, tmpFile, sha256, size))
                        Log.i("DXX-Setup", "Extracted from ZIP: $name ($size bytes, sha256=${sha256.take(16)}...)")
                    }
                    zis.closeEntry()
                    entry = zis.nextEntry
                }
            }
        }
    } catch (e: Exception) {
        Log.e("DXX-Setup", "ZIP extraction failed", e)
    }
    results
}

/** Clean up temporary extraction directory. */
private fun cleanupTmpDir(filesDir: File) {
    val tmpDir = File(filesDir, "tmp")
    if (tmpDir.exists()) tmpDir.deleteRecursively()
}

// ── Composables ─────────────────────────────────────────────────────────────

@Composable
private fun SetupScreen(
    filesDir: File,
    gameRunning: Boolean,
    refreshTrigger: Int,
    controllerAxes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    onLaunchGame: () -> Unit,
    onRefresh: () -> Unit,
    onDownloadStateChanged: (String, Int) -> Unit = { _, _ -> }
) {
    val fileSetManager = remember { FileSetManager(filesDir).also { it.migrateDefaultSetIfNeeded() } }
    var activeSetName by remember { mutableStateOf(fileSetManager.getActive()) }
    val setDir = remember(activeSetName) { fileSetManager.getSetDir(activeSetName) }
    val manifest = remember(activeSetName) { AssetManifest(setDir) }
    val safManifest = remember(activeSetName) { fileSetManager.safManifestForSet(activeSetName) }
    val d2FileList = remember(refreshTrigger, activeSetName) { detectD2FileList(setDir, safManifest) }
    val d2Statuses = remember(refreshTrigger, activeSetName) { checkFiles(setDir, d2FileList, manifest, safManifest) }
    val d1Statuses = remember(refreshTrigger, activeSetName) { checkFiles(setDir, D1_FILES, manifest, safManifest) }

    // ── Hashing progress state ──────────────────────────────
    var hashingFile by remember { mutableStateOf<String?>(null) }
    var hashingFileIndex by remember { mutableIntStateOf(0) }
    var hashingTotalFiles by remember { mutableIntStateOf(0) }
    var hashingProgress by remember { mutableFloatStateOf(0f) }
    val isHashing = hashingFile != null

    // ── Startup: hash any stale/new files ───────────────────
    LaunchedEffect(activeSetName) {
        val allGameNames = ALL_GAME_FILENAMES
        val staleFiles = manifest.findStaleFiles(allGameNames)
        if (staleFiles.isNotEmpty()) {
            hashingTotalFiles = staleFiles.size
            for ((i, file) in staleFiles.withIndex()) {
                hashingFileIndex = i + 1
                hashingFile = file.name
                hashingProgress = 0f
                val sha256 = AssetManifest.computeSha256(file) { bytesRead, totalBytes ->
                    if (totalBytes > 0) hashingProgress = bytesRead.toFloat() / totalBytes
                }
                manifest.upsert(file.name, sha256, file.length())
            }
            hashingFile = null
            onRefresh()
        }
    }

    val d2RequiredOk = d2Statuses.filter { it.info.required }.all { it.found }
    val d1RequiredOk = d1Statuses.filter { it.info.required }.all { it.found }
    val canLaunch = d2RequiredOk || d1RequiredOk

    // True when zero required files are found for either game
    val noRequiredFiles = d2Statuses.filter { it.info.required }.none { it.found }
            && d1Statuses.filter { it.info.required }.none { it.found }

    // Download state: filename → progress (0..100, -1 = error, -2 = complete)
    val downloadProgress = remember { mutableStateMapOf<String, Int>() }
    val scope = rememberCoroutineScope()

    // ── File detail popup state ─────────────────────────────
    var detailStatus by remember { mutableStateOf<FileStatus?>(null) }
    var detailIsD2 by remember { mutableStateOf(true) }

    // ── Set management dialog state ─────────────────────────
    var showSetDialog by remember { mutableStateOf(false) }

    // ── SAF file-search state ───────────────────────────────
    val context = androidx.compose.ui.platform.LocalContext.current
    var scanResults by remember { mutableStateOf<List<FoundFile>?>(null) }
    var scanning by remember { mutableStateOf(false) }
    var importStatus by remember { mutableStateOf("") }

    // ── Demo download state ─────────────────────────────────
    var demoDownloading by remember { mutableStateOf<String?>(null) }  // package name or null
    var demoDownloadProgress by remember { mutableIntStateOf(0) }
    var demoDownloadError by remember { mutableStateOf<String?>(null) }

    // ── ZIP extraction state ────────────────────────────
    var zipExtracted by remember { mutableStateOf<List<ExtractedFile>?>(null) }
    var zipPackageName by remember { mutableStateOf<String?>(null) }
    var zipExtracting by remember { mutableStateOf(false) }
    var zipProgressFile by remember { mutableStateOf("") }

    val filePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenMultipleDocuments()
    ) { uris: List<Uri> ->
        if (uris.isEmpty()) return@rememberLauncherForActivityResult
        scanning = true
        importStatus = ""
        scope.launch(Dispatchers.IO) {
            val zipUris = mutableListOf<Uri>()
            val gameUris = mutableListOf<FoundFile>()
            for (uri in uris) {
                val name = getDisplayName(context, uri)
                if (name != null) {
                    if (name.lowercase().endsWith(".zip")) {
                        zipUris.add(uri)
                    } else if (name.lowercase() in ALL_GAME_FILENAMES) {
                        gameUris.add(FoundFile(name, uri))
                    }
                }
            }
            withContext(Dispatchers.Main) {
                if (gameUris.isNotEmpty()) {
                    scanResults = gameUris
                }
                scanning = false
            }
            // Handle ZIP files
            if (zipUris.isNotEmpty()) {
                withContext(Dispatchers.Main) { zipExtracting = true }
                val tmpDir = File(filesDir, "tmp")
                val allExtracted = mutableListOf<ExtractedFile>()
                for (zipUri in zipUris) {
                    val extracted = extractZipContents(context, zipUri, tmpDir) { name ->
                        zipProgressFile = name
                    }
                    allExtracted.addAll(extracted)
                }
                // Identify package
                val fileHashes = allExtracted.associate { it.name to it.sha256 }
                val pkgName = KnownVersions.identifyPackage(fileHashes)
                withContext(Dispatchers.Main) {
                    zipExtracted = allExtracted
                    zipPackageName = pkgName
                    zipExtracting = false
                    zipProgressFile = ""
                }
            }
        }
    }

    // ── Page navigation state ────────────────────────────
    var showControllerPage by remember { mutableStateOf(false) }

    MaterialTheme(colorScheme = darkColorScheme()) {
        if (showControllerPage) {
            ControllerConfigPage(
                axes = controllerAxes,
                dpadAxes = dpadAxes,
                axisGeneration = axisGeneration,
                pressedButtons = pressedButtons,
                onBack = { showControllerPage = false }
            )
            return@MaterialTheme
        }
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background
        ) {
            val isLandscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE

            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .safeDrawingPadding()
                    .padding(if (isLandscape) 8.dp else 16.dp)
            ) {
                // ── Title + About ────────────────────────────
                var showAbout by remember { mutableStateOf(false) }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "DXX-Redux Setup",
                        fontSize = 22.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.primary
                    )
                    TextButton(onClick = { showAbout = true }) {
                        Text("About", fontSize = 12.sp)
                    }
                }
                if (!isLandscape) Spacer(modifier = Modifier.height(8.dp))

                if (showAbout) {
                    AlertDialog(
                        onDismissRequest = { showAbout = false },
                        confirmButton = {
                            TextButton(onClick = { showAbout = false }) { Text("OK") }
                        },
                        title = { Text("DXX-Redux") },
                        text = {
                            val arch = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown"
                            Text(
                                "Build ${BuildInfo.GIT_COMMIT_COUNT}" +
                                " (${BuildInfo.GIT_SHORT_HASH})" +
                                " ${BuildInfo.BUILD_TYPE}\n" +
                                "Date: ${BuildInfo.BUILD_DATE}" +
                                " ${BuildInfo.BUILD_TIME}\n" +
                                "Arch: $arch\n" +
                                "Renderer: ${BuildConfig.RENDERER}"
                            )
                        }
                    )
                }

                // ── File detail popup ──
                detailStatus?.let { status ->
                    FileDetailDialog(
                        status = status,
                        onDismiss = { detailStatus = null },
                        onDelete = when {
                            // SAF leave-in-place file — unlink from SAF manifest
                            status.safUri != null -> {
                                {
                                    safManifest.remove(status.info.filename)
                                    detailStatus = null
                                    onRefresh()
                                }
                            }
                            // File on disk with manifest entry — delete file + manifest entry
                            status.found && status.manifestEntry != null -> {
                                {
                                    val entry = status.manifestEntry
                                    File(setDir, entry.filename).delete()
                                    manifest.remove(entry.filename)
                                    detailStatus = null
                                    onRefresh()
                                }
                            }
                            // External import but missing from disk — forget the manifest entry
                            status.manifestEntry?.isExternal == true -> {
                                {
                                    manifest.remove(status.manifestEntry.filename)
                                    detailStatus = null
                                    onRefresh()
                                }
                            }
                            else -> null
                        }
                    )
                }

                // ── Set management dialog ──
                if (showSetDialog) {
                    SetManagementDialog(
                        fileSetManager = fileSetManager,
                        activeSetName = activeSetName,
                        onSwitchSet = { newSet ->
                            fileSetManager.setActive(newSet)
                            activeSetName = newSet
                            showSetDialog = false
                            onRefresh()
                        },
                        onDismiss = { showSetDialog = false }
                    )
                }

                // ── Shared composable blocks ──

                val filesPane: @Composable ColumnScope.() -> Unit = {
                    // ── Active set indicator ──────────────────────
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(bottom = 4.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "Files in use: ",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        Text(
                            text = activeSetName,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.Bold,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                        Spacer(modifier = Modifier.weight(1f))
                        TextButton(
                            onClick = { showSetDialog = true },
                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                            modifier = Modifier.height(28.dp)
                        ) {
                            Text("Change", fontSize = 12.sp)
                        }
                    }

                    // ── Missing-files help ──────────────────────
                    if (!canLaunch && !gameRunning) {
                        MissingFilesHelp()
                        Spacer(modifier = Modifier.height(8.dp))

                        // ── Demo download offers ──────────────────
                        for (demo in DEMO_DOWNLOADS) {
                            Card(
                                modifier = Modifier.fillMaxWidth(),
                                colors = CardDefaults.cardColors(
                                    containerColor = MaterialTheme.colorScheme.secondaryContainer
                                )
                            ) {
                                Column(modifier = Modifier.padding(12.dp)) {
                                    Text(
                                        text = "\uD83C\uDFAE ${demo.name}",
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer
                                    )
                                    Text(
                                        text = "${demo.description} (${demo.sizeBytes / 1_000_000} MB)",
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer
                                    )
                                    Spacer(modifier = Modifier.height(6.dp))
                                    if (demoDownloading == demo.name) {
                                        Text(
                                            text = "Downloading\u2026 $demoDownloadProgress%",
                                            fontSize = 12.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer
                                        )
                                        LinearProgressIndicator(
                                            progress = { demoDownloadProgress / 100f },
                                            modifier = Modifier.fillMaxWidth().height(4.dp),
                                            color = MaterialTheme.colorScheme.primary,
                                            trackColor = MaterialTheme.colorScheme.primaryContainer,
                                        )
                                    } else {
                                        if (demoDownloadError != null) {
                                            Text(
                                                text = "Error: $demoDownloadError",
                                                fontSize = 12.sp,
                                                color = MaterialTheme.colorScheme.error
                                            )
                                            Spacer(modifier = Modifier.height(4.dp))
                                        }
                                        Button(
                                            onClick = {
                                                demoDownloadError = null
                                                demoDownloading = demo.name
                                                demoDownloadProgress = 0
                                                scope.launch {
                                                    val tmpDir = File(filesDir, "tmp")
                                                    tmpDir.mkdirs()
                                                    val zipFile = File(tmpDir, "${demo.name.lowercase().replace(' ', '_')}.zip")
                                                    // Download ZIP
                                                    var downloadOk = false
                                                    downloadFile(
                                                        url = demo.url,
                                                        destDir = tmpDir,
                                                        filename = zipFile.name,
                                                        onProgress = { pct -> demoDownloadProgress = pct },
                                                        onDone = { success -> downloadOk = success }
                                                    )
                                                    if (!downloadOk) {
                                                        demoDownloading = null
                                                        demoDownloadError = "Download failed"
                                                        cleanupTmpDir(filesDir)
                                                        return@launch
                                                    }
                                                    // Extract ZIP contents
                                                    val zipUri = android.net.Uri.fromFile(zipFile)
                                                    val extracted = extractZipContents(context, zipUri, tmpDir) { _ -> }
                                                    if (extracted.isEmpty()) {
                                                        demoDownloading = null
                                                        demoDownloadError = "No game files found in ZIP"
                                                        cleanupTmpDir(filesDir)
                                                        return@launch
                                                    }
                                                    // Move files to setDir
                                                    var imported = 0
                                                    for (ef in extracted) {
                                                        val destFile = File(setDir, ef.name)
                                                        val ok = withContext(Dispatchers.IO) {
                                                            try {
                                                                ef.tmpFile.copyTo(destFile, overwrite = true)
                                                                true
                                                            } catch (e: Exception) {
                                                                Log.e("DXX-Setup", "Failed to move demo file ${ef.name}", e)
                                                                false
                                                            }
                                                        }
                                                        if (ok) {
                                                            imported++
                                                            manifest.upsert(ef.name, ef.sha256, ef.sizeBytes)
                                                        }
                                                    }
                                                    cleanupTmpDir(filesDir)
                                                    demoDownloading = null
                                                    importStatus = "Installed ${demo.name}: $imported files."
                                                    onRefresh()
                                                }
                                            },
                                            enabled = demoDownloading == null
                                        ) {
                                            Text("Download & Install", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                            Spacer(modifier = Modifier.height(8.dp))
                        }
                    }

                    // ── Hashing progress bar ──
                    if (isHashing) {
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors = CardDefaults.cardColors(
                                containerColor = MaterialTheme.colorScheme.primaryContainer
                            )
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Text(
                                    text = "Hashing: $hashingFile ($hashingFileIndex/$hashingTotalFiles)",
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = MaterialTheme.colorScheme.onPrimaryContainer
                                )
                                Spacer(modifier = Modifier.height(6.dp))
                                // Per-file progress
                                LinearProgressIndicator(
                                    progress = { hashingProgress },
                                    modifier = Modifier.fillMaxWidth().height(8.dp),
                                    color = MaterialTheme.colorScheme.primary,
                                    trackColor = MaterialTheme.colorScheme.primaryContainer,
                                )
                                // Overall progress across all files
                                if (hashingTotalFiles > 1) {
                                    Spacer(modifier = Modifier.height(4.dp))
                                    val overallProgress = ((hashingFileIndex - 1).toFloat() + hashingProgress) / hashingTotalFiles
                                    LinearProgressIndicator(
                                        progress = { overallProgress },
                                        modifier = Modifier.fillMaxWidth().height(4.dp),
                                        color = MaterialTheme.colorScheme.tertiary,
                                        trackColor = MaterialTheme.colorScheme.primaryContainer,
                                    )
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // ── Import files button ──
                    Button(
                        onClick = { filePickerLauncher.launch(arrayOf("application/octet-stream", "application/zip", "*/*")) },
                        enabled = !scanning && !isHashing && !zipExtracting,
                        modifier = Modifier.fillMaxWidth().height(44.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = MaterialTheme.colorScheme.secondary
                        )
                    ) {
                        Text(
                            text = if (scanning || zipExtracting) "Importing\u2026"
                                   else "\uD83D\uDCC2 Select Game Files or ZIP to Import",
                            fontSize = 14.sp
                        )
                    }
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        text = "Select .hog, .ham, .pig files or a .zip archive from Downloads or any folder.",
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.height(8.dp))

                    // ── Scan results / import card ──────────────
                    if (scanResults != null) {
                        val found = scanResults!!
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors = CardDefaults.cardColors(
                                containerColor = if (found.isEmpty())
                                    MaterialTheme.colorScheme.errorContainer
                                else MaterialTheme.colorScheme.secondaryContainer
                            )
                        ) {
                            Column(modifier = Modifier
                                .padding(12.dp)
                                .verticalScroll(rememberScrollState())
                            ) {
                                if (found.isEmpty()) {
                                    Text(
                                        text = "No game files found in that folder.",
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onErrorContainer
                                    )
                                    Text(
                                        text = "Try selecting the folder that contains .hog, .ham, and .pig files.",
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onErrorContainer
                                    )
                                } else {
                                    Text(
                                        text = "Found ${found.size} game file(s): ${found.joinToString(", ") { it.name }}",
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer
                                    )
                                    Spacer(modifier = Modifier.height(8.dp))
                                    Row(
                                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                                    ) {
                                        Button(
                                            onClick = {
                                                scope.launch {
                                                    var imported = 0
                                                    hashingTotalFiles = found.size
                                                    for ((i, f) in found.withIndex()) {
                                                        hashingFileIndex = i + 1
                                                        hashingFile = f.name
                                                        hashingProgress = 0f
                                                        val canonicalName = f.name.lowercase()
                                                        val destFile = File(setDir, canonicalName)
                                                        // Determine track: native data-dir vs external
                                                        val existedBefore = destFile.exists()
                                                        val existingEntry = manifest.getEntry(canonicalName)
                                                        val ok = withContext(Dispatchers.IO) {
                                                            importFile(context, f, setDir)
                                                        }
                                                        if (ok) {
                                                            imported++
                                                            val sha256 = AssetManifest.computeSha256(destFile) { bytesRead, totalBytes ->
                                                                if (totalBytes > 0) hashingProgress = bytesRead.toFloat() / totalBytes
                                                            }
                                                            // Data-dir track: file existed on disk without a sourceUri
                                                            val sourceUri = if (existedBefore && (existingEntry == null || !existingEntry.isExternal)) {
                                                                null
                                                            } else {
                                                                f.uri.toString()
                                                            }
                                                            manifest.upsert(destFile.name, sha256, destFile.length(), sourceUri)
                                                        }
                                                    }
                                                    hashingFile = null
                                                    importStatus = "Imported $imported of ${found.size} files."
                                                    scanResults = null
                                                    onRefresh()
                                                }
                                            }
                                        ) {
                                            Text("Import All", fontSize = 13.sp)
                                        }
                                        OutlinedButton(
                                            onClick = { scanResults = null }
                                        ) {
                                            Text("Dismiss", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    if (importStatus.isNotEmpty()) {
                        Text(
                            text = importStatus,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = Color(0xFF4CAF50),
                            modifier = Modifier.padding(bottom = 8.dp)
                        )
                    }

                    // ── ZIP extraction progress ─────────────────
                    if (zipExtracting) {
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors = CardDefaults.cardColors(
                                containerColor = MaterialTheme.colorScheme.secondaryContainer
                            )
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Text(
                                    text = "Extracting ZIP\u2026",
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 14.sp,
                                    color = MaterialTheme.colorScheme.onSecondaryContainer
                                )
                                if (zipProgressFile.isNotEmpty()) {
                                    Text(
                                        text = zipProgressFile,
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer
                                    )
                                }
                                Spacer(modifier = Modifier.height(4.dp))
                                LinearProgressIndicator(
                                    modifier = Modifier.fillMaxWidth().height(4.dp),
                                    color = MaterialTheme.colorScheme.primary,
                                    trackColor = MaterialTheme.colorScheme.primaryContainer,
                                )
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // ── ZIP results card ───────────────────────
                    if (zipExtracted != null) {
                        val extracted = zipExtracted!!
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors = CardDefaults.cardColors(
                                containerColor = if (extracted.isEmpty())
                                    MaterialTheme.colorScheme.errorContainer
                                else MaterialTheme.colorScheme.secondaryContainer
                            )
                        ) {
                            Column(modifier = Modifier
                                .padding(12.dp)
                                .verticalScroll(rememberScrollState())
                            ) {
                                if (extracted.isEmpty()) {
                                    Text(
                                        text = "No game files found in ZIP archive.",
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onErrorContainer
                                    )
                                    Spacer(modifier = Modifier.height(4.dp))
                                    OutlinedButton(
                                        onClick = {
                                            zipExtracted = null
                                            zipPackageName = null
                                            cleanupTmpDir(filesDir)
                                        }
                                    ) {
                                        Text("Dismiss", fontSize = 13.sp)
                                    }
                                } else {
                                    if (zipPackageName != null) {
                                        Text(
                                            text = "\u2705 Recognized: $zipPackageName",
                                            fontWeight = FontWeight.Bold,
                                            fontSize = 14.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer
                                        )
                                    } else {
                                        Text(
                                            text = "Found ${extracted.size} game file(s)",
                                            fontWeight = FontWeight.Bold,
                                            fontSize = 14.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer
                                        )
                                    }
                                    Spacer(modifier = Modifier.height(4.dp))
                                    for (ef in extracted) {
                                        Text(
                                            text = "\u2022 ${ef.name} (${ef.sizeBytes / 1024} KB)",
                                            fontSize = 12.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer
                                        )
                                    }
                                    Spacer(modifier = Modifier.height(8.dp))
                                    Row(
                                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                                    ) {
                                        Button(
                                            onClick = {
                                                scope.launch {
                                                    var imported = 0
                                                    hashingTotalFiles = extracted.size
                                                    for ((i, ef) in extracted.withIndex()) {
                                                        hashingFileIndex = i + 1
                                                        hashingFile = ef.name
                                                        hashingProgress = 1f
                                                        val destFile = File(setDir, ef.name)
                                                        val ok = withContext(Dispatchers.IO) {
                                                            try {
                                                                ef.tmpFile.copyTo(destFile, overwrite = true)
                                                                true
                                                            } catch (e: Exception) {
                                                                Log.e("DXX-Setup", "Failed to move extracted file ${ef.name}", e)
                                                                false
                                                            }
                                                        }
                                                        if (ok) {
                                                            imported++
                                                            manifest.upsert(ef.name, ef.sha256, ef.sizeBytes)
                                                        }
                                                    }
                                                    hashingFile = null
                                                    importStatus = "Imported $imported of ${extracted.size} files from ZIP."
                                                    zipExtracted = null
                                                    zipPackageName = null
                                                    cleanupTmpDir(filesDir)
                                                    onRefresh()
                                                }
                                            }
                                        ) {
                                            Text("Import to Current Set", fontSize = 13.sp)
                                        }
                                        OutlinedButton(
                                            onClick = {
                                                zipExtracted = null
                                                zipPackageName = null
                                                cleanupTmpDir(filesDir)
                                            }
                                        ) {
                                            Text("Dismiss", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // ── File sections ────────────────────
                    var d2Expanded by remember { mutableStateOf(false) }
                    var d1Expanded by remember { mutableStateOf(false) }

                    GameSectionHeader(
                        title = "Descent 2",
                        ready = d2RequiredOk,
                        expanded = d2Expanded,
                        onToggle = { d2Expanded = !d2Expanded }
                    )

                    if (d2Expanded) {
                        SectionHeader("Required Files")
                        d2Statuses.filter { it.info.required }.forEach {
                            FileStatusRow(it) { detailStatus = it; detailIsD2 = true }
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        SectionHeader("Optional Files")
                        d2Statuses.filter { !it.info.required }.forEach {
                            FileStatusRow(it) { detailStatus = it; detailIsD2 = true }
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))

                    GameSectionHeader(
                        title = "Descent 1",
                        ready = d1RequiredOk,
                        expanded = d1Expanded,
                        onToggle = { d1Expanded = !d1Expanded }
                    )

                    if (d1Expanded) {
                        SectionHeader("Required Files")
                        d1Statuses.filter { it.info.required }.forEach {
                            FileStatusRow(it) { detailStatus = it; detailIsD2 = false }
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        SectionHeader("Optional Files")
                        d1Statuses.filter { !it.info.required }.forEach { status ->
                        if (!status.found && status.info.downloadUrl != null) {
                            DownloadableFileRow(
                                status = status,
                                progress = downloadProgress[status.info.filename],
                                onInfo = { detailStatus = status; detailIsD2 = false },
                                onDownload = {
                                    scope.launch {
                                        downloadFile(
                                            url = status.info.downloadUrl,
                                            destDir = setDir,
                                            filename = status.info.filename,
                                            onProgress = { pct ->
                                                downloadProgress[status.info.filename] = pct
                                                onDownloadStateChanged(status.info.filename, pct)
                                            },
                                            onDone = { success ->
                                                val code = if (success) -2 else -1
                                                downloadProgress[status.info.filename] = code
                                                onDownloadStateChanged(status.info.filename, code)
                                                if (success) onRefresh()
                                            }
                                        )
                                    }
                                }
                            )
                        } else {
                            FileStatusRow(status) { detailStatus = status; detailIsD2 = false }
                        }
                    }
                    } // end if (d1Expanded)

                    Spacer(modifier = Modifier.height(16.dp))
                    MusicInfoSection(filesDir = filesDir, refreshTrigger = refreshTrigger, hasMidiSource = d2RequiredOk || d1RequiredOk)
                }

                val controlsPane: @Composable ColumnScope.() -> Unit = {
                    val prefs = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
                    ControllerSection(
                        axes = controllerAxes,
                        dpadAxes = dpadAxes,
                        axisGeneration = axisGeneration,
                        pressedButtons = pressedButtons,
                        prefs = prefs,
                        onDefineControls = { showControllerPage = true }
                    )

                    // ── Resolution picker ────────────────
                    Spacer(modifier = Modifier.height(12.dp))
                    ResolutionPicker(prefs = prefs, filesDir = filesDir)

                    Spacer(modifier = Modifier.height(12.dp))
                    Button(
                        onClick = { android.os.Process.killProcess(android.os.Process.myPid()) },
                        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary),
                        modifier = Modifier.fillMaxWidth().height(44.dp)
                    ) {
                        Text("Restart game", fontSize = 14.sp)
                    }

                    Spacer(modifier = Modifier.height(16.dp))
                    Button(
                        onClick = onLaunchGame,
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(56.dp),
                        enabled = (canLaunch || gameRunning) && !isHashing,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if ((!canLaunch && !gameRunning) || isHashing)
                                MaterialTheme.colorScheme.surfaceVariant
                            else
                                MaterialTheme.colorScheme.primary
                        )
                    ) {
                        Text(
                            text = if (gameRunning) "Return to Game" else "Launch Game",
                            fontSize = 18.sp
                        )
                    }
                }

                // ── Layout: landscape = side-by-side, portrait = stacked ──

                if (isLandscape) {
                    Row(modifier = Modifier.weight(1f)) {
                        val leftScroll = rememberScrollState()
                        Box(modifier = Modifier.weight(1f).fillMaxHeight()) {
                            Column(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .verticalScroll(leftScroll)
                                    .padding(end = 8.dp)
                            ) {
                                filesPane()
                            }
                            ScrollArrows(leftScroll)
                        }
                        val rightScroll = rememberScrollState()
                        Box(modifier = Modifier.weight(1f).fillMaxHeight()) {
                            Column(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .verticalScroll(rightScroll)
                                    .padding(start = 8.dp)
                            ) {
                                controlsPane()
                            }
                            ScrollArrows(rightScroll)
                        }
                    }
                } else {
                    val portraitScroll = rememberScrollState()
                    Box(modifier = Modifier.weight(1f)) {
                        Column(
                            modifier = Modifier
                                .fillMaxSize()
                                .verticalScroll(portraitScroll)
                        ) {
                            filesPane()
                            Spacer(modifier = Modifier.height(16.dp))
                            controlsPane()
                        }
                        ScrollArrows(portraitScroll)
                    }
                }
            }
        }
    }
}

@Composable
private fun BoxScope.ScrollArrows(scrollState: ScrollState) {
    if (scrollState.canScrollBackward) {
        Surface(
            modifier = Modifier.align(Alignment.TopCenter).padding(top = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowUp,
                contentDescription = "Scroll up",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
    if (scrollState.canScrollForward) {
        Surface(
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowDown,
                contentDescription = "Scroll down",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun GameSectionHeader(
    title: String,
    ready: Boolean,
    expanded: Boolean,
    onToggle: () -> Unit,
    notReadyLabel: String = "\u2717 Missing"
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = title,
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(1f)
        )
        Text(
            text = if (ready) "\u2713 Ready" else notReadyLabel,
            color = if (ready) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold
        )
        Spacer(modifier = Modifier.width(8.dp))
        TextButton(
            onClick = onToggle,
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp)
        ) {
            Text(
                text = if (expanded) "Hide Files" else "Show Files",
                fontSize = 12.sp
            )
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp)
    )
}

@Composable
private fun SectionHeader(title: String) {
    Text(
        text = title,
        fontSize = 15.sp,
        fontWeight = FontWeight.SemiBold,
        color = MaterialTheme.colorScheme.onSurface,
        modifier = Modifier.padding(bottom = 4.dp, top = 2.dp)
    )
}

@Composable
private fun MusicInfoSection(filesDir: File, refreshTrigger: Int, hasMidiSource: Boolean = false) {
    val musicStatuses = remember(refreshTrigger) { checkFiles(filesDir, MUSIC_FILES) }
    val redbookReady = musicStatuses.all { it.found }
    var expanded by remember { mutableStateOf(false) }
    var detailStatus by remember { mutableStateOf<FileStatus?>(null) }
    val musicLabel = when {
        redbookReady -> "\u2713 Ready"
        hasMidiSource -> "\u2717 Missing, will use MIDI"
        else -> "\u2717 Missing"
    }
    GameSectionHeader(
        title = "Music",
        ready = redbookReady,
        expanded = expanded,
        onToggle = { expanded = !expanded },
        notReadyLabel = musicLabel
    )
    if (expanded) {
        Text(
            text = "MIDI audio is supported from game files (with a built-in MIDI library). " +
                   "Redbook audio from the GOG bin/cue is supported.",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(start = 4.dp, end = 4.dp, bottom = 8.dp)
        )
        musicStatuses.forEach { status ->
            FileStatusRow(status) { detailStatus = status }
        }
    }
    detailStatus?.let { st ->
        FileDetailDialog(status = st, onDismiss = { detailStatus = null })
    }
}

/** Format epoch millis as a human-readable date/time string. */
private fun formatTimestamp(millis: Long): String {
    val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US)
    return sdf.format(Date(millis))
}

/** Format byte size as human-readable (KB, MB, GB). */
private fun formatSize(bytes: Long): String = when {
    bytes >= 1_073_741_824 -> "%.2f GB".format(bytes / 1_073_741_824.0)
    bytes >= 1_048_576 -> "%.1f MB".format(bytes / 1_048_576.0)
    bytes >= 1_024 -> "%.0f KB".format(bytes / 1_024.0)
    else -> "$bytes B"
}

@Composable
private fun FileDetailDialog(
    status: FileStatus,
    onDismiss: () -> Unit,
    onDelete: (() -> Unit)? = null
) {
    val entry = status.manifestEntry
    val name = status.foundName ?: status.info.filename
    val description = descriptionForFile(name)
    val isMissing = !status.found && entry != null
    val isExternal = entry?.isExternal == true
    var confirmingDelete by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        dismissButton = if (onDelete != null) {
            {
                if (status.safUri != null) {
                    // SAF leave-in-place: single-step "Unlink"
                    TextButton(onClick = onDelete) {
                        Text("Unlink", color = MaterialTheme.colorScheme.error)
                    }
                } else if (isExternal) {
                    // External files: single-step "Forget"
                    TextButton(onClick = onDelete) {
                        Text("Forget", color = MaterialTheme.colorScheme.error)
                    }
                } else if (!confirmingDelete) {
                    // Data-dir files: first step
                    TextButton(onClick = { confirmingDelete = true }) {
                        Text("Delete from data folder?", color = MaterialTheme.colorScheme.error)
                    }
                } else {
                    // Data-dir files: confirmation step
                    TextButton(onClick = onDelete) {
                        Text("Are you sure? Delete", color = MaterialTheme.colorScheme.error)
                    }
                }
            }
        } else null,
        title = {
            Text(name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            val scrollState = rememberScrollState()
            Box {
            Column(modifier = Modifier.verticalScroll(scrollState)) {
                // Category / description
                DetailRow("Category", description)
                DetailRow("Type", describeExtension(name))

                // Status
                val statusText = when {
                    status.found -> "Found"
                    isMissing -> "Error: not found"
                    else -> "Missing"
                }
                DetailRow("Status", statusText)
                if (status.info.required) {
                    DetailRow("Required", "Yes")
                }

                // Location
                if (status.safUri != null) {
                    DetailRow("Location", "leave-in-place (linked)")
                    if (status.safSizeBytes > 0) {
                        DetailRow("Size", formatSize(status.safSizeBytes))
                    }
                } else if (isExternal && entry?.sourceUri != null) {
                    DetailRow("Location", entry.sourceUri)
                } else if (entry != null) {
                    DetailRow("Location", "(in data folder)")
                }

                if (entry != null) {
                    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                    // File details from manifest
                    DetailRow("File on disk", entry.filename)
                    DetailRow("Size", formatSize(entry.sizeBytes))
                    DetailRow("Imported", formatTimestamp(entry.importedAt))

                    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                    // SHA-256 (full, selectable)
                    Text("SHA-256", fontSize = 11.sp, fontWeight = FontWeight.SemiBold,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                    SelectionContainer {
                        Text(entry.sha256, fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurface,
                            modifier = Modifier.padding(bottom = 4.dp))
                    }

                    // Version match
                    if (entry.versionName != null) {
                        DetailRow("Version match", entry.versionName)
                    } else {
                        DetailRow("Version match", "Unknown (#${entry.shortHash})")
                    }

                    if (isMissing) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                        Text(
                            "This file was previously imported but is no longer on disk.",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.error
                        )
                    }
                } else if (!status.found) {
                    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                    if (status.info.alternatives.isNotEmpty()) {
                        DetailRow("Alternatives",
                            status.info.alternatives.joinToString(", "))
                    }
                    if (status.info.downloadUrl != null) {
                        DetailRow("Download", status.info.downloadUrl)
                    }
                }
            }
            ScrollArrows(scrollState)
            }
        }
    )
}

@Composable
private fun DetailRow(label: String, value: String) {
    Row(modifier = Modifier.padding(vertical = 2.dp)) {
        Text("$label: ", fontSize = 12.sp, fontWeight = FontWeight.SemiBold,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurface)
    }
}

/**
 * Update ResolutionX/ResolutionY in descent.cfg.
 * If the file exists, replace existing lines; otherwise create with just those keys.
 */
private fun updateDescentCfgResolution(filesDir: File, resolution: String) {
    val parts = resolution.split("x")
    val w = parts.getOrNull(0)?.toIntOrNull() ?: return
    val h = parts.getOrNull(1)?.toIntOrNull() ?: return
    val cfgFile = File(filesDir, "descent.cfg")

    if (cfgFile.exists()) {
        var text = cfgFile.readText()
        val rxRegex = Regex("^ResolutionX=.*$", RegexOption.MULTILINE)
        val ryRegex = Regex("^ResolutionY=.*$", RegexOption.MULTILINE)
        text = if (rxRegex.containsMatchIn(text)) {
            rxRegex.replace(text, "ResolutionX=$w")
        } else {
            text.trimEnd() + "\nResolutionX=$w\n"
        }
        text = if (ryRegex.containsMatchIn(text)) {
            ryRegex.replace(text, "ResolutionY=$h")
        } else {
            text.trimEnd() + "\nResolutionY=$h\n"
        }
        cfgFile.writeText(text)
    } else {
        cfgFile.writeText("ResolutionX=$w\nResolutionY=$h\n")
    }
    Log.i("DXX-Setup", "Updated descent.cfg: ResolutionX=$w ResolutionY=$h")
}

@Composable
private fun ResolutionPicker(prefs: SharedPreferences, filesDir: File) {
    val options = listOf("640x480" to "Low (640×480)", "960x720" to "Medium (960×720)", "1280x960" to "High (1280×960)")
    var selected by remember { mutableStateOf(prefs.getString("render_resolution", "640x480") ?: "640x480") }

    Text("Render Resolution", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    options.forEach { (value, label) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 2.dp)
        ) {
            RadioButton(
                selected = selected == value,
                onClick = {
                    selected = value
                    prefs.edit().putString("render_resolution", value).apply()
                    updateDescentCfgResolution(filesDir, value)
                }
            )
            Text(text = label, fontSize = 13.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
    Text(
        "Takes effect on next launch",
        fontSize = 11.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant
    )
}

@Composable
private fun ControllerSection(
    axes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    prefs: SharedPreferences,
    onDefineControls: () -> Unit = {}
) {
    // Poll for controller connect/disconnect every 1 second
    var pollTick by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) {
        while (true) {
            kotlinx.coroutines.delay(1000)
            pollTick++
        }
    }

    // Detect connected gamepads (re-evaluated on axis events OR poll tick)
    val gamepads = remember(axisGeneration, pollTick) {
        InputDevice.getDeviceIds().toList()
            .mapNotNull { InputDevice.getDevice(it) }
            .filter { d ->
                val src = d.sources
                src and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                src and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
            }
    }

    val hasController = gamepads.isNotEmpty()
    var expanded by remember { mutableStateOf(false) }

    // ── Header ──
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = "Controller",
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(1f)
        )
        Text(
            text = if (hasController) "\u2713 ${gamepads.first().name}"
                   else "\u2717 Not detected",
            color = if (hasController) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold
        )
        if (hasController) {
            Spacer(modifier = Modifier.width(8.dp))
            TextButton(
                onClick = { expanded = !expanded },
                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                modifier = Modifier.height(28.dp)
            ) {
                Text(
                    text = if (expanded) "Hide" else "Test",
                    fontSize = 12.sp
                )
            }
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp)
    )

    // ── Touch overlay toggle ──
    val defaultOverlay = !hasController
    var touchOverlay by remember {
        mutableStateOf(prefs.getBoolean("touch_overlay_enabled", defaultOverlay))
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Checkbox(
            checked = touchOverlay,
            onCheckedChange = { checked ->
                touchOverlay = checked
                prefs.edit().putBoolean("touch_overlay_enabled", checked).apply()
            },
            modifier = Modifier.height(24.dp)
        )
        Spacer(modifier = Modifier.width(4.dp))
        Text(
            text = "Touch controls overlay",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurface
        )
    }

    // ── Define Controls button ──
    OutlinedButton(
        onClick = onDefineControls,
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
        modifier = Modifier.height(32.dp).padding(vertical = 2.dp)
    ) {
        Text("Define Controls", fontSize = 12.sp)
    }

    if (expanded && hasController) {
        // Read axes from the array (axisGeneration triggers recomposition)
        @Suppress("UNUSED_EXPRESSION") axisGeneration
        val lx = axes[0]; val ly = axes[1]
        val rx = axes[2]; val ry = axes[3]
        val lt = axes[4]; val rt = axes[5]

        val axisColor = MaterialTheme.colorScheme.onSurfaceVariant
        val labelColor = MaterialTheme.colorScheme.onSurface

        Text("Analog Sticks", fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
            color = labelColor, modifier = Modifier.padding(bottom = 2.dp))
        Row(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f)) {
                Text("Left Stick", fontSize = 12.sp, color = labelColor)
                Text("  X: ${"%.2f".format(lx)}", fontSize = 12.sp, color = axisColor)
                Text("  Y: ${"%.2f".format(ly)}", fontSize = 12.sp, color = axisColor)
            }
            Column(modifier = Modifier.weight(1f)) {
                Text("Right Stick", fontSize = 12.sp, color = labelColor)
                Text("  X: ${"%.2f".format(rx)}", fontSize = 12.sp, color = axisColor)
                Text("  Y: ${"%.2f".format(ry)}", fontSize = 12.sp, color = axisColor)
            }
        }

        Spacer(modifier = Modifier.height(4.dp))
        Text("Triggers", fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
            color = labelColor, modifier = Modifier.padding(bottom = 2.dp))
        Row(modifier = Modifier.fillMaxWidth()) {
            Text("  L: ${"%.2f".format(lt)}", fontSize = 12.sp, color = axisColor,
                modifier = Modifier.weight(1f))
            Text("  R: ${"%.2f".format(rt)}", fontSize = 12.sp, color = axisColor,
                modifier = Modifier.weight(1f))
        }

        val hatX = dpadAxes[0]; val hatY = dpadAxes[1]
        val dpadDir = buildString {
            if (hatY < -0.5f) append("Up ")
            if (hatY >  0.5f) append("Down ")
            if (hatX < -0.5f) append("Left ")
            if (hatX >  0.5f) append("Right ")
        }.trimEnd().ifEmpty { "(none)" }
        Spacer(modifier = Modifier.height(4.dp))
        Text("D-Pad", fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
            color = labelColor, modifier = Modifier.padding(bottom = 2.dp))
        Text("  $dpadDir", fontSize = 12.sp,
            color = if (dpadDir == "(none)") axisColor else Color(0xFF4CAF50))

        Spacer(modifier = Modifier.height(4.dp))
        Text("Buttons", fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
            color = labelColor, modifier = Modifier.padding(bottom = 2.dp))
        Text(
            text = if (pressedButtons.isEmpty()) "  (none pressed)"
                   else "  " + pressedButtons.joinToString(", "),
            fontSize = 12.sp,
            color = if (pressedButtons.isEmpty()) axisColor else Color(0xFF4CAF50)
        )
        Spacer(modifier = Modifier.height(8.dp))
    }
}

@Composable
private fun FileStatusRow(status: FileStatus, onClick: (() -> Unit)? = null) {
    val isMissing = !status.found && status.manifestEntry != null
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .let { if (onClick != null) it.clickable(onClick = onClick) else it }
            .padding(vertical = 1.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = when {
                status.found -> "\u2713"
                isMissing -> "\u26A0"
                else -> "\u2717"
            },
            color = when {
                status.found -> Color(0xFF4CAF50)
                isMissing -> Color(0xFFFF9800)  // orange warning
                else -> Color(0xFFF44336)
            },
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp)
        )

        val name = status.foundName ?: status.info.filename
        val altHint = if (!status.found && !isMissing && status.info.alternatives.isNotEmpty())
            " (or ${status.info.alternatives.joinToString(", ")})"
        else ""
        val versionHint = if (status.found && status.manifestEntry != null)
            " [${status.manifestEntry.versionDisplay}]"
        else ""
        val missingHint = if (isMissing) " [Error: not found]" else ""
        Text(
            text = "$name \u2014 ${status.info.description}$altHint$versionHint$missingHint",
            color = when {
                status.found -> MaterialTheme.colorScheme.onSurface
                isMissing -> Color(0xFFFF9800)
                else -> MaterialTheme.colorScheme.onSurfaceVariant
            },
            fontSize = 13.sp,
            maxLines = 1,
            modifier = Modifier.weight(1f)
        )
    }
}

@Composable
private fun DownloadableFileRow(
    status: FileStatus,
    progress: Int?,        // null = not started, 0..100 = %, -1 = error, -2 = done
    onDownload: () -> Unit,
    onInfo: (() -> Unit)? = null
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = "\u2717",
            color = Color(0xFFFFA726),   // orange for optional missing
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp)
        )

        Text(
            text = "${status.info.filename} \u2014 ${status.info.description}",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 13.sp,
            maxLines = 1,
            modifier = Modifier
                .weight(1f)
                .then(if (onInfo != null) Modifier.clickable(onClick = onInfo) else Modifier)
        )

        Spacer(modifier = Modifier.width(8.dp))

        when (progress) {
            null -> {
                // Not started — show download button
                Button(
                    onClick = onDownload,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp)
                ) {
                    Text("Download", fontSize = 11.sp)
                }
            }
            in 0..100 -> {
                // Downloading — show progress
                Text(
                    text = "$progress%",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.width(40.dp)
                )
            }
            -1 -> {
                // Error
                Text(
                    text = "Error",
                    fontSize = 12.sp,
                    color = Color(0xFFF44336)
                )
            }
            -2 -> {
                // Done (will be replaced by FileStatusRow on refresh)
                Text(
                    text = "\u2713",
                    fontSize = 14.sp,
                    color = Color(0xFF4CAF50),
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }
}

@Composable
private fun MissingFilesHelp() {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.errorContainer
        )
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text(
                text = "Missing Required Files",
                fontWeight = FontWeight.Bold,
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.onErrorContainer
            )
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = "Copy D2 files (from Steam/GOG) and/or D1 files to the app:\n" +
                        "  adb push <file> /data/data/com.dxxredux.app/files/\n" +
                        "Filenames are matched case-insensitively.\n" +
                        "Either Descent 2 or Descent 1 files are needed to launch.",
                color = MaterialTheme.colorScheme.onErrorContainer,
                fontSize = 12.sp,
                lineHeight = 16.sp
            )
        }
    }
}

@Composable
private fun SetManagementDialog(
    fileSetManager: FileSetManager,
    activeSetName: String,
    onSwitchSet: (String) -> Unit,
    onDismiss: () -> Unit
) {
    var newSetName by remember { mutableStateOf("") }
    var showNewSetInput by remember { mutableStateOf(false) }
    var confirmDelete by remember { mutableStateOf(false) }
    var errorMessage by remember { mutableStateOf<String?>(null) }

    val sets = remember { fileSetManager.listSets() }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = { Text("File Sets", fontWeight = FontWeight.Bold) },
        text = {
            Column {
                // Current set info
                Text("Current: $activeSetName", fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold)
                val usage = fileSetManager.diskUsage(activeSetName)
                Text("Size: ${formatSize(usage)}", fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)

                HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                // Other sets to switch to
                val otherSets = sets.filter { it.name != activeSetName }
                if (otherSets.isNotEmpty()) {
                    otherSets.forEach { set ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { onSwitchSet(set.name) }
                                .padding(vertical = 8.dp, horizontal = 4.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = "Switch to \"${set.name}\"",
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.weight(1f)
                            )
                            Text(
                                text = formatSize(fileSetManager.diskUsage(set.name)),
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                }

                // Add new set
                if (showNewSetInput) {
                    OutlinedTextField(
                        value = newSetName,
                        onValueChange = { newSetName = it; errorMessage = null },
                        label = { Text("Set name") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                        isError = errorMessage != null,
                        supportingText = errorMessage?.let { { Text(it) } }
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = {
                            try {
                                fileSetManager.createSet(newSetName.trim())
                                onSwitchSet(newSetName.trim())
                            } catch (e: IllegalArgumentException) {
                                errorMessage = e.message
                            }
                        }) {
                            Text("Create", fontSize = 13.sp)
                        }
                        OutlinedButton(onClick = {
                            showNewSetInput = false; newSetName = ""
                        }) {
                            Text("Cancel", fontSize = 13.sp)
                        }
                    }
                } else {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { showNewSetInput = true }
                            .padding(vertical = 8.dp, horizontal = 4.dp)
                    ) {
                        Text(
                            text = "+ Add new set\u2026",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                }

                // Delete current set (if not default)
                if (activeSetName != FileSetManager.DEFAULT_SET) {
                    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                    if (!confirmDelete) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { confirmDelete = true }
                                .padding(vertical = 8.dp, horizontal = 4.dp)
                        ) {
                            Text(
                                text = "Delete \"$activeSetName\"",
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.error
                            )
                        }
                    } else {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable {
                                    fileSetManager.deleteSet(activeSetName)
                                    onSwitchSet(FileSetManager.DEFAULT_SET)
                                }
                                .padding(vertical = 8.dp, horizontal = 4.dp)
                        ) {
                            Text(
                                text = "Confirm delete \"$activeSetName\"?",
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.error,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }
            }
        }
    )
}

// ── Download helper ─────────────────────────────────────────────────────────

private suspend fun downloadFile(
    url: String,
    destDir: File,
    filename: String,
    onProgress: (Int) -> Unit,
    onDone: (Boolean) -> Unit
) {
    withContext(Dispatchers.IO) {
        try {
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.connectTimeout = 15_000
            conn.readTimeout = 30_000
            conn.connect()

            if (conn.responseCode != 200) {
                Log.e("DXX-Setup", "Download failed: HTTP ${conn.responseCode} for $url")
                withContext(Dispatchers.Main) { onDone(false) }
                return@withContext
            }

            val totalBytes = conn.contentLength.toLong()
            val tmpFile = File(destDir, "$filename.tmp")
            var downloaded = 0L

            conn.inputStream.use { input ->
                FileOutputStream(tmpFile).use { output ->
                    val buf = ByteArray(8192)
                    while (true) {
                        val n = input.read(buf)
                        if (n <= 0) break
                        output.write(buf, 0, n)
                        downloaded += n
                        if (totalBytes > 0) {
                            val pct = (downloaded * 100 / totalBytes).toInt().coerceIn(0, 100)
                            withContext(Dispatchers.Main) { onProgress(pct) }
                        }
                    }
                }
            }

            // Rename .tmp → final
            val destFile = File(destDir, filename)
            tmpFile.renameTo(destFile)
            Log.i("DXX-Setup", "Downloaded $filename (${downloaded} bytes)")
            withContext(Dispatchers.Main) { onDone(true) }

        } catch (e: Exception) {
            Log.e("DXX-Setup", "Download error for $filename", e)
            withContext(Dispatchers.Main) { onDone(false) }
        }
    }
}
