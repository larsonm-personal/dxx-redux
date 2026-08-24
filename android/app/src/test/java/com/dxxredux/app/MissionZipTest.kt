package com.dxxredux.app

import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry
import org.apache.commons.compress.archivers.sevenz.SevenZOutputFile
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.InputStream
import java.util.zip.CRC32
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
        assertTrue(scan.constituents.all { it.compressedSizeBytes != null })
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
    fun canonicalizesMultiMissionIdentityAcrossBackendsAndEntryOrders() {
        val entries = descentMaximumEntries()
        val scans =
            listOf(
                MissionZip.inspect(createArchiveZip(entries)),
                MissionZip.inspect(createArchiveZip(entries.reversed())),
                MissionZip.inspect(createArchive7z(entries)),
                MissionZip.inspect(createArchive7z(entries.reversed())),
                inspectExtractedEntries(entries),
                inspectExtractedEntries(entries.reversed()),
            ).map(::requireNotNull)

        for (scan in scans) {
            assertEquals("Descent Maximum (fixed)", scan.mission.displayName)
            assertEquals("d2", scan.game)
            assertEquals(
                listOf("max_f.mn2", "maxlnk_f.mn2"),
                scan.missionSets.map { it.mission.path },
            )
            assertEquals(
                listOf("Descent Maximum (fixed)", "Descent Max Anarchy (fix)"),
                scan.missionSets.map { it.mission.displayName },
            )
        }
    }

    @Test
    fun keepsSameNamedMissionAssetsWithinEachVariantDirectory() {
        val zipFile = File.createTempFile("mission-variants", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            for (variant in listOf("D2X", "DOS", "Rebirth")) {
                zip.putNextEntry(ZipEntry("$variant/ULTERIOR.hog"))
                zip.write(hogBytes("LEVEL01.rl2"))
                zip.closeEntry()

                zip.putNextEntry(ZipEntry("$variant/ULTERIOR.mn2"))
                zip.write(
                    "name = Ulterior $variant\ntype = normal\nnum_levels = 1\nLEVEL01.rl2\n".toByteArray(),
                )
                zip.closeEntry()
            }
        }

        val scan = requireNotNull(MissionZip.inspect(zipFile))

        assertEquals(listOf("D2X", "DOS", "Rebirth"), scan.missionSets.map { it.mission.path.substringBefore('/') })
        assertEquals(
            listOf("D2X/ULTERIOR.hog", "DOS/ULTERIOR.hog", "Rebirth/ULTERIOR.hog"),
            scan.missionSets.map { set ->
                set.constituents.single { it.role == GameFileFormats.MISSION_ZIP_HOG }.path
            },
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
    fun detectsD2MissionFromSevenZipFile() {
        val sevenZipFile =
            createMission7z(
                "Seven.mn2",
                """
                name = Seven Pack
                type = normal
                num_levels = 1
                seven01.rl2
                """.trimIndent(),
                listOf("README.txt" to "7z readme"),
            )

        val scan = MissionZip.inspect(sevenZipFile)

        assertNotNull(scan)
        scan!!
        assertEquals("d2", scan.game)
        assertEquals("7z", scan.archiveFormat)
        assertEquals("extracted_bundle", scan.importMode)
        assertEquals("Seven Pack", scan.mission.displayName)
        assertEquals("README.txt", scan.readme!!.name)
        assertTrue(scan.constituents.all { it.compressedSizeBytes == null })
        assertEquals("7z readme", MissionZip.readTextFile(sevenZipFile, scan.readme.path).text)
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
    fun rejectsMalformedAndOrphanMissionDescriptors() {
        val malformed = createMissionZip("bad.mn2", "name = Bad\nnum_levels = 1\n")
        assertNull(MissionZip.inspect(malformed))

        val orphan = File.createTempFile("orphan-mission", ".zip")
        orphan.deleteOnExit()
        ZipOutputStream(orphan.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("orphan.mn2"))
            zip.write("name = Orphan\nnum_levels = 1\norphan01.rl2\n".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("other.hog"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()
        }

        assertNull(MissionZip.inspect(orphan))
        assertFalse(orphan.inputStream().use { MissionZip.isImportCandidate(it) })
    }

    @Test
    fun keepsValidMissionAndOmitsOrphanInSameArchive() {
        val zipFile = File.createTempFile("partial-mission", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            for (stem in listOf("valid", "orphan")) {
                zip.putNextEntry(ZipEntry("$stem.mn2"))
                zip.write("name = $stem\nnum_levels = 1\n$stem.rl2\n".toByteArray())
                zip.closeEntry()
            }
            zip.putNextEntry(ZipEntry("valid.hog"))
            zip.write(hogBytes("valid.rl2"))
            zip.closeEntry()
        }

        val scan = requireNotNull(MissionZip.inspect(zipFile))
        assertEquals(listOf("valid"), scan.missionSets.map { it.mission.displayName })
    }

    @Test
    fun rejectsSameStemArchivesWithoutEveryDeclaredLevel() {
        val zipFile = File.createTempFile("incomplete-mission", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("pack.mn2"))
            zip.write("name = Pack\nnum_levels = 2\nlevel01.rl2\nlevel02.rl2\n".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("pack.hog"))
            zip.write(hogBytes("level01.rl2"))
            zip.closeEntry()
        }

        assertNull(MissionZip.inspect(zipFile))
        assertFalse(zipFile.inputStream().use { MissionZip.isImportCandidate(it) })
    }

    @Test
    fun identifiesD2xxlExtendedHogWithoutMisclassifyingOtherInvalidHogs() {
        val extended =
            createArchiveZip(
                listOf(
                    "extended.mn2" to "name = Extended\nnum_levels = 1\nlevel01.rl2\n".toByteArray(),
                    "extended.hog" to "D2Xextended".toByteArray(Charsets.US_ASCII),
                ),
            )
        val invalid = createArchiveZip(listOf("invalid.hog" to "BADinvalid".toByteArray(Charsets.US_ASCII)))

        assertNull(MissionZip.inspect(extended))
        assertTrue(MissionZip.containsUnsupportedD2xxlHog(extended))
        assertTrue(extended.inputStream().use { MissionZip.containsUnsupportedD2xxlHog(it) })
        assertFalse(MissionZip.containsUnsupportedD2xxlHog(invalid))
    }

    @Test
    fun acceptsMissionWithAllOrdinaryLevelsAndMissingOptionalSecret() {
        val zipFile = File.createTempFile("missing-secret-mission", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("pack.mn2"))
            zip.write(
                "name = Pack\nnum_levels = 1\nlevel01.rl2\nnum_secrets = 1\nmissing.rl2,1\n".toByteArray(),
            )
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("pack.hog"))
            zip.write(hogBytes("level01.rl2"))
            zip.closeEntry()
        }

        val scan = requireNotNull(MissionZip.inspect(zipFile))
        assertEquals(listOf("level01.rl2"), scan.mission.levelNames)
        assertEquals(listOf("missing.rl2"), scan.mission.secretLevelNames)
    }

    @Test
    fun acceptsDeclaredLevelsFromValidatedHogDxaOrLooseFiles() {
        val zipFile = File.createTempFile("combined-mission", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("missions/pack.mn2"))
            zip.write("name = Pack\nnum_levels = 3\nlevel01.rl2\nlevel02.rl2\nlevel03.rl2\n".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("missions/pack.hog"))
            zip.write(hogBytes("level01.rl2"))
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("missions/pack.dxa"))
            zip.write(zipBytes("levels/level02.rl2"))
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("missions/level03.rl2"))
            zip.write(byteArrayOf(3))
            zip.closeEntry()
        }

        val scan = requireNotNull(MissionZip.inspect(zipFile))
        assertEquals(
            listOf("level03.rl2", "pack.dxa", "pack.hog", "pack.mn2"),
            scan.missionSets
                .single()
                .constituents
                .map { it.name }
                .sorted(),
        )
    }

    @Test
    fun rejectsTruncatedSameStemHog() {
        val zipFile = File.createTempFile("truncated-hog-mission", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("pack.mn2"))
            zip.write("name = Pack\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("pack.hog"))
            zip.write(hogBytes("level01.rl2").dropLast(1).toByteArray())
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
    fun importCandidateAcceptsArchiveLargerThanPreambleLimitWhenCallerSuppliesItsSize() {
        val zipFile = File.createTempFile("large-candidate", ".zip")
        zipFile.deleteOnExit()
        val fillerBytes = ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES + 1L
        val buffer = ByteArray(64 * 1024)
        val crc = CRC32()
        var remaining = fillerBytes
        while (remaining > 0L) {
            val count = minOf(buffer.size.toLong(), remaining).toInt()
            crc.update(buffer, 0, count)
            remaining -= count
        }
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("pack.mn2"))
            zip.write("name = Pack\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("pack.hog"))
            zip.write(hogBytes("level01.rl2"))
            zip.closeEntry()
            val filler =
                ZipEntry("padding.bin").apply {
                    method = ZipEntry.STORED
                    size = fillerBytes
                    compressedSize = fillerBytes
                    this.crc = crc.value
                }
            zip.putNextEntry(filler)
            remaining = fillerBytes
            while (remaining > 0L) {
                val count = minOf(buffer.size.toLong(), remaining).toInt()
                zip.write(buffer, 0, count)
                remaining -= count
            }
            zip.closeEntry()
        }

        assertTrue(
            zipFile.inputStream().use {
                MissionZip.isImportCandidate(it, zipFile.parentFile, zipFile.length())
            },
        )
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

    @Test
    fun prefersTxtReadmeOverExternalDocuments() {
        val zipFile =
            createMissionZipWithDocs(
                "Castaway.zip",
                listOf(
                    "notes.txt" to "text wins",
                    "README.pdf" to "pdf readme",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("notes.txt", scan!!.readme!!.name)
    }

    @Test
    fun usesPdfReadmeWhenNoTxtExists() {
        val zipFile =
            createMissionZipWithDocs(
                "Castaway.zip",
                listOf(
                    "manual.pdf" to "pdf readme",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        val readmeName = scan!!.readme!!.name
        assertEquals("manual.pdf", readmeName)
        assertTrue(MissionZip.isExternalReadmeCandidate(readmeName))
        assertEquals("application/pdf", MissionZip.externalViewMimeType(readmeName))
    }

    @Test
    fun prefersReadmeNamedExternalDocumentBeforeLargestFallback() {
        val zipFile =
            createMissionZipWithDocs(
                "Castaway.zip",
                listOf(
                    "large.pdf" to "this pdf is larger",
                    "README.pdf" to "pdf",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("README.pdf", scan!!.readme!!.name)
    }

    @Test
    fun prefersZipNameExternalDocumentBeforeLargestFallback() {
        val zipFile =
            createMissionZipWithDocs(
                "Castaway.zip",
                listOf(
                    "large.doc" to "this word document is larger",
                    "Castaway guide.docx" to "docx",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals("Castaway guide.docx", scan!!.readme!!.name)
        assertEquals(
            "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
            MissionZip.externalViewMimeType(scan.readme.name),
        )
    }

    @Test
    fun sortsInlineDocsBeforeExternalDocsBeforeOtherFiles() {
        val zipFile =
            createMissionZipWithDocs(
                "Castaway.zip",
                listOf(
                    "manual.pdf" to "pdf",
                    "notes.txt" to "txt",
                    "manual.rtf" to "rtf",
                ),
            )

        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        assertEquals(
            listOf("notes.txt", "manual.pdf", "manual.rtf"),
            scan!!.constituents.take(3).map { it.name },
        )
    }

    @Test
    fun readTextFileRejectsExternalDocuments() {
        val zipFile = createMissionZipWithDocs("Castaway.zip", listOf("README.pdf" to "pdf"))
        val scan = MissionZip.inspect(zipFile)

        assertNotNull(scan)
        val content = MissionZip.readTextFile(zipFile, scan!!.readme!!.path)

        assertEquals("Only .txt files can be viewed", content.problem)
    }

    private fun createMissionZip(
        missionName: String,
        missionText: String,
    ): File {
        val zipFile = File.createTempFile("missionzip", ".zip")
        zipFile.deleteOnExit()
        val stem = missionName.substringBeforeLast('.')
        val mission = GameFileFormats.parseMissionDescriptor(missionName, missionText)
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("$stem.dxa"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("$stem.hog"))
            zip.write(hogBytes(*(mission.levelNames + mission.secretLevelNames).toTypedArray()))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry(missionName))
            zip.write(missionText.toByteArray())
            zip.closeEntry()
        }
        return zipFile
    }

    private fun createMission7z(
        missionName: String,
        missionText: String,
        extraEntries: List<Pair<String, String>> = emptyList(),
    ): File {
        val archive = File.createTempFile("missionzip", ".7z")
        archive.deleteOnExit()
        val stem = missionName.substringBeforeLast('.')
        val mission = GameFileFormats.parseMissionDescriptor(missionName, missionText)
        SevenZOutputFile(archive).use { sevenZ ->
            sevenZ.writeEntry("$stem.dxa", byteArrayOf(1, 2, 3, 4))
            sevenZ.writeEntry("$stem.hog", hogBytes(*(mission.levelNames + mission.secretLevelNames).toTypedArray()))
            sevenZ.writeEntry(missionName, missionText.toByteArray())
            for ((name, text) in extraEntries) {
                sevenZ.writeEntry(name, text.toByteArray())
            }
        }
        return archive
    }

    private fun SevenZOutputFile.writeEntry(
        name: String,
        bytes: ByteArray,
    ) {
        val entry =
            SevenZArchiveEntry().apply {
                this.name = name
                size = bytes.size.toLong()
            }
        putArchiveEntry(entry)
        write(bytes)
        closeArchiveEntry()
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
            zip.write(hogBytes("Uneasy4.rl2"))
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
        val entries = descentMaximumEntries() + ("max_f.txt" to "readme".toByteArray())
        return createArchiveZip(entries)
    }

    private fun descentMaximumEntries(): List<Pair<String, ByteArray>> =
        listOf(
            "max_f.hog" to hogBytes("psx01.rl2", "psxs1a.rl2"),
            "max_f.mn2" to
                """
                name = Descent Maximum (fixed)
                type = normal
                num_levels = 1
                psx01.rl2
                num_secrets = 1
                psxs1a.rl2,1
                """.trimIndent().toByteArray(),
            "maxlnk_f.hog" to hogBytes("psxlink1.rl2"),
            "maxlnk_f.mn2" to
                """
                name = Descent Max Anarchy (fix)
                type = anarchy
                num_levels = 1
                psxlink1.rl2
                """.trimIndent().toByteArray(),
        )

    private fun createArchiveZip(entries: List<Pair<String, ByteArray>>): File {
        val zipFile = File.createTempFile("mission-entry-order", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            for ((name, bytes) in entries) {
                zip.putNextEntry(ZipEntry(name))
                zip.write(bytes)
                zip.closeEntry()
            }
        }
        return zipFile
    }

    private fun createArchive7z(entries: List<Pair<String, ByteArray>>): File {
        val archive = File.createTempFile("mission-entry-order", ".7z")
        archive.deleteOnExit()
        SevenZOutputFile(archive).use { sevenZ ->
            for ((name, bytes) in entries) sevenZ.writeEntry(name, bytes)
        }
        return archive
    }

    private fun inspectExtractedEntries(entries: List<Pair<String, ByteArray>>): MissionZip.ScanResult? {
        val root = File("build/test-mission-entry-order/${System.nanoTime()}").absoluteFile.apply { mkdirs() }
        val files =
            entries.map { (name, bytes) ->
                File(root, name).apply {
                    parentFile?.mkdirs()
                    writeBytes(bytes)
                }
                MissionZipExtractedFile(name, name, bytes.size.toLong())
            }
        return MissionZip.inspectExtracted(
            MissionZipExtractionRecord(
                ownerFilename = "descent-maximum.zip",
                ownerSizeBytes = entries.sumOf { it.second.size.toLong() },
                ownerLastModifiedMs = 0,
                rootDir = root,
                files = files,
            ),
        )
    }

    private fun hogBytes(vararg names: String): ByteArray {
        val output = ByteArrayOutputStream()
        output.write("DHF".toByteArray(Charsets.US_ASCII))
        for (name in names) {
            val encoded = name.toByteArray(Charsets.US_ASCII)
            require(encoded.size <= 13)
            output.write(encoded.copyOf(13))
            output.write(byteArrayOf(1, 0, 0, 0))
            output.write(0)
        }
        return output.toByteArray()
    }

    private fun zipBytes(vararg names: String): ByteArray {
        val output = ByteArrayOutputStream()
        ZipOutputStream(output).use { zip ->
            for (name in names) {
                zip.putNextEntry(ZipEntry(name))
                zip.write(0)
                zip.closeEntry()
            }
        }
        return output.toByteArray()
    }
}
