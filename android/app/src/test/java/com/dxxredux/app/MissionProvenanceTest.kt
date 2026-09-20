package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.util.GregorianCalendar
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

class MissionProvenanceTest {
    @Test
    fun archiveDatesSurviveStagingAndPersistentExtraction() {
        val root = File("build/test-provenance/rogue").absoluteFile
        root.mkdirs()
        val archive = File(root, "mods/ROGUE.zip")
        requireNotNull(archive.parentFile).mkdirs()
        val hogHeader = "DHF".toByteArray() + "level01.rdl".toByteArray().copyOf(13)
        writeArchive(
            archive,
            1998,
            6,
            26,
            mapOf(
                "ROG.MSN" to "name=Provenance fixture\ntype=normal\nnum_levels=1\nlevel01.rdl\n".toByteArray(),
                "ROG.HOG" to (hogHeader + byteArrayOf(1, 0, 0, 0, 0)),
                "ROG.REP" to "Report".toByteArray(),
            ),
        )
        val paths =
            ZipFile(archive).use { zip ->
                zip
                    .entries()
                    .asSequence()
                    .map { it.name }
                    .toList()
            }
        val staged = LevelMetadataAnalyzer.stageArchiveEntries(archive, paths, File(root, "staged"))
        assertEquals(3, staged.size)
        assertTrue(staged.all { it.modified == "1998-06-26" && it.kind == "zip_entry_mtime" })

        val scan = requireNotNull(MissionZip.inspect(archive))
        MissionZipExtractionStore(root).ensureExtracted(archive.name, archive, scan)
        val store = MissionZipExtractionStore(root)
        val record = requireNotNull(store.reusableRecord(archive.name, archive))
        val dates = record.provenanceDates(paths.toSet())
        assertEquals(staged.sortedBy { it.file }, dates.sortedBy { it.file })
        val target = requireNotNull(LevelMetadataTargets.missionZip(archive.absolutePath, root, scan))
        assertTrue(target.provenanceDates.any { it.file == "ROG.HOG" && it.modified == "1998-06-26" })
        val descriptor = scan.constituents.first { it.name.equals("ROG.MSN", ignoreCase = true) }
        val constituent = requireNotNull(LevelMetadataTargets.zipConstituent(archive.absolutePath, root, descriptor))
        assertEquals(setOf("ROG.HOG", "ROG.MSN"), constituent.provenanceDates.map { it.file }.toSet())
    }

    @Test
    fun projectionAndViewerPreserveConflictsAndSourceAttribution() {
        val raw = """{
          "provenance": {
            "credits": [{"field":"real_name","value":"Randy","sources":["ROG.HOG/MISSION.AUT"]}],
            "date_estimate": {"year":null,"basis":"conflicting_dates","confidence":"unknown","file_year_range":[1998,2017]},
            "file_dates": [{"file":"ROG.HOG","modified":"1998-06-26","kind":"zip_entry_mtime"}],
            "notes": ["Creation date unverified"]
          }, "levels": []
        }"""
        val result = LevelMetadataResult.fromJson(raw)
        val provenance = requireNotNull(result.provenance)
        val projected = JSONObject(MissionMetadataProjection.project(raw))
        assertEquals(provenance.toString(), projected.getJSONObject("provenance").toString())
        val display = provenanceDisplayText(provenance)
        assertTrue(display.contains("Estimated vintage: Unknown"))
        assertTrue(display.contains("RealName: Randy"))
        assertTrue(display.contains("ROG.HOG/MISSION.AUT"))
        assertTrue(display.contains("1998 - 2017"))
    }

    @Test
    fun archiveStagingRetainsMatchingProvenanceText() {
        val stage = File("build/test-provenance/chronolos").absoluteFile
        val fixture = File(stage.parentFile, "chron10b.zip")
        writeArchive(
            fixture,
            1999,
            1,
            29,
            mapOf(
                "chron10b.mn2" to "name=Fixture\n".toByteArray(),
                "chron10b.hog" to "DHF".toByteArray(),
                "chron10b.txt" to "Version: 1.0b\n".toByteArray(),
            ),
        )
        val dates = LevelMetadataAnalyzer.stageArchiveEntries(fixture, listOf("chron10b.mn2", "chron10b.hog"), stage)
        assertTrue(File(stage, "chron10b.txt").readText().contains("Version"))
        assertTrue(dates.any { it.file == "chron10b.txt" && it.modified == "1999-01-29" })
    }

    private fun writeArchive(
        file: File,
        year: Int,
        month: Int,
        day: Int,
        entries: Map<String, ByteArray>,
    ) {
        requireNotNull(file.parentFile).mkdirs()
        ZipOutputStream(file.outputStream()).use { zip ->
            entries.forEach { (name, bytes) ->
                zip.putNextEntry(
                    ZipEntry(name).apply {
                        time =
                            GregorianCalendar(year, month - 1, day, 12, 0).timeInMillis
                    },
                )
                zip.write(bytes)
                zip.closeEntry()
            }
        }
    }
}
