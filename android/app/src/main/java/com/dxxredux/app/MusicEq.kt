package com.dxxredux.app

/** IDs match music_synth.h and the generated music_eq_presets.h */
internal object MusicEq {
    const val FLAT = "flat"
    const val DETAIL = "measured-sc55-detail"
    const val BALANCED = "measured-sc55-balanced"
    const val BROAD = "measured-sc55-broad"

    // Generated preset provenance: assets/music_eq_sc55.json
    const val SOUNDFONT_SHA256 = "2f68d824f456e3367fe24105d590ee77b7e28faa9f622723b478fa90647d8a1a"
    val presets = listOf(FLAT, DETAIL, BALANCED, BROAD)
    val smoothing =
        linkedMapOf(
            DETAIL to "Detail (0.5 octave)",
            BALANCED to "Balanced (1 octave)",
            BROAD to "Broad (2 octaves)",
        )

    const val OPL3 = "opl3"
    const val BUNDLED = "bundled"

    fun profile(
        renderer: String,
        fontId: String,
    ): String = if (renderer == "ymfm") OPL3 else fontId.ifEmpty { BUNDLED }

    fun defaultPreset(profile: String): String = if (profile == BUNDLED) BALANCED else FLAT

    fun supportsProfile(profile: String): Boolean = profile == BUNDLED || profile == SOUNDFONT_SHA256

    fun decodeProfiles(value: String): Map<String, String> {
        val json = org.json.JSONObject(value)
        return json.keys().asSequence().associateWith { key ->
            require(key in listOf(OPL3, BUNDLED) || key.matches(Regex("[0-9a-f]{64}"))) { "Invalid EQ profile" }
            val preset = json.getString(key)
            require(preset in presets && (preset == FLAT || supportsProfile(key))) { "Invalid profile EQ preset" }
            preset
        }
    }

    fun encodeProfiles(profiles: Map<String, String>): String = org.json.JSONObject(profiles.toSortedMap()).toString()

    fun nativeId(preset: String): Int = presets.indexOf(preset).coerceAtLeast(0)

    fun supports(fontId: String): Boolean = fontId.isEmpty() || fontId == SOUNDFONT_SHA256
}
