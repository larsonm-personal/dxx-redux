package com.dxxredux.app

/** A launch selection: asset requirements are independent of the executable and its preferences */
internal enum class GameLaunchTarget(
    val id: String,
    val engine: String,
    val content: String,
    val displayName: String,
) {
    D1("d1", "d1", "d1", "Descent 1"),

    // Internal startup profile for standalone engine regression scripts, not a launcher choice
    D1_IN_D2("d1-in-d2", "d2", "d1", "Descent 1 in D2"),
    D2("d2", "d2", "d2", "Descent 2"),
    ;

    val startupArgument: String get() = "-$content"

    fun filesReady(
        d1Ready: Boolean,
        d2Ready: Boolean,
    ): Boolean = if (content == "d1") d1Ready else d2Ready

    companion object {
        val launcherChoices: List<GameLaunchTarget> = listOf(D1, D2)

        fun fromId(id: String): GameLaunchTarget = entries.first { it.id == id }
    }
}
