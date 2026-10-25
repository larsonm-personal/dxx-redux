package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.File
import java.nio.file.Files
import java.security.MessageDigest
import java.util.concurrent.TimeUnit
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class OriginalStaticTextureTest {
    @Test
    fun originalArchiveImportsPublishesAndResolvesStaticTexture() {
        val archive = System.getenv("DXX_XL_LEVELPACK")?.let(::File)
        val fixture = System.getenv("DXX_XL_TEXTURE_FILES")?.let(::File)
        val engine = System.getenv("DXX_XL_TEXTURE_ENGINE")?.let(::File)
        val retail = System.getenv("DXX_XL_RETAIL_DATA")?.let(::File)
        assumeTrue(
            "Optional pinned private archive/native engine fixture",
            archive?.isFile == true && fixture?.isDirectory == true && engine?.isFile == true &&
                retail?.isDirectory == true,
        )
        val root = Files.createTempDirectory("original-xl-texture").toFile()

        fun sha(file: File): String {
            val digest = MessageDigest.getInstance("SHA-256")
            file.inputStream().use { input ->
                val buffer = ByteArray(65536)
                while (true) {
                    val count = input.read(buffer)
                    if (count < 0) break
                    digest.update(buffer, 0, count)
                }
            }
            return digest.digest().joinToString("") { "%02x".format(it.toInt() and 255) }
        }
        try {
            assertEquals("b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719", sha(archive!!))
            // Exact original files, repacked into ZIP to isolate resource support from the
            // already-covered Android-only BCJ2 native extraction backend.
            val files =
                mapOf(
                    "missions/single/boilpnt.mn2" to "a9e35d1ca3e77693c1be2eab8e353a08ee108f9da0ac2b1e348b9107722433db",
                    "missions/single/boilpnt.hog" to "228dc2c6f4b8fbd4ef108966c43ee127007295d4215ac233b8e4ca40abae700c",
                    "mods/boilpnt/textures/ceil002.tga" to
                        "150c3603749fc87d877c9a0c97218431605b845098a89338a632252402635c3b",
                    "mods/boilpnt/textures/ceil002.bmp" to
                        "d801f9bb7858a868497a5136523407bfd1caafc01e168f4f3edf304454dee268",
                )
            val slice = root.resolve("original-slice.zip")
            ZipOutputStream(slice.outputStream()).use { zip ->
                for ((path, hash) in files) {
                    val file = fixture!!.resolve(File(path).name)
                    assertEquals(hash, sha(file))
                    zip.putNextEntry(ZipEntry(path))
                    file.inputStream().use { it.copyTo(zip) }
                    zip.closeEntry()
                }
            }
            val scan = requireNotNull(MissionZip.inspect(slice))
            val record =
                MissionZipExtractionStore(
                    root.resolve("store"),
                ).ensureExtracted("original-slice.zip", slice, scan)
            val pack = requireNotNull(missionLaunchPackage("original-slice.zip", scan, record, "d2", false))
            val catalog = MissionLaunchCatalog(listOf(pack))
            val selection = catalog.missions.single { it.key.descriptor.endsWith("/boilpnt.mn2", ignoreCase = true) }
            val resources = catalog.resourcesFor(selection.key)
            val original = resources.single { it.virtualPath == "mods/boilpnt/textures/ceil002.tga" }
            assertEquals("150c3603749fc87d877c9a0c97218431605b845098a89338a632252402635c3b", original.sha256)
            assertEquals("150c3603749fc87d877c9a0c97218431605b845098a89338a632252402635c3b", sha(original.source))
            assertTrue(resources.any { it.virtualPath == "mods/boilpnt/textures/ceil002.bmp" })
            val game = root.resolve("preview-user").apply { mkdirs() }
            val publication = catalog.publish(game, "original-texture-fixture")
            val manifest = JSONObject(publication.manifest)
            game.resolve(".mission_assets.json").writeText(publication.manifest)
            val entries = manifest.getJSONArray("entries")
            val entry =
                (0 until entries.length()).map(entries::getJSONObject).single {
                    it.getString("descriptor") ==
                        selection.key.descriptor
                }
            val log = root.resolve("native.log")
            val process =
                ProcessBuilder(
                    engine!!.absolutePath,
                    "-static-texture-test",
                    "original",
                    "-hogdir",
                    retail!!.absolutePath,
                    "-extra-dir",
                    root.absolutePath,
                    "-userdir",
                    game.absolutePath,
                    "-asset-alias",
                    entry.getString("alias"),
                    "-expected-texture",
                    original.source.absolutePath,
                    "-nosound",
                    "-nomusic",
                ).redirectErrorStream(true).redirectOutput(log).start()
            val exited = process.waitFor(60, TimeUnit.SECONDS)
            if (!exited) {
                process.destroyForcibly()
                process.waitFor(5, TimeUnit.SECONDS)
            }
            assertTrue("Native texture probe timed out", exited)
            assertEquals(log.readText(), 0, process.exitValue())
            assertTrue(log.readText().contains("exact bytes and stock teardown passed"))
        } finally {
            root.deleteRecursively()
        }
    }
}
