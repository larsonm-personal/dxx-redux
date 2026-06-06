package com.dxxredux.app

/*
 * Compatibility wrapper for Android game import checks.
 * Kotlin-side game file format knowledge lives in GameFileFormats.
 *
 * Keep GameFileFormats aligned with extract/game_file_extensions.c, which
 * serves the same role for the native extractor code.
 */
object AndroidGameFileExtensions {
    val gameExtensions: Set<String> = GameFileFormats.gameImportExtensions
    val discExtractExtensions: Set<String> = GameFileFormats.discExtractExtensions

    fun hasGameExtension(name: String): Boolean = GameFileFormats.hasGameImportExtension(name)

    fun isGogAudioFile(name: String): Boolean = GameFileFormats.isGogAudioFile(name)
}
