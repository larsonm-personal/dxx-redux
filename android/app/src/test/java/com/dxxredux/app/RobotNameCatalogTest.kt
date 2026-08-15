package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File

class RobotNameCatalogTest {
    @Test
    fun bundledTablesCoverStockRobotRanges() {
        val d1 = RobotNameCatalog.parse(asset("robot_names_d1.jsonc").readText(), 24)
        val d2 = RobotNameCatalog.parse(asset("robot_names_d2.jsonc").readText(), 66)

        assertEquals((0 until 24).toList(), d1.map { it.number })
        assertEquals((0 until 66).toList(), d2.map { it.number })
        assertEquals(
            listOf(
                "mech",
                "green claw",
                "spider",
                "josh",
                "violet",
                "cloak vulcan",
                "cloak mech",
                "brain",
                "onearm",
                "plasma",
                "toaster",
                "bird",
                "missile bird",
                "polyhedron",
                "baby spider",
                "mini boss",
                "super mech",
                "shareware boss",
                "cloak-green",
                "vulcan",
                "toad",
                "4-claw",
                "quad-laser",
                "super boss",
            ),
            d1.map { it.sourceCodeName },
        )
        assertEquals(d1.map { it.sourceCodeName }, d2.take(24).map { it.sourceCodeName })
    }

    @Test
    fun displayNameUsesFilledNameAndPreservesRobotNumber() {
        val entries =
            RobotNameCatalog.parse(
                """[{"number":0,"name":""},{"number":1,"name":"Lou Guard"},{"number":2,"name":"2"}]""",
                3,
            )

        assertEquals("Robot 0", RobotNameCatalog.displayName(entries, 0, "Robot 0"))
        assertEquals("Lou Guard (Robot 1)", RobotNameCatalog.displayName(entries, 1, "Robot 1"))
        assertEquals("Robot 2", RobotNameCatalog.displayName(entries, 2, "Robot 2"))
        assertEquals("Robot 80", RobotNameCatalog.displayName(entries, 80, "Robot 80"))
    }

    @Test
    fun parserAcceptsJsoncCommentsWithoutChangingStringContents() {
        val entries =
            RobotNameCatalog.parse(
                """
                [
                    // A line comment between JSON tokens
                    {"number":0,"name":"https://example.test/robot","source_code_name":"slash/*literal*/"},
                    /* A block comment
                       across two lines */
                    {"number":1,"name":"Quoted \"// literal\"","source_code_name":"backslash\\//literal"}
                ]
                """.trimIndent(),
                2,
            )

        assertEquals("https://example.test/robot", entries[0].name)
        assertEquals("slash/*literal*/", entries[0].sourceCodeName)
        assertEquals("Quoted \"// literal\"", entries[1].name)
        assertEquals("backslash\\//literal", entries[1].sourceCodeName)
    }

    private fun asset(name: String): File =
        listOf(
            File("src/main/assets/$name"),
            File("app/src/main/assets/$name"),
            File("android/app/src/main/assets/$name"),
        ).firstOrNull(File::isFile) ?: error("Could not find $name")
}
