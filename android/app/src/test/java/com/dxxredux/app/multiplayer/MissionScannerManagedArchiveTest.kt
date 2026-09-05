package com.dxxredux.app.multiplayer

import com.dxxredux.app.ModManager
import com.dxxredux.app.FileSetContentManager
import com.dxxredux.app.MissionDistributionPolicy
import com.dxxredux.app.MissionDownloadPolicy
import com.dxxredux.app.MissionZip
import com.dxxredux.app.D2_FILES
import com.dxxredux.app.ALL_GAME_FILENAMES
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
    fun managedVertigoIsAnEnabledMissionAndNotABaseAssetOrDownload() {
        assertFalse(D2_FILES.any { it.filename == "d2x.hog" })
        assertFalse("d2x.hog" in ALL_GAME_FILENAMES)
        val filesDir = temporaryFolder.newFolder("vertigo-files")
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        File(setDir, "d2x.mn2").writeText("zname = Descent 2: Vertigo\nnum_levels = 1\nd2xlvl01.rl2\n")
        File(setDir, "D2X.HOG").writeBytes(hogBytes("d2xlvl01.rl2", byteArrayOf(1)))
        val manager = FileSetContentManager(setDir)
        val entry = manager.reconcile().entries.single()
        val mission = MissionScanner.scan(filesDir, setDir, "d2", "coop").single { it.filename == "d2x" }
        assertFalse(mission.isBuiltin)
        assertEquals(MissionDownloadPolicy.PROPRIETARY, mission.downloadPolicy)
        assertFalse(MissionScanner.requirement("d2", mission, true).offerAvailable)
        manager.setEnabled(entry.id, false)
        assertFalse(MissionScanner.scan(filesDir, setDir, "d2", "coop").any { it.filename == "d2x" })
    }

    @Test
    fun vertigoWrapperIsPlayableButCannotBeOfferedEvenWithForgedRequirement() {
        val filesDir = temporaryFolder.newFolder("vertigo-wrapper")
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        val manager = ModManager(filesDir, setDir = setDir)
        val archive = createMissionZip("d2x", "normal")
        requireNotNull(manager.importMissionZipFile(archive, "official-expansion.zip"))
        val mission = MissionScanner.scan(filesDir, setDir, "d2", "coop").single { it.filename == "d2x" }
        assertFalse(mission.transferable)
        val requirement = MissionScanner.requirement("d2", mission, true)
        assertTrue(requirement.isValid)
        assertFalse(requirement.offerAvailable)
        assertFalse(MissionTransferService.hostArchiveAllowed(requirement.copy(missionKey = "custom", offerAvailable = true), archive))
    }

    @Test
    fun wholeWrapperPolicyFindsVertigoInsideRenamedHogsAndMixedMissionPacks() {
        for (embeddedName in listOf("D2X.HAM", "d2xlvl01.rl2", "d2xlvls3.rl2")) {
            val archive = temporaryFolder.newFile("pack-${embeddedName}.zip")
            ZipOutputStream(archive.outputStream()).use { zip ->
                zip.putNextEntry(ZipEntry("custom.mn2"))
                zip.write("name = Custom\nnum_levels = 1\ncustom.rl2\n".toByteArray())
                zip.closeEntry()
                zip.putNextEntry(ZipEntry("custom.hog"))
                zip.write(hogBytes("custom.rl2", byteArrayOf(1)))
                zip.closeEntry()
                zip.putNextEntry(ZipEntry("unused/renamed.hog"))
                zip.write(hogBytes(embeddedName, byteArrayOf(2)))
                zip.closeEntry()
            }
            val scan = requireNotNull(MissionZip.inspect(archive))
            assertEquals(MissionDownloadPolicy.PROPRIETARY, MissionDistributionPolicy.archivePolicy(scan))
            val requirement = MissionRequirement(
                revision = "test", game = "d2", missionKey = "custom", displayName = "Custom",
                kind = MissionRequirement.KIND_WRAPPER, wrapperFilename = archive.name,
                sizeBytes = archive.length(), sha256 = "ab".repeat(32), offerAvailable = true,
            )
            assertFalse(MissionTransferService.hostArchiveAllowed(requirement, archive))
        }
    }

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
        assertTrue(MissionTransferService.hostArchiveAllowed(requirement, manager.modFile(imported.filename)))

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
    fun opaqueNestedArchiveCannotBeOffered() {
        val archive = temporaryFolder.newFile("nested.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("custom.mn2"))
            zip.write("name = Custom\nnum_levels = 1\ncustom.rl2\n".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("custom.hog"))
            zip.write(hogBytes("custom.rl2", byteArrayOf(1)))
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("bundled.zip"))
            zip.write(byteArrayOf(1, 2, 3))
            zip.closeEntry()
        }
        assertEquals(MissionDownloadPolicy.UNVERIFIED, MissionDistributionPolicy.archivePolicy(requireNotNull(MissionZip.inspect(archive))))
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
