@file:Suppress("UNUSED_PARAMETER")

package com.dxxredux.app.multiplayer

// External Android/provider boundaries are deliberately unavailable in host tests
object PlayGamesAuth {
    val isConfigured = false

    suspend fun getServerAuthCode(activity: android.app.Activity): String? = error("GPGS requires device qualification")
}

class MultiplayerForegroundService

object CoopSaveCompatibility {
    fun hostWarning(
        filesDir: java.io.File,
        game: String,
        mission: String?,
    ): String? = error("Native save qualification requires the app target")
}
