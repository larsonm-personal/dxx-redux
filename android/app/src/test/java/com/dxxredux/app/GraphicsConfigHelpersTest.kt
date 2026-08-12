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
    fun renderResolutionValidationProtectsMaintainedBudgetAndConfig() {
        assertFalse(isSupportedAndroidRenderResolution(319, 200))
        assertFalse(isSupportedAndroidRenderResolution(320, 199))
        assertTrue(isSupportedAndroidRenderResolution(320, 200))
        assertTrue(isSupportedAndroidRenderResolution(3840, 2160))
        assertFalse(isSupportedAndroidRenderResolution(3841, 2160))
        assertFalse(isSupportedAndroidRenderResolution(4096, 4096))
        assertFalse(isSupportedAndroidRenderResolution(60000, 60000))
        assertEquals(3840 to 2160, parseSupportedAndroidRenderResolution("3840x2160"))
        assertEquals(null, parseSupportedAndroidRenderResolution("60000x60000"))

        val root = tmp.newFolder("render-resolution")
        val cfg = root.resolve("descent.cfg").apply { writeText("ResolutionX=640\nResolutionY=480\n") }
        for (value in listOf("60000x60000", "-1x480", "640x480junk", "640X480", "640x")) {
            assertFalse(updateDescentCfgResolution(root, value))
            assertTrue(cfg.readText().contains("ResolutionX=640\nResolutionY=480"))
        }
        assertTrue(updateDescentCfgResolution(root, "1920x1080"))
        assertTrue(cfg.readText().contains("ResolutionX=1920\nResolutionY=1080"))
    }

    @Test
    fun updateAllConfigFiles_writesReplacementMetacharactersLiterally() {
        val filesDir = filesDirWithConfigs()
        val values = listOf("\$9", "\$0", "\${name}", "\\", "foo\\bar", "\\\\")

        for (value in values) {
            updateAllConfigFiles(filesDir, listOf("TexFilt" to value))

            for (relative in listOf("descent.cfg", "d1x-redux/descent.cfg", "d2x-redux/descent.cfg")) {
                assertEquals("TexFilt=$value\n", cfgText(filesDir, relative))
            }
        }
    }

    @Test
    fun cornerTextInset_writesRootAndBothGameConfigs() {
        val filesDir = filesDirWithConfigs()

        updateAllConfigFiles(filesDir, listOf("CornerTextInset" to "2"))

        assertEquals("2", readConfigValueForGame(filesDir, "d1", "CornerTextInset"))
        assertEquals("2", readConfigValueForGame(filesDir, "d2", "CornerTextInset"))
        assertTrue(cfgText(filesDir, "descent.cfg").contains("CornerTextInset=2"))
    }

    @Test
    fun updateConfigFilesForGame_writesRootAndSelectedGameOnly() {
        val filesDir = filesDirWithConfigs()

        updateConfigFilesForGame(filesDir, "d2", listOf("MovieTexFilt" to "1"))

        assertTrue(cfgText(filesDir, "descent.cfg").contains("MovieTexFilt=1"))
        assertTrue(cfgText(filesDir, "d2x-redux/descent.cfg").contains("MovieTexFilt=1"))
        assertFalse(cfgText(filesDir, "d1x-redux/descent.cfg").contains("MovieTexFilt="))
    }

    @Test
    fun readGraphicsConfigSnapshot_capturesCompleteGameConfigBeforeApply() {
        val filesDir = filesDirWithConfigs()
        File(filesDir, "d2x-redux/descent.cfg").writeText(
            """
            GammaLevel=4
            TexFilt=2
            MenuTexFilt=1
            HudTexFilt=0
            MainViewFov=110
            CornerTextInset=2
            AnisoLevel=8
            MsaaLevel=4
            ClassicDepth=1
            MovieTexFilt=1
            """.trimIndent() + "\n",
        )

        val snapshot = readGraphicsConfigSnapshot(filesDir, "d2")
        File(filesDir, "d2x-redux/descent.cfg").writeText("GammaLevel=0\nTexFilt=0\n")

        assertEquals(
            listOf(
                "gamma_level" to 4,
                "tex_filt" to 2,
                "menu_tex_filt" to 1,
                "hud_tex_filt" to 0,
                "main_view_fov" to 110,
                "corner_text_inset" to 2,
                "aniso_level" to 8,
                "msaa_level" to 4,
                "classic_depth" to 1,
                "movie_tex_filt" to 1,
            ),
            snapshot,
        )
    }

    @Test
    fun readGraphicsConfigSnapshot_excludesD2OnlyAndInvalidValuesForD1() {
        val filesDir = filesDirWithConfigs()
        File(filesDir, "d1x-redux/descent.cfg").writeText(
            "TexFilt=invalid\nMenuTexFilt=1\nMovieTexFilt=1\n",
        )

        assertEquals(listOf("menu_tex_filt" to 1), readGraphicsConfigSnapshot(filesDir, "d1"))
    }

    @Test
    fun applyGraphicsOptionSnapshot_reportsFailureForGenerationRetry() {
        val applied = mutableListOf<String>()

        val complete =
            applyGraphicsOptionSnapshot(listOf("gamma_level" to 4, "tex_filt" to 2, "msaa_level" to 4)) {
                name,
                _,
                ->
                applied.add(name)
                name != "tex_filt"
            }

        assertFalse(complete)
        assertEquals(listOf("gamma_level", "tex_filt"), applied)
    }

    @Test
    fun updateAllConfigFiles_preservesLargeUnknownContent() {
        val filesDir = filesDirWithConfigs()
        val unknown = "Unknown=" + "x".repeat(65_536) + "\n"
        File(filesDir, "descent.cfg").writeText(unknown + "TexFilt=0\n")

        updateAllConfigFiles(filesDir, listOf("TexFilt" to "2"))

        assertEquals(unknown + "TexFilt=2\n", File(filesDir, "descent.cfg").readText())
    }

    @Test
    fun writeUtf8Batch_restoresEveryTargetAfterLaterPublishFailure() {
        val filesDir = tmp.newFolder("batchRollback")
        val root = File(filesDir, "root.cfg").apply { writeText("root-old") }
        val d1 = File(filesDir, "d1.cfg").apply { writeText("d1-old") }
        val d2 = File(filesDir, "d2.cfg").apply { writeText("d2-old") }

        try {
            AtomicFilePublication.writeUtf8Batch(
                listOf(root to "root-new", d1 to "d1-new", d2 to "d2-new"),
            ) { index, _, _ ->
                if (index == 2) error("injected third-target failure")
            }
            throw AssertionError("Expected injected publication failure")
        } catch (failure: IllegalStateException) {
            assertEquals("injected third-target failure", failure.message)
        }

        assertEquals("root-old", root.readText())
        assertEquals("d1-old", d1.readText())
        assertEquals("d2-old", d2.readText())
    }

    @Test
    fun writeUtf8Batch_removesNewTargetDuringRollback() {
        val filesDir = tmp.newFolder("batchNewTargetRollback")
        val root = File(filesDir, "root.cfg")
        val d1 = File(filesDir, "d1.cfg").apply { writeText("d1-old") }

        try {
            AtomicFilePublication.writeUtf8Batch(listOf(root to "root-new", d1 to "d1-new")) {
                index,
                _,
                _,
                ->
                if (index == 1) error("injected second-target failure")
            }
            throw AssertionError("Expected injected publication failure")
        } catch (_: IllegalStateException) {
        }

        assertFalse(root.exists())
        assertEquals("d1-old", d1.readText())
    }
}
