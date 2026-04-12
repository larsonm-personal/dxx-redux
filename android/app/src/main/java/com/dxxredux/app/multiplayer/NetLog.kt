package com.dxxredux.app.multiplayer

import android.content.Context
import java.io.File

/**
 * Network debug logger. Now delegates to [com.dxxredux.app.DebugLog] which
 * provides multi-category logging. This wrapper preserves the existing API
 * so callers (LobbyService, MatchmakingService, etc.) don't need changes.
 *
 * File management (list/share/delete) also delegates to DebugLog.
 */
object NetLog {
    private const val PREFS_NAME = "dxx_prefs"

    fun init(context: Context) {
        com.dxxredux.app.DebugLog
            .init(context)
    }

    fun initAppend(
        context: Context,
        filePath: String,
    ) {
        com.dxxredux.app.DebugLog
            .initAppend(context, filePath)
    }

    fun currentFilePath(): String? =
        com.dxxredux.app.DebugLog
            .currentFilePath()

    fun setEnabled(
        context: Context,
        on: Boolean,
    ) {
        com.dxxredux.app.DebugLog.setCategoryEnabled(
            context,
            com.dxxredux.app.DebugLogCategory.NETWORK,
            on,
        )
    }

    fun isEnabled(context: Context): Boolean =
        com.dxxredux.app.DebugLog.isCategoryEnabled(
            context,
            com.dxxredux.app.DebugLogCategory.NETWORK,
        )

    fun log(
        category: String,
        message: String,
    ) {
        com.dxxredux.app.DebugLog
            .log(com.dxxredux.app.DebugLogCategory.NETWORK, "[$category] $message")
    }

    fun listLogFiles(context: Context): List<File> =
        com.dxxredux.app.DebugLog
            .listLogFiles(context)

    fun shareLogFile(
        context: Context,
        file: File,
    ): Boolean =
        com.dxxredux.app.DebugLog
            .shareLogFile(context, file)

    fun deleteAllLogs(context: Context) {
        com.dxxredux.app.DebugLog
            .deleteAllLogs(context)
    }
}
