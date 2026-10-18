package com.dxxredux.app

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertThrows
import org.junit.Test

class MissionMetadataProjectionTest {
    @Test
    fun preservesUncappedFlyoutTimingAndMissingReasons() {
        for (flyout in listOf(
            """{"kind":"in_engine","seconds":123.456,"status":"estimated"}""",
            """{"kind":"none","seconds":null,"status":"no_normal_exit_trigger"}""",
        )) {
            val level =
                MissionMetadataProjection
                    .project(
                        Json
                            .parseToJsonElement(
                                rawLevel(
                                    "\"route_required_key_mask\":0,\"route_completing_key_mask_set\":1,\"flyout\":$flyout",
                                ),
                            ).jsonObject,
                    )["levels"]!!
                    .jsonArray
                    .single()
                    .jsonObject
            assertEquals(Json.parseToJsonElement(flyout), level["flyout"])
        }
    }

    @Test
    fun preservesRouteKeyMasks() {
        val projected =
            Json
                .parseToJsonElement(
                    MissionMetadataProjection.project(
                        rawLevel("\"route_required_key_mask\":3,\"route_completing_key_mask_set\":8"),
                    ),
                ).jsonObject
        val level = projected["levels"]!!.jsonArray.single().jsonObject
        assertEquals(3, level["route_required_key_mask"]!!.jsonPrimitive.int)
        assertEquals(8, level["route_completing_key_mask_set"]!!.jsonPrimitive.int)
    }

    @Test
    fun rejectsMissingRouteKeyMasks() {
        val error =
            assertThrows(IllegalArgumentException::class.java) {
                MissionMetadataProjection.project(rawLevel("\"route_required_key_mask\":3"))
            }
        assertEquals(
            "Native metadata level is missing required integer field route_completing_key_mask_set",
            error.message,
        )
    }

    @Test
    fun omitsMatcenRegressionWhenNotCaptured() {
        val level = projectMatcenLevel()
        assertFalse(level.containsKey("matcen_regression"))
    }

    @Test
    fun preservesVersionedMatcenRegressionExactly() {
        for (status in listOf("complete", "incomplete")) {
            val block = validMatcenRegression.replace("\"complete\"", "\"$status\"")
            assertEquals(Json.parseToJsonElement(block), projectMatcenLevel(block)["matcen_regression"])
        }
    }

    @Test
    fun rejectsNonObjectMatcenRegression() {
        for (block in listOf("null", "[]", "1", "true", "\"capture\"")) {
            assertThrows("block=$block", IllegalArgumentException::class.java) { projectMatcenLevel(block) }
        }
    }

    @Test
    fun rejectsMissingMatcenRegressionFields() {
        val valid = Json.parseToJsonElement(validMatcenRegression).jsonObject
        for (name in listOf(
            "schema_version",
            "capture_status",
            "admission",
            "declared_disk_count",
            "engine_limits",
            "runtime_counts",
            "bindings",
            "disk_records",
            "triggers",
            "robot_models",
        )) {
            val block = JsonObject(valid.filterKeys { it != name }).toString()
            assertThrows("missing $name", IllegalArgumentException::class.java) { projectMatcenLevel(block) }
        }
    }

    @Test
    fun rejectsInvalidMatcenRegressionScalars() {
        for (name in listOf("schema_version", "declared_disk_count")) {
            for (value in listOf("null", "true", "\"1\"", "1.5", "[]", "{}", "2147483648")) {
                assertInvalidMatcenField(name, value)
            }
        }
        for (value in listOf("0", "1", "-1")) assertInvalidMatcenField("schema_version", value)
        for (value in listOf("null", "true", "1", "[]", "{}", "\"unknown\"")) {
            assertInvalidMatcenField("capture_status", value)
        }
    }

    @Test
    fun rejectsInvalidMatcenRegressionCollections() {
        for (name in listOf("bindings", "disk_records", "triggers", "robot_models")) {
            for (value in listOf("null", "true", "1", "\"entries\"", "{}", "[null]", "[1]", "[[]]", "[{},false]")) {
                assertInvalidMatcenField(name, value)
            }
        }
    }

    @Test
    fun rejectsInvalidMatcenAdmissionAndCountObjects() {
        for (value in listOf("null", "true", "1", "[]", "{}", "\"unknown\"")) {
            assertInvalidMatcenField("admission", value)
        }
        for (name in listOf("engine_limits", "runtime_counts")) {
            for (value in listOf("null", "true", "1", "[]", "\"counts\"")) {
                assertInvalidMatcenField(name, value)
            }
        }
    }

