package com.dxxredux.app

import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry
import org.apache.commons.compress.archivers.sevenz.SevenZOutputFile
import org.junit.Assert.assertEquals
import org.junit.Test
import java.nio.file.Files
import java.time.Instant
import java.util.Date
import java.util.GregorianCalendar
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class ArchiveEntryDatesTest {
    @Test
    fun hostCatalogUsesTheSameDatesAsAndroidEntries() {
        val root = Files.createTempDirectory("provenance-dates").toFile()
        try {
            val zip = root.resolve("mission.zip")
            val entry =
                ZipEntry("ROG.HOG").apply {
                    time = GregorianCalendar(1998, 5, 26, 21, 21).timeInMillis
                }
            ZipOutputStream(zip.outputStream()).use {
                it.putNextEntry(entry)
                it.write("DHF".toByteArray())
                it.closeEntry()
            }
            val zipDate = ArchiveEntryDates.read(zip).getValue("ROG.HOG").jsonObject
            assertEquals("1998-06-26", zipDate.getValue("modified").jsonPrimitive.content)
            assertEquals(ArchiveEntryDates.zip(entry), zipDate.getValue("modified").jsonPrimitive.content)

            val sevenZ = root.resolve("mission.7z")
            val sevenEntry =
                SevenZArchiveEntry().apply {
                    name = "ROG.HOG"
                    lastModifiedDate = Date.from(Instant.parse("1998-06-26T23:59:59Z"))
                }
            SevenZOutputFile(sevenZ).use {
                it.putArchiveEntry(sevenEntry)
                it.write("DHF".toByteArray())
                it.closeArchiveEntry()
            }
            val sevenDate = ArchiveEntryDates.read(sevenZ).getValue("ROG.HOG").jsonObject
            assertEquals("1998-06-26", sevenDate.getValue("modified").jsonPrimitive.content)
            assertEquals(ArchiveEntryDates.sevenZ(sevenEntry), sevenDate.getValue("modified").jsonPrimitive.content)
        } finally {
            root.deleteRecursively()
        }
    }
}
