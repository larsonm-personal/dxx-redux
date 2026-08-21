package com.dxxredux.app

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive
import org.json.JSONArray
import org.json.JSONObject

object VisualReplacementPolicy {
    const val STOCK_VISUALS_ENFORCED = "stock_visuals_enforced"
    const val OMITTED_VISUAL_MOD_COUNT = "omitted_visual_mod_count"
    const val OMITTED_VISUAL_TEXTURE_COUNT = "omitted_visual_texture_count"
    const val OMITTED_VISUAL_MODS = "omitted_visual_mods"

    suspend fun summaryForPvp(
        context: Context,
        game: String,
        mode: String,
    ): ModManager.VisualReplacementSummary =
        withContext(Dispatchers.IO) {
            if (mode == "coop") {
                ModManager.VisualReplacementSummary()
            } else {
                ModManager.forActiveSet(context.filesDir, context).enabledVisualReplacementSummary(game)
            }
        }

    fun gameInfoFields(summary: ModManager.VisualReplacementSummary): Map<String, JsonElement> =
        if (!summary.hasOmittedVisuals) {
            emptyMap()
        } else {
            mapOf(
                STOCK_VISUALS_ENFORCED to JsonPrimitive(true),
                OMITTED_VISUAL_MOD_COUNT to JsonPrimitive(summary.omittedModCount),
                OMITTED_VISUAL_TEXTURE_COUNT to JsonPrimitive(summary.omittedTextureCount),
                OMITTED_VISUAL_MODS to JsonArray(summary.omittedModNames.map { JsonPrimitive(it) }),
            )
        }

    fun putJsonFields(
        json: JSONObject,
        summary: ModManager.VisualReplacementSummary,
    ) {
        putJsonFields(
            json,
            stockVisualsEnforced = summary.hasOmittedVisuals,
            omittedVisualModCount = summary.omittedModCount,
            omittedVisualTextureCount = summary.omittedTextureCount,
            omittedVisualModNames = summary.omittedModNames,
        )
    }

    fun putJsonFields(
        json: JSONObject,
        stockVisualsEnforced: Boolean,
        omittedVisualModCount: Int,
        omittedVisualTextureCount: Int,
        omittedVisualModNames: List<String>,
    ) {
        if (!stockVisualsEnforced || omittedVisualModCount <= 0) return
        json.put(STOCK_VISUALS_ENFORCED, true)
        json.put(OMITTED_VISUAL_MOD_COUNT, omittedVisualModCount)
        json.put(OMITTED_VISUAL_TEXTURE_COUNT, omittedVisualTextureCount)
        val names = JSONArray()
        for (name in omittedVisualModNames) names.put(name)
        json.put(OMITTED_VISUAL_MODS, names)
    }

    fun noticeText(summary: ModManager.VisualReplacementSummary): String? =
        noticeText(
            stockVisualsEnforced = summary.hasOmittedVisuals,
            omittedVisualModCount = summary.omittedModCount,
            omittedVisualModNames = summary.omittedModNames,
        )

    fun noticeText(gameInfo: JsonObject): String? {
        val names =
            (gameInfo[OMITTED_VISUAL_MODS] as? JsonArray)
                ?.mapNotNull { it.jsonPrimitive.contentOrNull }
                ?: emptyList()
        return noticeText(
            stockVisualsEnforced = gameInfo[STOCK_VISUALS_ENFORCED]?.jsonPrimitive?.booleanOrNull ?: false,
            omittedVisualModCount = gameInfo[OMITTED_VISUAL_MOD_COUNT]?.jsonPrimitive?.intOrNull ?: 0,
            omittedVisualModNames = names,
        )
    }

    fun noticeText(
        stockVisualsEnforced: Boolean,
        omittedVisualModCount: Int,
        omittedVisualModNames: List<String>,
    ): String? {
        if (!stockVisualsEnforced || omittedVisualModCount <= 0) return null
        val packWord = if (omittedVisualModCount == 1) "pack" else "packs"
        val suffix =
            if (omittedVisualModNames.isEmpty()) {
                ""
            } else {
                val more = if (omittedVisualModCount > omittedVisualModNames.size) ", ..." else ""
                ": ${omittedVisualModNames.joinToString(", ")}$more"
            }
        return "PVP uses stock visual textures and models; " +
            "$omittedVisualModCount enabled visual $packWord will be ignored$suffix"
    }

    fun namesFromJson(json: JSONObject): List<String> {
        val names = json.optJSONArray(OMITTED_VISUAL_MODS) ?: return emptyList()
        return buildList {
            for (i in 0 until names.length()) {
                val name = names.optString(i)
                if (name.isNotBlank()) add(name)
            }
        }
    }
}
