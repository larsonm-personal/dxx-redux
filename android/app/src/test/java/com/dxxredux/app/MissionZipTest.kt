package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
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
        assertEquals(listOf("Uneasy 4"), scan.missionSets.map { it.mission.displayName })
    }

    @Test
    fun keepsMultipleMissionSetsFromOneZip() {
        val zipFile = createDescentMaximumStyleZip()

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        scan!!
        assertEquals("Descent Maximum (fixed)", scan.mission.displayName)
        assertEquals(
            listOf("Descent Maximum (fixed)", "Descent Max Anarchy (fix)"),
            scan.missionSets.map { it.mission.displayName },
        )
        assertEquals(
            listOf(listOf("max_f.hog", "max_f.mn2"), listOf("maxlnk_f.hog", "maxlnk_f.mn2")),
            scan.missionSets.map { set -> set.constituents.map { it.name }.sorted() },
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

    @Test
    fun importCandidateRecognizesRebirthChildZip() {
        val zipFile = File.createTempFile("parent-missionzip", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("ewithin-xl.zip"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("ewithin-rebirth.zip"))
            zip.write(byteArrayOf(5, 6, 7, 8))
            zip.closeEntry()
        }

        assertTrue(zipFile.inputStream().use { MissionZip.isImportCandidate(it) })
    }

    @Test
    fun importCandidateIgnoresPlainZip() {
        val zipFile = File.createTempFile("plain-candidate", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("readme.txt"))
            zip.write("hello".toByteArray())
            zip.closeEntry()
        }

        assertFalse(zipFile.inputStream().use { MissionZip.isImportCandidate(it) })
    }

    @Test
    fun sortsTxtFilesFirstAndUsesSingleTxtAsReadme() {
        val zipFile = createMissionZipWithDocs("SingleDoc.zip", listOf("notes.txt" to "hello"))

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("notes.txt", scan!!.constituents.first().name)
        assertEquals("notes.txt", scan.readme!!.name)
        val content = MissionZip.readTextFile(zipFile, scan.readme.path)
        assertEquals("hello", content.text)
    }

    @Test
    fun prefersReadmeTxtWhenMultipleTxtFilesExist() {
        val zipFile =
            createMissionZipWithDocs(
                "Uneasy4.zip",
                listOf(
                    "Uneasy4-notes.txt" to "zip prefix",
                    "README.txt" to "readme",
                    "large.txt" to "this is larger",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("README.txt", scan!!.readme!!.name)
    }

    @Test
    fun prefersZipNameTxtBeforeLargestFallback() {
        val zipFile =
            createMissionZipWithDocs(
                "Uneasy4.zip",
                listOf(
                    "Uneasy4-notes.txt" to "zip prefix",
                    "large.txt" to "this is larger",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("Uneasy4-notes.txt", scan!!.readme!!.name)
    }

    @Test
    fun usesLargestTxtWhenNoReadmeOrZipNameMatch() {
        val zipFile =
            createMissionZipWithDocs(
                "NoPrefix.zip",
                listOf(
                    "small.txt" to "tiny",
                    "large.txt" to "this is larger",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("large.txt", scan!!.readme!!.name)
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

    private fun createMissionZipWithDocs(
        filename: String,
        docs: List<Pair<String, String>>,
    ): File {
        val dir = File("build/test-missionzip-readme-rules").absoluteFile
        dir.mkdirs()
        val zipFile = File(dir, filename)
        zipFile.delete()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("Uneasy4.dxa"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Uneasy4.hog"))
            zip.write(byteArrayOf(5, 6, 7, 8))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Uneasy4.mn2"))
            zip.write("name = Uneasy 4\nnum_levels = 1\nUneasy4.rl2\n".toByteArray())
            zip.closeEntry()

            for ((name, text) in docs) {
                zip.putNextEntry(ZipEntry(name))
                zip.write(text.toByteArray())
                zip.closeEntry()
            }
        }
        zipFile.deleteOnExit()
        return zipFile
    }

    private fun createDescentMaximumStyleZip(): File {
        val zipFile = File.createTempFile("descent-maximum-style", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("max_f.hog"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("max_f.mn2"))
            zip.write(
                """
                name = Descent Maximum (fixed)
                type = normal
                num_levels = 1
                psx01.rl2
                num_secrets = 1
                psxs1a.rl2,1
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("maxlnk_f.hog"))
            zip.write(byteArrayOf(5, 6, 7, 8))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("maxlnk_f.mn2"))
            zip.write(
                """
                name = Descent Max Anarchy (fix)
                type = anarchy
                num_levels = 1
                psxlink1.rl2
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("max_f.txt"))
            zip.write("readme".toByteArray())
            zip.closeEntry()
        }
        return zipFile
    }
}
