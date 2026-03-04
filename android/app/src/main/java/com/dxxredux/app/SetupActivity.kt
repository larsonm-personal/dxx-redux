package com.dxxredux.app

import android.content.Intent
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.runtime.mutableIntStateOf
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val gameRunning = intent.getBooleanExtra("gameRunning", false)
        val filesDir = filesDir

        setContent {
            SetupScreen(
                filesDir = filesDir,
                gameRunning = gameRunning,
                refreshTrigger = refreshTrigger.intValue,
                onLaunchGame = {
                    if (gameRunning) {
                        finish()   // return to the already-running game
                    } else {
                        startActivity(Intent(this, MainActivity::class.java))
                        // Don't finish() — stay in back stack so quitting
                        // the game returns here instead of the launcher.
                    }
                },
                onRefresh = { refreshTrigger.intValue++ }
            )
        }
    }

    override fun onResume() {
        super.onResume()
        refreshTrigger.intValue++
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

// ── Composables ─────────────────────────────────────────────────────────────

@Composable
private fun SetupScreen(
    filesDir: File,
    gameRunning: Boolean,
    refreshTrigger: Int,
    onLaunchGame: () -> Unit,
    onRefresh: () -> Unit
) {
    val d2Statuses = remember(refreshTrigger) { checkFiles(filesDir, D2_FILES) }
    val d1Statuses = remember(refreshTrigger) { checkFiles(filesDir, D1_FILES) }

    val d2RequiredOk = d2Statuses.filter { it.info.required }.all { it.found }
    val d1RequiredOk = d1Statuses.filter { it.info.required }.all { it.found }
    val canLaunch = d2RequiredOk || d1RequiredOk

    // Download state: filename → progress (0..100, -1 = error, -2 = complete)
    val downloadProgress = remember { mutableStateMapOf<String, Int>() }
    val scope = rememberCoroutineScope()

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

                // ── Scrollable file list ────────────────────
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .verticalScroll(rememberScrollState())
                ) {
                    // ── Descent 2 section ───────────────────
                    GameSectionHeader("Descent 2", d2RequiredOk)

                    SectionHeader("Required Files")
                    d2Statuses.filter { it.info.required }.forEach {
                        FileStatusRow(it)
                    }
                    Spacer(modifier = Modifier.height(4.dp))
                    SectionHeader("Optional Files")
                    d2Statuses.filter { !it.info.required }.forEach {
                        FileStatusRow(it)
                    }

                    Spacer(modifier = Modifier.height(16.dp))

                    // ── Descent 1 section ───────────────────
                    GameSectionHeader("Descent 1", d1RequiredOk)

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
                                            },
                                            onDone = { success ->
                                                downloadProgress[status.info.filename] =
                                                    if (success) -2 else -1
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
private fun GameSectionHeader(title: String, ready: Boolean) {
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
                        "File names must be lowercase on Android.\n" +
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
