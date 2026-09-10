package com.dxxredux.app

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import java.io.File
import java.io.FileOutputStream

internal data class EngineExitRecord(
    val pid: Int,
    val timestamp: Long,
    val reason: Int,
    val status: Int,
    val description: String,
)

/** Android retains exit reasons even when the native crash handler cannot run */
internal object GameProcessExitDiagnostics {
    private var lastLoggedTimestamp = 0L

    internal fun recoverMarkers(
        directory: File,
        exits: List<EngineExitRecord>,
        processExists: ((Int) -> Boolean)? = null,
        onFailure: (Throwable) -> Unit = {},
    ) {
        val markers =
            directory
                .listFiles()
                ?.filter {
                    it.name.startsWith("engine_pending_") && it.extension == "txt"
                }.orEmpty()
        markers.forEach { marker ->
            runCatching {
                check(marker.length() <= 65536) { "Oversized engine session marker" }
                val contents = marker.readText()
                val fields =
                    contents.lineSequence().take(3).associate { line ->
                        line.substringBefore('=') to line.substringAfter('=', "")
                    }
                val pid = fields["pid"]?.toIntOrNull() ?: return@runCatching
                val started = fields["started_ms"]?.toLongOrNull() ?: return@runCatching
                val exit =
                    exits.filter { it.pid == pid && it.timestamp >= started }.minByOrNull { it.timestamp }
                        ?: if (processExists?.invoke(pid) == false) {
                            EngineExitRecord(
                                pid,
                                0,
                                0,
                                0,
                                "Process no longer exists; historical exit details unavailable",
                            )
                        } else {
                            null
                        }
                        ?: return@runCatching
                if (fields["expected_exit"].isNullOrBlank()) {
                    val key = marker.name.removePrefix("engine_pending_").removeSuffix(".txt")
                    val report = File(directory, "crash_error_exit_$key.txt")
                    if (!report.exists()) {
                        val temporary = File(directory, ".${report.name}.tmp")
                        val title =
                            if (contents.lineSequence().any { it == "Restore active: 1" }) {
                                "Interrupted save restore"
                            } else {
                                "Unexpected engine exit (no native crash dump)"
                            }
                        val text =
                            "$title\nLauncher fallback report; no native stack trace available\n" +
                                "Exit timestamp: ${exit.timestamp.takeIf {
                                    it > 0
                                } ?: "unavailable"}\nReason: ${exit.reason}\n" +
                                "Signal/status: ${exit.status}\nDescription: ${exit.description}\n\n$contents"
                        FileOutputStream(temporary).use { output ->
                            output.write(text.toByteArray(Charsets.UTF_8))
                            output.fd.sync()
                        }
                        check(temporary.renameTo(report)) { "Could not publish engine exit report" }
                    }
                }
                marker.delete()
            }.onFailure(onFailure)
        }
    }

    fun logRecent(context: Context) {
        runCatching {
            val manager = context.getSystemService(ActivityManager::class.java)
            val exits =
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    manager
                        .getHistoricalProcessExitReasons(context.packageName, 0, 16)
                        .filter { it.processName == "${context.packageName}:game" }
                } else {
                    emptyList()
                }
            // Reconcile every marker, even when the debug log already contains this exit
            recoverMarkers(
                File(context.filesDir, "tombstones"),
                exits.map { EngineExitRecord(it.pid, it.timestamp, it.reason, it.status, it.description.orEmpty()) },
                processExists = { pid ->
                    try {
                        Os.kill(pid, 0)
                        true
                    } catch (error: ErrnoException) {
                        // Permission errors cannot establish whether our old session ended
                        error.errno != OsConstants.ESRCH
                    }
                },
            ) {
                DebugLog.log(DebugLogCategory.LAUNCHER, "game exit report recovery failed: ${it.message}")
            }
            exits
                .filter { it.timestamp > lastLoggedTimestamp }
                .sortedBy { it.timestamp }
                .forEach { exit ->
                    DebugLog.log(
                        DebugLogCategory.LAUNCHER,
                        "game process exit timestamp_ms=${exit.timestamp} pid=${exit.pid} " +
                            "reason=${exit.reason} status=${exit.status} importance=${exit.importance} " +
                            "pss_kb=${exit.pss} rss_kb=${exit.rss} description=${exit.description}",
                    )
                    lastLoggedTimestamp = exit.timestamp
                }
        }.onFailure {
            DebugLog.log(DebugLogCategory.LAUNCHER, "game process exit lookup failed: ${it.message}")
        }
    }
}
