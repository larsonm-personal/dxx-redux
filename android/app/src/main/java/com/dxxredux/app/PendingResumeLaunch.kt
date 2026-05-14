package com.dxxredux.app

import android.content.Context
import org.json.JSONObject
import java.io.File

private const val PENDING_RESUME_LAUNCH_FILE = "pending_resume_launch.json"
private const val PENDING_RESUME_LAUNCH_MAX_AGE_MS = 10 * 60 * 1000L

internal data class PendingResumeLaunch(
    val game: String,
    val savePath: String,
    val callsign: String?,
    val token: String,
)

internal fun writePendingResumeLaunch(
    context: Context,
    game: String,
    savePath: String,
    callsign: String?,
    token: String,
) {
    val stateFile = pendingResumeLaunchFile(context)
    val tempFile = File(stateFile.parentFile, "$PENDING_RESUME_LAUNCH_FILE.tmp")
    try {
        val json =
            JSONObject()
                .put("game", game)
                .put("resume_save_path", savePath)
                .put("resume_callsign", callsign ?: "")
                .put("token", token)
                .put("created_at_ms", System.currentTimeMillis())
                .toString()
        tempFile.writeText(json)
        if (!tempFile.renameTo(stateFile)) {
            stateFile.writeText(json)
            tempFile.delete()
        }
        DebugLog.log(
            DebugLogCategory.GAME,
            "pending resume launch write: game=$game path=$savePath callsign=${callsign ?: ""} token=$token",
        )
    } catch (e: Exception) {
        DebugLog.log(
            DebugLogCategory.GAME,
            "pending resume launch write failed: ${e.message ?: e.javaClass.simpleName}",
        )
    }
}

internal fun readPendingResumeLaunch(
    context: Context,
    expectedGame: String,
): PendingResumeLaunch? {
    val stateFile = pendingResumeLaunchFile(context)
    val json =
        try {
            JSONObject(stateFile.readText())
        } catch (e: Exception) {
            DebugLog.log(
                DebugLogCategory.GAME,
                "pending resume launch read failed: expected_game=$expectedGame error=${e.message ?: e.javaClass.simpleName}",
            )
            return null
        }
    val pendingSummary = pendingResumeLaunchSummary(json)
    val createdAtMs = json.optLong("created_at_ms", 0L)
    if (createdAtMs <= 0L || System.currentTimeMillis() - createdAtMs > PENDING_RESUME_LAUNCH_MAX_AGE_MS) {
        DebugLog.log(
            DebugLogCategory.GAME,
            "pending resume launch rejected: reason=stale expected_game=$expectedGame $pendingSummary",
        )
        stateFile.delete()
        return null
    }
    val game = json.optString("game", "")
    if (game != expectedGame) {
        DebugLog.log(
            DebugLogCategory.GAME,
            "pending resume launch rejected: reason=game_mismatch expected_game=$expectedGame $pendingSummary",
        )
        return null
    }
    val savePath =
        json.optString("resume_save_path", "").takeIf { it.isNotBlank() }
            ?: run {
                DebugLog.log(
                    DebugLogCategory.GAME,
                    "pending resume launch rejected: reason=missing_save_path expected_game=$expectedGame $pendingSummary",
                )
                return null
            }
    val token =
        json.optString("token", "").takeIf { it.isNotBlank() }
            ?: run {
                DebugLog.log(
                    DebugLogCategory.GAME,
                    "pending resume launch rejected: reason=missing_token expected_game=$expectedGame $pendingSummary",
                )
                return null
            }
    DebugLog.log(
        DebugLogCategory.GAME,
        "pending resume launch accepted: expected_game=$expectedGame $pendingSummary",
    )
    return PendingResumeLaunch(
        game = game,
        savePath = savePath,
        callsign = json.optString("resume_callsign", "").takeIf { it.isNotBlank() },
        token = token,
    )
}

internal fun clearPendingResumeLaunch(
    context: Context,
    token: String? = null,
) {
    val stateFile = pendingResumeLaunchFile(context)
    if (token != null) {
        val existingState =
            try {
                JSONObject(stateFile.readText())
            } catch (_: Exception) {
                null
            }
        val existingToken = existingState?.optString("token", "") ?: ""
        if (existingToken.isNotBlank() && existingToken != token) {
            DebugLog.log(
                DebugLogCategory.GAME,
                "pending resume launch clear skipped: requested_token=$token existing_token=$existingToken " +
                    (existingState?.let(::pendingResumeLaunchSummary) ?: "state=unreadable"),
            )
            return
        }
    }
    DebugLog.log(
        DebugLogCategory.GAME,
        "pending resume launch clear: token=${token ?: ""}",
    )
    stateFile.delete()
}

private fun pendingResumeLaunchSummary(json: JSONObject): String {
    val game = json.optString("game", "")
    val savePath = json.optString("resume_save_path", "")
    val callsign = json.optString("resume_callsign", "")
    val token = json.optString("token", "")
    return "game=$game path=$savePath callsign=$callsign token=$token"
}

private fun pendingResumeLaunchFile(context: Context): File = File(context.filesDir, PENDING_RESUME_LAUNCH_FILE)
