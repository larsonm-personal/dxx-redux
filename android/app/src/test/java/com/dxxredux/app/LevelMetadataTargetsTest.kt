package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class LevelMetadataTargetsTest {
    @Test
    fun directBaseHogExposesItsLevelsForBackgroundScheduling() {
        val setDir = File("build/test-level-metadata-targets/base").absoluteFile
        setDir.deleteRecursively()
        setDir.mkdirs()
        val hog = File(setDir, "descent2.hog").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val metadata =
            GameFileMetadata.Summary(
                format = "HOG",
                scope = "Game archive",
                game = "D2",
                detailRows = emptyList(),
                categories = emptyList(),
                contents =
                    listOf(
                        GameFileMetadata.EntrySummary("d2leva-1.rl2", 1, "D2 level"),
                        GameFileMetadata.EntrySummary("d2leva-s.rl2", 1, "D2 level"),
                    ),
            )

        val target = LevelMetadataTargets.directFile(hog, setDir, metadata)

        assertNotNull(target)
        assertEquals("hog", target!!.sourceType)
        assertEquals("d2", target.missionName)
        assertEquals(listOf("d2leva-1.rl2"), target.normalLevelFiles)
        assertEquals(listOf("d2leva-s.rl2"), target.secretLevelFiles)
    }

    @Test
    fun directVertigoHogUsesAdjacentDescriptorForSecretLevels() {
        val setDir = File("build/test-level-metadata-targets/vertigo").absoluteFile
        setDir.deleteRecursively()
        setDir.mkdirs()
        val hog = File(setDir, "d2x.hog")
        hog.writeBytes(byteArrayOf(1, 2, 3))
        File(setDir, "d2x.mn2").writeText(
            """
            zname = Descent 2: Vertigo
            type = normal
            num_levels = 20
            d2xlvl01.rl2
            d2xlvl02.rl2
            d2xlvl03.rl2
            d2xlvl04.rl2
            d2xlvl05.rl2
            d2xlvl06.rl2
            d2xlvl07.rl2
            d2xlvl08.rl2
            d2xlvl09.rl2
            d2xlvl10.rl2
            d2xlvl11.rl2
            d2xlvl12.rl2
            d2xlvl13.rl2
            d2xlvl14.rl2
            d2xlvl15.rl2
            d2xlvl16.rl2
            d2xlvl17.rl2
            d2xlvl18.rl2
            d2xlvl19.rl2
            d2xlvl20.rl2
            num_secrets = 3
            d2xlvls1.rl2,1
            d2xlvls2.rl2,10
            d2xlvls3.rl2,16
            """.trimIndent(),
        )
        val metadata =
            GameFileMetadata.Summary(
                format = "HOG",
                scope = "Mission or game archive",
                game = "D2",
                detailRows = emptyList(),
                categories = emptyList(),
                contents =
                    (
                        (1..20).map {
                            GameFileMetadata.EntrySummary("d2xlvl%02d.rl2".format(it), 1, "D2 level")
                        } +
                            (1..3).map {
                                GameFileMetadata.EntrySummary("d2xlvls$it.rl2", 1, "D2 level")
                            }
                    ),
            )

        val target = LevelMetadataTargets.directFile(hog, setDir, metadata)

        assertNotNull(target)
        target!!
        assertEquals("hog", target.sourceType)
        assertEquals("d2x", target.missionName)
        assertEquals("Descent 2: Vertigo", target.missionDisplayName)
        assertEquals(20, target.normalLevelFiles.size)
        assertEquals(listOf("d2xlvls1.rl2", "d2xlvls2.rl2", "d2xlvls3.rl2"), target.secretLevelFiles)
    }

    @Test
    fun directDescriptorUsesAdjacentHogForMetadata() {
        val setDir = File("build/test-level-metadata-targets/direct-descriptor").absoluteFile
        setDir.deleteRecursively()
        setDir.mkdirs()
        File(setDir, "max_f.hog").writeBytes(byteArrayOf(1, 2, 3, 4))
        val descriptor =
            File(setDir, "max_f.mn2").also {
                it.writeText(
                    """
                    name = Descent Maximum (fixed)
                    type = normal
                    num_levels = 1
                    psx01.rl2
                    num_secrets = 1
                    psxs1a.rl2,1
                    """.trimIndent(),
                )
            }

        val target = LevelMetadataTargets.directFile(descriptor, setDir, metadata = null)

        assertNotNull(target)
        target!!
        assertEquals("max_f.mn2", target.displayName)
        assertEquals("hog", target.sourceType)
        assertEquals("max_f", target.missionName)
        assertEquals("Descent Maximum (fixed)", target.missionDisplayName)
        assertEquals(File(setDir, "max_f.hog").absolutePath, target.sourcePath)
        assertEquals(listOf("psx01.rl2"), target.normalLevelFiles)
        assertEquals(listOf("psxs1a.rl2"), target.secretLevelFiles)
    }

    @Test
    fun missionZipTargetsExposeEachMissionSet() {
        val setDir = File("build/test-level-metadata-targets/multi-zip-set").absoluteFile
        val zipFile = File("build/test-level-metadata-targets/descent_maximum_fixed.zip").absoluteFile
        setDir.deleteRecursively()
        zipFile.parentFile?.mkdirs()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("max_f.hog"))
            zip.write(hogBytes("psx01.rl2", "psxs1a.rl2"))
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
            zip.write(hogBytes("psxlink1.rl2"))
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
        }
        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        val targets = LevelMetadataTargets.missionZipTargets(zipFile.absolutePath, setDir, scan!!)

        assertEquals(listOf("Descent Maximum (fixed)", "Descent Max Anarchy (fix)"), targets.map { it.displayName })
        assertEquals(listOf("max_f", "maxlnk_f"), targets.map { it.missionName })
        assertEquals(
            listOf("Descent Maximum (fixed)", "Descent Max Anarchy (fix)"),
            targets.map { it.missionDisplayName },
        )
        assertEquals(listOf("normal", "anarchy"), targets.map { it.missionType })
        assertEquals(listOf(listOf("max_f.hog"), listOf("maxlnk_f.hog")), targets.map { it.hogFiles })
        assertEquals(listOf(listOf("psxs1a.rl2"), emptyList<String>()), targets.map { it.secretLevelFiles })
        assertEquals(
            listOf(listOf("max_f.hog", "max_f.mn2"), listOf("maxlnk_f.hog", "maxlnk_f.mn2")),
            targets.map {
                it.archiveEntries.sorted()
            },
        )
    }

    @Test
    fun missionZipTargetsIncludeLooseLevelsReferencedByDescriptor() {
        val setDir = File("build/test-level-metadata-targets/loose-level-zip-set").absoluteFile
        val zipFile = File("build/test-level-metadata-targets/loose_level.zip").absoluteFile
        setDir.deleteRecursively()
        zipFile.parentFile?.mkdirs()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("Extra/MAD.MSN"))
            zip.write(
                """
                name = Mad Decorator!
                type = normal
                num_levels = 1
                mad.rdl
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Extra/MAD.RDL"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Extra/OTHER.HOG"))
            zip.write(byteArrayOf(5, 6, 7, 8))
            zip.closeEntry()
        }
        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        val targets = LevelMetadataTargets.missionZipTargets(zipFile.absolutePath, setDir, scan!!)

        assertEquals(listOf("Mad Decorator!"), targets.map { it.displayName })
        assertEquals(listOf("mad.rdl"), targets.single().normalLevelFiles)
        assertEquals(listOf("Extra/MAD.MSN", "Extra/MAD.RDL"), targets.single().archiveEntries.sorted())
    }

    @Test
    fun zipDescriptorConstituentUsesSameStemHogForMetadata() {
        val setDir = File("build/test-level-metadata-targets/zip-descriptor-set").absoluteFile
        val zipFile = File("build/test-level-metadata-targets/descriptor_pair.zip").absoluteFile
        setDir.deleteRecursively()
        zipFile.parentFile?.mkdirs()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("max_f.hog"))
            zip.write(hogBytes("psx01.rl2", "psxs1a.rl2"))
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
        }
        val scan = MissionZip.inspect(zipFile)
        assertNotNull(scan)
        val descriptor = scan!!.constituents.single { it.name == "max_f.mn2" }

        val target = LevelMetadataTargets.zipConstituent(zipFile.absolutePath, setDir, descriptor)

        assertNotNull(target)
        target!!
        assertEquals("max_f.mn2", target.displayName)
        assertEquals("mission_files", target.sourceType)
        assertEquals("max_f", target.missionName)
        assertEquals(listOf("max_f.hog"), target.hogFiles)
        assertEquals(listOf("psx01.rl2"), target.normalLevelFiles)
        assertEquals(listOf("psxs1a.rl2"), target.secretLevelFiles)
        assertEquals(listOf("max_f.hog", "max_f.mn2"), target.archiveEntries.sorted())
    }

    private fun hogBytes(vararg names: String): ByteArray =
        ByteArrayOutputStream().use { output ->
            output.write("DHF".toByteArray(Charsets.US_ASCII))
            for (name in names) {
                output.write(name.toByteArray(Charsets.US_ASCII).copyOf(13))
                output.write(byteArrayOf(1, 0, 0, 0, 0))
            }
            output.toByteArray()
        }
}
