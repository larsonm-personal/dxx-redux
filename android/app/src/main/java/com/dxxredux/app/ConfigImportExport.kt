package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.core.content.FileProvider
import org.json.JSONObject
import java.io.File

/** Utilities for exporting and importing touch-layout and controller configs. */
object ConfigImportExport {
    private const val TAG = "ConfigImportExport"
    private const val AUTHORITY = "com.dxxredux.app.fileprovider"
    private const val EXPORT_DIR = "config_exports"
    private const val MIME_JSON = "application/json"

    // ── Export ───────────────────────────────────────────────────────────────

    /** Export current touch layout as human-readable JSON via share sheet. */
    fun exportTouchLayout(context: Context): Boolean {
        val layout = TouchLayoutRepository.load(context)
        val json = HumanReadableConfig.touchLayoutToHumanJson(layout)
        return shareJson(context, json, "touch_layout.json", "Share Touch Layout")
    }

    /** Export current controller config as human-readable JSON via share sheet. */
    fun exportControllerConfig(context: Context): Boolean {
        val file = File(context.filesDir, "controller_config.json")
        if (!file.exists()) {
            Log.w(TAG, "No controller config to export")
            return false
        }
        val saved = JSONObject(file.readText())
        val parsed = parseControllerFields(saved)
        val json =
            HumanReadableConfig.controllerConfigToHumanJson(
                parsed.bindings,
                parsed.inverts,
                parsed.thresholds,
            )
        return shareJson(context, json, "controller_config.json", "Share Controller Config")
    }

    /** Export both configs as a combined JSON via share sheet. */
    fun exportAll(context: Context): Boolean {
        val combined = JSONObject()
        combined.put("type", "combined_config")
        combined.put("version", 1)

        val layout = TouchLayoutRepository.load(context)
        combined.put("touch_layout", HumanReadableConfig.touchLayoutToHumanJson(layout))

        val ctrlFile = File(context.filesDir, "controller_config.json")
        if (ctrlFile.exists()) {
            val saved = JSONObject(ctrlFile.readText())
            val parsed = parseControllerFields(saved)
            combined.put(
                "controller_config",
                HumanReadableConfig.controllerConfigToHumanJson(
                    parsed.bindings,
                    parsed.inverts,
                    parsed.thresholds,
                ),
            )
        }

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

        return shareJson(context, combined, "dxx_redux_config.json", "Share Config")
    }

    // ── Import ──────────────────────────────────────────────────────────────

    /**
     * Import a config from a content:// URI. Returns a human-readable summary
     * of what was imported, or an error message.
     */
    fun importFromUri(
        context: Context,
        uri: Uri,
    ): String {
        val text =
            try {
                context.contentResolver
                    .openInputStream(uri)
                    ?.bufferedReader()
                    ?.use { it.readText() }
                    ?: return "Error: could not open file"
            } catch (e: Exception) {
                Log.e(TAG, "Failed to read import file", e)
                return "Error: ${e.message}"
            }
        return importFromText(context, text)
    }

    /** Import a config from raw JSON text. Returns a summary or error message. */
    fun importFromText(
        context: Context,
        text: String,
    ): String {
        val json =
            try {
                JSONObject(text)
            } catch (e: Exception) {
                return "Error: invalid JSON - ${e.message}"
            }

        val type = HumanReadableConfig.detectConfigType(json)
        return when (type) {
            "touch_layout" -> importTouchLayout(context, json)
            "controller_config" -> importControllerConfig(context, json)
            "combined_config" -> importCombined(context, json)
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
        TouchLayoutRepository.save(context, result.value)
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
        // Write as a simple bindings+inverts+thresholds JSON that loadConfig() can read
        val outJson = JSONObject()
        outJson.put("version", 1)
        val bindingsObj = JSONObject()
        for ((k, v) in result.value.bindings) bindingsObj.put(k, v)
        outJson.put("bindings", bindingsObj)
        val invertsArr = org.json.JSONArray()
        for (inv in result.value.inverts) invertsArr.put(inv)
        outJson.put("inverts", invertsArr)
        if (result.value.thresholds.isNotEmpty()) {
            val tObj = JSONObject()
            for ((k, v) in result.value.thresholds) tObj.put(k, v)
            outJson.put("thresholds", tObj)
        }
        File(context.filesDir, "controller_config.json").writeText(outJson.toString(2))
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
        if (json.has("touch_layout")) {
            results.add(importTouchLayout(context, json.getJSONObject("touch_layout")))
        }
        if (json.has("controller_config")) {
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
                    val primLen = primEntries.size / 2
                    val secLen = arr.length() - primLen
                    if (secLen > 0) {
                        val prim = IntArray(primLen) { arr.getInt(it) }
                        val sec = IntArray(secLen) { arr.getInt(primLen + it) }
                        val count = NativeAutoselectPatcher.writeAutoselect(game, filesDir, prim, sec)
                        results.add("Autoselect ($game): patched $count file(s)")
                    }
                } catch (e: Exception) {
                    results.add("Autoselect ($game) import failed: ${e.message}")
                }
            }
        }
        if (results.isEmpty()) return "Combined config had no recognizable sections."
        return results.joinToString("\n")
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    private data class ControllerFields(
        val bindings: Map<String, String>,
        val inverts: Set<String>,
        val thresholds: Map<String, Int>,
    )

    private fun parseControllerFields(saved: JSONObject): ControllerFields {
        val bindings = mutableMapOf<String, String>()
        val inverts = mutableSetOf<String>()
        val thresholds = mutableMapOf<String, Int>()
        if (saved.has("bindings")) {
            val obj = saved.getJSONObject("bindings")
            for (key in obj.keys()) bindings[key] = obj.getString(key)
        }
        if (saved.has("inverts")) {
            val arr = saved.getJSONArray("inverts")
            for (i in 0 until arr.length()) inverts.add(arr.getString(i))
        }
        val tObj = saved.optJSONObject("thresholds")
        if (tObj != null) {
            for (key in tObj.keys()) thresholds[key] = tObj.getInt(key)
        }
        return ControllerFields(bindings, inverts, thresholds)
    }

    private fun shareJson(
        context: Context,
        json: JSONObject,
        filename: String,
        chooserTitle: String,
    ): Boolean =
        try {
            val exportDir = File(context.cacheDir, EXPORT_DIR)
            exportDir.mkdirs()
            val file = File(exportDir, filename)
            file.writeText(json.toString(2))

            val uri = FileProvider.getUriForFile(context, AUTHORITY, file)
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
