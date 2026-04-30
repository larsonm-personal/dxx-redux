package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class InputDemoManagerTest {
    @get:Rule
    val tmp = TemporaryFolder()

    @Test
    fun listStagedDemos_parsesHeaderAndOrdersNewestFirst() {
        val filesDir = tmp.newFolder("filesDir")
        val older = writeDemo(filesDir, "d1x-redux", "older.dximdemo", "descent", 1, 12)
        val newer = writeDemo(filesDir, "d2x-redux", "newer.dximdemo", "descent2", 2, 34)
        File(newer.parentFile, newer.name + InputDemoManager.INPUT_DEMO_RNG_TRACE_SUFFIX).writeText("{\"type\":\"meta\"}\n")
        File(newer.parentFile, "newer${InputDemoManager.CLASSIC_DEMO_EXTENSION}").writeText("classic demo\n")

        older.setLastModified(1_000L)
        newer.setLastModified(2_000L)

        val demos = InputDemoManager.listStagedDemos(filesDir)

        assertEquals(listOf("newer.dximdemo", "older.dximdemo"), demos.map { it.file.name })
        assertEquals("d2", demos.first().game)
        assertEquals("descent2", demos.first().mission)
        assertEquals(2, demos.first().level)
        assertEquals(34, demos.first().frameCount)
        assertEquals(3_000L, demos.first().durationMillis)
        assertEquals("newer.dximdemo.rngtrace.jsonl", demos.first().traceFile?.name)
        assertEquals("newer.dem", demos.first().classicDemoFile?.name)
        assertTrue(demos.all { it.headerReadable })
    }

    @Test
    fun installToActiveSet_copiesIntoActiveSetDemosAndDeletesStagedFiles() {
        val filesDir = tmp.newFolder("filesDir")
        val source = writeDemo(filesDir, "d1x-redux", "stage.dximdemo", "descent", 1, 8)
        val trace = File(source.parentFile, source.name + InputDemoManager.INPUT_DEMO_RNG_TRACE_SUFFIX)
        val classicDemo = File(source.parentFile, "stage${InputDemoManager.CLASSIC_DEMO_EXTENSION}")
        trace.writeText("{\"type\":\"meta\"}\n")
        classicDemo.writeText("classic demo\n")
        val sourceText = source.readText()
        val traceText = trace.readText()
        val classicDemoText = classicDemo.readText()
        val manager = FileSetManager(filesDir)
        val demo = InputDemoManager.listStagedDemos(filesDir).single()
        val activeSetDir = manager.getSetDir(FileSetManager.DEFAULT_SET)

        val dest = InputDemoManager.installToSet(demo, activeSetDir, "Boss Fight #1")
        val destTrace = File(activeSetDir, "demos/Boss_Fight_1.dximdemo${InputDemoManager.INPUT_DEMO_RNG_TRACE_SUFFIX}")
        val destClassicDemo = File(activeSetDir, "demos/Boss_Fight_1${InputDemoManager.CLASSIC_DEMO_EXTENSION}")

        assertFalse(source.exists())
        assertFalse(trace.exists())
        assertFalse(classicDemo.exists())
        assertTrue(dest.exists())
        assertTrue(destTrace.exists())
        assertTrue(destClassicDemo.exists())
        assertEquals(
            File(manager.getSetDir(FileSetManager.DEFAULT_SET), "demos/Boss_Fight_1.dximdemo").absolutePath,
            dest.absolutePath,
        )
        assertEquals(sourceText, dest.readText())
        assertEquals(traceText, destTrace.readText())
        assertEquals(classicDemoText, destClassicDemo.readText())
    }

    @Test
    fun deleteAllStagedDemos_removesFilesAcrossBothGames() {
        val filesDir = tmp.newFolder("filesDir")

        writeDemo(filesDir, "d1x-redux", "first.dximdemo", "descent", 1, 4)
        File(File(File(filesDir, "d1x-redux"), "input_demo_recordings/new"), "first${InputDemoManager.CLASSIC_DEMO_EXTENSION}").writeText("classic demo\n")
        writeDemo(filesDir, "d2x-redux", "second.dximdemo", "descent2", 2, 5)

        val deleted = InputDemoManager.deleteAllStagedDemos(filesDir)

        assertEquals(2, deleted)
        assertTrue(InputDemoManager.listStagedDemos(filesDir).isEmpty())
    }

    private fun writeDemo(
        filesDir: File,
        prefDir: String,
        name: String,
        mission: String,
        level: Int,
        frameCount: Int,
    ): File {
        val dir = File(File(filesDir, prefDir), "input_demo_recordings/new").also { it.mkdirs() }
        val file = File(dir, name)
        file.writeText(
            "{\"type\":\"header\",\"version\":1,\"game\":\"${if (prefDir.startsWith("d1")) "d1" else "d2"}\",\"mission\":\"$mission\",\"level\":$level,\"difficulty\":2,\"start_mode\":\"new_level\",\"rng_mode\":\"lcg_state\",\"frame_count\":$frameCount}\n" +
                "{\"type\":\"frame\",\"f\":0,\"ft\":65536,\"input\":{},\"rng\":{\"s\":1}}\n" +
                "{\"type\":\"frame\",\"f\":1,\"input\":{},\"rng\":{\"s\":2}}\n" +
                "{\"type\":\"frame\",\"f\":2,\"ft\":32768,\"input\":{},\"rng\":{\"s\":3}}\n" +
                "{\"type\":\"frame\",\"f\":3,\"input\":{},\"rng\":{\"s\":4}}\n" +
                "{\"type\":\"result\",\"result\":{\"v\":1,\"g\":\"d2\",\"m\":\"$mission\",\"l\":$level,\"d\":2,\"fr\":$frameCount}}\n",
        )
        return file
    }
}