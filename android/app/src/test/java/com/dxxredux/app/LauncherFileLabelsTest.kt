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
}