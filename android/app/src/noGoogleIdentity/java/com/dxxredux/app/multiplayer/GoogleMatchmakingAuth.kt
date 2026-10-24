package com.dxxredux.app.multiplayer

import android.app.Activity

/** CI distributions omit Google provider dependencies */
object GoogleMatchmakingAuth {
    val isConfigured: Boolean = false

    @Suppress("UNUSED_PARAMETER")
    suspend fun signIn(
        activity: Activity,
        nonce: String,
    ): String = error("Google sign-in is unavailable in this build")

    @Suppress("UNUSED_PARAMETER")
    suspend fun clearAccountSelection(activity: Activity) = Unit
}
