package com.dxxredux.app

/*
 * Compatibility wrapper for Android game import checks.
 * GameFileFormats owns extension policy. Native extractor tables mirror its
 * roles and AndroidGameFileExtensionsTest enforces exact parity.
 */
object AndroidGameFileExtensions {
    val gameExtensions: Set<String> = GameFileFormats.gameImportExtensions
    val discExtractExtensions: Set<String> = GameFileFormats.discExtractExtensions

    fun hasGameExtension(name: String): Boolean = GameFileFormats.hasGameImportExtension(name)

    fun isGogAudioFile(name: String): Boolean = GameFileFormats.isGogAudioFile(name)
}
