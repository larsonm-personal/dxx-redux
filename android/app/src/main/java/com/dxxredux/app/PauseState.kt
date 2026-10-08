package com.dxxredux.app

/** Engine-published state. Keep constants and array layout synchronized with android_pause.h and JNI. */
internal data class PauseState(
    val revision: Long = 0,
    val session: Long = 0,
    val uiOwner: Long = 0,
    val uiRevision: Long = 0,
    val request: Long = 0,
    val repairs: Long = 0,
    val reasons: Int = 0,
    val simulationPaused: Boolean = false,
    val inputAllowed: Boolean = false,
    val canResume: Boolean = false,
    val hasGame: Boolean = false,
    val gameFront: Boolean = false,
    val result: Int = 0,
    val legacyDepth: Int = 0,
) {
    val hint: String
        get() =
            when {
                reasons and BACKGROUND != 0 -> "App in background"
                reasons and GRAPHICS != 0 -> "Finish graphics setup"
                reasons and COOP != 0 -> "Waiting for co-op"
                reasons and OPERATION != 0 -> "Game operation in progress"
                reasons and MENU != 0 -> "Close the game menu to continue"
                canResume -> "Tap to resume"
                else -> "Paused"
            }

    companion object {
        const val UI = 1
        const val MENU = 2
        const val USER = 4
        const val GRAPHICS = 8
        const val BACKGROUND = 16
        const val COOP = 32
        const val OPERATION = 64
        const val TRAY = 1
        const val MUSIC = 2
        const val QUICK_LOAD_PROMPT = 4
        const val RESUME = 1
        const val OPEN_MENU = 2
        const val OPEN_SAVE = 3
        const val OPEN_LOAD = 4
        const val QUICK_LOAD = 5

        fun fromNative(v: LongArray): PauseState {
            require(v.size == 14)
            return PauseState(
                v[0],
                v[1],
                v[2],
                v[3],
                v[4],
                v[5],
                v[6].toInt(),
                v[7] != 0L,
                v[8] != 0L,
                v[9] != 0L,
                v[10] != 0L,
                v[11] != 0L,
                v[12].toInt(),
                v[13].toInt(),
            )
        }
    }
}
