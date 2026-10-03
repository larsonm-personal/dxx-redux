package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class JsoncTest {
    @Test
    fun stripsTrailingCommasFromNestedObjectsAndArrays() {
        assertEquals("{\"rows\":[{\"n\":1 } ] }", Jsonc.strip("{\"rows\":[{\"n\":1,},],}"))
    }

    @Test
    fun removesTrailingCommaBeforeCommentsWithoutChangingLineNumbers() {
        assertEquals("[1  \n\n]", Jsonc.strip("[1, // last item\n/* footer\n*/]"))
    }

    @Test
    fun preservesCommaLikeTextEscapesAndCommentMarkersInsideStrings() {
        val text = """[",]}","a\"//b","/*literal*/","\\",2,3]"""
        assertEquals(text, Jsonc.strip(text))
    }
}
