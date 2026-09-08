package com.dxxredux.app

import java.util.Locale

data class MissionVariantPreference(
    val id: String,
    val displayName: String,
    val directoryNames: Set<String>,
    val archiveAliases: Set<String>,
    val supportedByRedux: Boolean = true,
)

val MISSION_VARIANT_MASK_PRECEDENCE =
    arrayOf(
        MissionVariantPreference("rebirth", "Rebirth", setOf("REBIRTH"), setOf("rebirth")),
        MissionVariantPreference("dos", "DOS", setOf("DOS"), setOf("dos")),
        MissionVariantPreference("d2x", "D2X", setOf("D2X"), setOf("d2x")),
        MissionVariantPreference(
            "d2xxl",
            "D2X-XL",
            emptySet(),
            setOf("d2x-xl", "d2xxl", "xl"),
            supportedByRedux = false,
        ),
    )

data class PreferredMissionVariant<T>(
    val value: T,
    val preference: MissionVariantPreference,
)

fun missionVariantForDirectoryName(name: String): MissionVariantPreference? {
    val normalized = name.uppercase(Locale.US)
    return MISSION_VARIANT_MASK_PRECEDENCE.firstOrNull { normalized in it.directoryNames }
}

fun missionVariantForArchiveFilename(filename: String): MissionVariantPreference? {
    val normalized =
        filename
            .substringAfterLast('/')
            .substringAfterLast('\\')
            .substringBeforeLast('.')
            .lowercase(Locale.US)
            .replace(Regex("[^a-z0-9]+"), "-")
            .trim('-')
    return MISSION_VARIANT_MASK_PRECEDENCE.firstOrNull { preference ->
        preference.archiveAliases.any { alias -> normalized == alias || normalized.endsWith("-$alias") }
    }
}

fun <T> selectPreferredMissionVariant(
    candidates: List<T>,
    preferenceOf: (T) -> MissionVariantPreference?,
): PreferredMissionVariant<T>? {
    val recognized = candidates.mapNotNull { candidate -> preferenceOf(candidate)?.let { candidate to it } }
    for (preference in MISSION_VARIANT_MASK_PRECEDENCE) {
        val matches = recognized.filter { it.second == preference }
        if (matches.size > 1) return null
        matches.singleOrNull()?.let { return PreferredMissionVariant(it.first, preference) }
    }
    return null
}
