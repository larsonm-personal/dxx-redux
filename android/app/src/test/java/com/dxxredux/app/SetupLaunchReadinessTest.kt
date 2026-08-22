package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
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
    fun completeRetailD2SetWinsOverStrayDemoFiles() {
        val setDir = createTempDirectory("d2-retail-with-demo-fragment").toFile()
        writeD2Files(setDir)
        writeFile(setDir, "d2demo.ham")

        assertTrue(
            launchDataReadyForGame(
                game = "d2",
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            ),
        )
        assertTrue(detectD2FileList(setDir) === D2_FILES)
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

    @Test
    fun d2OemSetCountsAsLaunchReady() {
        val setDir = createTempDirectory("d2-oem-ready").toFile()
        writeD2PartialFiles(setDir, hogSize = 6132957)

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
    fun d2Quartzon3dSetCountsAsLaunchReady() {
        val setDir = createTempDirectory("d2-quartzon-3d-ready").toFile()
        writeD2PartialFiles(setDir, hogSize = 14024077)

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
    fun incompleteD2OemSetIsNotLaunchReady() {
        val setDir = createTempDirectory("d2-oem-missing").toFile()
        writeD2PartialFiles(setDir, hogSize = 6132957)
        File(setDir, "water.pig").delete()

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
    fun d1InD2ReadinessIsReadyWhenNoD1MissionZipIsEnabled() {
        val filesDir = createTempDirectory("d1-in-d2-not-needed").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        writeD2Files(setDir)

        val readiness =
            d1InD2Readiness(
                filesDir = filesDir,
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            )

        assertFalse(readiness.needed)
        assertTrue(readiness.ready)
        assertFalse(readiness.degraded)
        assertFalse(readiness.blocked)
    }

    @Test
    fun d1InD2ReadinessIsDegradedUntilD1BaseFilesArePresent() {
        val filesDir = createTempDirectory("d1-in-d2-assets").toFile()
        val setDir = File(filesDir, "sets/default").also { it.mkdirs() }
        writeD2Files(setDir)
        val imported = ModManager(filesDir, setDir = setDir).importMissionZipFile(createD1MissionZip(), "d1pack.zip")
        assertTrue(imported?.game == "d1")

        val degraded =
            d1InD2Readiness(
                filesDir = filesDir,
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            )

        assertTrue(degraded.needed)
        assertFalse(degraded.ready)
        assertTrue(degraded.degraded)
        assertFalse(degraded.blocked)

        writeFile(setDir, "descent.hog")
        writeFile(setDir, "descent.pig")
        val ready =
            d1InD2Readiness(
                filesDir = filesDir,
                setDir = setDir,
                manifest = AssetManifest(setDir),
                safManifest = SafManifest.forDir(setDir),
            )

        assertTrue(ready.needed)
        assertTrue(ready.ready)
        assertFalse(ready.degraded)
        assertFalse(ready.blocked)
    }

    private fun writeD2Files(setDir: File) {
        for (info in D2_FILES.filter { it.required }) {
            writeFile(setDir, info.filename)
        }
    }

    private fun writeD2PartialFiles(
        setDir: File,
        hogSize: Int,
    ) {
        for (info in D2_PARTIAL_FILES.filter { it.required }) {
            writeFile(
                setDir,
                info.filename,
                size = if (info.filename == "descent2.hog") hogSize else 4,
            )
        }
    }

    private fun writeFile(
        dir: File,
        name: String,
        size: Int = 4,
    ) {
        File(dir, name).writeBytes(ByteArray(size))
    }

    private fun createD1MissionZip(): File {
        val zipFile = File.createTempFile("d1-mission-pack", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("d1pack.hog"))
            zip.write(createHogBytes("level01.rdl" to ByteArray(12)))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("d1pack.msn"))
            zip.write("name = D1 Pack\nnum_levels = 1\nlevel01.rdl\n".toByteArray())
            zip.closeEntry()
        }
        return zipFile
    }

    private fun createHogBytes(vararg entries: Pair<String, ByteArray>): ByteArray =
        ByteArrayOutputStream().use { output ->
            output.write("DHF".toByteArray(Charsets.US_ASCII))
            entries.forEach { (name, data) ->
                output.write(fixedName(name, 13))
                output.write(leInt(data.size))
                output.write(data)
            }
            output.toByteArray()
        }

    private fun fixedName(
        name: String,
        size: Int,
    ): ByteArray {
        val out = ByteArray(size)
        val bytes = name.toByteArray(Charsets.US_ASCII)
        bytes.copyInto(out, endIndex = minOf(bytes.size, size))
        return out
    }

    private fun leInt(value: Int): ByteArray =
        byteArrayOf(
            (value and 0xff).toByte(),
            ((value shr 8) and 0xff).toByte(),
            ((value shr 16) and 0xff).toByte(),
            ((value shr 24) and 0xff).toByte(),
        )
}
