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

    @Test
    fun d1TestFlightSetIsNotLaunchReady() {
        val setDir = createTempDirectory("d1-test-flight").toFile()
        writeFile(setDir, "descent.hog", size = 1626232)
        writeFile(setDir, "descent.pig", size = 28518)

        assertFalse(
            launchDataReadyForGame(
                game = "d1",
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            ),
        )
    }

    @Test
    fun ordinaryD1SetCountsAsLaunchReady() {
        val setDir = createTempDirectory("d1-ready").toFile()
        writeFile(setDir, "descent.hog")
        writeFile(setDir, "descent.pig")

        assertTrue(
            launchDataReadyForGame(
                game = "d1",
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            ),
        )
    }

    private fun writeFile(
        dir: File,
        name: String,
        size: Int = 4,
    ) {
        File(dir, name).writeBytes(ByteArray(size))
    }
}
