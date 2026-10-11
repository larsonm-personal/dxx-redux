package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class ModUsageTest {
    @Test
    fun historicalUseIsScopedToTheArchiveAndEngine() {
        val root = createTempDirectory("mod-usage").toFile()
        try {
            val report = File(root, "d2x-redux/mod_usage.json")
            report.parentFile!!.mkdirs()
            report.writeText(
                """{"schema":1,"sounds":[
                    {"source":"/sets/a/sounds.dxa","played":true},
                    {"source":"/sets/a/sounds.dxa","played":false},
                    {"source":"/sets/b/sounds.dxa","played":true}],
                    "presentation":[{"source":"/sets/a/sounds.dxa","kind":"font"}]}""",
            )
            assertEquals(
                listOf("Last D2 engine run" to "2 sounds loaded, 1 played; 0 images, 1 fonts loaded"),
                readModUsage(root, "/sets/a/sounds.dxa"),
            )
            assertEquals(
                listOf("Last D2 engine run" to "No sound, image or font use recorded"),
                readModUsage(root, "/sets/c/sounds.dxa"),
            )
            report.writeText(
                """{"schema":1,"presentation":[
                    {"source":"/sets/a/uud.dxa","kind":"metadata"},
                    {"source":"/sets/a/uud.dxa","kind":"bitmap_patch"},
                    {"source":"/sets/b/uud.dxa","kind":"metadata"}]}""",
            )
            assertEquals(
                listOf(
                    "Last D2 engine run" to
                        "0 sounds loaded, 0 played; 0 images, 0 fonts loaded; 2 data patches applied",
                ),
                readModUsage(root, "/sets/a/uud.dxa"),
            )
            report.writeText("partial write")
            assertTrue(readModUsage(root, "/sets/a/sounds.dxa").isEmpty())
        } finally {
            root.deleteRecursively()
        }
    }
}
