package com.dxxredux.app

import android.content.Intent
import android.os.Bundle
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

/**
 * Pre-game setup screen built with Jetpack Compose.
 *
 * Shows the readiness status of required and optional game data files,
 * instructions for installing missing files, and a button to launch
 * (or return to) the game.
 *
 * This is the launcher activity.  It can also be opened from [MainActivity]
 * via a swipe-from-left-edge gesture during gameplay.
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
                }
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
    val alternatives: List<String> = emptyList()
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

private val GAME_FILES = listOf(
    // Required – core engine files (game won't start without these)
    GameFileInfo("descent2.hog", "Main game data",
        required = true, alternatives = listOf("d2demo.hog")),
    GameFileInfo("descent2.ham", "Models & objects",
        required = true, alternatives = listOf("d2demo.ham")),
    GameFileInfo("groupa.pig", "Main textures",
        required = true, alternatives = listOf("d2demo.pig")),
    GameFileInfo("descent2.s22", "Sound effects (22 kHz)",
        required = true, alternatives = listOf("descent2.s11")),

    // Required – level texture packs (levels crash without these)
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

// ── Composables ─────────────────────────────────────────────────────────────

@Composable
private fun SetupScreen(
    filesDir: File,
    gameRunning: Boolean,
    refreshTrigger: Int,
    onLaunchGame: () -> Unit
) {
    val statuses = remember(refreshTrigger) { checkFiles(filesDir, GAME_FILES) }
    val requiredMissing = statuses.any { it.info.required && !it.found }

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

                // ── Missing-files help (above the list) ─────
                if (requiredMissing) {
                    MissingFilesHelp()
                    Spacer(modifier = Modifier.height(8.dp))
                }

                // ── Scrollable file list ────────────────────
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .verticalScroll(rememberScrollState())
                ) {
                    SectionHeader("Required Files")
                    statuses.filter { it.info.required }.forEach {
                        FileStatusRow(it)
                    }

                    Spacer(modifier = Modifier.height(8.dp))

                    SectionHeader("Optional Files")
                    statuses.filter { !it.info.required }.forEach {
                        FileStatusRow(it)
                    }
                }

                // ── Launch / Return button ──────────────────
                Spacer(modifier = Modifier.height(16.dp))
                Button(
                    onClick = onLaunchGame,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(56.dp),
                    enabled = !requiredMissing || gameRunning,
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (requiredMissing && !gameRunning)
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
        // Compact status indicator
        Text(
            text = if (status.found) "\u2713" else "\u2717",     // ✓ / ✗
            color = if (status.found) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp)
        )

        // Single-line: filename — description
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
                text = "Copy game data files (from Steam/GOG) to the app:\n" +
                        "  adb push <file> /data/data/com.dxxredux.app/files/\n" +
                        "File names must be lowercase on Android.",
                color = MaterialTheme.colorScheme.onErrorContainer,
                fontSize = 12.sp,
                lineHeight = 16.sp
            )
        }
    }
}
