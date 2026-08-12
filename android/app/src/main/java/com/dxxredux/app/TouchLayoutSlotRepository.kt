package com.dxxredux.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import kotlin.math.min

internal object TouchLayoutSlotRepository {
    private const val FILENAME = "touch_layout_slots.json"
    private const val TYPE = "touch_layout_slots"
    private const val VERSION = 1

    fun load(context: Context): ConfigSlotSet<TouchLayout> {
        val file = File(context.filesDir, FILENAME)
        if (file.exists()) {
            try {
                val loaded = slotSetFromStorageJson(JSONObject(file.readText()))
                if (loaded != null) return loaded
            } catch (_: Exception) {
            }
        }
        return ConfigSlotSet(0, listOf(ConfigSlot(DEFAULT_CONFIG_SLOT_NAME, TouchLayoutRepository.load(context))))
    }

    fun saveActiveLayout(
        context: Context,
        layout: TouchLayout,
    ): ConfigSlotSet<TouchLayout> {
        val slotSet = load(context)
        val slots = slotSet.slots.toMutableList()
        val activeIndex = slotSet.safeActiveIndex
        slots[activeIndex] = slots[activeIndex].copy(value = TouchLayoutRepository.migrateForCurrentVersion(layout))
        val updated = normalizeSlotSet(ConfigSlotSet(activeIndex, slots), TouchLayoutRepository.defaultLayout(context))
        write(context, updated)
        TouchLayoutRepository.save(context, updated.activeSlot.value)
        return updated
    }

    fun selectSlot(
        context: Context,
        slotIndex: Int,
    ): ConfigSlotSet<TouchLayout> {
        val current = load(context)
        val updated =
            normalizeSlotSet(current.copy(activeIndex = slotIndex), TouchLayoutRepository.defaultLayout(context))
        write(context, updated)
        TouchLayoutRepository.save(context, updated.activeSlot.value)
        return updated
    }

    fun renameActiveSlot(
        context: Context,
        name: String,
    ): ConfigSlotSet<TouchLayout> {
        val current = load(context)
        if (current.safeActiveIndex == 0) return current
        val slots = current.slots.toMutableList()
        slots[current.safeActiveIndex] = slots[current.safeActiveIndex].copy(name = normalizeConfigSlotName(name))
        val updated = normalizeSlotSet(current.copy(slots = slots), TouchLayoutRepository.defaultLayout(context))
        write(context, updated)
        return updated
    }

    fun addDefaultSlot(
        context: Context,
        name: String,
    ): ConfigSlotSet<TouchLayout> {
        val current = load(context)
        val slots =
            current.slots + ConfigSlot(normalizeConfigSlotName(name), TouchLayoutRepository.defaultLayout(context))
        val updated =
            normalizeSlotSet(ConfigSlotSet(slots.lastIndex, slots), TouchLayoutRepository.defaultLayout(context))
        write(context, updated)
        TouchLayoutRepository.save(context, updated.activeSlot.value)
        return updated
    }

    fun duplicateActiveSlot(
        context: Context,
        name: String,
        sourceLayout: TouchLayout,
    ): ConfigSlotSet<TouchLayout> {
        val current = load(context)
        val slots =
            current.slots +
                ConfigSlot(normalizeConfigSlotName(name), TouchLayoutRepository.migrateForCurrentVersion(sourceLayout))
        val updated =
            normalizeSlotSet(ConfigSlotSet(slots.lastIndex, slots), TouchLayoutRepository.defaultLayout(context))
        write(context, updated)
        TouchLayoutRepository.save(context, updated.activeSlot.value)
        return updated
    }

    fun deleteActiveSlot(context: Context): ConfigSlotSet<TouchLayout> {
        val current = load(context)
        val activeIndex = current.safeActiveIndex
        if (activeIndex == 0) return current
        val slots = current.slots.toMutableList()
        slots.removeAt(activeIndex)
        val updated =
            normalizeSlotSet(
                ConfigSlotSet(min(activeIndex, slots.lastIndex), slots),
                TouchLayoutRepository.defaultLayout(context),
            )
        write(context, updated)
        TouchLayoutRepository.save(context, updated.activeSlot.value)
        return updated
    }

