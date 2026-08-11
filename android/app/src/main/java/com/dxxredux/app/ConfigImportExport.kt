package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import com.dxxredux.app.multiplayer.HostGameDefaults
import org.json.JSONObject
import java.io.File

/** Utilities for exporting and importing touch-layout and controller configs. */
object ConfigImportExport {
    private const val TAG = "ConfigImportExport"
    private const val MIME_JSON = "application/json"
    private const val COMBINED_CONFIG_VERSION = 3

    internal enum class ExportedPreferenceType(
        val displayName: String,
    ) {
        BOOLEAN("Boolean"),
        STRING("String"),
        ;

        fun accepts(value: Any?): Boolean =
            when (this) {
                BOOLEAN -> value is Boolean
                STRING -> value is String
            }
    }

    internal data class ExportedPreference(
        val key: String,
        val type: ExportedPreferenceType,
    )

    // SharedPreferences included in config export with their declared runtime type.
    // Excludes debug/session/device-specific keys like selected_game, host_*, debug_log_*.
    internal val EXPORTED_PREFERENCES =
        listOf(
            ExportedPreference("render_resolution", ExportedPreferenceType.STRING),
            ExportedPreference("game_orientation", ExportedPreferenceType.STRING),
            ExportedPreference("music_mode", ExportedPreferenceType.STRING),
            ExportedPreference("touch_overlay_enabled", ExportedPreferenceType.BOOLEAN),
            ExportedPreference(PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE, ExportedPreferenceType.BOOLEAN),
            ExportedPreference(PREF_ALLOW_ACOUSTID_WEB_LOOKUPS, ExportedPreferenceType.BOOLEAN),
            ExportedPreference(PREF_SHOW_RESUME_OFFER, ExportedPreferenceType.BOOLEAN),
            ExportedPreference(PREF_SHOW_DEMO_INSTALLER_OFFER, ExportedPreferenceType.BOOLEAN),
            ExportedPreference(PREF_GUIDEBOT_HELPER_LINE, ExportedPreferenceType.BOOLEAN),
            ExportedPreference(PREF_NEAREST_PLAYER_LINE, ExportedPreferenceType.BOOLEAN),
            ExportedPreference(PREF_HEADLIGHT_OFF_BY_DEFAULT, ExportedPreferenceType.BOOLEAN),
        )

    internal data class DecodedPreferences(
        val values: Map<ExportedPreference, Any> = emptyMap(),
        val error: String? = null,
    )

    internal fun exportPreferenceValues(allPrefs: Map<String, *>): JSONObject {
        val json = JSONObject()
        for (pref in EXPORTED_PREFERENCES) {
            if (!allPrefs.containsKey(pref.key)) continue
            val value = allPrefs[pref.key]
            if (pref.type.accepts(value)) json.put(pref.key, value)
        }
        return json
    }

    internal fun decodePreferenceValues(json: JSONObject): DecodedPreferences {
        val decoded = linkedMapOf<ExportedPreference, Any>()
        for (pref in EXPORTED_PREFERENCES) {
            if (!json.has(pref.key)) continue
            val value = json.get(pref.key)
            if (!pref.type.accepts(value)) {
                return DecodedPreferences(error = "'${pref.key}' must be ${pref.type.displayName}")
            }
            decoded[pref] = value
        }
        return DecodedPreferences(values = decoded)
    }

    // descent.cfg keys managed through the launcher UI
    private val EXPORTED_CFG_KEYS =
        listOf(
            "TexFilt",
            "ColorDepth",
            "MsaaLevel",
            "AnisoLevel",
            "MenuTexFilt",
            "HudTexFilt",
            "MainViewFov",
            "CornerTextInset",
            "ClassicDepth",
            "MovieTexFilt",
        )

    // ── Export ───────────────────────────────────────────────────────────────

    /** Export current touch layout as human-readable JSON via share sheet. */
    fun exportTouchLayout(context: Context): Boolean {
        val layout = TouchLayoutSlotRepository.load(context).activeSlot.value
        val json = HumanReadableConfig.touchLayoutToHumanJson(layout)
        return shareJson(context, json, "touch_layout.json", "Share Touch Layout")
    }

