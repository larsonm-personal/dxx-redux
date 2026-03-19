package com.dxxredux.app

import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dxxredux.app.multiplayer.NetLog
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun AdvancedSettingsPage(
    filesDir: File,
    fileSetManager: FileSetManager,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)

    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val scrollState = rememberScrollState()

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier =
                Modifier
                    .fillMaxSize()
                    .safeDrawingPadding()
                    .padding(16.dp),
        ) {
            // Top bar
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TextButton(onClick = onBack) {
                    Text("< Back", fontSize = 14.sp)
                }
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    "Advanced Settings",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                )
            }

            Spacer(modifier = Modifier.height(12.dp))

            Column(
                modifier =
                    Modifier
                        .weight(1f)
                        .verticalScroll(scrollState),
            ) {
                // -- Render Resolution --
                ResolutionPickerAdvanced(prefs = prefs, filesDir = filesDir)

                Spacer(modifier = Modifier.height(16.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(16.dp))

                // -- Export / Import configs --
                Text("Config Management", fontWeight = FontWeight.Bold, fontSize = 14.sp)
                Spacer(modifier = Modifier.height(8.dp))

                val configImportLauncher =
                    rememberLauncherForActivityResult(
                        contract =
                            androidx.activity.result.contract.ActivityResultContracts
                                .OpenDocument(),
                    ) { uri ->
                        if (uri == null) return@rememberLauncherForActivityResult
                        val msg = ConfigImportExport.importFromUri(ctx, uri)
                        Toast.makeText(ctx, msg, Toast.LENGTH_LONG).show()
                    }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = { ConfigImportExport.exportAll(ctx) },
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                        modifier = Modifier.height(32.dp),
                    ) {
                        Text("Export All Configs", fontSize = 12.sp)
                    }
                    OutlinedButton(
                        onClick = { configImportLauncher.launch(arrayOf("application/json", "*/*")) },
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                        modifier = Modifier.height(32.dp),
                    ) {
                        Text("Import Config", fontSize = 12.sp)
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(16.dp))

                // -- Network Logging --
                NetworkLoggingSection()

                Spacer(modifier = Modifier.height(16.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(16.dp))

                // -- Crash Reports --
                CrashReportsSection()

                Spacer(modifier = Modifier.height(16.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(16.dp))

                // -- Dangerous zone --
                Text("Danger Zone", fontWeight = FontWeight.Bold, fontSize = 14.sp, color = Color(0xFFF44336))
                Spacer(modifier = Modifier.height(8.dp))

                // Reset All Controls
                var showResetDialog by remember { mutableStateOf(false) }
                OutlinedButton(
                    onClick = { showResetDialog = true },
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    modifier = Modifier.height(36.dp),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
                ) {
                    Text("Reset All Controls", fontSize = 12.sp)
                }
                if (showResetDialog) {
                    AlertDialog(
                        onDismissRequest = { showResetDialog = false },
                        title = { Text("Reset All Controls") },
                        text = {
                            Text(
                                "This will reset ALL control bindings to defaults:\n\n" +
                                    "- Touch layout (positions, sizes, bindings)\n" +
                                    "- Physical controller mappings\n" +
                                    "- In-game keyboard, joystick, and mouse settings for every pilot\n\n" +
                                    "The app will restart after reset.",
                                fontSize = 13.sp,
                            )
                        },
                        confirmButton = {
                            TextButton(onClick = {
                                File(ctx.filesDir, "controller_config.json").delete()
                                File(ctx.filesDir, "touch_layout.json").delete()
                                NativePilotPatcher.nativeResetToDefaults(ctx.filesDir.absolutePath, "d2")
                                NativePilotPatcher.nativeResetToDefaults(ctx.filesDir.absolutePath, "d1")
                                showResetDialog = false
                                android.os.Process.killProcess(android.os.Process.myPid())
                            }) {
                                Text("Reset & Restart", color = Color(0xFFF44336))
                            }
                        },
                        dismissButton = {
                            TextButton(onClick = { showResetDialog = false }) { Text("Cancel") }
                        },
                    )
                }

                Spacer(modifier = Modifier.height(8.dp))

                // Delete All Player Files
                var showDeletePilotsDialog by remember { mutableStateOf(false) }
                OutlinedButton(
                    onClick = { showDeletePilotsDialog = true },
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    modifier = Modifier.height(36.dp),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
                ) {
                    Text("Delete All Player Files", fontSize = 12.sp)
                }
                if (showDeletePilotsDialog) {
                    AlertDialog(
                        onDismissRequest = { showDeletePilotsDialog = false },
                        title = { Text("Delete All Player Files") },
                        text = {
                            Text(
                                "This will delete ALL pilot files (.plr), extended configs (.plx), " +
                                    "effects (.eff), new game plus (.ngp), and saved games " +
                                    "(.sg*, .mg*) for both Descent 1 and Descent 2 across all " +
                                    "file sets.\n\nThis cannot be undone.\n\n" +
                                    "The app will restart after deletion.",
                                fontSize = 13.sp,
                            )
                        },
                        confirmButton = {
                            TextButton(onClick = {
                                val deleted = fileSetManager.deleteAllPilotFiles()
                                showDeletePilotsDialog = false
                                Toast.makeText(ctx, "Deleted $deleted file(s)", Toast.LENGTH_SHORT).show()
                                android.os.Process.killProcess(android.os.Process.myPid())
                            }) {
                                Text("Delete & Restart", color = Color(0xFFF44336))
                            }
                        },
                        dismissButton = {
                            TextButton(onClick = { showDeletePilotsDialog = false }) { Text("Cancel") }
                        },
                    )
                }

                Spacer(modifier = Modifier.height(16.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(16.dp))

                // Restart App
                Button(
                    onClick = { android.os.Process.killProcess(android.os.Process.myPid()) },
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary),
                    modifier = Modifier.fillMaxWidth().height(44.dp),
                ) {
                    Text("Restart App", fontSize = 14.sp)
                }

                Spacer(modifier = Modifier.height(16.dp))
            }
        }
    }
}

