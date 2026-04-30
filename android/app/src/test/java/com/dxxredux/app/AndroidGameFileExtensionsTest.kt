package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AndroidGameFileExtensionsTest {
    @Test
    fun recognizesSharedGameExtensions() {
        assertTrue(AndroidGameFileExtensions.hasGameExtension("custom.mn2"))
        assertTrue(AndroidGameFileExtensions.hasGameExtension("DESCENT2.HOG"))
        assertTrue(AndroidGameFileExtensions.hasGameExtension("demo.DEM"))
        assertFalse(AndroidGameFileExtensions.hasGameExtension("readme.txt"))
    }

    @Test
    fun recognizesGogAudioSubset() {
        assertTrue(AndroidGameFileExtensions.isGogAudioFile("descent_ii.gog"))
        assertTrue(AndroidGameFileExtensions.isGogAudioFile("descent_ii.inst"))
        assertFalse(AndroidGameFileExtensions.isGogAudioFile("descent2.hog"))
    }
}