    /** Export current controller config as human-readable JSON via share sheet. */
    fun exportControllerConfig(context: Context): Boolean {
        val config = ControllerConfigSlotRepository.load(context).activeSlot.value
        val json = controllerConfigStateToHumanJson(config)
        return shareJson(context, json, "controller_config.json", "Share Controller Config")
    }

    /** Export both configs as a combined JSON via share sheet. */
    fun exportAll(context: Context): Boolean {
        val combined = JSONObject()
        combined.put("type", "combined_config")
        combined.put("version", COMBINED_CONFIG_VERSION)

        val touchSlots = TouchLayoutSlotRepository.load(context)
        combined.put("touch_layout_slots", TouchLayoutSlotRepository.toExportJsonArray(touchSlots))
        combined.put("active_touch_layout_slot", touchSlots.safeActiveIndex)

        val controllerSlots = ControllerConfigSlotRepository.load(context)
        combined.put("controller_config_slots", ControllerConfigSlotRepository.toExportJsonArray(controllerSlots))
        combined.put("active_controller_config_slot", controllerSlots.safeActiveIndex)

        // Autoselect weapon ordering for both games
        val filesDir = context.filesDir.absolutePath
        for (game in listOf("d1", "d2")) {
            try {
                val data = NativeAutoselectPatcher.readAutoselect(game, filesDir)
                if (data.isNotEmpty()) {
                    val arr = org.json.JSONArray()
                    for (v in data) arr.put(v)
                    combined.put("autoselect_$game", arr)
                }
            } catch (_: Exception) {
                // Native lib may not be loaded in all contexts; skip quietly
            }
        }

        // App settings: SharedPreferences + descent.cfg graphics keys
        val prefs = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        val appSettings = exportPreferenceValues(prefs.all)
        val cfgObj = JSONObject()
        for (key in EXPORTED_CFG_KEYS) {
            val v = readConfigValue(context.filesDir, key)
            if (v != null) cfgObj.put(key, v)
        }
        if (cfgObj.length() > 0) appSettings.put("descent_cfg", cfgObj)
        if (appSettings.length() > 0) combined.put("app_settings", appSettings)

        val enginePrefs = exportEnginePrefs(context.filesDir)
        if (enginePrefs.length() > 0) combined.put("engine_prefs", enginePrefs)

        combined.put("host_defaults", exportHostDefaults(context))

        return shareJson(context, combined, "dxx_redux_config.json", "Share Config")
    }

    // ── Import ──────────────────────────────────────────────────────────────

    /**
     * Import a config from a content:// URI. Returns a human-readable summary
     * of what was imported, or an error message.
     */
    internal suspend fun prepareFromUri(
        context: Context,
        uri: Uri,
    ): ConfigImportPreparation =
        onConfigImportWorker {
            val size = ImportStorageGuard.queryUriSizeBytes(context.contentResolver, uri)
            if (size != null && size > MAX_CONFIG_IMPORT_BYTES) {
                return@onConfigImportWorker ConfigImportPreparation.Error(
                    "Error: configuration file exceeds $MAX_CONFIG_IMPORT_BYTES bytes",
                )
            }
            try {
                context.contentResolver
                    .openInputStream(uri)
                    ?.use(::prepareConfigImport)
                    ?: ConfigImportPreparation.Error("Error: could not open file")
            } catch (e: Exception) {
                Log.e(TAG, "Failed to read import file", e)
                ConfigImportPreparation.Error("Error: ${e.message}")
            }
        }

    suspend fun importFromUri(
        context: Context,
        uri: Uri,
    ): String =
        when (val preparation = prepareFromUri(context, uri)) {
            is ConfigImportPreparation.Error -> preparation.message
            is ConfigImportPreparation.Ready -> importPrepared(context, preparation.config)
        }

    internal suspend fun importPrepared(
        context: Context,
        prepared: PreparedConfigImport,
    ): String =
        onConfigImportWorker {
            when (prepared.type) {
                "touch_layout" -> importTouchLayout(context, prepared.json)
                "controller_config" -> importControllerConfig(context, prepared.json)
                "combined_config" -> importCombined(context, prepared.json)
                else -> "Error: unrecognized config type"
            }
        }

