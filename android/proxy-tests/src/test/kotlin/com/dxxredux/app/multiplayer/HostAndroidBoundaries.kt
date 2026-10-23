@file:Suppress("UNUSED_PARAMETER")

package com.dxxredux.app.multiplayer

// External Android/provider boundaries are deliberately unavailable in host tests
object GoogleMatchmakingAuth {
    val isConfigured = false

    suspend fun signIn(
        activity: android.app.Activity,
        nonce: String,
    ): String = error("Google sign-in requires device qualification")
}

class MultiplayerForegroundService

object CoopSaveCompatibility {
    fun hostWarning(
        filesDir: java.io.File,
        game: String,
        mission: String?,
    ): String? = error("Native save qualification requires the app target")
}