    fun replaceSlots(
        context: Context,
        slotSet: ConfigSlotSet<TouchLayout>,
    ): ConfigSlotSet<TouchLayout> {
        val updated = normalizeSlotSet(slotSet, TouchLayoutRepository.defaultLayout(context))
        write(context, updated)
        TouchLayoutRepository.save(context, updated.activeSlot.value)
        return updated
    }

    fun clear(context: Context) {
        File(context.filesDir, FILENAME).delete()
    }

    internal fun toExportJsonArray(slotSet: ConfigSlotSet<TouchLayout>): JSONArray {
        val slotsArray = JSONArray()
        val normalized = normalizeSlotSet(slotSet, slotSet.activeSlot.value)
        normalized.slots.forEach { slot ->
            slotsArray.put(
                JSONObject()
                    .put("name", slot.name)
                    .put("layout", HumanReadableConfig.touchLayoutToHumanJson(slot.value)),
            )
        }
        return slotsArray
    }

    internal fun fromExportJsonArray(
        slotsArray: JSONArray,
        activeIndex: Int,
    ): ConfigSlotSet<TouchLayout>? {
        if (slotsArray.length() == 0 || activeIndex !in 0 until slotsArray.length()) return null
        val slots = mutableListOf<ConfigSlot<TouchLayout>>()
        for (slotIndex in 0 until slotsArray.length()) {
            val slotObject = slotsArray.optJSONObject(slotIndex) ?: return null
            val layoutObject = slotObject.optJSONObject("layout") ?: return null
            if (slotObject.has("name") && slotObject.opt("name") !is String) return null
            val parsed = HumanReadableConfig.humanJsonToTouchLayout(layoutObject)
            if (parsed.warnings.isNotEmpty()) return null
            val layout = parsed.value?.let { TouchLayoutRepository.migrateForCurrentVersion(it) } ?: return null
            val name =
                if (slotIndex == 0) {
                    DEFAULT_CONFIG_SLOT_NAME
                } else {
                    normalizeConfigSlotName(slotObject.optString("name", "slot ${slotIndex + 1}"))
                }
            slots.add(ConfigSlot(name, layout))
        }
        return normalizeSlotSet(ConfigSlotSet(activeIndex, slots), slots.first().value)
    }

    private fun slotSetFromStorageJson(jsonObject: JSONObject): ConfigSlotSet<TouchLayout>? {
        val slotsArray = jsonObject.optJSONArray("slots") ?: return null
        return fromExportJsonArray(slotsArray, jsonObject.optInt("active", 0))
    }

    private fun write(
        context: Context,
        slotSet: ConfigSlotSet<TouchLayout>,
    ) {
        val normalized = normalizeSlotSet(slotSet, slotSet.activeSlot.value)
        val jsonObject =
            JSONObject()
                .put("type", TYPE)
                .put("version", VERSION)
                .put("active", normalized.safeActiveIndex)
                .put("slots", toExportJsonArray(normalized))
        File(context.filesDir, FILENAME).writeText(jsonObject.toString(2))
    }

    private fun normalizeSlotSet(
        slotSet: ConfigSlotSet<TouchLayout>,
        fallbackLayout: TouchLayout,
    ): ConfigSlotSet<TouchLayout> {
        val sourceSlots = slotSet.slots.ifEmpty { listOf(ConfigSlot(DEFAULT_CONFIG_SLOT_NAME, fallbackLayout)) }
        val normalizedSlots =
            sourceSlots.mapIndexed { slotIndex, slot ->
                val name =
                    if (slotIndex == 0) {
                        DEFAULT_CONFIG_SLOT_NAME
                    } else {
                        normalizeConfigSlotName(slot.name, "slot ${slotIndex + 1}")
                    }
                slot.copy(name = name, value = TouchLayoutRepository.migrateForCurrentVersion(slot.value))
            }
        return ConfigSlotSet(slotSet.activeIndex.coerceIn(0, normalizedSlots.lastIndex), normalizedSlots)
    }
}
