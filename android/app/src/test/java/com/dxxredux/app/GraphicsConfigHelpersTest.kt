package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class GraphicsConfigHelpersTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun filesDirWithConfigs(): File {
        val filesDir = tmp.newFolder("filesDir")
        File(filesDir, "d1x-redux").mkdirs()
        File(filesDir, "d2x-redux").mkdirs()
        File(filesDir, "descent.cfg").writeText("TexFilt=0\n")
        File(filesDir, "d1x-redux/descent.cfg").writeText("TexFilt=1\n")
        File(filesDir, "d2x-redux/descent.cfg").writeText("TexFilt=2\n")
        return filesDir
    }

    private fun cfgText(
        filesDir: File,
        relative: String,
    ): String = File(filesDir, relative).readText()

    @Test
    fun readConfigValueForGame_usesRequestedGameBeforeRoot() {
        val filesDir = filesDirWithConfigs()

        assertEquals("1", readConfigValueForGame(filesDir, "d1", "TexFilt"))
        assertEquals("2", readConfigValueForGame(filesDir, "d2", "TexFilt"))
    }

    @Test
    fun updateAllConfigFiles_writesRootAndBothGameConfigs() {
        val filesDir = filesDirWithConfigs()

        updateAllConfigFiles(filesDir, listOf("ClassicDepth" to "1"))

        assertTrue(cfgText(filesDir, "descent.cfg").contains("ClassicDepth=1"))
        assertTrue(cfgText(filesDir, "d1x-redux/descent.cfg").contains("ClassicDepth=1"))
        assertTrue(cfgText(filesDir, "d2x-redux/descent.cfg").contains("ClassicDepth=1"))
    }

    @Test
    fun updateConfigFilesForGame_writesRootAndSelectedGameOnly() {
        val filesDir = filesDirWithConfigs()

        updateConfigFilesForGame(filesDir, "d2", listOf("MovieTexFilt" to "1"))

        assertTrue(cfgText(filesDir, "descent.cfg").contains("MovieTexFilt=1"))
        assertTrue(cfgText(filesDir, "d2x-redux/descent.cfg").contains("MovieTexFilt=1"))
        assertFalse(cfgText(filesDir, "d1x-redux/descent.cfg").contains("MovieTexFilt="))
    }
}