@Composable
private fun NetworkLoggingSection() {
    val ctx = LocalContext.current
    var loggingEnabled by remember { mutableStateOf(NetLog.isEnabled(ctx)) }
    var logFiles by remember { mutableStateOf(NetLog.listLogFiles(ctx)) }
    var showDeleteDialog by remember { mutableStateOf(false) }
    val dateFmt = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US) }

    Text("Network Logging", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "Log network events (connections, lobbies, STUN, relay) to files for debugging.",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(8.dp))

    Row(verticalAlignment = Alignment.CenterVertically) {
        Text("Enable Logging", fontSize = 13.sp)
        Spacer(modifier = Modifier.weight(1f))
        Switch(
            checked = loggingEnabled,
            onCheckedChange = { on ->
                NetLog.setEnabled(ctx, on)
                loggingEnabled = on
                logFiles = NetLog.listLogFiles(ctx)
            },
        )
    }

    if (logFiles.isNotEmpty()) {
        Spacer(modifier = Modifier.height(8.dp))
        Text("Log Files (${logFiles.size})", fontSize = 12.sp, fontWeight = FontWeight.Medium)
        Spacer(modifier = Modifier.height(4.dp))
        for (file in logFiles) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(file.name, fontSize = 11.sp)
                    val sizeKb = file.length() / 1024
                    val date = dateFmt.format(Date(file.lastModified()))
                    Text(
                        "$date -- ${sizeKb}KB",
                        fontSize = 10.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                OutlinedButton(
                    onClick = { NetLog.shareLogFile(ctx, file) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Export", fontSize = 11.sp)
                }
            }
        }
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedButton(
            onClick = { showDeleteDialog = true },
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
            colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
        ) {
            Text("Delete All Logs", fontSize = 12.sp)
        }
        if (showDeleteDialog) {
            AlertDialog(
                onDismissRequest = { showDeleteDialog = false },
                title = { Text("Delete All Logs") },
                text = { Text("Delete all network log files? This cannot be undone.") },
                confirmButton = {
                    TextButton(onClick = {
                        NetLog.deleteAllLogs(ctx)
                        logFiles = emptyList()
                        showDeleteDialog = false
                    }) {
                        Text("Delete", color = Color(0xFFF44336))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showDeleteDialog = false }) { Text("Cancel") }
                },
            )
        }
    }
}

