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

    fun nativeId(preset: String): Int = presets.indexOf(preset).coerceAtLeast(0)

    fun supports(fontId: String): Boolean = fontId.isEmpty() || fontId == SOUNDFONT_SHA256
}
