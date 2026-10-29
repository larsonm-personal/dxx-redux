package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeNotNull
import org.junit.Test
import java.io.File
import java.nio.file.Files
import java.security.MessageDigest

/** Optional unchanged distribution fixture; the public streaming controls require no mission data */
class ExtendedHogOriginalPackageTest {
    @Test
    fun importsOriginalPumoDistributionAndPreservesItsOwnedResources() {
        val path = System.getProperty("dxx.xlOriginalPackage")
        assumeNotNull(path)
        val source = File(requireNotNull(path))
        val expectedHash = "82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf"
        assertEquals(expectedHash, hash(source))
        val root = Files.createTempDirectory(File("build").toPath(), "pumo-original-").toFile()
        try {
            val scan = requireNotNull(MissionZip.inspect(source))
            assertEquals("[-Pumo Mines-]", scan.mission.displayName)
            assertEquals(14, scan.mission.levelNames.size)
            assertEquals(
                205,
                scan.constituents
                    .single { it.name == "pmines.hog" }
                    .archiveEntries!!
                    .size,
            )
            val manager = ModManager(root)
            val imported = requireNotNull(manager.importMissionZipFile(source, "PumoOriginal.7z"))
            assertEquals("extracted_bundle", imported.importMode)
            assertEquals(expectedHash, hash(File(root, "mods/PumoOriginal.7z")))
            val details = manager.getModDetails(imported, File(root, "set"))
            val record = requireNotNull(details.missionZipExtraction)
            val hog = File(record.rootPath).walkTopDown().single { it.isFile && it.name == "pmines.hog" }
            assertEquals(196505200L, hog.length())
            assertEquals("6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043", hash(hog))
            val metadata = requireNotNull(GameFileMetadata.summarizeLocalFile(hog))
            assertTrue(metadata.problems.joinToString(), metadata.problems.isEmpty())
            assertEquals(205, metadata.contents.size)
            assertEquals(14, metadata.contents.count { it.name.endsWith(".rl2") })
            assertEquals(
                scan.mission.levelNames.toSet(),
                metadata.contents
                    .filter {
                        it.name.endsWith(".rl2")
                    }.map { it.name }
                    .toSet(),
            )
            val music = requireNotNull(details.missionZipMusic)
            val playable = music.sources.flatMap { it.tracks }.filter { it.playable }
            assertTrue(playable.isNotEmpty())
            assertTrue(manager.hasEnabledMissionZipSoundtrack("d2"))
            val compressed = playable.first { it.kind == MissionZipMusic.KIND_COMPRESSED_AUDIO }
            val stage = MissionZipMusicStageManager(File(root, "preview-cache"))
            val preview = requireNotNull(stage.stageCompressedAudioTrack(music, compressed))
            assertEquals(compressed.sizeBytes, preview.length())
            val originalTrack = File(requireNotNull(compressed.sourceFilePath))
            assertEquals(hash(originalTrack), hash(preview))
            stage.cleanupOldFiles(maxAgeMs = 0, maxBytes = 0)
            assertTrue(!preview.exists())
            println(
                "Original Pumo package: 205 HOG members, 14 level payloads, ${playable.size} playable music entries; exact staged soundtrack and archive hashes preserved. Gameplay unqualified.",
            )
        } finally {
            root.deleteRecursively()
            assertEquals(expectedHash, hash(source))
        }
    }

    private fun hash(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                digest.update(buffer, 0, count)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }
}
