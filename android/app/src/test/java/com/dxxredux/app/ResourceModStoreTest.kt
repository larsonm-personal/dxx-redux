package com.dxxredux.app

import org.apache.commons.compress.archivers.sevenz.SevenZFile
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.io.IOException

class ResourceModStoreTest {
    @get:Rule val temporary = TemporaryFolder()

    private fun source(
        path: String,
        content: String,
    ) = ResourceModStore.Source(path, content.toByteArray().size.toLong()) { content.byteInputStream() }

    @Test
    fun groupedModUpdatesPreserveToggleAndPublishOnlyEnabledOwners() {
        val files = temporary.newFolder("files")
        val manager = ModManager(files)

        fun add(
            name: String,
            text: String,
        ) = manager.importResourceMod(XL_RESOURCE_MOD) { destination ->
            ResourceModStore.update(XL_RESOURCE_MOD, destination, listOf(source(name, text)))
        }
        val first = add("d2x-xl.hog", "camera")
        assertTrue(first.enabled)
        manager.setEnabled(first.filename, false)
        val updated = add("d2x.ham", "vertigo")
        assertFalse(updated.enabled)
        assertEquals(first.order, updated.order)
        assertEquals(1, manager.listMods().size)
        assertEquals(ModManager.MOD_KIND_RESOURCES, updated.kind)
        val details = manager.getModDetails(updated, files)
        assertEquals(2, details.fileCount)
        assertTrue(details.problems.isEmpty())
        manager.writeEnabledModPaths("d2")
        val publication = files.resolve("d2x-redux/.mission_assets.json")
        assertEquals(0, JSONObject(publication.readText()).getJSONArray("installation_mounts").length())
        manager.setEnabled(first.filename, true)
        manager.writeEnabledModPaths("d2")
        val mounts = JSONObject(publication.readText()).getJSONArray("installation_mounts")
        assertEquals(1, mounts.length())
        assertTrue(mounts.getString(0).endsWith("xl-resources"))
        assertEquals(mounts.getString(0), files.resolve("d2x-redux/.active_mod_paths").readText())
        manager.writeEnabledModPaths("d1")
        val d1 = JSONObject(files.resolve("d1x-redux/.mission_assets.json").readText())
        assertEquals(0, d1.getJSONArray("installation_mounts").length())
    }

    @Test
    fun rawSoundsAndAlternateImagesStayInOneResourceSlot() {
        assertEquals("sounds/d2x-xl/lightning.wav", XL_RESOURCE_MOD.target("LIGHTNING.WAV"))
        assertNull(XL_RESOURCE_MOD.target("unrelated.wav"))
        assertTrue(isDirectoryImportCandidateName("smoke.png", emptySet()))
        assertTrue(isDirectoryImportCandidateName("lightning.wav", emptySet()))
        assertFalse(isDirectoryImportCandidateName("unrelated.png", emptySet()))
        assertThrows(IllegalArgumentException::class.java) {
            XL_RESOURCE_MOD.select(listOf("smoke.png", "smoke.tga"))
        }
        val pack = temporary.root.resolve("images")
        ResourceModStore.update(XL_RESOURCE_MOD, pack, listOf(source("smoke.tga", "old")))
        ResourceModStore.update(XL_RESOURCE_MOD, pack, listOf(source("smoke.png", "new")))
        assertFalse(pack.resolve("textures/d2x-xl/smoke.tga").exists())
        assertTrue(pack.resolve("textures/d2x-xl/smoke.png").isFile)
    }

    @Test
    fun suppliedDistributionExtractsOnlySelectedFiles() {
        val distribution = System.getenv("DXX_XL_DISTRIBUTION")?.let(::File)
        assumeTrue("Optional user-owned source", distribution?.isFile == true)
        val pack = temporary.root.resolve("xl-resources")
        val paths =
            SevenZFile
                .builder()
                .setFile(distribution!!)
                .setMaxMemoryLimitKiB(ExtractionLimits.MAX_SEVEN_Z_DECODER_MEMORY_KIB)
                .get()
                .use {
                    ResourceModStore.importSevenZ(XL_RESOURCE_MOD, pack, it)
                }
        assertTrue("d2x-xl.hog" in paths)
        assertFalse("d2x.ham" in paths)
        assertTrue("textures/d2x-xl/smokingfire.png" in paths)
        assertTrue("sounds/d2x-xl/lightning.wav" in paths)
        assertTrue(
            paths.all {
                it == "d2x-xl.hog" || it.startsWith("textures/d2x-xl/") ||
                    it.startsWith("sounds/d2x-xl/")
            },
        )
        assertTrue(pack.isDirectory)
        assertTrue(ResourceModStore.valid(pack))
        assertFalse(temporary.root.walkTopDown().any { it.extension == "dxa" })
    }

    @Test
    fun distributionSelectionExcludesRetailAndEnhancementsAndRejectsAmbiguity() {
        val paths =
            listOf(
                "D2X-XL\\data\\d2x-xl.hog",
                "D2X-XL/textures/d2x-xl/smoke.png",
                "D2X-XL/sounds/d2x-xl/lightning.wav",
                "D2X-XL/textures/hires/wall.png",
                "data/exit.ham",
                "extra.hog",
                "descent2.ham",
            )
        assertEquals(
            listOf("d2x-xl.hog", "sounds/d2x-xl/lightning.wav", "textures/d2x-xl/smoke.png"),
            XL_RESOURCE_MOD.select(paths).map { it.target }.sorted(),
        )
        assertThrows(IllegalArgumentException::class.java) {
            XL_RESOURCE_MOD.select(listOf("data/d2x.ham", "other/D2X.HAM"))
        }
        assertThrows(IllegalArgumentException::class.java) { XL_RESOURCE_MOD.target("../d2x.ham") }
    }

    @Test
    fun incrementalImportKeepsComponentsAndFailedUpdatePreservesDirectory() {
        val pack = temporary.root.resolve("xl-resources")
        ResourceModStore.update(
            XL_RESOURCE_MOD,
            pack,
            listOf(source("data/d2x-xl.hog", "camera"), source("smoke.png", "image")),
        )
        ResourceModStore.update(XL_RESOURCE_MOD, pack, listOf(source("d2x.ham", "vertigo")))
        assertEquals("camera", pack.resolve("d2x-xl.hog").readText())
        assertEquals("vertigo", pack.resolve("d2x.ham").readText())
        assertTrue(pack.resolve("textures/d2x-xl/smoke.png").isFile)
        val before = ResourceModStore.inventory(pack)
        assertThrows(IOException::class.java) {
            ResourceModStore.update(
                XL_RESOURCE_MOD,
                pack,
                listOf(
                    ResourceModStore.Source("d2x.ham", 12) {
                        throw IOException("interrupted provider")
                    },
                ),
            )
        }
        assertEquals(before, ResourceModStore.inventory(pack))
        assertEquals(listOf("xl-resources"), temporary.root.listFiles()!!.map { it.name })
    }
}
