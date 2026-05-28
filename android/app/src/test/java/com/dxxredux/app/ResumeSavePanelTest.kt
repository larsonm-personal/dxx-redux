package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ResumeSavePanelTest {
    @Test
    fun rgb6ThumbnailChannelsExpandToFullEightBitRange() {
        assertEquals(0, resumeSaveRgb6ChannelToRgb8(0))
        assertEquals(125, resumeSaveRgb6ChannelToRgb8(31))
        assertEquals(255, resumeSaveRgb6ChannelToRgb8(63))
        assertEquals(255, resumeSaveRgb6ChannelToRgb8(255))
    }

    @Test
    fun headerTextOrderPutsResumeTitleBeforeStopShowingButton() {
        assertEquals(listOf("Resume Recent Save", "Stop Showing This"), resumePanelHeaderTextOrder())
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
        val minimize = candidate("auto_minimize", slot = 9)

        val rows =
            resumeSaveChoiceRows(
                ResumeSaveBridge.ResumeSaveOptions(
                    latestOverall = latest,
                    highestProgress = highest,
                    lastExit = exit,
                    lastMinimize = minimize,
                ),
            )

        assertEquals(
            listOf("Highest Progress", "Last Exit Save", "Last Minimize Save"),
            rows.map { it.label },
        )
        assertEquals(listOf(highest, exit, minimize), rows.map { it.candidate })
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
                    lastMinimize = minimize,
                ),
            )

        assertEquals(listOf("Last Minimize Save"), rows.map { it.label })
        assertEquals(listOf(minimize), rows.map { it.candidate })
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
        slot = slot,
        hasThumbnail = hasThumbnail,
        thumbnailWidth = thumbnailWidth,
        thumbnailHeight = thumbnailHeight,
        metadataBacked = true,
        thumbnailRgb6 = thumbnailRgb6,
    )
}