    @Test
    fun omitsOnlyCompleteLoadedEmptyMatcenBlocks() {
        val valid = Json.parseToJsonElement(validMatcenRegression).jsonObject.toMutableMap()
        for (name in listOf("bindings", "disk_records", "triggers", "robot_models")) {
            valid[name] = Json.parseToJsonElement("[]")
        }
        valid["declared_disk_count"] = Json.parseToJsonElement("0")
        assertFalse(projectMatcenLevel(JsonObject(valid).toString()).containsKey("matcen_regression"))
        for ((name, value) in listOf("capture_status" to "incomplete", "admission" to "rejected_or_missing")) {
            val retained = JsonObject(valid + (name to kotlinx.serialization.json.JsonPrimitive(value)))
            assertEquals(retained, projectMatcenLevel(retained.toString())["matcen_regression"])
        }
        for (name in listOf("bindings", "disk_records", "triggers", "robot_models")) {
            val retained = JsonObject(valid + (name to Json.parseToJsonElement("[{}]")))
            assertEquals(retained, projectMatcenLevel(retained.toString())["matcen_regression"])
        }
        for (count in listOf(-1, 1)) {
            val retained = JsonObject(valid + ("declared_disk_count" to kotlinx.serialization.json.JsonPrimitive(count)))
            assertEquals(retained, projectMatcenLevel(retained.toString())["matcen_regression"])
        }
    }

    @Test
    fun matcenRowsRoundTripAndRegenerateCanonically() {
        val pretty = Json { prettyPrint = true }
        val source =
            Json.parseToJsonElement(
                """[{"outside":{"preserved":[1,2]},"levels":[{"matcen_regression":$validMatcenRegression},
                    {"matcen_regression":$validMatcenRegression}],"trailing":"escaped \" text\nnext"}]""",
            )
        val original = pretty.encodeToString(source)
        val formatted = MissionMetadataProjection.formatMatcenRows(original)
        assertEquals(source, Json.parseToJsonElement(formatted))
        assertEquals(formatted, MissionMetadataProjection.formatMatcenRows(formatted))
        assertEquals(
            formatted,
            MissionMetadataProjection.formatMatcenRows(pretty.encodeToString(Json.parseToJsonElement(formatted))),
        )
        assertTrue(formatted.contains("{\"segment\": 9, \"robot_flags\": [4294967295, 0]}"))
        assertTrue(formatted.contains("{\"links\": [{\"segment\": 9, \"side\": 3}]}"))
        val firstBlock = original.indexOf("\"matcen_regression\"")
        assertEquals(original.substring(0, firstBlock), formatted.substring(0, firstBlock))
        val tail = original.substring(original.lastIndexOf("\"trailing\""))
        assertTrue(formatted.endsWith(tail))
    }

    @Test
    fun leavesUnrelatedMetadataFormattingUnchanged() {
        val text = """{"levels": [], "unrelated": {"keep": [1, 2]}}""" + "\n"
        assertEquals(text, MissionMetadataProjection.formatMatcenRows(text))
    }

    private fun assertInvalidMatcenField(
        name: String,
        value: String,
    ) {
        val valid = Json.parseToJsonElement(validMatcenRegression).jsonObject.toMutableMap()
        valid[name] = Json.parseToJsonElement(value)
        assertThrows("$name=$value", IllegalArgumentException::class.java) {
            projectMatcenLevel(JsonObject(valid).toString())
        }
    }

    private fun projectMatcenLevel(block: String? = null): JsonObject {
        val optional = block?.let { ",\"matcen_regression\":$it" }.orEmpty()
        return MissionMetadataProjection
            .project(
                Json
                    .parseToJsonElement(
                        rawLevel("\"route_required_key_mask\":0,\"route_completing_key_mask_set\":1$optional"),
                    ).jsonObject,
            )["levels"]!!
            .jsonArray
            .single()
            .jsonObject
    }

    private val validMatcenRegression =
        """{
            "schema_version":2,
            "admission":"loaded",
            "capture_status":"complete",
            "declared_disk_count":1,
            "engine_limits":{"robot_centers":100},
            "runtime_counts":{"robot_centers":2},
            "bindings":[{"segment":9,"effective_ordinal":0,"record_overrides":{"station":1},"station_overrides":{"type":3}}],
            "disk_records":[{"segment":9,"robot_flags":[4294967295,0]}],
            "triggers":[{"links":[{"segment":9,"side":3}]}],
            "robot_models":[{"type":0,"model":3,"status":"valid_model"}],
            "future_additive_field":{"preserved":true}
        }"""

    @Test
    fun preservesLevelDataWarningsIncludingEmptyLists() {
        val warned =
            Json
                .parseToJsonElement(
                    MissionMetadataProjection.project(
                        rawLevel(
                            "\"route_required_key_mask\":0,\"route_completing_key_mask_set\":0," +
                                "\"data_warnings\":[\"Skipped 2 invalid dynamic-light target references\"]",
                        ),
                    ),
                ).jsonObject["levels"]!!
                .jsonArray
                .single()
                .jsonObject
        assertEquals(
            "Skipped 2 invalid dynamic-light target references",
            warned["data_warnings"]!!
                .jsonArray
                .single()
                .jsonPrimitive.content,
        )

        val normal =
            Json
                .parseToJsonElement(
                    MissionMetadataProjection.project(
                        rawLevel("\"route_required_key_mask\":0,\"route_completing_key_mask_set\":0"),
                    ),
                ).jsonObject["levels"]!!
                .jsonArray
                .single()
                .jsonObject
        assertEquals(0, normal["data_warnings"]!!.jsonArray.size)
    }

    private fun rawLevel(routeFields: String) =
        """{"status":"ok","levels":[{"level_num":1,$routeFields,"route_steps":[]}]}"""
}
