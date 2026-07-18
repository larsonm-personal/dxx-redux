package com.dxxredux.app

import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionZipExtractionStoreTest {
    @Test
    fun freshRecordRejectsSameSizeArchiveWithChangedModificationTime() {
        val filesDir = File("build/test-mission-zip-extraction/freshness").absoluteFile
        filesDir.deleteRecursively()
        val modsDir = File(filesDir, "mods").apply { mkdirs() }
        val archive = File(modsDir, "preview.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("preview.mn2"))
            zip.write(
                """
                name = Preview
                type = normal
                num_levels = 1
                preview.rl2
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("preview.hog"))
            zip.write(byteArrayOf('D'.code.toByte(), 'H'.code.toByte(), 'F'.code.toByte()))
            zip.closeEntry()
        }
        val scan = requireNotNull(MissionZip.inspect(archive))
        val store = MissionZipExtractionStore(filesDir)
        val record = store.ensureExtracted(archive.name, archive, scan)

        assertNotNull(store.freshRecord(archive.name, archive))
        val changedTime = record.ownerLastModifiedMs + 2_000L
        check(archive.setLastModified(changedTime))
        assertNull(store.freshRecord(archive.name, archive))
    }
}
