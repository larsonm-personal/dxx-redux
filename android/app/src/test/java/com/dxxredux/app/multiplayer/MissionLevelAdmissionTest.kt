package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.RandomAccessFile

class MissionLevelAdmissionTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    private val mission =
        MissionScanner.MissionInfo(
            filename = "sample",
            displayName = "Sample Mission",
            levelCount = 3,
        )

    @Test
    fun `selection must resolve uniquely to the current scan`() {
        assertEquals(mission, resolveMissionSelection(listOf(mission), "SAMPLE"))
        assertNull(resolveMissionSelection(listOf(mission), "missing"))
        assertNull(resolveMissionSelection(listOf(mission, mission.copy(displayName = "Duplicate")), "sample"))
    }

    @Test
    fun `level must be inside a known nonempty mission range`() {
        assertTrue(selectedMissionLevelIsValid(mission, 1))
        assertTrue(selectedMissionLevelIsValid(mission, 3))
        assertFalse(selectedMissionLevelIsValid(mission, 0))
        assertFalse(selectedMissionLevelIsValid(mission, 4))
        assertFalse(selectedMissionLevelIsValid(mission, Int.MAX_VALUE))
        assertFalse(selectedMissionLevelIsValid(mission.copy(levelCount = 0), 1))
        assertFalse(selectedMissionLevelIsValid(null, 1))
        assertFalse(selectedMissionLevelIsValid(mission, null))
    }

    @Test
    fun `builtin ranges match full and demo mission variants`() {
        val fullDir = temporaryFolder.newFolder("full")
        assertEquals(24, MissionScanner.builtins(fullDir, "d2").first().levelCount)

        val demoDir = temporaryFolder.newFolder("demo")
        RandomAccessFile(demoDir.resolve("d2demo.hog"), "rw").use { it.setLength(2_292_566L) }
        val demo = MissionScanner.builtins(demoDir, "d2").first()
        assertEquals("d2demo", demo.filename)
        assertEquals(3, demo.levelCount)
    }

    @Test
    fun `full D1 host warns seven level demo client without disabling ready`() {
        val fullDir = temporaryFolder.newFolder("d1-full")
        RandomAccessFile(fullDir.resolve("descent.hog"), "rw").use { it.setLength(6_856_701L) }
        val demoDir = temporaryFolder.newFolder("d1-demo")
        RandomAccessFile(demoDir.resolve("descent.hog"), "rw").use { it.setLength(2_339_773L) }

        for (engine in listOf("d1", "d2")) {
            val host = MissionScanner.builtins(fullDir, engine).single { it.contentGame == "d1" }
            val client = MissionScanner.builtins(demoDir, engine).single { it.contentGame == "d1" }
            val requirement = MissionScanner.requirement(engine, host, offerDownload = true)
            assertTrue(requirement.isValid)
            assertFalse(requirement.offerAvailable)
            assertEquals(host.contentSizeBytes, requirement.sizeBytes)
            assertEquals(host.contentSha256, requirement.sha256)
            assertEquals(7, client.levelCount)
            assertEquals(host.filename, client.filename)
            assertFalse(MissionCompatibilityResolver.resolveContent(requirement, host).gameDataWarning)
            assertFalse(selectedMissionLevelIsValid(client, 10))
            val report = MissionCompatibilityResolver.resolveContent(requirement, client)
            assertTrue(report.gameDataWarning)
            assertEquals(MissionCompatibilityStatus.MATCH, report.status)
            assertTrue(report.validFor(requirement))
            assertTrue(report.status.userLabel(report).contains("game data differs"))
        }
    }

    @Test
    fun `same size and level count campaign mutation is detected by SHA256`() {
        val directory = temporaryFolder.newFolder("same-size-campaign")
        val hog = directory.resolve("descent.hog")
        hog.writeBytes(byteArrayOf(1, 2, 3, 4))
        for (engine in listOf("d1", "d2")) {
            val before = MissionScanner.builtins(directory, engine).single { it.contentGame == "d1" }
            val requirement = MissionScanner.requirement(engine, before, false)
            hog.writeBytes(hog.readBytes().also { it[0] = (it[0] + 1).toByte() })
            assertTrue(hog.setLastModified(hog.lastModified() + 2_000L))
            val after = MissionScanner.builtins(directory, engine).single { it.contentGame == "d1" }
            assertEquals(before.levelCount, after.levelCount)
            assertEquals(before.contentSizeBytes, after.contentSizeBytes)
            assertFalse(before.contentSha256 == after.contentSha256)
            assertFalse(requirement.revision == MissionScanner.requirement(engine, after, false).revision)
            assertTrue(MissionCompatibilityResolver.resolveContent(requirement, after).gameDataWarning)

            // Mission ZIPs use the same comparison before synchronization
            val wrapper = before.copy(isBuiltin = false, wrapperFilename = "mission.zip")
            val wrapperRequirement = MissionScanner.requirement(engine, wrapper, true)
            assertTrue(wrapperRequirement.isValid)
            assertEquals(
                MissionCompatibilityStatus.HASH_MISMATCH,
                MissionCompatibilityResolver.resolveContent(wrapperRequirement, after.copy(isBuiltin = false)).status,
            )
            assertEquals(
                MissionCompatibilityStatus.SIZE_MISMATCH,
                MissionCompatibilityResolver
                    .resolveContent(
                        wrapperRequirement,
                        wrapper.copy(contentSizeBytes = 5),
                    ).status,
            )
            assertEquals(
                MissionCompatibilityStatus.MISSING,
                MissionCompatibilityResolver
                    .resolveContent(
                        wrapperRequirement,
                        wrapper.copy(contentSha256 = null),
                    ).status,
            )
            assertEquals(
                MissionCompatibilityStatus.MATCH,
                MissionCompatibilityResolver.resolveContent(wrapperRequirement, wrapper).status,
            )
        }
    }

    @Test
    fun `D2 campaign identity follows the selected full or demo HOG`() {
        val directory = temporaryFolder.newFolder("d2-identity")
        val full = directory.resolve("descent2.hog").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val demo =
            directory.resolve("d2demo.hog").apply {
                RandomAccessFile(this, "rw").use { it.setLength(2_292_566L) }
            }
        val fullMission = MissionScanner.builtins(directory, "d2").first()
        assertEquals("d2", fullMission.filename)
        assertEquals(full.length(), fullMission.contentSizeBytes)
        assertTrue(MissionScanner.requirement("d2", fullMission, true).isValid)
        assertTrue(full.delete())
        val demoMission = MissionScanner.builtins(directory, "d2").first()
        assertEquals("d2demo", demoMission.filename)
        assertEquals(demo.length(), demoMission.contentSizeBytes)
        assertFalse(fullMission.contentSha256 == demoMission.contentSha256)
        assertTrue(MissionScanner.requirement("d2", demoMission, true).isValid)
    }
}
