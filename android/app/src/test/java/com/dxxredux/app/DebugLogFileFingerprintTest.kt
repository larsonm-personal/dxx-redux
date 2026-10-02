package com.dxxredux.app

import org.junit.Assert.assertNotEquals
import org.junit.Test
import java.nio.file.Files

class DebugLogFileFingerprintTest {
    @Test
    fun fingerprintChangesWhenActiveLogAppearsOrGrows() {
        val dir = Files.createTempDirectory("debug-log-fingerprint").toFile()
        try {
            val original = dir.resolve("debuglog_old.txt").apply { writeText("old") }
            val before = debugLogFileFingerprint(listOf(original))
            val active = dir.resolve("debuglog_active.txt").apply { writeText("start") }
            val afterCreate = debugLogFileFingerprint(listOf(active, original))
            active.appendText("\nnew profiling data")
            val afterGrowth = debugLogFileFingerprint(listOf(active, original))

            assertNotEquals(before, afterCreate)
            assertNotEquals(afterCreate, afterGrowth)
        } finally {
            dir.deleteRecursively()
        }
    }
}
