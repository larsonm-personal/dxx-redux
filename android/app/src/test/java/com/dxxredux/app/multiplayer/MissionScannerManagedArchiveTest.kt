package com.dxxredux.app.multiplayer

import com.dxxredux.app.ModManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionScannerManagedArchiveTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun enabledManagedMissionUsesPersistedWrapperIdentityInHostCatalog() {
        val filesDir = temporaryFolder.newFolder("files")
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        val manager = ModManager(filesDir, setDir = setDir)
        val imported = requireNotNull(manager.importMissionZipFile(createMissionZip("castaway", "normal"), "castaway_redux.zip"))

        val catalog = MissionScanner.scan(filesDir, setDir, "d2", "coop")
        val mission = catalog.single { it.filename == "castaway" }
        val requirement = MissionScanner.requirement("d2", mission, offerDownload = true)

        assertEquals("PTMC Castaway Redux", mission.displayName)
        assertEquals(imported.filename, mission.wrapperFilename)
        assertNotNull(mission.archiveSha256)
        assertTrue(mission.archiveSizeBytes!! > 0L)
        assertTrue(requirement.isValid)
        assertTrue(requirement.offerAvailable)

        val persisted = ModManager(filesDir, setDir = setDir).listMods().single()
        assertEquals(mission.archiveSha256, persisted.archiveSha256)
        assertFalse(persisted.archiveChunkSha256.isEmpty())
    }

    @Test
    fun coopExcludesAnarchyOnlyManagedMissionButAnarchyIncludesIt() {
        val filesDir = temporaryFolder.newFolder("anarchy-files")
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        val manager = ModManager(filesDir, setDir = setDir)
        requireNotNull(manager.importMissionZipFile(createMissionZip("arena", "anarchy"), "arena.zip"))

        assertFalse(MissionScanner.scan(filesDir, setDir, "d2", "coop").any { it.filename == "arena" })
        assertTrue(MissionScanner.scan(filesDir, setDir, "d2", "anarchy").any { it.filename == "arena" })
    }

    @Test
    fun sameSizeWrapperMutationInvalidatesPersistedIdentity() {
        val filesDir = temporaryFolder.newFolder("mutation-files")
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        val manager = ModManager(filesDir, setDir = setDir)
        val imported = requireNotNull(manager.importMissionZipFile(createMissionZip("mutated", "normal"), "mutated.zip"))
        val before = requireNotNull(manager.ensureMissionContentIdentity(imported.filename))
        val wrapper = manager.modFile(imported.filename)
        val bytes = wrapper.readBytes()
        bytes[bytes.lastIndex] = (bytes.last() + 1).toByte()
        wrapper.writeBytes(bytes)
        assertTrue(wrapper.setLastModified(imported.archiveModifiedAtMs + 2_000L))

        val after = requireNotNull(manager.ensureMissionContentIdentity(imported.filename))

        assertEquals(before.sizeBytes, after.sizeBytes)
        assertFalse(before.sha256 == after.sha256)
    }

    private fun createMissionZip(
        key: String,
        type: String,
    ): File {
        val archive = temporaryFolder.newFile("$key.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("$key.hog"))
            zip.write(hogBytes("$key.rl2", byteArrayOf(1)))
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("$key.mn2"))
            zip.write(
                """
                name = ${if (key == "castaway") "PTMC Castaway Redux" else "Arena"}
                type = $type
                num_levels = 1
                $key.rl2
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()
        }
        return archive
    }

    private fun hogBytes(
        name: String,
        data: ByteArray,
    ): ByteArray {
        val header = ByteArray(13)
        name.toByteArray(Charsets.US_ASCII).copyInto(header, endIndex = minOf(name.length, header.size))
        val size = data.size
        return "DHF".toByteArray(Charsets.US_ASCII) +
            header +
            byteArrayOf(
                (size and 0xff).toByte(),
                ((size shr 8) and 0xff).toByte(),
                ((size shr 16) and 0xff).toByte(),
                ((size shr 24) and 0xff).toByte(),
            ) +
            data
    }
}
