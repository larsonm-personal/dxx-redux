package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class GameProcessExitDiagnosticsTest {
    @get:Rule
    val temporary = TemporaryFolder()

    private fun marker(expected: String = ""): File =
        File(temporary.root, "engine_pending_1000_42.txt").apply {
            writeText("pid=42\nstarted_ms=1000\nexpected_exit=$expected\nRestore active: 1\nPhase: replacing world\nSave: example.mg3\n")
        }

    @Test
    fun interruptedRestoreProducesOneReportInTheExistingCrashSection() {
        val pending = marker()
        val exit = EngineExitRecord(42, 1500, 2, 9, "signal")
        GameProcessExitDiagnostics.recoverMarkers(temporary.root, listOf(exit)) { throw it }
        val report = File(temporary.root, "crash_error_exit_1000_42.txt")
        assertTrue(report.readText().startsWith("Interrupted save restore\n"))
        assertTrue(report.readText().contains("no native stack trace available"))
        assertTrue(report.readText().contains("Phase: replacing world"))
        assertTrue(report.readText().contains("Signal/status: 9"))
        assertTrue(CrashLog.isCrashReportFile(report))
        assertFalse(pending.exists())
        val contents = report.readText()
        marker()
        GameProcessExitDiagnostics.recoverMarkers(temporary.root, listOf(exit)) { throw it }
        assertEquals(contents, report.readText())
        assertEquals(1, temporary.root.listFiles()!!.size)
    }

    @Test
    fun liveOrReusedPidDoesNotCreateAFailureReport() {
        val pending = marker()
        GameProcessExitDiagnostics.recoverMarkers(temporary.root, emptyList()) { throw it }
        GameProcessExitDiagnostics.recoverMarkers(
            temporary.root,
            listOf(EngineExitRecord(42, 900, 2, 9, "previous process"), EngineExitRecord(43, 1500, 2, 9, "other PID")),
        ) { throw it }
        assertTrue(pending.exists())
        assertEquals(1, temporary.root.listFiles()!!.size)
    }

    @Test
    fun missingProcessWithoutExitHistoryStillProducesAReport() {
        marker()
        GameProcessExitDiagnostics.recoverMarkers(temporary.root, emptyList(), processExists = { false }) { throw it }
        val report = File(temporary.root, "crash_error_exit_1000_42.txt")
        assertTrue(report.readText().contains("historical exit details unavailable"))
        assertTrue(report.readText().contains("Exit timestamp: unavailable"))
        assertEquals(1, temporary.root.listFiles()!!.size)
    }

    @Test
    fun runningProcessWithoutExitHistoryKeepsItsMarker() {
        val pending = marker()
        GameProcessExitDiagnostics.recoverMarkers(temporary.root, emptyList(), processExists = { true }) { throw it }
        assertTrue(pending.exists())
        assertEquals(1, temporary.root.listFiles()!!.size)
    }

    @Test
    fun intentionalQuitClearsMarkerWithoutCreatingACrashReport() {
        marker("user quit")
        GameProcessExitDiagnostics.recoverMarkers(temporary.root, listOf(EngineExitRecord(42, 1500, 2, 9, ""))) { throw it }
        assertEquals(0, temporary.root.listFiles()!!.size)
    }

    @Test
    fun incompleteNativeReportIsNotOfferedForDownload() {
        val partial = File(temporary.root, "crash_error_exit_1000_42.txt.tmp").apply { writeText("partial") }
        assertFalse(CrashLog.isCrashReportFile(partial))
    }

    @Test
    fun nativeReportWinsOverLauncherFallback() {
        marker()
        val report = File(temporary.root, "crash_error_exit_1000_42.txt").apply { writeText("native detailed report") }
        GameProcessExitDiagnostics.recoverMarkers(temporary.root, listOf(EngineExitRecord(42, 1500, 2, 9, ""))) { throw it }
        assertEquals("native detailed report", report.readText())
    }
}
