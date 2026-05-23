package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class SetupLaunchReadinessTest {
    @Test
    fun d2DemoSetCountsAsLaunchReady() {
        val setDir = createTempDirectory("d2-demo-ready").toFile()
        writeFile(setDir, "d2demo.hog")
        writeFile(setDir, "d2demo.ham")
        writeFile(setDir, "d2demo.pig")

        assertTrue(
            launchDataReadyForGame(
                game = "d2",
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            ),
        )
    }

    @Test
    fun incompleteD2DemoSetIsNotLaunchReady() {
        val setDir = createTempDirectory("d2-demo-missing").toFile()
        writeFile(setDir, "d2demo.hog")
        writeFile(setDir, "d2demo.ham")

        assertFalse(
            launchDataReadyForGame(
                game = "d2",
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            ),
        )
    }

    private fun writeFile(
        dir: File,
        name: String,
    ) {
        File(dir, name).writeText("test")
    }
}