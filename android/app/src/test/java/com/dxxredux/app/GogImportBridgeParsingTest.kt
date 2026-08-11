package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class GogImportBridgeParsingTest {
    @Test
    fun parsesStrictPkgManifestRecords() {
        assertEquals(
            listOf(GogImportBridge.GogFile("DESCENT.HOG", 42, 0xffffffffL)),
            parseGogFileList(arrayOf("DESCENT.HOG|42|4294967295")),
        )
    }

    @Test
    fun retainsLegacyTwoFieldInnoRecords() {
        assertEquals(
            listOf(GogImportBridge.GogFile("DESCENT.HOG", 42)),
            parseGogFileList(arrayOf("DESCENT.HOG|42")),
        )
    }

    @Test
    fun rejectsMalformedOrCollidingRecords() {
        assertNull(parseGogFileList(arrayOf("DESCENT.HOG|bad|1")))
        assertNull(parseGogFileList(arrayOf("DESCENT.HOG|-1|1")))
        assertNull(parseGogFileList(arrayOf("DESCENT.HOG|1|4294967296")))
        assertNull(parseGogFileList(arrayOf("bad|name|1|2")))
        assertNull(parseGogFileList(arrayOf("DESCENT.HOG|1", "descent.hog|1")))
    }
}
