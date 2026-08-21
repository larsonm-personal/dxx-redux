package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class FileSetContentCatalogTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun classifiesOnlyRootKnownNamesAsBaseFiles() {
        assertEquals(FileSetFileClass.BASE, FileSetContentCatalog.classify("descent2.hog"))
        assertEquals(FileSetFileClass.BASE, FileSetContentCatalog.classify("D2X.HOG"))
        assertEquals(FileSetFileClass.MANAGED_CONTENT, FileSetContentCatalog.classify("panic.hog"))
        assertEquals(FileSetFileClass.MANAGED_CONTENT, FileSetContentCatalog.classify("missions/descent2.hog"))
        assertEquals(FileSetFileClass.LAUNCHER_STATE, FileSetContentCatalog.classify("assets.json"))
        assertEquals(FileSetFileClass.LAUNCHER_STATE, FileSetContentCatalog.classify(".content/item/payload/a.hog"))
        assertEquals(
            FileSetFileClass.LAUNCHER_STATE,
            FileSetContentCatalog.classify(".assets.json.12345678-1234-1234-1234-123456789abc.tmp"),
        )
        assertEquals(FileSetFileClass.MANAGED_CONTENT, FileSetContentCatalog.classify("notes.tmp"))
        assertEquals(FileSetFileClass.PLAYER_STATE, FileSetContentCatalog.classify("Players/test.plr"))
        assertEquals(FileSetFileClass.PLAYER_STATE, FileSetContentCatalog.classify("slot.sg4"))
    }

    @Test
    fun groupsDescriptorDependenciesAndOwnsEveryManagedFileOnce() {
        val setDir = temporaryFolder.newFolder("default")
        File(setDir, "descent2.hog").writeText("base")
        File(setDir, "panic.mn2").writeText(
            "name = Vertigo Series\nbriefing = panic.tex\nnum_levels = 1\npanic01.rl2\n",
        )
        File(setDir, "panic.hog").writeText("mission")
        File(setDir, "panic.tex").writeText("briefing")
        File(setDir, "panic01.rl2").writeText("level")
        File(setDir, "readme.weird").writeText("unknown")
        File(setDir, "custom.ogg").writeText("music")
        File(setDir, "assets.json").writeText("[]")
        File(setDir, "Players").mkdirs()
        File(setDir, "Players/pilot.plr").writeText("pilot")

        val entries = FileSetContentCatalog.scan(setDir)
        val mission = entries.single { it.kind == FileSetContentCatalog.KIND_LOOSE_MISSION }
        val ownedNames = mission.files.map { it.name.lowercase() }.toSet()

        assertEquals("Vertigo Series", mission.displayName)
        assertEquals("d2", mission.game)
        assertEquals(setOf("panic.mn2", "panic.hog", "panic.tex", "panic01.rl2"), ownedNames)
        assertEquals(
            setOf("missions/panic.mn2", "missions/panic.hog", "missions/panic.tex", "missions/panic01.rl2"),
            mission.virtualPaths.toSet(),
        )
        assertEquals(
            FileSetContentCatalog.KIND_MUSIC,
            entries.single { it.files.size == 1 && it.files.single().name == "custom.ogg" }.kind,
        )
        assertEquals(
            FileSetContentCatalog.KIND_OTHER,
            entries.single { it.files.size == 1 && it.files.single().name == "readme.weird" }.kind,
        )
        assertFalse(entries.any { entry -> entry.files.any { it.name == "descent2.hog" } })
        assertFalse(entries.any { entry -> entry.files.any { it.name == "assets.json" || it.name == "pilot.plr" } })

        val allOwned = entries.flatMap { it.files }
        assertEquals(allOwned.size, allOwned.distinctBy { it.canonicalPath }.size)
        assertEquals(6, allOwned.size)
    }

    @Test
    fun sharedDependencyCreatesOneOwnerAndStableIdentity() {
        val setDir = temporaryFolder.newFolder("shared")
        File(setDir, "one.mn2").writeText("name = One\nbriefing = shared.tex\nnum_levels = 1\none.rl2\n")
        File(setDir, "two.mn2").writeText("name = Two\nbriefing = shared.tex\nnum_levels = 1\ntwo.rl2\n")
        File(setDir, "one.rl2").writeText("one")
        File(setDir, "two.rl2").writeText("two")
        File(setDir, "shared.tex").writeText("shared")

        val first = FileSetContentCatalog.scan(setDir).single()
        val second = FileSetContentCatalog.scan(setDir).single()

        assertEquals(5, first.files.size)
        assertEquals(first.id, second.id)
        File(setDir, "unrelated.hog").writeText("other")
        assertEquals(first.id, FileSetContentCatalog.scan(setDir).single { it.displayName == first.displayName }.id)
    }

    @Test
    fun malformedDescriptorAndArchiveRemainVisibleTogether() {
        val setDir = temporaryFolder.newFolder("malformed")
        File(setDir, "broken.msn").writeText("name = Broken\nnum_levels = nope\n")
        File(setDir, "BROKEN.HOG").writeText("payload")

        val entry = FileSetContentCatalog.scan(setDir).single()

        assertEquals(FileSetContentCatalog.KIND_LOOSE_MISSION, entry.kind)
        assertTrue(entry.problem?.isNotBlank() == true)
        assertEquals(setOf("broken.msn", "broken.hog"), entry.files.map { it.name.lowercase() }.toSet())
    }

    @Test
    fun cueOwnsEveryReferencedBinAsOneMusicEntry() {
        val setDir = temporaryFolder.newFolder("cue")
        File(setDir, "disc.cue").writeText(
            "FILE \"disc one.bin\" BINARY\n  TRACK 01 MODE1/2352\nFILE disc-two.bin BINARY\n",
        )
        File(setDir, "disc one.bin").writeText("one")
        File(setDir, "disc-two.bin").writeText("two")

        val entry = FileSetContentCatalog.scan(setDir).single()

        assertEquals(FileSetContentCatalog.KIND_MUSIC, entry.kind)
        assertEquals(setOf("disc.cue", "disc one.bin", "disc-two.bin"), entry.files.map { it.name }.toSet())
    }
}
