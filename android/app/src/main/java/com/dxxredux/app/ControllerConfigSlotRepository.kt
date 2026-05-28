package com.dxxredux.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import kotlin.math.min

internal object ControllerConfigSlotRepository {
    private const val FILENAME = "controller_config_slots.json"
    private const val TYPE = "controller_config_slots"
    private const val VERSION = 1

    fun load(context: Context): ConfigSlotSet<ControllerConfigState> {
        val file = File(context.filesDir, FILENAME)
        if (file.exists()) {
            try {
                val loaded = slotSetFromStorageJson(JSONObject(file.readText()))
                if (loaded != null) return loaded
            } catch (_: Exception) {
            }
        }
        val activeConfig = readActiveControllerConfig(context) ?: loadDefaultControllerConfig(context)
        return ConfigSlotSet(0, listOf(ConfigSlot(DEFAULT_CONFIG_SLOT_NAME, activeConfig)))
    }

    fun saveActiveConfig(
        context: Context,
        config: ControllerConfigState,
        gameVariant: String = "d2",
    ): ConfigSlotSet<ControllerConfigState> {
        val slotSet = load(context)
        val slots = slotSet.slots.toMutableList()
        val activeIndex = slotSet.safeActiveIndex
        slots[activeIndex] = slots[activeIndex].copy(value = config)
        val updated = normalizeSlotSet(ConfigSlotSet(activeIndex, slots), loadDefaultControllerConfig(context))
        write(context, updated)
        saveConfig(context, config, gameVariant)
        return updated
    }

    fun selectSlot(
        context: Context,
        slotIndex: Int,
        gameVariant: String = "d2",
    ): ConfigSlotSet<ControllerConfigState> {
        val current = load(context)
        val updated = normalizeSlotSet(current.copy(activeIndex = slotIndex), loadDefaultControllerConfig(context))
        write(context, updated)
        saveConfig(context, updated.activeSlot.value, gameVariant)
        return updated
    }

    fun renameActiveSlot(
        context: Context,
        name: String,
    ): ConfigSlotSet<ControllerConfigState> {
        val current = load(context)
        if (current.safeActiveIndex == 0) return current
        val slots = current.slots.toMutableList()
        slots[current.safeActiveIndex] = slots[current.safeActiveIndex].copy(name = normalizeConfigSlotName(name))
        val updated = normalizeSlotSet(current.copy(slots = slots), loadDefaultControllerConfig(context))
        write(context, updated)
        return updated
    }

    fun addDefaultSlot(
        context: Context,
        name: String,
        gameVariant: String = "d2",
    ): ConfigSlotSet<ControllerConfigState> {
        val current = load(context)
        val slots = current.slots + ConfigSlot(normalizeConfigSlotName(name), loadDefaultControllerConfig(context))
        val updated = normalizeSlotSet(ConfigSlotSet(slots.lastIndex, slots), loadDefaultControllerConfig(context))
        write(context, updated)
        saveConfig(context, updated.activeSlot.value, gameVariant)
        return updated
    }

    fun duplicateActiveSlot(
        context: Context,
        name: String,
        sourceConfig: ControllerConfigState,
        gameVariant: String = "d2",
    ): ConfigSlotSet<ControllerConfigState> {
        val current = load(context)
        val slots = current.slots + ConfigSlot(normalizeConfigSlotName(name), sourceConfig)
        val updated = normalizeSlotSet(ConfigSlotSet(slots.lastIndex, slots), loadDefaultControllerConfig(context))
        write(context, updated)
        saveConfig(context, updated.activeSlot.value, gameVariant)
        return updated
    }

    fun deleteActiveSlot(
        context: Context,
        gameVariant: String = "d2",
    ): ConfigSlotSet<ControllerConfigState> {
        val current = load(context)
        val activeIndex = current.safeActiveIndex
        if (activeIndex == 0) return current
        val slots = current.slots.toMutableList()
        slots.removeAt(activeIndex)
        val updated =
            normalizeSlotSet(
                ConfigSlotSet(min(activeIndex, slots.lastIndex), slots),
                loadDefaultControllerConfig(context),
            )
        write(context, updated)
        saveConfig(context, updated.activeSlot.value, gameVariant)
        return updated
    }

    fun replaceSlots(
        context: Context,
        slotSet: ConfigSlotSet<ControllerConfigState>,
        gameVariant: String = "d2",
    ): ConfigSlotSet<ControllerConfigState> {
        val updated = normalizeSlotSet(slotSet, loadDefaultControllerConfig(context))
        write(context, updated)
        saveConfig(context, updated.activeSlot.value, gameVariant)
        return updated
    }

    fun clear(context: Context) {
        File(context.filesDir, FILENAME).delete()
    }

    internal fun toExportJsonArray(slotSet: ConfigSlotSet<ControllerConfigState>): JSONArray {
        val slotsArray = JSONArray()
        val normalized = normalizeSlotSet(slotSet, slotSet.activeSlot.value)
        normalized.slots.forEach { slot ->
            slotsArray.put(
                JSONObject()
                    .put("name", slot.name)
                    .put("config", controllerConfigStateToHumanJson(slot.value)),
            )
        }
        return slotsArray
    }

    internal fun fromExportJsonArray(
        slotsArray: JSONArray,
        activeIndex: Int,
    ): ConfigSlotSet<ControllerConfigState>? {
        val slots = mutableListOf<ConfigSlot<ControllerConfigState>>()
        for (slotIndex in 0 until slotsArray.length()) {
            val slotObject = slotsArray.optJSONObject(slotIndex) ?: continue
            val configObject = slotObject.optJSONObject("config") ?: continue
            val parsed = controllerConfigStateFromHumanJson(configObject)
            val config = parsed.value ?: continue
            val name =
                if (slotIndex == 0) {
                    DEFAULT_CONFIG_SLOT_NAME
                } else {
                    normalizeConfigSlotName(slotObject.optString("name", "slot ${slotIndex + 1}"))
                }
            slots.add(ConfigSlot(name, config))
        }
        if (slots.isEmpty()) return null
        return normalizeSlotSet(ConfigSlotSet(activeIndex, slots), slots.first().value)
    }

    private fun slotSetFromStorageJson(jsonObject: JSONObject): ConfigSlotSet<ControllerConfigState>? {
        val slotsArray = jsonObject.optJSONArray("slots") ?: return null
        return fromExportJsonArray(slotsArray, jsonObject.optInt("active", 0))
    }

    private fun write(
        context: Context,
        slotSet: ConfigSlotSet<ControllerConfigState>,
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
        slotSet: ConfigSlotSet<ControllerConfigState>,
        fallbackConfig: ControllerConfigState,
    ): ConfigSlotSet<ControllerConfigState> {
        val sourceSlots = slotSet.slots.ifEmpty { listOf(ConfigSlot(DEFAULT_CONFIG_SLOT_NAME, fallbackConfig)) }
        val normalizedSlots =
            sourceSlots.mapIndexed { slotIndex, slot ->
                val name =
                    if (slotIndex == 0) {
                        DEFAULT_CONFIG_SLOT_NAME
                    } else {
                        normalizeConfigSlotName(slot.name, "slot ${slotIndex + 1}")
                    }
                slot.copy(name = name)
            }
        return ConfigSlotSet(slotSet.activeIndex.coerceIn(0, normalizedSlots.lastIndex), normalizedSlots)
    }
}
