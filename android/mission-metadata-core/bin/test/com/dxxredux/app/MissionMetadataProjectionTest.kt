package com.dxxredux.app

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class MissionMetadataProjectionTest {
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

    private fun rawLevel(routeFields: String) =
        """{"status":"ok","levels":[{"level_num":1,$routeFields,"route_steps":[]}]}"""
}
