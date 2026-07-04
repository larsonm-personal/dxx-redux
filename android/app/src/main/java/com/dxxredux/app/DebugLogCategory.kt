package com.dxxredux.app

/**
 * Debug log category IDs matching debug_log_categories.h (C side).
 * Keep both files in sync when adding new categories.
 */
object DebugLogCategory {
    const val NETWORK = 0
    const val GRAPHICS = 1
    const val TEXTURE = 2
    const val GAME = 3
    const val LAUNCHER = 4
    const val PROFILING = 5
    const val COOP_DESYNC = 6
    const val COUNT = 7

    /** Human-readable labels for UI checkboxes, indexed by category ID. */
    val labels = arrayOf("Network", "Graphics", "Texture", "Game Logs", "Launcher", "Profiling", "Coop Desync")

    /** SharedPreferences key for each category toggle. */
    fun prefKey(category: Int): String = "dlog_${labels[category].lowercase()}_enabled"
}
