package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class MissionLaunchPublicationTest {
    @get:Rule val temporary = TemporaryFolder()

    @Test
    fun discoveryHasOnlyDescriptorsAndEachContextHasItsOwnBank() {
        val source = temporary.newFolder("source")
        val game = temporary.newFolder("game")
        val keys = listOf("first", "second").map { MissionLaunchKey(it, "missions/same.mn2", "d2") }
        val packages = keys.map { key ->
            val descriptor = File(source, "${key.owner}.mn2").apply { writeText("name = ${key.owner}\nnum_levels = 1\nfirst.rl2\n") }
            val bank = File(source, "${key.owner}.s22").apply { writeText("bank of ${key.owner}") }
            MissionLaunchPackage(key.owner, "revision", listOf(MissionLaunchEntry(key, key.owner)), listOf(
                MissionLaunchResource(descriptor, key.descriptor, missionLaunchHash(descriptor.readText()), setOf(key)),
                MissionLaunchResource(bank, "descent2.s22", missionLaunchHash(bank.readText()), emptySet()),
            ))
        }
        val catalog = MissionLaunchCatalog(packages)
        val publication = catalog.publish(game, "global-v1")
        val entries = JSONObject(publication.manifest).getJSONArray("entries")
        assertEquals(2, entries.length())
        assertEquals(2, publication.discoveryDir.walkTopDown().filter { it.isFile }.count())
        assertTrue(publication.discoveryDir.walkTopDown().filter { it.isFile }.all { it.extension == "mn2" })
        for (index in keys.indices) {
            val entry = entries.getJSONObject(index)
            assertTrue(File(publication.discoveryDir, entry.getString("alias")).isFile)
            val context = File(entry.getJSONArray("mounts").getString(0))
            assertEquals("bank of ${keys[index].owner}", File(context, "descent2.s22").readText())
        }
        assertEquals(publication, catalog.publish(game, "global-v1"))
        val missing = File(entries.getJSONObject(0).getJSONArray("mounts").getString(0), "descent2.s22")
        assertTrue(missing.delete())
        assertEquals(publication, catalog.publish(game, "global-v1"))
        assertEquals("bank of first", missing.readText())
        val changed = catalog.publish(game, "global-v2")
        assertNotEquals(publication.discoveryDir, changed.discoveryDir)
        assertTrue(publication.discoveryDir.isDirectory)
        assertTrue(catalog.resourcesFor(null).isEmpty())
    }
}
