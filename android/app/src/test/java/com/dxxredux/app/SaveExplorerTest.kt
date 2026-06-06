package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class SaveExplorerTest {
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

    private fun slot(
        path: String,
        relativePath: String,
        saveKind: String,
        saveTimeUnixSeconds: Long,
        modifiedUnixSeconds: Long = saveTimeUnixSeconds,
        slot: Int,
    ) = SaveExplorerBridge.SaveExplorerSlot(
        path = path,
        relativePath = relativePath,
        game = "d2",
        scope = "single",
        pilot = "test",
        missionKey = "d2",
        saveKind = saveKind,
        saveTimeUnixSeconds = saveTimeUnixSeconds,
        callsign = "test",
        description = "AUTO SAVE",
        missionName = "Counterstrike!",
        levelNum = 1,
        levelName = "Lunar Outpost",
        levelSeconds = 120L,
        totalSeconds = 240L,
        difficultyChanged = false,
        difficultyMin = 0,
        difficultyMax = 0,
        musicType = 2,
        slot = slot,
        hasThumbnail = false,
        thumbnailWidth = 0,
        thumbnailHeight = 0,
        metadataBacked = true,
        loadable = true,
        orphan = false,
        orphanReason = "",
        sizeBytes = 1024L,
        modifiedUnixSeconds = modifiedUnixSeconds,
    )
}
