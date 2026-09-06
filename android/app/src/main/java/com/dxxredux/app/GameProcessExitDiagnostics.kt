package com.dxxredux.app

import android.app.ActivityManager
import android.content.Context
import android.os.Build

/** Android retains exit reasons even when the native crash handler cannot run */
internal object GameProcessExitDiagnostics {
    private var lastLoggedTimestamp = 0L

    fun logRecent(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
        runCatching {
            val manager = context.getSystemService(ActivityManager::class.java)
            manager
                .getHistoricalProcessExitReasons(context.packageName, 0, 16)
                .filter { it.processName == "${context.packageName}:game" && it.timestamp > lastLoggedTimestamp }
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
