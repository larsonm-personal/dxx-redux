package com.dxxredux.app

import java.io.File
import java.time.Instant

/** Export cached metadata verbatim without refreshing manifests or hashing game files */
internal object LogAssetSnapshot {
    fun capture(filesDir: File): String =
        buildString {
            appendLine()
            appendLine("=== Game file metadata at log export ===")
            appendLine("Exported at: ${Instant.now()}")
            appendLine("Current launcher configuration and last generated launch paths; not the historical log session")
            appendLine("Hashes are cached import metadata and have not been revalidated")
            try {
                val sets = FileSetManager(filesDir)
                val active = sets.getActive()
                val setDir = sets.getSetDir(active)
                appendLine("Active file set: $active")
                appendLine("File set directory: ${setDir.absolutePath}")
                val metadata =
                    listOf(
                        File(filesDir, "file_sets.json"),
                        File(setDir, "assets.json"),
                        File(setDir, ".saf_manifest.json"),
                        File(setDir, ".content/mods/mod_manifest.json"),
                    ) +
                        listOf("d1x-redux", "d2x-redux").flatMap { game ->
                            listOf(
                                ".active_set_path",
                                ".active_mod_paths",
                                ".mission_assets.json",
                                ".saf_manifest.json",
                            ).map {
                                File(filesDir, "$game/$it")
                            }
                        }
                for (file in metadata) {
                    appendLine()
                    appendLine("--- ${file.absolutePath} ---")
                    try {
                        appendLine(if (file.isFile) file.readText() else "[not present]")
                    } catch (e: Exception) {
                        appendLine("[unavailable: ${e.message}]")
                    }
                }
            } catch (e: Exception) {
                appendLine("[snapshot unavailable: ${e.message}]")
            }
            appendLine("=== End game file metadata ===")
        }
}
