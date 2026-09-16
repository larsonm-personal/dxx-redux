package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class MissionLaunchCatalogTest {
    @Test
    fun switchingSiblingsSelectsOnlyTheirResourcesAndPackageSharedData() {
        val first = MissionLaunchKey("collection", "missions/first.mn2", "d2")
        val second = MissionLaunchKey("collection", "missions/second.mn2", "d2")
        fun resource(path: String, vararg keys: MissionLaunchKey) = MissionLaunchResource(File(path), path, "fixture", keys.toSet())
        val shared = resource("shared.dxa")
        val firstFiles = listOf(resource(first.descriptor, first), resource("missions/first.hog", first))
        val secondFiles = listOf(resource(second.descriptor, second), resource("missions/second.hog", second))
        val catalog =
            MissionLaunchCatalog(
                listOf(
                    MissionLaunchPackage(
                        "collection",
                        "revision-1",
                        listOf(MissionLaunchEntry(first, "Same title"), MissionLaunchEntry(second, "Same title")),
                        firstFiles + secondFiles + shared,
                    ),
                ),
            )
        repeat(20) {
            assertEquals(firstFiles + shared, catalog.resourcesFor(first))
            assertEquals(secondFiles + shared, catalog.resourcesFor(second))
            assertTrue(catalog.resourcesFor(null).isEmpty())
        }
        assertEquals(first, catalog.resolveLegacy("FIRST", "d2"))
        assertEquals(null, catalog.resolveLegacy("first", "d1"))
    }

    @Test
    fun sameVirtualResourceNameMayDifferBetweenSiblingsButCannotCollideWithinSelection() {
        val first = MissionLaunchKey("pack", "first.mn2", "d2")
        val second = MissionLaunchKey("pack", "second.mn2", "d2")
        val resources =
            listOf(
                MissionLaunchResource(File("first"), first.descriptor, "a", setOf(first)),
                MissionLaunchResource(File("second"), second.descriptor, "b", setOf(second)),
                MissionLaunchResource(File("first-bank"), "descent2.s22", "c", setOf(first)),
                MissionLaunchResource(File("second-bank"), "DESCENT2.S22", "d", setOf(second)),
            )
        val pack = MissionLaunchPackage("pack", "revision", listOf(MissionLaunchEntry(first, "First"), MissionLaunchEntry(second, "Second")), resources)
        val catalog = MissionLaunchCatalog(listOf(pack))
        assertEquals("c", catalog.resourcesFor(first).last().sha256)
        assertEquals("d", catalog.resourcesFor(second).last().sha256)
        val conflict = resources.last().copy(missions = emptySet())
        val invalid = MissionLaunchCatalog(listOf(pack.copy(resources = resources.dropLast(1) + conflict)))
        assertThrows(IllegalArgumentException::class.java) { invalid.resourcesFor(first) }
    }
}
