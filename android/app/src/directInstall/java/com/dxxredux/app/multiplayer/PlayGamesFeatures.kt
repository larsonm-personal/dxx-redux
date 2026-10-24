package com.dxxredux.app.multiplayer

import android.app.Activity
import android.content.Context

/** Provider-free distribution: availability does not depend on matchmaking configuration */
@Suppress("UNUSED_PARAMETER")
object PlayGamesFeatures {
    suspend fun state(context: Context) = PlayGamesFeatureState(false, false, null)

    suspend fun enable(activity: Activity) = PlayGamesFeatureState(false, false, null)

    suspend fun refresh(activity: Activity) = PlayGamesFeatureState(false, false, null)

    suspend fun disable(context: Context) = Unit
}
