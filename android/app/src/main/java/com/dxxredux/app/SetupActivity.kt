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
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.runtime.mutableIntStateOf
import androidx.core.view.WindowCompat
import java.io.File
import java.io.FileOutputStream
import java.io.FileWriter
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

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

    /** Active download progress visible to introspection. */
    internal val downloadStates = mutableMapOf<String, Int>()

    // ── Controller live-state ───────────────────────────────────────────
    /** Axis values observable by Compose (LX, LY, RX, RY, LT, RT). */
    internal val controllerAxes = FloatArray(6)
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
            val d2Statuses = checkFiles(dir, D2_FILES)
            val d1Statuses = checkFiles(dir, D1_FILES)
            val d2Ready = d2Statuses.filter { it.info.required }.all { it.found }
            val d1Ready = d1Statuses.filter { it.info.required }.all { it.found }

            val root = JSONObject()
            root.put("screen", "setup")
            root.put("can_launch", d2Ready || d1Ready)

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
            arr.put(obj)
        }
        return arr
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Let the system draw behind bars but we'll add padding in Compose
        WindowCompat.setDecorFitsSystemWindows(window, true)

        // Register introspection receiver
        val filter = IntentFilter("com.dxxredux.SETUP_INTROSPECT")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(introspectReceiver, filter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(introspectReceiver, filter)
        }

        val gameRunning = intent.getBooleanExtra("gameRunning", false)
        val filesDir = filesDir

        setContent {
            SetupScreen(
                filesDir = filesDir,
                gameRunning = gameRunning,
                refreshTrigger = refreshTrigger.intValue,
                controllerAxes = controllerAxes,
                axisGeneration = axisGeneration.intValue,
                pressedButtons = pressedButtons,
                onLaunchGame = {
                    if (gameRunning) {
                        finish()   // return to the already-running game
                    } else {
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

    override fun onResume() {
        super.onResume()
        refreshTrigger.intValue++
    }

    override fun onDestroy() {
        try { unregisterReceiver(introspectReceiver) } catch (_: Exception) {}
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
    val foundName: String?
)

// ── Helpers ─────────────────────────────────────────────────────────────────

/** Case-insensitive file lookup (Android ext4 is case-sensitive). */
private fun findFile(dir: File, name: String): String? {
    val files = dir.listFiles() ?: return null
    return files.firstOrNull { it.name.equals(name, ignoreCase = true) }?.name
}

private fun checkFiles(dir: File, fileList: List<GameFileInfo>): List<FileStatus> =
    fileList.map { info ->
        val primaryMatch = findFile(dir, info.filename)
        val altMatch = if (primaryMatch == null)
            info.alternatives.firstNotNullOfOrNull { findFile(dir, it) }
        else null
        val foundName = primaryMatch ?: altMatch
        FileStatus(info, found = foundName != null, foundName = foundName)
    }

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

// ── SAF directory scanning ───────────────────────────────────────────────────

/** All filenames we care about (D2 + D1), lowercase for matching. */
private val ALL_GAME_FILENAMES: Set<String> by lazy {
    (D2_FILES + D1_FILES).flatMap { info ->
        listOf(info.filename) + info.alternatives
    }.map { it.lowercase() }.toSet()
}

/** Result of scanning a user-chosen directory tree. */
private data class FoundFile(
    val name: String,        // original filename (preserving case)
    val uri: Uri             // content:// URI to read from
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

// ── Composables ─────────────────────────────────────────────────────────────

@Composable
private fun SetupScreen(
    filesDir: File,
    gameRunning: Boolean,
    refreshTrigger: Int,
    controllerAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    onLaunchGame: () -> Unit,
    onRefresh: () -> Unit,
    onDownloadStateChanged: (String, Int) -> Unit = { _, _ -> }
) {
    val d2Statuses = remember(refreshTrigger) { checkFiles(filesDir, D2_FILES) }
    val d1Statuses = remember(refreshTrigger) { checkFiles(filesDir, D1_FILES) }

    val d2RequiredOk = d2Statuses.filter { it.info.required }.all { it.found }
    val d1RequiredOk = d1Statuses.filter { it.info.required }.all { it.found }
    val canLaunch = d2RequiredOk || d1RequiredOk

    // True when zero required files are found for either game
    val noRequiredFiles = d2Statuses.filter { it.info.required }.none { it.found }
            && d1Statuses.filter { it.info.required }.none { it.found }

    // Download state: filename → progress (0..100, -1 = error, -2 = complete)
    val downloadProgress = remember { mutableStateMapOf<String, Int>() }
    val scope = rememberCoroutineScope()

    // ── SAF file-search state ───────────────────────────────
    val context = androidx.compose.ui.platform.LocalContext.current
    var scanResults by remember { mutableStateOf<List<FoundFile>?>(null) }
    var scanning by remember { mutableStateOf(false) }
    var importStatus by remember { mutableStateOf("") }

    val filePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenMultipleDocuments()
    ) { uris: List<Uri> ->
        if (uris.isEmpty()) return@rememberLauncherForActivityResult
        scanning = true
        importStatus = ""
        scope.launch(Dispatchers.IO) {
            val found = uris.mapNotNull { uri ->
                val name = getDisplayName(context, uri)
                if (name != null && name.lowercase() in ALL_GAME_FILENAMES) {
                    FoundFile(name, uri)
                } else null
            }
            withContext(Dispatchers.Main) {
                scanResults = found
                scanning = false
            }
        }
    }

    MaterialTheme(colorScheme = darkColorScheme()) {
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(16.dp)
            ) {
                // ── Title ───────────────────────────────────
                Text(
                    text = "DXX-Redux Setup",
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.padding(bottom = 8.dp)
                )

                // ── Missing-files help ──────────────────────
                if (!canLaunch && !gameRunning) {
                    MissingFilesHelp()
                    Spacer(modifier = Modifier.height(8.dp))
                }

                // ── Import files button (always available) ──
                Button(
                    onClick = { filePickerLauncher.launch(arrayOf("application/octet-stream", "*/*")) },
                    enabled = !scanning,
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = MaterialTheme.colorScheme.secondary
                    )
                ) {
                    Text(
                        text = if (scanning) "Importing\u2026"
                               else "\uD83D\uDCC2 Select Game Files to Import",
                        fontSize = 14.sp
                    )
                }
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "Select .hog, .ham, .pig files from Downloads or any folder.",
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
                                    text = "Found ${found.size} game file(s): ${found.joinToString(", ") { it.name ?: "?" }}",
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
                                            scope.launch(Dispatchers.IO) {
                                                var imported = 0
                                                found.forEach { f ->
                                                    if (importFile(context, f, filesDir)) imported++
                                                }
                                                withContext(Dispatchers.Main) {
                                                    importStatus = "Imported $imported of ${found.size} files."
                                                    scanResults = null
                                                    onRefresh()
                                                }
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

                // ── Scrollable file list ────────────────────
                var d2Expanded by remember { mutableStateOf(false) }
                var d1Expanded by remember { mutableStateOf(false) }

                Column(
                    modifier = Modifier
                        .weight(1f)
                        .verticalScroll(rememberScrollState())
                ) {
                    // ── Descent 2 section ───────────────────
                    GameSectionHeader(
                        title = "Descent 2",
                        ready = d2RequiredOk,
                        expanded = d2Expanded,
                        onToggle = { d2Expanded = !d2Expanded }
                    )

                    if (d2Expanded) {
                        SectionHeader("Required Files")
                        d2Statuses.filter { it.info.required }.forEach {
                            FileStatusRow(it)
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        SectionHeader("Optional Files")
                        d2Statuses.filter { !it.info.required }.forEach {
                            FileStatusRow(it)
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))

                    // ── Descent 1 section ───────────────────
                    GameSectionHeader(
                        title = "Descent 1",
                        ready = d1RequiredOk,
                        expanded = d1Expanded,
                        onToggle = { d1Expanded = !d1Expanded }
                    )

                    if (d1Expanded) {
                        SectionHeader("Required Files")
                        d1Statuses.filter { it.info.required }.forEach {
                            FileStatusRow(it)
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        SectionHeader("Optional Files")
                        d1Statuses.filter { !it.info.required }.forEach { status ->
                        if (!status.found && status.info.downloadUrl != null) {
                            DownloadableFileRow(
                                status = status,
                                progress = downloadProgress[status.info.filename],
                                onDownload = {
                                    scope.launch {
                                        downloadFile(
                                            url = status.info.downloadUrl,
                                            destDir = filesDir,
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
                            FileStatusRow(status)
                        }
                    }
                    } // end if (d1Expanded)

                    Spacer(modifier = Modifier.height(16.dp))

                    // ── Controller section ──────────────────
                    ControllerSection(
                        axes = controllerAxes,
                        axisGeneration = axisGeneration,
                        pressedButtons = pressedButtons
                    )
                }

                // ── Launch / Return button ──────────────────
                Spacer(modifier = Modifier.height(16.dp))
                Button(
                    onClick = onLaunchGame,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(56.dp),
                    enabled = canLaunch || gameRunning,
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (!canLaunch && !gameRunning)
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
        }
    }
}

@Composable
private fun GameSectionHeader(
    title: String,
    ready: Boolean,
    expanded: Boolean,
    onToggle: () -> Unit
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
            text = if (ready) "\u2713 Ready" else "\u2717 Missing files",
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
private fun ControllerSection(
    axes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>
) {
    // Detect connected gamepads
    val gamepads = remember(axisGeneration) {
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
private fun FileStatusRow(status: FileStatus) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 1.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = if (status.found) "\u2713" else "\u2717",
            color = if (status.found) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp)
        )

        val name = status.foundName ?: status.info.filename
        val altHint = if (!status.found && status.info.alternatives.isNotEmpty())
            " (or ${status.info.alternatives.joinToString(", ")})"
        else ""
        Text(
            text = "$name \u2014 ${status.info.description}$altHint",
            color = if (status.found) MaterialTheme.colorScheme.onSurface
                    else MaterialTheme.colorScheme.onSurfaceVariant,
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
    onDownload: () -> Unit
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
            modifier = Modifier.weight(1f)
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
