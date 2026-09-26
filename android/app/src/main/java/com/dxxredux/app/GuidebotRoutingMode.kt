package com.dxxredux.app

internal const val PREF_GUIDEBOT_ROUTING_MODE = "guidebot_routing_mode"

// Keep values synchronized with d2/main/guidebot_routing.h and native save/replay metadata
internal object GuidebotRoutingMode {
    const val ORIGINAL = 0
    const val ENHANCED = 1

    fun sanitize(value: Int): Int = if (value == ORIGINAL) ORIGINAL else ENHANCED
}