    private fun importTouchLayout(
        context: Context,
        json: JSONObject,
    ): String {
        val result = HumanReadableConfig.humanJsonToTouchLayout(json)
        if (result.value == null) {
            val msg = result.warnings.joinToString("; ")
            return "Touch layout import failed: $msg"
        }
        TouchLayoutSlotRepository.saveActiveLayout(context, result.value)
        val warnings =
            if (result.warnings.isNotEmpty()) {
                "\nWarnings: ${result.warnings.joinToString("; ")}"
            } else {
                ""
            }
        return "Touch layout imported successfully.$warnings"
    }

    private fun importControllerConfig(
        context: Context,
        json: JSONObject,
    ): String {
        val result = HumanReadableConfig.humanJsonToControllerConfig(json)
        if (result.value == null) {
            val msg = result.warnings.joinToString("; ")
            return "Controller config import failed: $msg"
        }
        ControllerConfigSlotRepository.saveActiveConfig(context, controllerConfigStateFromHumanData(result.value))
        val warnings =
            if (result.warnings.isNotEmpty()) {
                "\nWarnings: ${result.warnings.joinToString("; ")}"
            } else {
                ""
            }
        return "Controller config imported successfully.$warnings"
    }

    private fun importCombined(
        context: Context,
        json: JSONObject,
    ): String {
        val results = mutableListOf<String>()
        if (json.has("touch_layout_slots")) {
            val imported =
                TouchLayoutSlotRepository.fromExportJsonArray(
                    json.getJSONArray("touch_layout_slots"),
                    json.optInt("active_touch_layout_slot", 0),
                )
            if (imported != null) {
                val saved = TouchLayoutSlotRepository.replaceSlots(context, imported)
                results.add("Touch layout slots: imported ${saved.slots.size} slot(s)")
            } else {
                results.add("Touch layout slots import failed")
            }
        } else if (json.has("touch_layout")) {
            results.add(importTouchLayout(context, json.getJSONObject("touch_layout")))
        }
        if (json.has("controller_config_slots")) {
            val imported =
                ControllerConfigSlotRepository.fromExportJsonArray(
                    json.getJSONArray("controller_config_slots"),
                    json.optInt("active_controller_config_slot", 0),
                )
            if (imported != null) {
                val saved = ControllerConfigSlotRepository.replaceSlots(context, imported)
                results.add("Controller config slots: imported ${saved.slots.size} slot(s)")
            } else {
                results.add("Controller config slots import failed")
            }
        } else if (json.has("controller_config")) {
            results.add(importControllerConfig(context, json.getJSONObject("controller_config")))
        }
        // Autoselect ordering
        val filesDir = context.filesDir.absolutePath
        for (game in listOf("d1", "d2")) {
            val key = "autoselect_$game"
            if (json.has(key)) {
                try {
                    val arr = json.getJSONArray(key)
                    val primEntries = NativeAutoselectPatcher.getPrimaryWeaponEntries(game)
                    val secEntries = NativeAutoselectPatcher.getSecondaryWeaponEntries(game)
                    val primLen = primEntries.size / 2
                    val secLen = secEntries.size / 2
                    if (arr.length() == primLen + secLen) {
                        val prim = IntArray(primLen) { arr.getInt(it) }
                        val sec = IntArray(secLen) { arr.getInt(primLen + it) }
                        val count = NativeAutoselectPatcher.writeAutoselect(game, filesDir, prim, sec)
                        if (count >= 0) {
                            results.add("Autoselect ($game): patched $count file(s)")
                        } else {
                            results.add("Autoselect ($game) import failed: invalid weapon order")
                        }
                    } else {
                        results.add("Autoselect ($game) import failed: invalid entry count")
                    }
                } catch (e: Exception) {
                    results.add("Autoselect ($game) import failed: ${e.message}")
                }
            }
        }
        // App settings: SharedPreferences + descent.cfg
        if (json.has("app_settings")) {
            results.add(importAppSettings(context, json.getJSONObject("app_settings")))
        }
        if (json.has("engine_prefs")) {
            results.add(importEnginePrefs(context, json.getJSONObject("engine_prefs")))
        }
        if (json.has("host_defaults")) {
            results.add(importHostDefaults(context, json.getJSONObject("host_defaults")))
        }
        if (results.isEmpty()) return "Combined config had no recognizable sections."
        return results.joinToString("\n")
    }

