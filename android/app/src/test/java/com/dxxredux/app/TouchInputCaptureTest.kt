package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Collections

class TouchInputCaptureTest {
    @Test
    fun captureExportsBothProducersAndFlushesTailWithoutRestarting() {
        val batches = Collections.synchronizedList(mutableListOf<String>())
        val native = mutableListOf<Boolean>()
        val capture = TouchInputCapture({ 1000L }, { batches.add(it) }, { native.add(it) })
        capture.record { error("Disabled capture must not evaluate records") }
        capture.start { "viewport=2400,1080" }
        try {
            val threads =
                listOf("motion", "native_ms=1000 quantize").map { stage ->
                    Thread { repeat(100) { capture.record { "$stage value=$it" } } }.apply { start() }
                }
            threads.forEach { it.join() }
            capture.flush()
            capture.record { "release discard_pending=0.001,0.002" }
        } finally {
            capture.finish("activity_pause")
        }
        capture.start { error("A finished capture must not restart") }
        capture.record { error("Finished capture must not evaluate records") }
        capture.finish("activity_destroy")
        val lines =
            batches
                .joinToString("")
                .lineSequence()
                .filter { it.isNotEmpty() }
                .toList()
        assertEquals(203, lines.size)
        assertEquals((0L..202L).toList(), lines.map { Regex("seq=(\\d+)").find(it)!!.groupValues[1].toLong() })
        assertTrue(lines.first().contains("start version=1"))
        assertTrue(lines.last().contains("end reason=activity_pause records=201 dropped=0"))
        assertEquals(listOf(true, false), native)
        assertFalse(capture.active)
    }

    @Test
    fun expiryExcludesLateRecordsAndExportsWithoutAnotherTouch() {
        var time = 500L
        val batches = mutableListOf<String>()
        val capture = TouchInputCapture({ time }, { batches.add(it) }, {})
        capture.start { "test" }
        try {
            capture.record { "motion" }
            time += 30_000L
            capture.record { "too_late" }
            capture.flush()
            assertFalse(capture.active)
            assertTrue(batches.single().contains("end reason=duration records=1 dropped=0 elapsed_ms=30000"))
            assertFalse(batches.single().contains("too_late"))
        } finally {
            capture.finish("test_cleanup")
        }
    }

    @Test
    fun outputLimitStopsCaptureAndReportsLoss() {
        val batches = mutableListOf<String>()
        val capture = TouchInputCapture({ 0L }, { batches.add(it) }, {}, maxRecords = 2)
        capture.start { "test" }
        try {
            repeat(5) { capture.record { "sample=$it" } }
            capture.flush()
            assertFalse(capture.active)
            assertTrue(batches.single().contains("end reason=record_limit records=2 dropped=3"))
        } finally {
            capture.finish("test_cleanup")
        }
    }

    @Test
    fun fullBatchDropsExplicitlyAndRecoversAfterFlush() {
        val batches = mutableListOf<String>()
        val capture = TouchInputCapture({ 0L }, { batches.add(it) }, {}, maxBatchChars = 512)
        capture.start { "test" }
        try {
            capture.record { "x".repeat(512) }
            capture.flush()
            capture.record { "small_sample" }
        } finally {
            capture.finish("activity_pause")
        }
        val output = batches.joinToString("")
        assertTrue(output.contains("small_sample"))
        assertTrue(output.contains("records=1 dropped=1"))
        assertTrue(output.contains("gap dropped=1 first_t_ms=0 last_t_ms=0"))
        assertFalse(output.contains("x".repeat(512)))
    }
}
