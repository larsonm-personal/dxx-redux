package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File

class RobotNameCatalogTest {
    @Test
    fun bundledTablesCoverStockRobotRanges() {
        val d1 = RobotNameCatalog.parse(asset("robot_names_d1.json").readText(), 30)
        val d2 = RobotNameCatalog.parse(asset("robot_names_d2.json").readText(), 66)

        assertEquals((0 until 30).toList(), d1.map { it.number })
        assertEquals((0 until 66).toList(), d2.map { it.number })
    }

    @Test
    fun displayNameUsesFilledNameAndPreservesRobotNumber() {
        val entries =
            RobotNameCatalog.parse(
                """[{"number":0,"name":""},{"number":1,"name":"Lou Guard"}]""",
                2,
            )

        assertEquals("Robot 0", RobotNameCatalog.displayName(entries, 0, "Robot 0"))
        assertEquals("Lou Guard (Robot 1)", RobotNameCatalog.displayName(entries, 1, "Robot 1"))
        assertEquals("Robot 80", RobotNameCatalog.displayName(entries, 80, "Robot 80"))
    }

    private fun asset(name: String): File =
        listOf(
            File("src/main/assets/$name"),
            File("app/src/main/assets/$name"),
            File("android/app/src/main/assets/$name"),
        ).firstOrNull(File::isFile) ?: error("Could not find $name")
}
