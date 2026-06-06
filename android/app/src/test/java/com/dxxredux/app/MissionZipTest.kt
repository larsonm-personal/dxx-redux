package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.File
import java.io.InputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionZipTest {
    @Test
    fun detectsD2MissionZipFromZipFile() {
        val zipFile =
            createMissionZip(
                "Uneasy4.mn2",
                """
                name = Uneasy 4
                type = normal
                num_levels = 1
                Uneasy4.rl2
                author = Blarget 2 and Nightsurfer
                editor = Inferno 1.0.22
                """.trimIndent(),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        scan!!
        assertEquals("d2", scan.game)
        assertEquals("levels", scan.category)
        assertEquals("stored_zip", scan.importMode)
        assertEquals("Uneasy 4", scan.mission.displayName)
        assertEquals("Blarget 2 and Nightsurfer", scan.mission.author)
        assertEquals(listOf("Uneasy4.rl2"), scan.mission.levelNames)
        assertEquals(
            listOf("mission_descriptor", "mission_hog", "mod_archive"),
            scan.constituents.map { it.role }.sorted(),
        )
    }

    @Test
    fun detectsD1MissionFromMsnAndRdl() {
        val zipFile =
            createMissionZip(
                "Custom.msn",
                """
                name = Custom D1
                type = normal
                num_levels = 1
                custom01.rdl
                """.trimIndent(),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("d1", scan!!.game)
        assertEquals("Custom D1", scan.mission.displayName)
        assertEquals(listOf("custom01.rdl"), scan.mission.levelNames)
    }

    @Test
    fun streamInspectionRecognizesMissionZip() {
        val zipFile = createMissionZip("Uneasy4.mn2", "name = Uneasy 4\nnum_levels = 1\nUneasy4.rl2\n")
        val scan =
            zipFile.inputStream().use { input: InputStream ->
                MissionZip.inspect(input)
            }

        assertNotNull(scan)
        assertEquals("d2", scan!!.game)
        assertEquals("Uneasy 4", scan.mission.displayName)
    }

    @Test
    fun ignoresPlainZipWithoutMissionDescriptor() {
        val zipFile = File.createTempFile("plain", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("readme.txt"))
            zip.write("hello".toByteArray())
            zip.closeEntry()
        }

        assertNull(MissionZip.inspect(zipFile))
    }

    private fun createMissionZip(
        missionName: String,
        missionText: String,
    ): File {
        val zipFile = File.createTempFile("missionzip", ".zip")
        zipFile.deleteOnExit()
        val stem = missionName.substringBeforeLast('.')
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("$stem.dxa"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("$stem.hog"))
            zip.write(byteArrayOf(5, 6, 7, 8))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry(missionName))
            zip.write(missionText.toByteArray())
            zip.closeEntry()
        }
        return zipFile
    }
}
