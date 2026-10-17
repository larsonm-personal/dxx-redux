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
            raw["provenance"]?.takeUnless { it is JsonNull }?.let { put("provenance", it) }
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
                    "flyout",
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
            level["matcen_regression"]?.let {
                val block = validateMatcenRegression(it)
                if (block.string("admission") != "loaded" || block.string("capture_status") != "complete" ||
                    block.int("declared_disk_count") != 0 ||
                    listOf("bindings", "disk_records", "triggers", "robot_models").any { name -> block.array(name).isNotEmpty() }
                ) {
                    put("matcen_regression", block)
                }
            }
            put("route_required_key_mask", level.requiredInt("route_required_key_mask"))
            put("route_completing_key_mask_set", level.requiredInt("route_completing_key_mask_set"))
            put("route_steps", level["route_steps"] ?: JsonArray(emptyList()))
            put("data_warnings", level["data_warnings"] ?: JsonArray(emptyList()))
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

    // The block is optional, but captured data must use the current native schema.
    private fun validateMatcenRegression(value: JsonElement): JsonObject {
        val block = requireNotNull(value as? JsonObject) { "matcen_regression must be an object" }

        fun requiredInteger(name: String): Int {
            val primitive = block[name] as? JsonPrimitive
            return requireNotNull(primitive?.takeUnless { it.isString }?.intOrNull) {
                "matcen_regression.$name must be an integer"
            }
        }
        require(requiredInteger("schema_version") == 2) { "Unsupported matcen_regression.schema_version" }
        val status = block["capture_status"] as? JsonPrimitive
        require(status?.isString == true && status.content in setOf("complete", "incomplete")) {
            "matcen_regression.capture_status must be complete or incomplete"
        }
        val admission = block["admission"] as? JsonPrimitive
        require(admission?.isString == true && admission.content in setOf("loaded", "rejected_or_missing")) {
            "matcen_regression.admission must be loaded or rejected_or_missing"
        }
        requiredInteger("declared_disk_count")
        for (name in listOf("engine_limits", "runtime_counts")) {
            require(block[name] is JsonObject) { "matcen_regression.$name must be an object" }
        }
        for (name in listOf("bindings", "disk_records", "triggers", "robot_models")) {
            val entries = requireNotNull(block[name] as? JsonArray) { "matcen_regression.$name must be an array" }
            require(entries.all { it is JsonObject }) { "matcen_regression.$name entries must be objects" }
        }
        return block
    }

    // Keep the caller's existing pretty printing outside this block byte-for-byte.
    // Each collection entry is a semantic row; nested masks, links and overrides stay inline.
    fun formatMatcenRows(prettyText: String): String {
        val startPattern = Regex("""(?m)^([ ]*)"matcen_regression": \{""")
        val output = StringBuilder()
        var offset = 0
        for (match in startPattern.findAll(prettyText)) {
            val indent = match.groupValues[1]
            val start = match.range.last
            val end = prettyText.indexOf("\n$indent}", start)
            require(end >= 0) { "Unclosed pretty-printed matcen_regression block" }
            val close = end + indent.length + 2
            val block = Json.parseToJsonElement(prettyText.substring(start, close)).jsonObject
            output.append(prettyText, offset, start).append("{\n")
            block.entries.forEachIndexed { index, (name, value) ->
                output.append(indent).append("  ").append(JsonPrimitive(name)).append(": ")
                if (value is JsonArray && value.isNotEmpty()) {
                    output.append("[\n")
                    value.forEachIndexed { rowIndex, row ->
                        output.append(indent).append("    ").append(inlineJson(row))
                        if (rowIndex < value.lastIndex) output.append(',')
                        output.append('\n')
                    }
                    output.append(indent).append("  ]")
                } else {
                    output.append(inlineJson(value))
                }
                if (index < block.size - 1) output.append(',')
                output.append('\n')
            }
            output.append(indent).append('}')
            offset = close
        }
        return output.append(prettyText, offset, prettyText.length).toString()
    }

    private fun inlineJson(value: JsonElement): String =
        when (value) {
            is JsonObject -> value.entries.joinToString(", ", "{", "}") { (name, child) ->
                "${JsonPrimitive(name)}: ${inlineJson(child)}"
            }
            is JsonArray -> value.joinToString(", ", "[", "]", transform = ::inlineJson)
            else -> value.toString()
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