    // ── App settings ───────────────────────────────────────────────────────

    private fun importAppSettings(
        context: Context,
        json: JSONObject,
    ): String {
        val decoded = decodePreferenceValues(json)
        if (decoded.error != null) return "App settings import failed: ${decoded.error}"

        var count = 0
        val editor = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE).edit()
        for ((pref, value) in decoded.values) {
            when (pref.type) {
                ExportedPreferenceType.BOOLEAN -> editor.putBoolean(pref.key, value as Boolean)
                ExportedPreferenceType.STRING -> editor.putString(pref.key, value as String)
            }
            count++
        }
        editor.apply()

        // descent.cfg graphics settings
        if (json.has("descent_cfg")) {
            val cfgObj = json.getJSONObject("descent_cfg")
            val pairs = mutableListOf<Pair<String, String>>()
            val d2Pairs = mutableListOf<Pair<String, String>>()
            for (key in EXPORTED_CFG_KEYS) {
                if (cfgObj.has(key)) {
                    val pair = key to cfgObj.getString(key)
                    if (key == "MovieTexFilt") d2Pairs.add(pair) else pairs.add(pair)
                }
            }
            if (pairs.isNotEmpty()) {
                updateAllConfigFiles(context.filesDir, pairs)
                count += pairs.size
            }
            if (d2Pairs.isNotEmpty()) {
                updateConfigFilesForGame(context.filesDir, "d2", d2Pairs)
                count += d2Pairs.size
            }
            // Resolution is stored in SharedPreferences, applied to cfg via helper
            if (json.has("render_resolution")) {
                updateDescentCfgResolution(context.filesDir, json.getString("render_resolution"))
            }
        }
        return "App settings: imported $count setting(s)"
    }

    private fun exportEnginePrefs(filesDir: File): JSONObject {
        val json = JSONObject()
        for (game in listOf("d1", "d2")) {
            try {
                val prefs = NativePilotPreferences.readEnginePrefs(game, filesDir.absolutePath)
                if (!prefs.hasPilotFile) continue
                val homingPrefs = NativePilotPreferences.readOriginalHomingPrefs(game, filesDir.absolutePath)
                json.put(
                    game,
                    JSONObject().apply {
                        put("cockpit_mode", prefs.cockpitMode)
                        put("auto_leveling", prefs.autoLeveling)
                        put("show_robot_hostage_counts", prefs.showRobotHostageCounts)
                        put("show_boss_health_bar", prefs.showBossHealthBar)
                        put("headlight_active_default", prefs.headlightActiveDefault)
                        put("original_homing", homingPrefs.enabled)
                    },
                )
            } catch (_: Exception) {
                // Native libs are not always available in every context.
            }
        }
        return json
    }

    private fun importEnginePrefs(
        context: Context,
        json: JSONObject,
    ): String {
        val results = mutableListOf<String>()
        for (game in listOf("d1", "d2")) {
            val obj = json.optJSONObject(game) ?: continue
            if (!obj.has("cockpit_mode") || !obj.has("auto_leveling")) continue
            try {
                val engineCount =
                    NativePilotPreferences.writeEnginePrefs(
                        game,
                        context.filesDir.absolutePath,
                        obj.getInt("cockpit_mode"),
                        obj.getBoolean("auto_leveling"),
                        obj.optBoolean("show_robot_hostage_counts", false),
                        obj.optBoolean("show_boss_health_bar", true),
                        obj.optBoolean("headlight_active_default", false),
                    )
                if (engineCount < 0) {
                    results.add("Engine prefs ($game) import failed: invalid cockpit mode")
                    continue
                }
                val homingCount =
                    if (obj.has("original_homing")) {
                        NativePilotPreferences.writeOriginalHomingPrefs(
                            game,
                            context.filesDir.absolutePath,
                            obj.getBoolean("original_homing"),
                        )
                    } else {
                        0
                    }
                val count = maxOf(engineCount, homingCount)
                results.add(
                    if (count > 0) {
                        "Engine prefs ($game): patched $count file(s)"
                    } else {
                        "Engine prefs ($game): no pilot files found"
                    },
                )
            } catch (e: Exception) {
                results.add("Engine prefs ($game) import failed: ${e.message}")
            }
        }
        return if (results.isNotEmpty()) results.joinToString("\n") else "Engine prefs: no recognized settings"
    }

    private fun exportHostDefaults(context: Context): JSONObject {
        val prefs = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        return JSONObject().apply {
            put("game", prefs.getString("host_game", "d2") ?: "d2")
            put("mode", prefs.getString("host_mode", "coop") ?: "coop")
            put("difficulty", prefs.getInt("host_difficulty", 1))
            put("level_num", prefs.getInt("host_level_num", 1))
            put("max_players", prefs.getInt("host_max_players", 4))
            put("coop_qol", prefs.getBoolean("host_coop_qol", true))
            put(
                "duplicate_energy_shields",
                prefs.getBoolean("host_duplicate_energy_shields", false),
            )
            put("full_death_spew", prefs.getBoolean("host_full_death_spew", true))
            put("player_spew_no_expire", prefs.getBoolean("host_player_spew_no_expire", true))
            put("clients_can_request_rewind", prefs.getBoolean("host_clients_can_request_rewind", false))
            put(
                "mission_d1",
                prefs.getString("host_mission_d1", HostGameDefaults.defaultMissionForGame("d1")) ?: "",
            )
            put(
                "mission_d2",
                prefs.getString("host_mission_d2", HostGameDefaults.defaultMissionForGame("d2")) ?: "d2",
            )
        }
    }

    private fun importHostDefaults(
        context: Context,
        json: JSONObject,
    ): String {
        val editor = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE).edit()
        var count = 0
        if (json.has("game")) {
            editor.putString("host_game", json.getString("game"))
            count++
        }
        if (json.has("mode")) {
            editor.putString("host_mode", json.getString("mode"))
            count++
        }
        if (json.has("difficulty")) {
            editor.putInt("host_difficulty", json.getInt("difficulty"))
            count++
        }
        if (json.has("level_num")) {
            editor.putInt("host_level_num", json.getInt("level_num"))
            count++
        }
        if (json.has("max_players")) {
            editor.putInt("host_max_players", json.getInt("max_players"))
            count++
        }
        if (json.has("coop_qol")) {
            editor.putBoolean("host_coop_qol", json.getBoolean("coop_qol"))
            count++
        }
        if (json.has("duplicate_energy_shields")) {
            editor.putBoolean(
                "host_duplicate_energy_shields",
                json.getBoolean("duplicate_energy_shields"),
            )
            count++
        }
        if (json.has("full_death_spew")) {
            editor.putBoolean("host_full_death_spew", json.getBoolean("full_death_spew"))
            count++
        }
        if (json.has("player_spew_no_expire")) {
            editor.putBoolean("host_player_spew_no_expire", json.getBoolean("player_spew_no_expire"))
            count++
        }
        if (json.has("clients_can_request_rewind")) {
            editor.putBoolean("host_clients_can_request_rewind", json.getBoolean("clients_can_request_rewind"))
            count++
        }
        if (json.has("mission_d1")) {
            editor.putString("host_mission_d1", json.getString("mission_d1"))
            count++
        }
        if (json.has("mission_d2")) {
            editor.putString("host_mission_d2", json.getString("mission_d2"))
            count++
        }
        editor.apply()
        return "Host defaults: imported $count setting(s)"
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    private fun shareJson(
        context: Context,
        json: JSONObject,
        filename: String,
        chooserTitle: String,
    ): Boolean =
        try {
            val uri =
                FileProviderGrantStore.writeUtf8(
                    context,
                    FileProviderGrantStore.CONFIG_EXPORTS,
                    filename,
                    json.toString(2),
                )
            val intent =
                Intent(Intent.ACTION_SEND).apply {
                    type = MIME_JSON
                    putExtra(Intent.EXTRA_STREAM, uri)
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
            val chooser = Intent.createChooser(intent, chooserTitle)
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            context.startActivity(chooser)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Share failed", e)
            false
        }
}
