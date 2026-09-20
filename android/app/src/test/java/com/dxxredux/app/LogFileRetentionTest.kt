package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class LogFileRetentionTest {
    @get:Rule
    val temporary = TemporaryFolder()

    private fun file(name: String, time: Long): File =
        File(temporary.root, name).apply {
            writeText(name)
            assertTrue(setLastModified(time))
        }

    @Test
    fun crashReportsShareOneLimitAndLeaveSupportingFilesAlone() {
        val reports =
            listOf(
                "tombstone_old.java.xcrash",
                "crash_error_42.txt",
                "tombstone_new.native.xcrash",
                "crash_error_exit_123.txt",
                "crash_error_restore_123.txt",
                "tombstone_new.anr.xcrash",
                "crash_error_emergency_123.txt",
            ).mapIndexed { index, name -> file(name, (index + 1) * 1000L) }
        val supporting =
            listOf("crash_breadcrumbs_latest.txt", "engine_pending_123.txt", "crash_error_in_progress.txt.tmp")
                .map { file(it, 1000L) }
        val directory = File(temporary.root, "crash_error_directory").apply { mkdir() }

        pruneOldLogFiles(temporary.root, CrashLog.MAX_FILES, CrashLog::isCrashReportFile)

        assertEquals(reports.takeLast(5), reports.filter { it.exists() })
        assertTrue(supporting.all { it.exists() })
        assertTrue(directory.isDirectory)
        pruneOldLogFiles(temporary.root, CrashLog.MAX_FILES, CrashLog::isCrashReportFile)
        assertEquals(reports.takeLast(5), reports.filter { it.exists() })
    }

    @Test
    fun exitRecoveryPrunesExistingFallbackReportsOnDisk() {
        val reports = (1..8).map { file("crash_error_exit_$it.txt", it * 1000L) }

        GameProcessExitDiagnostics.recoverMarkers(temporary.root, emptyList())

        assertEquals(reports.takeLast(5), reports.filter { it.exists() })
    }

    @Test
    fun debugLogsReserveOneSlotBeforeCreatingTheNextLog() {
        val logs = (1..7).map { file("debuglog_$it.txt", it * 1000L) }
        val other = file("other.txt", 1000L)

        pruneOldLogFiles(temporary.root, 4, matches = { it.name.startsWith("debuglog_") })
        val newest = file("debuglog_8.txt", 8000L)

        assertEquals(logs.takeLast(4) + newest, (logs + newest).filter { it.exists() })
        assertTrue(other.exists())
    }
}
