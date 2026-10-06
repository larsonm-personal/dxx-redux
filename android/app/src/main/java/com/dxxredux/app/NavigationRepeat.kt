package com.dxxredux.app

private const val NAV_REPEAT_INITIAL_DELAY_FALLBACK_MS = 500L
private const val NAV_REPEAT_INTERVAL_FALLBACK_MS = 125L

internal data class NavigationRepeatTiming(
    val initialDelayMs: Long,
    val repeatIntervalMs: Long,
)

private fun androidStaticRepeatTiming(methodName: String): Long? =
    runCatching {
        when (val value = android.view.ViewConfiguration::class.java.getMethod(methodName).invoke(null)) {
            is Int -> value.toLong()
            is Number -> value.toLong()
            null -> null
            else -> null
        }
    }.getOrNull()

internal val navigationRepeatTiming: NavigationRepeatTiming by lazy {
    NavigationRepeatTiming(
        initialDelayMs = androidStaticRepeatTiming("getKeyRepeatTimeout") ?: NAV_REPEAT_INITIAL_DELAY_FALLBACK_MS,
        repeatIntervalMs = androidStaticRepeatTiming("getKeyRepeatDelay") ?: NAV_REPEAT_INTERVAL_FALLBACK_MS,
    )
}

internal interface NavigationRepeatScheduler {
    fun postDelayed(
        action: Runnable,
        delayMs: Long,
    )

    fun removeCallbacks(action: Runnable)
}
