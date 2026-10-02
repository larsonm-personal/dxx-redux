package com.dxxredux.app.multiplayer

import android.app.Activity

/** Direct APKs use the existing non-Play authentication path */
@Suppress("UNUSED_PARAMETER")
object PlayGamesAuth {
    val isConfigured: Boolean = false

    fun initialize(activity: Activity) = Unit

    suspend fun isAuthenticated(activity: Activity): Boolean = false

    suspend fun getServerAuthCode(
        activity: Activity,
        forceRefresh: Boolean = false,
    ): String? = null
}
