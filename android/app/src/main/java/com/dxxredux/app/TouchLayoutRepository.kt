package com.dxxredux.app

import android.content.Context
import android.util.Log
import org.json.JSONObject
import java.io.File

internal const val DEFAULT_TOUCH_PRESET_NAME = "Advanced"
internal const val CONTROLLER_MENU_TOUCH_PRESET_NAME = "Controller Menus"

internal fun defaultTouchPresetName(hasTouchscreen: Boolean): String =
    if (hasTouchscreen) DEFAULT_TOUCH_PRESET_NAME else CONTROLLER_MENU_TOUCH_PRESET_NAME

internal fun isControllerMenuOnlyTouchLayout(layout: TouchLayout): Boolean =
    layout.name == CONTROLLER_MENU_TOUCH_PRESET_NAME

/**
 * Loads, saves, and provides preset TouchLayouts.
 *
 * Active layout: context.filesDir / "touch_layout.json" (user-owned copy).
 * Bundled presets: assets/configs/touch/ (JSON files, read-only templates).
 * User presets:    context.filesDir / "configs/touch/" (JSON files, imported).
 *
 * On first launch (or reset), a bundled preset is copied into the active slot.
 * All editing happens on the active copy. The bundled files are never modified.
 */
object TouchLayoutRepository {
    private const val TAG = "TouchLayoutRepository"
    private const val FILENAME = "touch_layout.json"
    private const val CURRENT_VERSION = 4
    private const val LEGACY_BTN_CHEATS_MENU = 100
    private const val BUNDLED_DIR = "configs/touch"
    private const val USER_DIR = "configs/touch"

    fun load(context: Context): TouchLayout {
        val file = File(context.filesDir, FILENAME)
        if (!file.exists()) return defaultLayout(context)
        return try {
            migrateForCurrentVersion(TouchLayout.fromJson(JSONObject(file.readText())))
        } catch (_: Exception) {
            defaultLayout(context)
        }
    }

    /** Apply migrations from older layout versions to CURRENT_VERSION. */
    internal fun migrateForCurrentVersion(layout: TouchLayout): TouchLayout {
        var migrated = layout
        if (migrated.version < 2) {
            migrated =
                migrated.copy(
                    version = 2,
                    buttons =
                        migrated.buttons.map { button ->
                            if (button.binding == TouchBindings.BTN_GYRO_RECENTER &&
                                !button.longPressEnabled &&
                                button.longPressBinding < 0
                            ) {
                                button.copy(
                                    longPressEnabled = true,
                                    longPressBinding = TouchBindings.META_GYRO_TOGGLE,
                                    longPressDurationMs = TouchBindings.DEFAULT_LONG_PRESS_DURATION_MS,
                                )
                            } else {
                                button
                            }
                        },
                )
        }
        if (migrated.version < 3) {
            migrated =
                migrated.copy(
                    version = 3,
                    radialMenus = migrated.radialMenus.map { migrateGuideWheelSecretSegment(it) },
                )
        }
        if (migrated.version < 4) {
            migrated =
                migrated.copy(
                    version = 4,
                    buttons = migrated.buttons.mapNotNull { migrateLegacyCheatsButton(it) },
                    radialMenus = migrated.radialMenus.map { migrateLegacyCheatsRadial(it) },
                )
        }
        if (migrated.version >= CURRENT_VERSION) return migrated
        return migrated.copy(version = CURRENT_VERSION)
    }

    private fun migrateLegacyCheatsButton(button: ButtonControl): ButtonControl? {
        if (button.binding == LEGACY_BTN_CHEATS_MENU) return null
        if (button.longPressBinding != LEGACY_BTN_CHEATS_MENU) return button
        return button.copy(longPressEnabled = false, longPressBinding = -1)
    }

    private fun migrateLegacyCheatsRadial(radial: RadialMenuControl): RadialMenuControl =
        radial.copy(
            segments = radial.segments.filter { it.binding != LEGACY_BTN_CHEATS_MENU },
            centerBinding = if (radial.centerBinding == LEGACY_BTN_CHEATS_MENU) -1 else radial.centerBinding,
        )

    private fun migrateGuideWheelSecretSegment(radial: RadialMenuControl): RadialMenuControl {
        if (radial.id != "Guide" ||
            radial.segments.any { it.binding == TouchBindings.META_GUIDE_FIND_SECRET } ||
            radial.segments.size >= 12
        ) {
            return radial
        }
        val secretSegment = RadialSegment("Secret", TouchBindings.META_GUIDE_FIND_SECRET)
        val releaseIndex = radial.segments.indexOfFirst { it.binding == TouchBindings.META_GUIDE_RELEASE_CONTROL }
        val segments =
            if (releaseIndex >= 0) {
                radial.segments.toMutableList().also { it.add(releaseIndex, secretSegment) }
            } else {
                radial.segments + secretSegment
            }
        return radial.copy(segments = segments)
    }

