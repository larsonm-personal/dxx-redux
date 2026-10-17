package com.dxxredux.app

import org.json.JSONArray
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream

class BoundedJsonInputTest {
    @Test
    fun textReaderRejectsInputBeyondBudget() {
        val failure =
            runCatching {
                ByteArrayInputStream(ByteArray(17)).readUtf8TextBounded(16, "test input")
            }.exceptionOrNull()

        assertTrue(failure is IllegalArgumentException)
        assertTrue(failure?.message.orEmpty().contains("exceeds 16 bytes"))
    }

    @Test
    fun jsonValidatorRejectsExcessiveDepth() {
        val failure =
            runCatching {
                preflightJsonComplexity(
                    "[[[[[0]]]]]",
                    JsonComplexityLimits(nodes = 16, keys = 16, depth = 4, stringBytes = 16),
                    "test JSON",
                )
            }.exceptionOrNull()

        assertTrue(failure is IllegalArgumentException)
        assertTrue(failure?.message.orEmpty().contains("nesting depth"))
    }

    @Test
    fun jsonValidatorRejectsAggregateStringBytes() {
        val root = JSONArray().put("1234").put("5678")
        val failure =
            runCatching {
                validateJsonComplexity(
                    root,
                    JsonComplexityLimits(nodes = 8, keys = 8, depth = 4, stringBytes = 7),
                    "test JSON",
                )
            }.exceptionOrNull()

        assertTrue(failure is IllegalArgumentException)
        assertTrue(failure?.message.orEmpty().contains("JSON string bytes"))
    }

    @Test
    fun jsonPreflightRejectsNodesBeforeMaterialization() {
        val failure =
            runCatching {
                preflightJsonComplexity(
                    "[0,1,2,3]",
                    JsonComplexityLimits(nodes = 4, keys = 8, depth = 4, stringBytes = 16),
                    "test JSON",
                )
            }.exceptionOrNull()

        assertTrue(failure is IllegalArgumentException)
        assertTrue(failure?.message.orEmpty().contains("JSON nodes"))
    }
}