@Composable
private fun CrashReportsSection() {
    val ctx = LocalContext.current
    var crashFiles by remember { mutableStateOf(CrashLog.listCrashFiles(ctx)) }
    var showDeleteDialog by remember { mutableStateOf(false) }
    val dateFmt = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US) }

    Text("Crash Reports", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "Crash reports are captured automatically when the app or game crashes.",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )

    if (crashFiles.isNotEmpty()) {
        Spacer(modifier = Modifier.height(8.dp))
        Text("Reports (${crashFiles.size})", fontSize = 12.sp, fontWeight = FontWeight.Medium)
        Spacer(modifier = Modifier.height(4.dp))
        for (file in crashFiles) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(file.name, fontSize = 11.sp)
                    val sizeKb = file.length() / 1024
                    val date = dateFmt.format(Date(file.lastModified()))
                    Text(
                        "$date -- ${sizeKb}KB",
                        fontSize = 10.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                OutlinedButton(
                    onClick = { CrashLog.shareCrashFile(ctx, file) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Export", fontSize = 11.sp)
                }
            }
        }
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedButton(
            onClick = { showDeleteDialog = true },
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
            colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
        ) {
            Text("Delete All Reports", fontSize = 12.sp)
        }
        if (showDeleteDialog) {
            AlertDialog(
                onDismissRequest = { showDeleteDialog = false },
                title = { Text("Delete All Crash Reports") },
                text = { Text("Delete all crash report files? This cannot be undone.") },
                confirmButton = {
                    TextButton(onClick = {
                        CrashLog.deleteAllCrashFiles(ctx)
                        crashFiles = emptyList()
                        showDeleteDialog = false
                    }) {
                        Text("Delete", color = Color(0xFFF44336))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showDeleteDialog = false }) { Text("Cancel") }
                },
            )
        }
    } else {
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            "No crash reports.",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun ResolutionPickerAdvanced(
    prefs: android.content.SharedPreferences,
    filesDir: File,
) {
    val ctx = LocalContext.current
    val options =
        remember {
            computeResolutionOptions(ctx)
        }
    val validValues = remember(options) { options.map { it.first }.toSet() }
    val defaultValue = remember(options) { options.firstOrNull()?.first ?: "640x480" }
    var selected by remember {
        val stored = prefs.getString("render_resolution", null) ?: ""
        mutableStateOf(if (stored in validValues) stored else defaultValue)
    }

    Text("Render Resolution", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    options.forEach { (value, label) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(vertical = 2.dp),
        ) {
            RadioButton(
                selected = selected == value,
                onClick = {
                    selected = value
                    prefs.edit().putString("render_resolution", value).apply()
                    updateDescentCfgResolution(filesDir, value)
                },
            )
            Text(text = label, fontSize = 13.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
    Text(
        "Takes effect on next launch",
        fontSize = 11.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}

/** Compute resolution options as fractions of the device's real screen size. */
internal fun computeResolutionOptions(ctx: android.content.Context): List<Pair<String, String>> {
    val wm = ctx.getSystemService(android.content.Context.WINDOW_SERVICE) as android.view.WindowManager
    val (rawW, rawH) =
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            val bounds = wm.currentWindowMetrics.bounds
            Pair(bounds.width(), bounds.height())
        } else {
            @Suppress("DEPRECATION")
            val size = android.graphics.Point()
            @Suppress("DEPRECATION")
            wm.defaultDisplay.getRealSize(size)
            Pair(size.x, size.y)
        }
    // Normalize to landscape (width >= height)
    val screenW = maxOf(rawW, rawH)
    val screenH = minOf(rawW, rawH)

    val labels = listOf("Full" to 1, "1/2" to 2, "1/3" to 3, "1/4" to 4)
    val result = mutableListOf<Pair<String, String>>()
    for ((label, divisor) in labels) {
        // Round to nearest even to avoid odd-pixel issues
        val w = (screenW / divisor + 1) and 0x7FFFFFFE
        val h = (screenH / divisor + 1) and 0x7FFFFFFE
        if (w < 640 || h < 480) break
        val value = "${w}x$h"
        result.add(value to "$label ($value)")
    }
    // Always offer at least 640x480 as a fallback
    if (result.isEmpty()) {
        result.add("640x480" to "Low (640x480)")
    }
    return result
}
