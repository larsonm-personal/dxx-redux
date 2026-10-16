package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class LauncherScriptInputTest {
    @Test
    fun rejectsEmptyMetadataOnlyAndNonObjectScripts() {
        for (source in listOf("[]", "[{\"_info\":{}}]", "[null]", "[1]", "[\"assert\"]")) {
            assertThrows(IllegalArgumentException::class.java) { launcherScriptSteps(source) }
        }
    }

    @Test
    fun rejectsMissingAndInvalidActionsInsteadOfDroppingSteps() {
        for (source in listOf("[{}]", "[{\"action\":null}]", "[{\"action\":1}]", "[{\"action\":\" \"}]")) {
            assertThrows(IllegalArgumentException::class.java) { launcherScriptSteps(source) }
        }
        assertThrows(IllegalArgumentException::class.java) {
            launcherScriptSteps("""[{"action":"enter_launcher"},null,{"action":"assert_button"}]""")
        }
    }

    @Test
    fun rejectsMalformedJson() {
        assertThrows(org.json.JSONException::class.java) { launcherScriptSteps("[{") }
    }

    @Test
    fun preservesNativeOwnedActionsAndHandoffStepIndices() {
        val steps =
            launcherScriptSteps(
                """
                [
                    {"_info":{"description":"mixed launcher and native script"}},
                    {"action":"enter_launcher"},
                    {"action":"enter_game","game":"d2"},
                    {"action":"introspect"},
                    {"action":"enter_launcher"},
                    {"action":"assert_button","text":"Play"},
                ]
                """.trimIndent(),
            )
        assertEquals(5, steps.length())
        assertEquals("enter_game", steps.getJSONObject(1).getString("action"))
        assertEquals("introspect", steps.getJSONObject(2).getString("action"))
        assertEquals("enter_launcher", steps.getJSONObject(3).getString("action"))
    }
}
