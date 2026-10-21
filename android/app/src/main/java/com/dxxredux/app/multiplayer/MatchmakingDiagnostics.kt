package com.dxxredux.app.multiplayer

import android.util.Log

/** Control bodies and parser exceptions can contain credentials; log fixed events only */
internal object MatchmakingDiagnostics {
    fun parse(text: String): ServerMessage? {
        Log.d("MatchmakingService", "Received matchmaking message")
        return try {
            ServerMessage.parse(text)
        } catch (_: Exception) {
            Log.e("MatchmakingService", "Invalid matchmaking message")
            NetLog.log("ERROR", "Invalid matchmaking message")
            null
        }
    }
}
