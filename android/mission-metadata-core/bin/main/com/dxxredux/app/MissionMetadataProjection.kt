package com.dxxredux.app

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put

data class MissionProjectionOverrides(
    val sourceName: String = "",
    val missionFilename: String = "",
    val missionPath: String = "",
    val targetIndex: Int? = null,
)

object MissionMetadataProjection {
    fun project(
        rawText: String,
        overrides: MissionProjectionOverrides = MissionProjectionOverrides(),
    ): String = Json.encodeToString(project(Json.parseToJsonElement(rawText).jsonObject, overrides))

    fun project(
        raw: JsonObject,
        overrides: MissionProjectionOverrides = MissionProjectionOverrides(),
    ): JsonObject {
        val levels = raw.array("levels").map { projectLevel(it.jsonObject) }
        return buildJsonObject {
            copy(raw, "status", "ok")
            put("source", overrides.sourceName.ifBlank { raw.string("source").ifBlank { raw.string("mission_name") } })
            copy(raw, "game", "")
            copy(raw, "mission_name", "")
            put("mission_filename", overrides.missionFilename.ifBlank { raw.string("mission_filename") })
            raw["mission_intent"]?.takeUnless { it is JsonNull }?.let { put("mission_intent", it) }
            putIfNonBlank("coop_starts", raw.string("coop_starts"))
            val tracks = raw.array("music_tracks")
            if (tracks.isNotEmpty()) {
                put(
                    "track_names",
                    buildJsonArray { tracks.forEach { add(projectTrack(it.jsonObject)) } },
                )
            }
            put("level_count", levels.size)
            put("levels", JsonArray(levels))
            putIfNonEmpty("problems", raw.array("problems"))
            putIfNonEmpty("diagnostics", raw.array("diagnostics"))
            putIfNonBlank("mission_path", overrides.missionPath)
            overrides.targetIndex?.let { put("target_index", it) }
        }
    }

    private fun projectTrack(track: JsonObject) =
        buildJsonObject {
            copy(track, "slot_index", 0, "track")
            val name = track.string("resolved_name")
            require(name.isNotBlank()) { "Native metadata track ${track.int("slot_index")} has no resolved_name" }
            put("name", name)
            copy(track, "filename", "")
            copy(track, "format", "")
            val duration = track.int("duration_ms")
            put("length_s", if (duration > 0) (duration + 500) / 1000 else 0)
            val parseStatus = track.string("parse_status")
            if (parseStatus !in setOf("ok", "no_tags")) {
                put("parse_status", parseStatus)
            } else if (duration <= 0) {
                put("parse_status", "duration_unavailable")
            }
        }

    private fun projectLevel(level: JsonObject) =
        buildJsonObject {
            val names =
                listOf(
                    "level_num",
                    "secret",
                    "level_name",
                    "level_file",
                    "segment_count",
                    "wall_count",
                    "trigger_count",
                    "object_count",
                    "texture_count",
                    "player_starts",
                    "coop_only_starts",
                    "powerups",
                    "reactors",
                    "robots",
                    "hostages",
                    "secrets",
                    "matcens",
                    "energy_centers",
                    "mine_volume",
                    "mine_volume_normalized",
                    "mine_volume_text",
                    "travel_distance",
                    "travel_time_seconds",
                    "travel_time_text",
                    "guidebot_count",
                    "guidebot_placed",
                    "guidebot_accessible",
                    "route_status",
                )
            names.forEach { name -> level[name]?.let { put(name, it) } }
            put("route_required_key_mask", level.requiredInt("route_required_key_mask"))
            put("route_completing_key_mask_set", level.requiredInt("route_completing_key_mask_set"))
            put("route_steps", level["route_steps"] ?: JsonArray(emptyList()))
            listOf("guidebot_placement_note", "guidebot_note", "route_problem", "route_note").forEach { name ->
                putIfNonBlank(name, level.string(name))
            }
            putIfNonEmpty("problems", level.array("problems"))
            val notes = linkedSetOf<String>()
            level.array("notes").mapNotNullTo(notes) { it.jsonPrimitive.contentOrNull?.takeIf(String::isNotBlank) }
            listOf("route_note", "guidebot_placement_note", "guidebot_note").mapTo(notes) { level.string(it) }
            notes.remove("")
            if (notes.isNotEmpty()) put("notes", buildJsonArray { notes.forEach { add(JsonPrimitive(it)) } })
            val status = level.string("status", "ok")
            if (status != "ok") put("status", status)
        }

    private fun JsonObject.string(
        name: String,
        default: String = "",
    ) = this[name]?.jsonPrimitive?.contentOrNull ?: default

    private fun JsonObject.int(
        name: String,
        default: Int = 0,
    ) = this[name]?.jsonPrimitive?.intOrNull ?: default

    private fun JsonObject.requiredInt(name: String) =
        requireNotNull(this[name]?.jsonPrimitive?.intOrNull) {
            "Native metadata level is missing required integer field $name"
        }

    private fun JsonObject.array(name: String) =
        this[name]?.let { runCatching { it.jsonArray }.getOrNull() } ?: JsonArray(emptyList())

    private fun kotlinx.serialization.json.JsonObjectBuilder.copy(
        source: JsonObject,
        name: String,
        default: String,
        output: String = name,
    ) = put(output, source[name] ?: JsonPrimitive(default))

    private fun kotlinx.serialization.json.JsonObjectBuilder.copy(
        source: JsonObject,
        name: String,
        default: Int,
        output: String = name,
    ) = put(output, source[name] ?: JsonPrimitive(default))

    private fun kotlinx.serialization.json.JsonObjectBuilder.putIfNonBlank(
        name: String,
        value: String,
    ) {
        if (value.isNotBlank()) put(name, value)
    }

    private fun kotlinx.serialization.json.JsonObjectBuilder.putIfNonEmpty(
        name: String,
        value: JsonArray,
    ) {
        if (value.isNotEmpty()) put(name, value)
    }
}
