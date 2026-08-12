package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertThrows
import org.junit.Test

class D2HamPatchSchemaTest {
    @Test
    fun acceptsMaintainedFieldAndRowShapes() {
        D2HamPatchSchema.validate(
            JSONObject("""{"op":"replace","path":"/sections/sounds/110/Sound","value":194}"""),
        )
    }

    @Test
    fun rejectsNativeIncompatibleOperationsAndValues() {
        for (text in listOf(
            """{"op":"remove","path":"/sections/sounds/1/Sound"}""",
            """{"op":"replace","path":"/sections/sounds/1/Sound"}""",
            """{"op":"replace","path":"/sections/unknown/1/Field","value":1}""",
            """{"op":"replace","path":"/sections/sounds/-1/Sound","value":1}""",
            """{"op":"replace","path":"/sections/sounds/254/Sound","value":1}""",
            """{"op":"replace","path":"/sections/sounds/1/Unknown","value":1}""",
            """{"op":"replace","path":"/sections/sounds/1/Sound","value":"1"}""",
            """{"op":"replace","path":"/sections/sounds/1/Sound~1extra","value":1}""",
            """{"op":"replace","path":"/sections/sounds/1/Sound","value":256}""",
            """{"op":"add","path":"/sections/textures/1","value":{"Index":1}}""",
        )) {
            assertThrows(text, IllegalArgumentException::class.java) {
                D2HamPatchSchema.validate(JSONObject(text))
            }
        }
    }
}
