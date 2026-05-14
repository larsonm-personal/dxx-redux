package com.dxxredux.app

import android.app.ActivityManager
import android.content.Context
import android.os.Process
import org.json.JSONObject
import java.io.File

private const val GAME_ACTIVITY_STATE_FILE = "game_activity_state.json"

internal data class GameActivityState(
    val pid: Int,
    val game: String,
)

internal fun writeGameActivityState(
    context: Context,
    game: String,
) {
    val stateFile = gameActivityStateFile(context)
    val tempFile = File(stateFile.parentFile, "$GAME_ACTIVITY_STATE_FILE.tmp")
    try {
        val json =
            JSONObject()
                .put("pid", Process.myPid())
                .put("game", game)
                .put("updated_at_ms", System.currentTimeMillis())
                .toString()
        tempFile.writeText(json)
        if (!tempFile.renameTo(stateFile)) {
            stateFile.writeText(json)
            tempFile.delete()
        }
    } catch (_: Exception) {
    }
}

internal fun clearGameActivityState(context: Context) {
    val stateFile = gameActivityStateFile(context)
    try {
        val pid = JSONObject(stateFile.readText()).optInt("pid", 0)
        if (pid == 0 || pid == Process.myPid()) stateFile.delete()
    } catch (_: Exception) {
        stateFile.delete()
    }
}

internal fun readReturnableGameActivityState(context: Context): GameActivityState? {
    val stateFile = gameActivityStateFile(context)
    val json =
        try {
            JSONObject(stateFile.readText())
        } catch (_: Exception) {
            return null
        }
    val pid = json.optInt("pid", 0)
    val game = json.optString("game", "")
    if (pid <= 0 || game !in setOf("d1", "d2")) return null
    return if (isGameProcessRunning(context, pid)) GameActivityState(pid, game) else null
}

private fun gameActivityStateFile(context: Context): File = File(context.filesDir, GAME_ACTIVITY_STATE_FILE)

private fun isGameProcessRunning(
    context: Context,
    pid: Int,
): Boolean {
    val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
    val gameProcessName = "${context.packageName}:game"
    return activityManager
        ?.runningAppProcesses
        ?.any { it.pid == pid && it.processName == gameProcessName } == true
}