    fun save(
        context: Context,
        layout: TouchLayout,
    ) {
        File(context.filesDir, FILENAME).writeText(migrateForCurrentVersion(layout).toJson().toString(2))
    }

    /** Default layout: first bundled preset, or a minimal hard-coded fallback. */
    fun defaultLayout(context: Context): TouchLayout {
        val bundled = loadBundledPresets(context)
        val defaultName = defaultTouchPresetName(context.hasTouchscreen())
        return bundled.firstOrNull { it.name == defaultName } ?: bundled.firstOrNull() ?: FALLBACK_LAYOUT
    }

    /** Overload without context for field initializers (overlay view, etc.).
     *  Returns a minimal fallback; callers should replace via load(context) ASAP. */
    fun defaultLayout(): TouchLayout = FALLBACK_LAYOUT

    // -- Preset loading --

    /** Load all bundled presets from assets/configs/touch/. */
    fun loadBundledPresets(context: Context): List<TouchLayout> =
        try {
            val files = context.assets.list(BUNDLED_DIR) ?: emptyArray()
            files
                .filter { it.endsWith(".json") }
                .sorted()
                .mapNotNull { loadAssetPreset(context, it) }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to list bundled presets", e)
            emptyList()
        }

    private fun loadAssetPreset(
        context: Context,
        filename: String,
    ): TouchLayout? =
        try {
            val text =
                context.assets
                    .open("$BUNDLED_DIR/$filename")
                    .bufferedReader()
                    .readText()
            val result = HumanReadableConfig.humanJsonToTouchLayout(JSONObject(text))
            if (result.warnings.isNotEmpty()) {
                Log.w(TAG, "Warnings loading $filename: ${result.warnings}")
            }
            result.value?.let { migrateForCurrentVersion(it) }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load bundled preset $filename", e)
            null
        }

    /** Load user-imported presets from filesDir/configs/touch/. */
    fun loadUserPresets(context: Context): List<TouchLayout> {
        val dir = File(context.filesDir, USER_DIR)
        if (!dir.isDirectory) return emptyList()
        return dir
            .listFiles()
            ?.filter { it.extension == "json" }
            ?.sorted()
            ?.mapNotNull { file ->
                try {
                    val result =
                        HumanReadableConfig.humanJsonToTouchLayout(
                            JSONObject(file.readText()),
                        )
                    if (result.warnings.isNotEmpty()) {
                        Log.w(TAG, "Warnings loading ${file.name}: ${result.warnings}")
                    }
                    result.value?.let { migrateForCurrentVersion(it) }
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to load user preset ${file.name}", e)
                    null
                }
            } ?: emptyList()
    }

    /** All available presets: bundled + user-imported. */
    fun allPresets(context: Context): List<TouchLayout> = loadBundledPresets(context) + loadUserPresets(context)

    /** Save a layout as a user preset (for import "add as preset" flow). */
    fun saveUserPreset(
        context: Context,
        layout: TouchLayout,
    ) {
        val dir = File(context.filesDir, USER_DIR)
        dir.mkdirs()
        val safeName = layout.name.replace(Regex("[^a-zA-Z0-9_-]"), "_").lowercase()
        val file = File(dir, "$safeName.json")
        val json = HumanReadableConfig.touchLayoutToHumanJson(layout)
        file.writeText(json.toString(2))
    }

    /** Minimal hard-coded fallback if all JSON loading fails. */
    private val FALLBACK_LAYOUT =
        TouchLayout(
            name = "Fallback",
            sticks =
                listOf(
                    AnalogStickControl(
                        id = "move",
                        xPct = 18f,
                        yPct = 75f,
                        axisX = TouchBindings.AXIS_LEFT_X,
                        axisY = TouchBindings.AXIS_LEFT_Y,
                    ),
                ),
            buttons =
                listOf(
                    ButtonControl(
                        id = "fire1",
                        xPct = 87f,
                        yPct = 90f,
                        binding = TouchBindings.BTN_FIRE_PRIMARY,
                        label = "A",
                    ),
                    ButtonControl(
                        id = "fire2",
                        xPct = 92f,
                        yPct = 82f,
                        binding = TouchBindings.BTN_FIRE_SECONDARY,
                        label = "B",
                    ),
                ),
        )
}
