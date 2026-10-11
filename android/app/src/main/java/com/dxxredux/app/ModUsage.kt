package com.dxxredux.app

import org.json.JSONObject
import java.io.File

/** Historical engine evidence, not a prediction based on the enabled checkbox */
internal fun readModUsage(
    filesDir: File,
    archivePath: String,
): List<Pair<String, String>> =
    listOf("d1" to "d1x-redux", "d2" to "d2x-redux").mapNotNull { (game, directory) ->
        // Schema and filename are shared with android_mod_assets.cpp
        val report = File(filesDir, "$directory/mod_usage.json")
        runCatching {
            if (!report.isFile || report.length() > 2 * 1024 * 1024) return@runCatching null
            val json = JSONObject(report.readText())
            if (json.optInt("schema") != 1) return@runCatching null

            fun matching(key: String): List<JSONObject> {
                val array = json.optJSONArray(key) ?: return emptyList()
                return (0 until array.length()).mapNotNull { index ->
                    array.optJSONObject(index)?.takeIf { it.optString("source") == archivePath }
                }
            }
            val sounds = matching("sounds")
            val presentation = matching("presentation")
            val played = sounds.count { it.optBoolean("played") }
            val images = presentation.count { it.optString("kind") == "image" }
            val fonts = presentation.count { it.optString("kind") == "font" }
            val patches = presentation.count { it.optString("kind") in setOf("metadata", "bitmap_patch") }
            val text =
                if (sounds.isEmpty() && presentation.isEmpty()) {
                    "No sound, image or font use recorded"
                } else {
                    "${sounds.size} sounds loaded, $played played; $images images, $fonts fonts loaded"
                }
            val details = if (patches > 0) "$text; $patches data patches applied" else text
            "Last ${game.uppercase()} engine run" to details
        }.getOrNull()
    }
