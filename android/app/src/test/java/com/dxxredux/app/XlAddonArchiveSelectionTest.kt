package com.dxxredux.app

import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry
import org.apache.commons.compress.archivers.sevenz.SevenZFile
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.File
import java.security.MessageDigest

class XlAddonArchiveSelectionTest {
    @Test
    fun selectionNormalizesSeparatorsAndRejectsCollisionsBeforeReading() {
        fun entry(name: String) = SevenZArchiveEntry().apply { this.name = name }
        val camera = entry("D2X-XL\\data\\D2X-XL.HOG")
        val vertigo = entry("data/d2x.ham")
        assertEquals(
            listOf(camera, vertigo),
            selectSevenZGameEntries(listOf(camera, vertigo, entry("hoard.ham")), XL_ADDON_FILENAMES),
        )
        assertThrows(IllegalArgumentException::class.java) {
            selectSevenZGameEntries(listOf(camera, entry("other/d2x-xl.hog")), XL_ADDON_FILENAMES)
        }
        assertTrue(D2_FILES.filter { it.filename in XL_ADDON_FILENAMES }.all { !it.required })
    }

    @Test
    fun suppliedDistributionReadsOnlyCameraHogThroughRandomAccess() {
        val source = System.getenv("DXX_XL_DISTRIBUTION")?.let(::File)
        assumeTrue("Optional user-supplied distribution", source?.isFile == true)
        SevenZFile
            .builder()
            .setFile(
                source!!,
            ).setMaxMemoryLimitKiB(ExtractionLimits.MAX_SEVEN_Z_DECODER_MEMORY_KIB)
            .get()
            .use { archive ->
                val selected = selectSevenZGameEntries(archive.entries, XL_ADDON_FILENAMES)
                assertEquals(listOf("d2x-xl.hog"), selected.map { archiveGameFilename(it.name) })
                val digest = MessageDigest.getInstance("SHA-256")
                var bytes = 0L
                archive.getInputStream(selected.single()).use { input ->
                    val buffer = ByteArray(65536)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        bytes += count
                        digest.update(buffer, 0, count)
                    }
                }
                assertEquals(3815963L, bytes)
                assertEquals(
                    "e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6",
                    digest.digest().joinToString("") {
                        "%02x".format(it.toInt() and 255)
                    },
                )
            }
    }
}
