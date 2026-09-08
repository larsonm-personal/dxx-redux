package com.dxxredux.metadata.cli

import com.dxxredux.app.MISSION_VARIANT_MASK_PRECEDENCE
import com.dxxredux.app.MissionDescriptorPolicy
import com.dxxredux.app.MissionMetadataProjection
import com.dxxredux.app.MissionProjectionOverrides
import com.dxxredux.app.missionVariantForArchiveFilename
import com.dxxredux.app.selectPreferredMissionVariant
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import java.io.File

fun main(args: Array<String>) {
    if (args.firstOrNull() == "--select-archive-variant") {
        val selected = selectPreferredMissionVariant(args.drop(1), ::missionVariantForArchiveFilename)
        if (selected?.preference?.supportedByRedux == true) println("DXXVARIANT\t${selected.value}")
        return
    }
    if (args.singleOrNull() == "--capabilities") {
        println(
            "mission-metadata-cli protocol=1 variants=" +
                MISSION_VARIANT_MASK_PRECEDENCE.joinToString(",") { it.id },
        )
        return
    }
    if (args.singleOrNull() == "--directory-precedence") {
        MISSION_VARIANT_MASK_PRECEDENCE.flatMap { it.directoryNames }.forEach(::println)
        return
    }
    if (args.singleOrNull() == "--server") {
        generateSequence(::readlnOrNull).forEach { line ->
            val request = Json.parseToJsonElement(line).jsonObject
            val op = request["op"]?.jsonPrimitive?.contentOrNull
            val response =
                when (op) {
                    "descriptor" -> {
                        descriptorJson(File(request.getValue("path").jsonPrimitive.content))
                    }

                    "project" -> {
                        MissionMetadataProjection.project(
                            Json
                                .parseToJsonElement(
                                    File(request.getValue("raw_path").jsonPrimitive.content).readText(),
                                ).jsonObject,
                            MissionProjectionOverrides(
                                sourceName = request["source_name"]?.jsonPrimitive?.contentOrNull.orEmpty(),
                                missionFilename = request["mission_filename"]?.jsonPrimitive?.contentOrNull.orEmpty(),
                                missionPath = request["mission_path"]?.jsonPrimitive?.contentOrNull.orEmpty(),
                                targetIndex = request["target_index"]?.jsonPrimitive?.contentOrNull?.toIntOrNull(),
                            ),
                        )
                    }

                    else -> {
                        buildJsonObject { put("error", "unknown operation") }
                    }
                }
            println("DXXKOTLIN\t${Json.encodeToString(response)}")
            System.out.flush()
        }
        return
    }
    error(
        "usage: mission-metadata-cli --capabilities|--directory-precedence|--select-archive-variant [names...]|--server",
    )
}

private fun descriptorJson(file: File) =
    MissionDescriptorPolicy.parse(file.name, MissionDescriptorPolicy.decode(file.readBytes())).let { descriptor ->
        buildJsonObject {
            put("display_name", descriptor.displayName)
            put("filename", file.name)
            put("type", descriptor.type ?: "normal")
            put("game", descriptor.game)
            put("valid", descriptor.valid)
            descriptor.problem?.let { put("problem", it) }
            put("mode_flags", buildJsonArray { descriptor.modeFlags.forEach { add(JsonPrimitive(it)) } })
            put("normal_level_files", buildJsonArray { descriptor.levelNames.forEach { add(JsonPrimitive(it)) } })
            put("secret_level_files", buildJsonArray { descriptor.secretLevelNames.forEach { add(JsonPrimitive(it)) } })
        }
    }
