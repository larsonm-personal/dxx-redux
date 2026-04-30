package com.dxxredux.app

/*
 * Single Kotlin source of truth for extension-based Android game import checks.
 * Keep this aligned with extract/game_file_extensions.c, which serves the same
 * role for the native extractor code.
 *
 * This does not replace exact filename sets like ALL_GAME_FILENAMES. Those are
 * a separate concept: known required asset names versus generic game file
 * extensions that should be recognized during import.
 */
object AndroidGameFileExtensions {
    val gameExtensions =
        setOf(
            "hog",
            "pig",
            "ham",
            "s11",
            "s22",
            "dem",
            "mvl",
            "msn",
            "mn2",
            "gog",
            "inst",
        )

    private val gogAudioExtensions = setOf("gog", "inst")

    fun hasGameExtension(name: String): Boolean = extensionOf(name) in gameExtensions

    fun isGogAudioFile(name: String): Boolean = extensionOf(name) in gogAudioExtensions

    private fun extensionOf(name: String): String = name.substringAfterLast('.', "").lowercase()
}
