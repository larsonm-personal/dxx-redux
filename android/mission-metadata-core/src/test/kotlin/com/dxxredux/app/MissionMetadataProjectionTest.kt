package com.dxxredux.app

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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
            "schema_version", "capture_status", "generated_count", "disk_count",
            "authored_segments", "disk_records", "triggers", "runtime_bindings",
        )) {
            val block = JsonObject(valid.filterKeys { it != name }).toString()
            assertThrows("missing $name", IllegalArgumentException::class.java) { projectMatcenLevel(block) }
        }
    }

    @Test
    fun rejectsInvalidMatcenRegressionScalars() {
        for (name in listOf("schema_version", "generated_count", "disk_count")) {
            for (value in listOf("null", "true", "\"1\"", "1.5", "[]", "{}", "2147483648", "-1")) {
                assertInvalidMatcenField(name, value)
            }
        }
        for (value in listOf("0", "2")) assertInvalidMatcenField("schema_version", value)
        for (value in listOf("null", "true", "1", "[]", "{}", "\"unknown\"")) {
            assertInvalidMatcenField("capture_status", value)
        }
    }

    @Test
    fun rejectsInvalidMatcenRegressionCollections() {
        for (name in listOf("authored_segments", "disk_records", "triggers", "runtime_bindings")) {
            for (value in listOf("null", "true", "1", "\"entries\"", "{}", "[null]", "[1]", "[[]]", "[{},false]")) {
                assertInvalidMatcenField(name, value)
            }
        }
    }

    @Test
    fun acceptsEmptyMatcenRegressionCollectionsAndZeroCounts() {
        val valid = Json.parseToJsonElement(validMatcenRegression).jsonObject.toMutableMap()
        for (name in listOf("authored_segments", "disk_records", "triggers", "runtime_bindings")) {
            valid[name] = Json.parseToJsonElement("[]")
        }
        for (name in listOf("generated_count", "disk_count")) valid[name] = Json.parseToJsonElement("0")
        val block = JsonObject(valid)
        assertEquals(block, projectMatcenLevel(block.toString())["matcen_regression"])
    }

    private fun assertInvalidMatcenField(name: String, value: String) {
        val valid = Json.parseToJsonElement(validMatcenRegression).jsonObject.toMutableMap()
        valid[name] = Json.parseToJsonElement(value)
        assertThrows("$name=$value", IllegalArgumentException::class.java) {
            projectMatcenLevel(JsonObject(valid).toString())
        }
    }

    private fun projectMatcenLevel(block: String? = null): JsonObject {
        val optional = block?.let { ",\"matcen_regression\":$it" }.orEmpty()
        return MissionMetadataProjection.project(
            Json.parseToJsonElement(
                rawLevel("\"route_required_key_mask\":0,\"route_completing_key_mask_set\":1$optional"),
            ).jsonObject,
        )["levels"]!!.jsonArray.single().jsonObject
    }

    private val validMatcenRegression =
        """{
            "schema_version":1,
            "capture_status":"complete",
            "generated_count":2,
            "disk_count":1,
            "authored_segments":[{"segment":9,"nested":{"flags":[1,2],"note":null}}],
            "disk_records":[{"segment":9,"robot_flags":[4294967295,0]}],
            "triggers":[{"links":[{"segment":9,"side":3}]}],
            "runtime_bindings":[{"segment":9,"binding":-1}],
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
