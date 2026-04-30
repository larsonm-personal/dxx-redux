package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class ImportChooserConfigTest {
    @Test
    fun usesMultipleFileLabelOnPhone() {
        val config = importChooserConfigForDevice(isAndroidTv = false)

        assertEquals("Pick Multiple Files", config.directPickLabel)
        assertEquals(
            "Pick multiple files or a folder containing the files to import",
            config.helpText,
        )
    }

    @Test
    fun usesSingleFileLabelOnTv() {
        val config = importChooserConfigForDevice(isAndroidTv = true)

        assertEquals("Pick Single File", config.directPickLabel)
        assertEquals(
            "Pick a single file or a folder containing the files to import",
            config.helpText,
        )
    }
}