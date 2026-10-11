package com.dxxredux.app

import org.json.JSONObject

/** Current engine residency, never inferred from an enabled launcher checkbox */
internal fun texturePackStatusRows(report: JSONObject): List<Pair<String, String>> {
    val loaded = report.optInt("loaded")
    val status =
        when (report.optString("status")) {
            "blocked_multiplayer" -> "Blocked in competitive multiplayer"
            "active" -> "Active: $loaded replacements loaded"
            "none_loaded" -> "No replacements loaded"
            else -> "Waiting for engine"
        }
    val packs = report.optJSONArray("packs")
    val names =
        if (packs == null) {
            emptyList()
        } else {
            (0 until packs.length()).map { index ->
                val pack = packs.getJSONObject(index)
                val name = pack.optString("name").removeSuffix(".dxa")
                val known = Regex("d([12])-hires-(128|256|512)-textures-ktx2").matchEntire(name)
                val label = if (known == null) name else "D${known.groupValues[1]} ${known.groupValues[2]}px"
                "$label (${pack.optInt("loaded")})"
            }
        }
    return listOf(
        "Textures:" to status,
        "Packs:" to names.joinToString(", ").ifEmpty { "None in use" },
        "Mipmaps:" to "${report.optInt("mipmapped")} textures; ${report.optInt("adapted")} resized by mip",
    )
}
