package com.dxxredux.app.multiplayer

/** Platform profile only; never a matchmaking identity or an authorization signal */
data class PlayGamesFeatureState(
    val available: Boolean,
    val enabled: Boolean,
    val profileId: String?,
) {
    val ready: Boolean get() = available && enabled && profileId != null
}

/** Main-thread owner rejects late results after opt-out, profile refresh or replacement */
internal class PlayGamesFeatureGate(
    private val available: Boolean,
    enabled: Boolean = false,
) {
    private var generation = 0L
    var state = PlayGamesFeatureState(available, available && enabled, null)
        private set

    fun begin(): Long {
        generation++
        state = PlayGamesFeatureState(available, available, null)
        return generation
    }

    fun disable() {
        generation++
        state = PlayGamesFeatureState(available, false, null)
    }

    fun publish(
        expected: Long,
        profile: String?,
    ) {
        if (isCurrent(expected)) {
            state = state.copy(profileId = profile?.takeIf { it.isNotBlank() })
        }
    }

    fun isCurrent(expected: Long): Boolean = expected == generation && state.enabled
}
