package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class LauncherFileLabelsTest {
    @Test
    fun labelsDemoAndDxaFilesSpecifically() {
        assertEquals("Demo recording", launcherFileTypeLabel("yep9.dem"))
        assertEquals("Game mod", launcherFileTypeLabel("hires.dxa"))
        assertEquals("Game mod", launcherFileTypeLabel("uud1sp.dxa (1)"))
        assertEquals(".dxa - game mod", launcherExtensionDescription("uud1sp.dxa (1)"))
        assertEquals(".dxarchive file", launcherFileTypeLabel("not_a_mod.dxarchive"))
    }

    @Test
    fun storagePurposeUsesSpecificImportedFileTypes() {
        assertEquals(
            "Demo recording",
            launcherStorageFilePurpose(File("yep9.dem"), "demos/yep9.dem", importedRootFile = true),
        )
        assertEquals(
            "Game mod",
            launcherStorageFilePurpose(File("hires.dxa"), "mods/hires.dxa", importedRootFile = true),
        )
        assertEquals(
            "Game mod",
            launcherStorageFilePurpose(File("uud1sp.dxa (1)"), "mods/uud1sp.dxa (1)", importedRootFile = true),
        )
    }

    @Test
    fun storagePurposeLabelsGeneratedMergedCdAudioSpecifically() {
        val tempDir = createTempDirectory("merged-cd-label").toFile()
        val cueFile = File(tempDir, "infinite_abyss_disc_1.cue")
        cueFile.writeText("$GENERATED_MERGED_CUE_MARKER\nFILE \"infinite_abyss_disc_1.bin\" BINARY\n")
        val binFile = File(tempDir, "infinite_abyss_disc_1.bin")
        binFile.writeText("test")

        assertEquals(
            "Imported (merged) CD audio",
            launcherStorageFilePurpose(binFile, binFile.name, importedRootFile = false),
        )
        assertEquals(
            "Imported (merged) CD audio",
            launcherStorageFilePurpose(cueFile, cueFile.name, importedRootFile = false),
        )
    }

    @Test
    fun storagePurposeLabelsRouteAnalysisArtifactsSpecifically() {
        assertEquals(
            "Route analysis cache",
            launcherStorageFilePurpose(
                File("abc.bin"),
                "d2x-redux/route-cache/g6/abc.bin",
                importedRootFile = false,
            ),
        )
        assertEquals(
            "Route analysis checkpoint",
            launcherStorageFilePurpose(
                File("abc.bin.samples-000042"),
                "d2x-redux/route-cache/g6/abc.bin.samples-000042",
                importedRootFile = false,
            ),
        )
    }
}
