package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SafUriPermissionsTest {
    @Test
    fun derivesOwningTreeUriFromTreeDocumentUri() {
        val uri =
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss/document/6634-3535%3AInf%20abyss%2FDESCENT_II_ABYSS%20(Track%201).bin"

        assertEquals(
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss",
            deriveOwningTreeUriString(uri),
        )
    }

    @Test
    fun returnsNullForDocumentUriWithoutTreeSegment() {
        val uri = "content://com.android.externalstorage.documents/document/6634-3535%3Atrack.bin"

        assertNull(deriveOwningTreeUriString(uri))
    }
}