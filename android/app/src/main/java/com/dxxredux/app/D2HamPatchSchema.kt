package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject

internal object D2HamPatchSchema {
    const val PATH = "patches/d2/ham_patch.rfc6902.json"

    private val rowLimits = mapOf("textures" to 1200, "vclips" to 110, "eclips" to 110, "wclips" to 60)
    private val fieldLimits =
        mapOf(
            "sounds" to 254,
            "eclips" to 110,
            "wclips" to 60,
            "robots" to 85,
            "weapons" to 70,
            "objBitmaps" to 610,
        )
    private val fixedRobotFields =
        setOf(
            "Exp1Sound",
            "Exp1Vclip",
            "Exp2Sound",
            "Exp2Vclip",
            "WeaponType",
            "WeaponType2",
            "NGuns",
            "ContainsId",
            "ContainsCount",
            "ContainsProb",
            "ContainsType",
            "Kamikaze",
            "ScoreValue",
            "Badass",
            "EnergyDrain",
            "Lighting",
            "Strength",
            "Mass",
            "Drag",
            "CloakType",
            "AttackType",
            "SeeSound",
            "AttackSound",
            "ClawSound",
            "TauntSound",
            "BossFlag",
            "Companion",
            "SmartBlobs",
            "EnergyBlobs",
            "Thief",
            "Pursuit",
            "Lightcast",
            "DeathRoll",
            "Flags",
            "DeathrollSound",
            "Glow",
            "Behavior",
            "Aim",
            "Always0xabcd",
        )

    fun validate(operation: JSONObject) {
        val op = operation.optString("op")
        require(op == "test" || op == "add" || op == "replace") { "unsupported JSON Patch operation '$op'" }
        require(operation.has("path") && operation.opt("path") is String) { "operation is missing path" }
        require(
            operation.has("value") && operation.opt("value") !== JSONObject.NULL,
        ) { "$op operation is missing value" }

        val path = operation.getString("path")
        require('~' !in path) { "escaped JSON Pointer paths are not supported by the native HAM parser" }
        val segments = path.split('/')
        require(segments.size in 4..5 && segments[0].isEmpty() && segments[1] == "sections") {
            "unsupported patch path $path"
        }
        val section = segments[2]
        val indexText = segments[3]
        require(indexText.isNotEmpty() && indexText.all { it in '0'..'9' }) { "invalid patch index in $path" }
        val index = indexText.toIntOrNull() ?: throw IllegalArgumentException("invalid patch index in $path")
        val field = segments.getOrNull(4)
        require(field == null || field.isNotEmpty()) { "invalid patch field in $path" }

        val value = operation.get("value")
        if (field == null) {
            val limit = rowLimits[section]
            require(limit != null && index < limit) { "unsupported or out-of-range HAM row $path" }
            require(value is JSONObject) { "HAM row value must be an object at $path" }
            require(value.optInt("Index", -1) == index) { "HAM row Index must match $path" }
            validateRow(section, value, path)
        } else {
            val limit = fieldLimits[section]
            require(limit != null && index < limit) { "unsupported or out-of-range HAM field $path" }
            require(isSupportedField(section, field)) { "unsupported HAM patch field $path" }
            require(isInteger(value)) { "HAM field value must be an integer at $path" }
            validateSimpleRange(section, field, (value as Number).toLong(), path)
        }
    }

    private fun isInteger(value: Any): Boolean {
        if (value !is Number) return false
        val number = value.toDouble()
        return number.isFinite() && number == kotlin.math.floor(number) &&
            number in Long.MIN_VALUE.toDouble()..Long.MAX_VALUE.toDouble()
    }

    private fun validateRow(
        section: String,
        value: JSONObject,
        path: String,
    ) {
        val integerFields =
            when (section) {
                "textures" -> {
                    listOf(
                        "Index",
                        "Bitmap",
                        "Flags",
                        "Pad0",
                        "Pad1",
                        "Pad2",
                        "Lighting",
                        "Damage",
                        "Eclip",
                        "Destroyed",
                        "SlideU",
                        "SlideV",
                    )
                }

                "vclips" -> {
                    listOf("Index", "PlayTime", "NumFrames", "FrameTime", "Flags", "Sound", "Light")
                }

                "eclips" -> {
                    listOf(
                        "Index",
                        "PlayTime",
                        "NumFrames",
                        "FrameTime",
                        "VclipFlags",
                        "VclipSound",
                        "Light",
                        "TimeLeft",
                        "FrameCount",
                        "ChangingWall",
                        "ChangingObject",
                        "Flags",
                        "CritClip",
                        "DestBm",
                        "DestVclip",
                        "DestEclip",
                        "DestSize",
                        "Sound",
                        "Seg",
                        "Side",
                    )
                }

                "wclips" -> {
                    listOf("Index", "PlayTime", "NumFrames", "OpenSound", "CloseSound", "Flags", "Pad")
                }

                else -> {
                    emptyList()
                }
            }
        require(integerFields.all { value.has(it) && isInteger(value.get(it)) }) {
            "HAM row has missing or non-integer fields at $path"
        }
        if (section != "textures") {
            require(value.opt("Frames") is JSONArray) { "HAM row Frames must be an array at $path" }
        }
        if (section == "wclips") {
            require(value.opt("Filename") is String) { "HAM row Filename must be a string at $path" }
        }
    }

    private fun isSupportedField(
        section: String,
        field: String,
    ): Boolean =
        when (section) {
            "sounds" -> field == "Sound" || field == "AltSound"
            "eclips" -> field == "FrameTime" || field == "Sound"
            "wclips" -> field == "PlayTime" || field == "OpenSound" || field == "CloseSound"
            "weapons" -> field == "FlashSound" || field == "RobotHitSound" || field == "WallHitSound"
            "objBitmaps" -> field == "Bitmap" || field == "Pointer"
            "robots" -> field in fixedRobotFields || isIndexedRobotField(field)
            else -> false
        }

    private fun isIndexedRobotField(field: String): Boolean {
        if (Regex("GunPoint[0-7][XYZ]").matches(field)) return true
        if (Regex("AnimState[0-8]_[0-4](Joints|Offset)").matches(field)) return true
        if (Regex("GunSubmodel[0-7]").matches(field)) return true
        return Regex(
            "(FieldOfView|FiringWait2|FiringWait|TurnTime|MaxSpeed|CircleDistance|RapidfireCount|EvadeSpeed)[0-4]",
        ).matches(field)
    }

    private fun validateSimpleRange(
        section: String,
        field: String,
        value: Long,
        path: String,
    ) {
        val range =
            when (section to field) {
                "sounds" to "Sound", "sounds" to "AltSound" -> 0L..255L

                "eclips" to "FrameTime", "wclips" to "PlayTime" -> 1L..0x40000000L

                "eclips" to "Sound", "wclips" to "OpenSound", "wclips" to "CloseSound",
                "weapons" to "FlashSound", "weapons" to "RobotHitSound", "weapons" to "WallHitSound",
                -> -1L..253L

                "objBitmaps" to "Bitmap" -> 0L..2619L

                "objBitmaps" to "Pointer" -> 0L..609L

                else -> -0x40000000L..0x40000000L
            }
        require(value in range) { "HAM field value is out of range at $path" }
    }
}
