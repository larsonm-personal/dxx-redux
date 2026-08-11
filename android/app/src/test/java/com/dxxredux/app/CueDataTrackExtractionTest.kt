package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class CueDataTrackExtractionTest {
    private fun track(
        number: Int,
        type: Int,
        fileIndex: Int,
        sectors: Int,
		sectorMode: Int = if (type == 1) DiscImportBridge.CUE_SECTOR_AUDIO else DiscImportBridge.CUE_SECTOR_MODE1_2352,
		sectorSize: Int = 2352,
		userDataOffset: Int = if (type == 1) 0 else 16,
	) =
		DiscImportBridge.CueTrack(
			number, type, fileIndex, 0, sectors, "Track $number",
			sectorMode, sectorSize, userDataOffset,
		)

    @Test
    fun extractsEveryDataTrackInCueOrderAndLaterBytesWin() {
        val root = createTempDirectory("cue-data-tracks").toFile()
        try {
            val setDir = File(root, "set")
            val tracks =
                listOf(
                    track(1, 0, 0, 10),
                    track(2, 1, 0, 5),
                    track(3, 0, 0, 20),
                    track(4, 0, 1, 30),
                )
            val visited = mutableListOf<Pair<Int, Int>>()

            val result =
                extractCueDataTracks(
                    setDir = setDir,
                    tracks = tracks,
                    imageCount = 2,
                    extractTrack = { cueTrack, outputDir, _ ->
                        visited += cueTrack.trackNum to cueTrack.fileIndex
                        File(outputDir, "track-${cueTrack.trackNum}.hog").writeText("unique")
                        File(outputDir, "overlay.hog").writeText("track-${cueTrack.trackNum}")
                        CueDataTrackAttempt(1, 0)
                    },
                    postProcess = { _, _ -> 0 },
                )

            assertTrue(result.succeeded)
            assertEquals(3, result.processedTracks)
            assertEquals(listOf(1 to 0, 3 to 0, 4 to 1), visited)
            assertEquals("track-4", File(setDir, "overlay.hog").readText())
            assertTrue(File(setDir, "track-1.hog").isFile)
            assertTrue(File(setDir, "track-3.hog").isFile)
            assertTrue(File(setDir, "track-4.hog").isFile)
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun laterFailureDoesNotPublishEarlierTrack() {
        val root = createTempDirectory("cue-data-failure").toFile()
        try {
            val setDir = File(root, "set").apply { mkdirs() }
            File(setDir, "existing.hog").writeText("original")
            val tracks = listOf(track(1, 0, 0, 10), track(2, 0, 1, 20))

            val result =
                extractCueDataTracks(
                    setDir = setDir,
                    tracks = tracks,
                    imageCount = 2,
                    extractTrack = { cueTrack, outputDir, _ ->
                        File(outputDir, "new.hog").writeText("track-${cueTrack.trackNum}")
                        if (cueTrack.trackNum == 2) {
                            CueDataTrackAttempt(-1, -1)
                        } else {
                            CueDataTrackAttempt(1, 0)
                        }
                    },
                    postProcess = { _, _ -> 0 },
                )

            assertFalse(result.succeeded)
            assertEquals(2, result.failedTrackNumber)
            assertEquals("original", File(setDir, "existing.hog").readText())
            assertFalse(File(setDir, "new.hog").exists())
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun nestedArchiveFailureDoesNotPublishPrimaryFiles() {
        val root = createTempDirectory("cue-sow-failure").toFile()
        try {
            val setDir = File(root, "set")
            val result =
                extractCueDataTracks(
                    setDir = setDir,
                    tracks = listOf(track(1, 0, 0, 10)),
                    imageCount = 1,
                    extractTrack = { _, outputDir, _ ->
                        File(outputDir, "primary.hog").writeText("complete")
                        CueDataTrackAttempt(1, 0)
                    },
                    postProcess = { outputDir, _ ->
                        File(outputDir, "partial.pig").writeText("partial")
                        -1
                    },
                )

            assertFalse(result.succeeded)
            assertFalse(File(setDir, "primary.hog").exists())
            assertFalse(File(setDir, "partial.pig").exists())
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun archivePublicationRollsBackEarlierReplacement() {
        val root = createTempDirectory("archive-publish-failure").toFile()
        try {
            val setDir = File(root, "set").apply { mkdirs() }
            val stagingDir = File(root, "staging").apply { mkdirs() }
            File(setDir, "first.hog").writeText("original")
            File(setDir, "blocked").writeText("not a directory")
            File(stagingDir, "first.hog").writeText("replacement")
            File(stagingDir, "blocked/second.hog").apply {
                parentFile?.mkdirs()
                writeText("second")
            }

            try {
                publishStagedArchiveFiles(stagingDir, setDir)
                fail("publication should fail when a destination parent is a file")
            } catch (_: IllegalStateException) {
                assertEquals("original", File(setDir, "first.hog").readText())
                assertFalse(File(setDir, "blocked/second.hog").exists())
            }
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun cancellationStopsBeforeNextTrackAndProgressCoversAllTracks() {
        val root = createTempDirectory("cue-data-cancel").toFile()
        try {
            val setDir = File(root, "set")
            val tracks = listOf(track(1, 0, 0, 10), track(2, 0, 0, 20))
            val progressValues = mutableListOf<Pair<Long, Long>>()
            var calls = 0
            val progress =
                object : DiscImportBridge.ExtractProgress {
                    override fun onProgress(
                        currentFile: String,
                        bytesDone: Long,
                        bytesTotal: Long,
                    ): Int {
                        progressValues += bytesDone to bytesTotal
                        return 1
                    }
                }

            val result =
                extractCueDataTracks(
                    setDir = setDir,
                    tracks = tracks,
                    imageCount = 1,
                    progress = progress,
                    extractTrack = { _, outputDir, trackProgress ->
                        calls++
                        File(outputDir, "partial.hog").writeText("partial")
                        trackProgress?.onProgress("partial.hog", 100, 100)
                        CueDataTrackAttempt(1, 0)
                    },
                    postProcess = { _, _ -> 0 },
                )

            assertTrue(result.cancelled)
            assertEquals(1, calls)
            assertEquals(23_520L to 70_560L, progressValues.single())
            assertFalse(File(setDir, "partial.hog").exists())
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun rejectsMalformedLaterTrackBeforeExtraction() {
        val root = createTempDirectory("cue-data-invalid").toFile()
        try {
            val tracks = listOf(track(1, 0, 0, 10), track(2, 0, 2, 20))
            var calls = 0
            val result =
                extractCueDataTracks(
                    setDir = File(root, "set"),
                    tracks = tracks,
                    imageCount = 2,
                    extractTrack = { _, _, _ ->
                        calls++
                        CueDataTrackAttempt(1, 0)
                    },
                    postProcess = { _, _ -> 0 },
                )

            assertFalse(result.succeeded)
            assertEquals(2, result.failedTrackNumber)
            assertEquals(0, calls)
            assertEquals(70_560L, cueDataTrackStorageBytes(tracks))
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun peakStorageIncludesOneNonseekableImage() {
        val tracks = listOf(track(1, 0, 0, 10), track(2, 1, 0, 20))

        assertEquals(723_520L, cueDataTrackPeakStorageBytes(tracks, 700_000L))
    }

    @Test
    fun peakStorageRejectsInvalidOrOverflowingStageSize() {
        val tracks = listOf(track(1, 0, 0, 10))

        try {
            cueDataTrackPeakStorageBytes(tracks, -1L)
            fail("negative staging size should fail")
        } catch (_: IllegalArgumentException) {
        }
        try {
            cueDataTrackPeakStorageBytes(tracks, Long.MAX_VALUE)
            fail("overflowing staging size should fail")
        } catch (_: ArithmeticException) {
        }
    }

	@Test
	fun storageAndProgressHonorPerTrackSectorSize() {
		val tracks =
			listOf(
				track(
					1, 0, 0, 10,
					DiscImportBridge.CUE_SECTOR_MODE1_2048, 2048, 0,
				),
				track(2, 0, 1, 20),
			)

		assertEquals(67_520L, cueDataTrackStorageBytes(tracks))
	}

	@Test
	fun rejectsInconsistentDataSectorGeometryBeforeExtraction() {
		val root = createTempDirectory("cue-data-geometry").toFile()
		try {
			val invalid =
				track(
					1, 0, 0, 10,
					DiscImportBridge.CUE_SECTOR_MODE1_2048, 2352, 0,
				)
			var calls = 0
			val result =
				extractCueDataTracks(
					setDir = File(root, "set"),
					tracks = listOf(invalid),
					imageCount = 1,
					extractTrack = { _, _, _ ->
						calls++
						CueDataTrackAttempt(1, 0)
					},
				)

			assertFalse(result.succeeded)
			assertEquals(0, calls)
		} finally {
			root.deleteRecursively()
		}
	}
}
