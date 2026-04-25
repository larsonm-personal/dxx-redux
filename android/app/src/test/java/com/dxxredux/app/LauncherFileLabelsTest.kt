package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File

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
}