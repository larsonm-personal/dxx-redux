package com.dxxredux.app

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class BaseGameAssetAuditTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun sharedBytesDoNotMisidentifyACompleteRelease() {
        val assets =
            listOf(
                BaseGameAsset("descent2.hog", "mac", 1, 0),
                BaseGameAsset("groupa.pig", "shared", 1, 0),
            )
        val packages = { _: String, hash: String ->
            if (hash == "mac") setOf("Mac") else setOf("PC", "Mac")
        }
        assertTrue(classifyBaseGameAssets("d2", assets, packages).reasons.isEmpty())
        assertTrue(
            classifyBaseGameAssets("d2", assets + BaseGameAsset("alien1.pig", "pc", 1, 0)) { _, hash ->
                when (hash) {
                    "mac" -> setOf("Mac")
                    "pc" -> setOf("PC")
                    else -> setOf("PC", "Mac")
                }
            }.reasons.isNotEmpty(),
        )
    }

    @Test
    fun publicationReplacesCaseVariantsAndCleanupPreservesUserContent() =
        runBlocking {
            val set = temporaryFolder.newFolder("default")
            val preserved =
                listOf(
                    "descent.hog",
                    "descent.pig",
                    "pilot.plr",
                    "pilot.sg0",
                    "config.json",
                    "music/song.ogg",
                    "missions/custom.hog",
                )
            preserved.forEach { name ->
                File(set, name).apply {
                    parentFile!!.mkdirs()
                    writeText("keep $name")
                }
            }
            File(set, "d2demo.hog").writeText("demo")
            File(set, "groupa.pig").writeText("Mac")
            val staging = temporaryFolder.newFolder("gog")
            File(staging, "GROUPA.PIG").writeText("PC")
            File(staging, "DESCENT2.HOG").writeText("retail")
            publishStagedArchiveFiles(staging, set)
            assertEquals(
                listOf("groupa.pig"),
                set.listFiles()!!.filter { it.name.equals("groupa.pig", true) }.map { it.name },
            )
            assertEquals("PC", File(set, "groupa.pig").readText())
            val manifest = AssetManifest(set)
            manifest.upsert("groupa.pig", "a".repeat(64), 2)
            manifest.upsert("descent.pig", "b".repeat(64), 16)
            SafManifest.forDir(set).write(
                listOf(
                    SafManifest.SafFileEntry("descent2.s11", "content://fixtures/sound", 50),
                    SafManifest.SafFileEntry("descent.rsrc", "content://fixtures/mac-d1", 100),
                ),
            )
            val audit = auditBaseGameAssets(set, "d2")
            assertTrue(audit.reasons.any { it.contains("Demo and retail") })
            clearBaseGameAssets(set, audit)
            assertTrue(auditBaseGameAssets(set, "d2").assets.isEmpty())
            preserved.forEach { assertEquals("keep $it", File(set, it).readText()) }
            assertEquals(listOf("descent.pig"), manifest.load().map { it.filename })
            assertEquals(listOf("descent.rsrc"), SafManifest.forDir(set).read().map { it.filename })
        }

    @Test
    fun cleanupRejectsChangesSinceReview() =
        runBlocking {
            val set = temporaryFolder.newFolder("changed")
            File(set, "groupa.pig").writeText("Mac")
            val reviewed = auditBaseGameAssets(set, "d2")
            File(set, "groupa.pig").writeText("GOG")
            try {
                clearBaseGameAssets(set, reviewed)
                fail("Changed assets must require a new review")
            } catch (_: IllegalStateException) {
                assertEquals("GOG", File(set, "groupa.pig").readText())
            }
        }

    @Test
    fun actualMacAndGogPigFilesReplaceEachOtherWithoutMasking() =
        runBlocking {
            val mac = File("../../game_data/CD images/d2 mac/data_tracks")
            val gog = File("../../temp/mac-edition-support/gog-d2/{app}")
            assumeTrue("Local extracted Mac and GOG fixtures required", mac.isDirectory && gog.isDirectory)
            val set = temporaryFolder.newFolder("real")
            val rows =
                JSONObject(
                    Jsonc.strip(File("src/main/assets/known_versions.jsonc").readText()),
                ).getJSONArray("versions")
            val versions = (0 until rows.length()).map { rows.getJSONObject(it) }
            val packages = { name: String, hash: String ->
                versions
                    .filter {
                        portableGameFilenameIdentity(it.getString("file")) == portableGameFilenameIdentity(name) &&
                            it.getString("sha256").equals(hash, true)
                    }.map { it.getString("version") }
                    .toSet()
            }
            for ((index, source) in listOf(mac, gog).withIndex()) {
                val staging = temporaryFolder.newFolder("real-stage-$index")
                source
                    .listFiles()!!
                    .filter {
                        it.isFile && portableGameFilenameIdentity(
                            it.name,
                        ) in baseGameAssetNames("d2") && !it.name.endsWith(".mvl", true)
                    }.forEach { it.copyTo(File(staging, it.name)) }
                publishStagedArchiveFiles(staging, set)
                val inventory = auditBaseGameAssets(set, "d2").assets
                assertTrue(
                    "A complete real edition must not be reported as mixed",
                    classifyBaseGameAssets("d2", inventory, packages).reasons.isEmpty(),
                )
            }
            for (name in listOf("groupa.pig", "alien1.pig", "alien2.pig", "fire.pig", "ice.pig", "water.pig")) {
                val installed = set.listFiles()!!.filter { it.name.equals(name, true) }
                assertEquals(listOf(name), installed.map { it.name })
                val original = gog.listFiles()!!.single { it.name.equals(name, true) }
                assertEquals(AssetManifest.computeSha256(original), AssetManifest.computeSha256(installed.single()))
            }
            File(mac, "groupa.pig").copyTo(File(set, "groupa.pig"), overwrite = true)
            assertTrue(
                "Mac textures with GOG definitions must be detected",
                classifyBaseGameAssets("d2", auditBaseGameAssets(set, "d2").assets, packages).reasons.isNotEmpty(),
            )
        }
}
