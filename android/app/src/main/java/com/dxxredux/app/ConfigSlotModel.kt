package com.dxxredux.app

internal const val CONFIG_SLOT_NAME_MAX_LENGTH = 25
internal const val DEFAULT_CONFIG_SLOT_NAME = "default"

internal data class ConfigSlot<T>(
    val name: String,
    val value: T,
)

internal data class ConfigSlotSet<T>(
    val activeIndex: Int,
    val slots: List<ConfigSlot<T>>,
) {
    val safeActiveIndex: Int
        get() = activeIndex.coerceIn(0, slots.lastIndex)

    val activeSlot: ConfigSlot<T>
        get() = slots[safeActiveIndex]
}

internal fun normalizeConfigSlotName(
    value: String,
    fallback: String = "slot",
): String {
    val printable = value.trim().filter { character -> !Character.isISOControl(character) }
    val normalized = printable.replace(Regex("\\s+"), " ")
    return normalized.ifEmpty { fallback }.take(CONFIG_SLOT_NAME_MAX_LENGTH)
}
