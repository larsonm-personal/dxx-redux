package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.File

class ResumeSavePanelTest {
    @Test
    fun rgb6ThumbnailChannelsExpandToFullEightBitRange() {
        assertEquals(0, resumeSaveRgb6ChannelToRgb8(0))
        assertEquals(125, resumeSaveRgb6ChannelToRgb8(31))
        assertEquals(255, resumeSaveRgb6ChannelToRgb8(63))
        assertEquals(255, resumeSaveRgb6ChannelToRgb8(255))
    }

    @Test
    fun headerTextOrderPutsResumeTitleBeforeSaveExplorerButton() {
        assertEquals(listOf("Resume Recent Save", "Save Explorer"), resumePanelHeaderTextOrder())
    }

    @Test
    fun collapsedPanelUsesSaveExplorerLabel() {
        assertEquals("Save Explorer", resumePanelCollapsedLabel())
    }

    @Test
    fun launcherThumbnailDimensionsAreDoubleInGameThumbnailSize() {
        assertEquals(200, RESUME_SAVE_THUMBNAIL_WIDTH)
        assertEquals(100, RESUME_SAVE_THUMBNAIL_HEIGHT)
        assertEquals(60_000, RESUME_SAVE_THUMBNAIL_RGB6_BYTES)
    }

    @Test
    fun decodeThumbnailRejectsOldSizeMetadata() {
        val oldThumbnailRgb6 = ByteArray(100 * 50 * 3) { 1 }

        assertNull(
            decodeResumeSaveThumbnail(
                candidate(
                    "auto_exit",
                    slot = 8,
                    hasThumbnail = true,
                    thumbnailWidth = 100,
                    thumbnailHeight = 50,
                    thumbnailRgb6 = oldThumbnailRgb6,
                ),
            ),
        )
    }

    @Test
    fun choiceRowsUseAvailableCandidatesInDialogOrder() {
        val latest = candidate("auto_exit", slot = 8)
        val highest = candidate("auto_progress", slot = 7)
        val exit = candidate("auto_exit", slot = 8)
        val abort = candidate("auto_abort", slot = 6)
        val minimize = candidate("auto_minimize", slot = 9)

        val rows =
            resumeSaveChoiceRows(
                ResumeSaveBridge.ResumeSaveOptions(
                    latestOverall = latest,
                    highestProgress = highest,
                    lastExit = exit,
                    lastAbort = abort,
                    lastMinimize = minimize,
                ),
            )

        assertEquals(
            listOf("Highest Progress", "Last Exit Save", "Last Abort Save", "Last Minimize Save"),
            rows.map { it.label },
        )
        assertEquals(listOf(highest, exit, abort, minimize), rows.map { it.candidate })
    }

    @Test
    fun choiceRowsSkipMissingCandidates() {
        val minimize = candidate("auto_minimize", slot = 9)

        val rows =
            resumeSaveChoiceRows(
                ResumeSaveBridge.ResumeSaveOptions(
                    latestOverall = minimize,
                    highestProgress = null,
                    lastExit = null,
                    lastAbort = null,
                    lastMinimize = minimize,
                ),
            )

        assertEquals(listOf("Last Minimize Save"), rows.map { it.label })
        assertEquals(listOf(minimize), rows.map { it.candidate })
    }

    @Test
    fun resumeLaunchPathIsRelativeToActiveGameRoot() {
        val launchPath = resolveResumeSaveLaunchPath(File("/data/user/0/com.dxxredux.app/files"), candidate("auto_exit", 8))

        assertEquals("Players/test.sg8", launchPath)
    }

    @Test
    fun scopedResumeLaunchPathIsRelativeToActiveGameRoot() {
        val launchPath =
            resolveResumeSaveLaunchPath(
                File("/data/user/0/com.dxxredux.app/files"),
                candidate("auto_exit", 8).copy(
                    path = "/data/user/0/com.dxxredux.app/files/d2x-redux/Players/save_sets/single/test/d2/test.sg8",
                    relativePath = "d2x-redux/Players/save_sets/single/test/d2/test.sg8",
                ),
            )

        assertEquals("Players/save_sets/single/test/d2/test.sg8", launchPath)
    }

    @Test
    fun d1ResumeLaunchPathUsesD1Root() {
        val launchPath =
            resolveResumeSaveLaunchPath(
                File("/data/user/0/com.dxxredux.app/files"),
                candidate("auto_exit", 8).copy(
                    path = "/data/user/0/com.dxxredux.app/files/d1x-redux/Players/ace.sg8",
                    relativePath = "d1x-redux/Players/ace.sg8",
                    game = "d1",
                    callsign = "ace",
                ),
            )

        assertEquals("Players/ace.sg8", launchPath)
    }

    private fun candidate(
        saveKind: String,
        slot: Int,
        hasThumbnail: Boolean = false,
        thumbnailWidth: Int = 0,
        thumbnailHeight: Int = 0,
        thumbnailRgb6: ByteArray? = null,
    ) = ResumeSaveBridge.ResumeSaveCandidate(
        path = "/data/user/0/com.dxxredux.app/files/d2x-redux/Players/test.sg$slot",
        relativePath = "d2x-redux/Players/test.sg$slot",
        game = "d2",
        saveKind = saveKind,
        saveTimeUnixSeconds = 1_700_000_000L + slot,
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
        hasThumbnail = hasThumbnail,
        thumbnailWidth = thumbnailWidth,
        thumbnailHeight = thumbnailHeight,
        metadataBacked = true,
        thumbnailRgb6 = thumbnailRgb6,
    )
}
