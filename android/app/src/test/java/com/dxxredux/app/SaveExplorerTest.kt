package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class SaveExplorerTest {
    @Test
    fun modeLabelsPutMostRecentFirstAndDefault() {
        assertEquals(listOf("Choose Save", "Most Recent", "Save Set", "All Slots"), saveExplorerModeLabels())
        assertEquals("Choose Save", saveExplorerDefaultModeLabel())
    }

    @Test
    fun modeNavigationMovesAcrossTabsAndWraps() {
        assertEquals("Most Recent", saveExplorerModeLabelAfter("Choose Save", 1))
        assertEquals("All Slots", saveExplorerModeLabelAfter("Choose Save", -1))
        assertEquals("Choose Save", saveExplorerModeLabelAfter("All Slots", 1))
    }

    @Test
    fun saveSetRowsDefaultToMostRecentSavesBeforeEmptySlots() {
        val newest =
            slot(
                path = "/files/d2x-redux/Players/save_sets/single/test/d2/test.sg4",
                relativePath = "d2x-redux/Players/save_sets/single/test/d2/test.sg4",
                saveKind = "manual",
                saveTimeUnixSeconds = 1_700_000_300L,
                slot = 4,
            )
        val middle =
            newest.copy(
                path = "/files/d2x-redux/Players/save_sets/single/test/d2/test.sg1",
                relativePath = "d2x-redux/Players/save_sets/single/test/d2/test.sg1",
                saveTimeUnixSeconds = 1_700_000_200L,
                slot = 1,
            )
        val oldest =
            newest.copy(
                path = "/files/d2x-redux/Players/save_sets/single/test/d2/test.sg7",
                relativePath = "d2x-redux/Players/save_sets/single/test/d2/test.sg7",
                saveTimeUnixSeconds = 1_700_000_100L,
                slot = 7,
            )

        val rows =
            saveExplorerSaveSetRows(
                listOf(oldest, newest, middle),
                selectedGame = "d2",
                selectedScope = "single",
                selectedPilot = "test",
                selectedMission = "d2",
            )

        assertEquals(listOf(4, 1, 7, 0, 2, 3, 5, 6, 8, 9), rows.map { it.slotIndex })
        assertEquals(listOf(newest, middle, oldest), rows.take(3).map { it.slot })
        assertEquals(List(7) { null }, rows.drop(3).map { it.slot })
    }

    @Test
    fun saveSetRowsUseModifiedTimeWhenSaveTimeIsMissing() {
        val undatedNewer =
            slot(
                path = "/files/d2x-redux/Players/save_sets/single/test/d2/test.sg2",
                relativePath = "d2x-redux/Players/save_sets/single/test/d2/test.sg2",
                saveKind = "manual",
                saveTimeUnixSeconds = 0L,
                modifiedUnixSeconds = 1_700_000_400L,
                slot = 2,
            )
        val datedOlder =
            undatedNewer.copy(
                path = "/files/d2x-redux/Players/save_sets/single/test/d2/test.sg3",
                relativePath = "d2x-redux/Players/save_sets/single/test/d2/test.sg3",
                saveTimeUnixSeconds = 1_700_000_300L,
                modifiedUnixSeconds = 1_700_000_300L,
                slot = 3,
            )

        val rows =
            saveExplorerSaveSetRows(
                listOf(datedOlder, undatedNewer),
                selectedGame = "d2",
                selectedScope = "single",
                selectedPilot = "test",
                selectedMission = "d2",
            )

        assertEquals(listOf(2, 3), rows.take(2).map { it.slotIndex })
    }

    @Test
    fun recentSlotsCollapseAutosavesFromSameSaveMomentBeforeTakingTen() {
        val duplicateMinimize =
            slot(
                path = "/files/d2x-redux/Players/test.sg9",
                relativePath = "d2x-redux/Players/test.sg9",
                saveKind = "auto_minimize",
                saveTimeUnixSeconds = 1_700_000_100L,
                slot = 9,
            )
        val duplicateProgress =
            duplicateMinimize.copy(
                path = "/files/d2x-redux/Players/save_sets/single/test/d2/test.sg7",
                relativePath = "d2x-redux/Players/save_sets/single/test/d2/test.sg7",
                saveKind = "auto_progress",
                slot = 7,
            )
        val olderDistinct =
            slot(
                path = "/files/d2x-redux/Players/test.sg8",
                relativePath = "d2x-redux/Players/test.sg8",
                saveKind = "auto_exit",
                saveTimeUnixSeconds = 1_700_000_000L,
                slot = 8,
            )

        val recent = saveExplorerRecentSlots(listOf(olderDistinct, duplicateProgress, duplicateMinimize))

        assertEquals(listOf(duplicateMinimize, olderDistinct), recent)
    }

    @Test
    fun recentSlotsKeepManualSavesWithDifferentPaths() {
        val first =
            slot(
                path = "/files/d2x-redux/Players/test.sg0",
                relativePath = "d2x-redux/Players/test.sg0",
                saveKind = "manual",
                saveTimeUnixSeconds = 1_700_000_100L,
                slot = 0,
            )
        val second =
            first.copy(
                path = "/files/d2x-redux/Players/test.sg1",
                relativePath = "d2x-redux/Players/test.sg1",
                slot = 1,
            )

        assertEquals(listOf(first, second), saveExplorerRecentSlots(listOf(second, first)))
    }

    @Test
    fun metadataMissingLoadableSlotsCanStillLaunchByPath() {
        val loadableLegacy =
            slot(
                path = "/files/d2x-redux/Players/test.sg0",
                relativePath = "d2x-redux/Players/test.sg0",
                saveKind = "unknown",
                saveTimeUnixSeconds = 1_700_000_100L,
                slot = 0,
                metadataBacked = false,
                loadable = true,
                orphan = true,
                orphanReason = "metadata_footer_missing",
            )
        val blockedLegacy = loadableLegacy.copy(loadable = false)

        assertNotNull(loadableLegacy.toResumeCandidate())
        assertNull(blockedLegacy.toResumeCandidate())
    }

    @Test
    fun detailRowsIncludeMissionSetAndResumeMetadata() {
        val rows =
            saveExplorerDetailRows(
                slot(
                    path = "/files/d2x-redux/Players/save_sets/single/test/obsidian/test.sg8",
                    relativePath = "d2x-redux/Players/save_sets/single/test/obsidian/test.sg8",
                    saveKind = "auto_exit",
                    saveTimeUnixSeconds = 1_700_000_100L,
                    slot = 8,
                    missionKey = "obsidian",
                    missionName = "Obsidian",
                    levelSeconds = 121L,
                    totalSeconds = 3661L,
                ),
            ).associate { it.label to it.value }

        assertEquals("Descent 2 (d2)", rows["Game"])
        assertEquals("Obsidian (obsidian)", rows["Level Set"])
        assertEquals("Auto-save on exit", rows["Save Kind"])
        assertEquals("2:01", rows["Level Time"])
        assertEquals("1:01:01", rows["Total Time"])
    }

    @Test
    fun coopNotLoadableStatusExplainsMultiplayerRestore() {
        val coopSlot =
            slot(
                path = "/files/d2x-redux/Players/save_sets/coop/d2/coopsave.sg8",
                relativePath = "d2x-redux/Players/save_sets/coop/d2/coopsave.sg8",
                saveKind = "auto_progress",
                saveTimeUnixSeconds = 1_700_000_100L,
                slot = 8,
                scope = "coop",
                pilot = "coopsave",
                loadable = false,
                orphan = true,
                orphanReason = "not_loadable_from_launcher",
            )
        val rows = saveExplorerDetailRows(coopSlot).associate { it.label to it.value }

        assertEquals("Co-op save: use Multiplayer > Host LAN Game > Restore from save", rows["Status"])
        assertEquals("Co-op save: use Multiplayer > Host LAN Game > Restore from save", saveExplorerStatusMessage(coopSlot))
    }

    private fun slot(
        path: String,
        relativePath: String,
        saveKind: String,
        saveTimeUnixSeconds: Long,
        modifiedUnixSeconds: Long = saveTimeUnixSeconds,
        slot: Int,
        metadataBacked: Boolean = true,
        loadable: Boolean = true,
        orphan: Boolean = false,
        orphanReason: String = "",
        scope: String = "single",
        pilot: String = "test",
        missionKey: String = "d2",
        missionName: String = "Counterstrike!",
        levelSeconds: Long = 120L,
        totalSeconds: Long = 240L,
    ) = SaveExplorerBridge.SaveExplorerSlot(
        path = path,
        relativePath = relativePath,
        game = "d2",
        scope = scope,
        pilot = pilot,
        missionKey = missionKey,
        saveKind = saveKind,
        saveTimeUnixSeconds = saveTimeUnixSeconds,
        callsign = "test",
        description = "AUTO SAVE",
        missionName = missionName,
        levelNum = 1,
        levelName = "Lunar Outpost",
        levelSeconds = levelSeconds,
        totalSeconds = totalSeconds,
        difficultyChanged = false,
        difficultyMin = 0,
        difficultyMax = 0,
        musicType = 2,
        slot = slot,
        hasThumbnail = false,
        thumbnailWidth = 0,
        thumbnailHeight = 0,
        metadataBacked = metadataBacked,
        loadable = loadable,
        orphan = orphan,
        orphanReason = orphanReason,
        sizeBytes = 1024L,
        modifiedUnixSeconds = modifiedUnixSeconds,
    )
}
