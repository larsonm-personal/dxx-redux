package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.BufferedWriter
import java.io.Writer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit

class DebugLogAsyncTest {
    @Test
    fun profilingDoesNotBlockOnWriterAndKeepsBatchOrder() {
        val writerField = DebugLog::class.java.getDeclaredField("writer").apply { isAccessible = true }
        val flagsField = DebugLog::class.java.getDeclaredField("enabledCategories").apply { isAccessible = true }
        val executorField =
            DebugLog::class.java
                .getDeclaredField(
                    "diagnosticBatchExecutor",
                ).apply { isAccessible = true }
        val queue = executorField.get(DebugLog) as ThreadPoolExecutor
        val flags = flagsField.get(DebugLog) as BooleanArray
        val previousWriter = writerField.get(DebugLog)
        val previousFlag = flags[DebugLogCategory.PROFILING]
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val output = StringBuilder()
        val caller = Executors.newSingleThreadExecutor()
        val sink =
            object : Writer() {
                override fun write(
                    chars: CharArray,
                    offset: Int,
                    length: Int,
                ) {
                    entered.countDown()
                    check(release.await(10, TimeUnit.SECONDS)) { "Test writer was not released" }
                    output.append(chars, offset, length)
                }

                override fun flush() = Unit

                override fun close() = Unit
            }
        try {
            writerField.set(DebugLog, BufferedWriter(sink))
            flags[DebugLogCategory.PROFILING] = true
            // A synchronous implementation cannot return while this sink is blocked
            caller.submit { DebugLog.log(DebugLogCategory.PROFILING, "profile-first") }.get(2, TimeUnit.SECONDS)
            assertTrue(entered.await(2, TimeUnit.SECONDS))
            caller
                .submit {
                    DebugLog.logBatch(DebugLogCategory.PROFILING, "profile-second\nprofile-third\n")
                }.get(2, TimeUnit.SECONDS)
            assertTrue(output.isEmpty())
            release.countDown()
            queue.submit {}.get(5, TimeUnit.SECONDS)
            val text = output.toString()
            assertTrue(text.contains("profile-first"))
            assertTrue(text.indexOf("profile-first") < text.indexOf("profile-second"))
            assertTrue(text.indexOf("profile-second") < text.indexOf("profile-third"))
            flags[DebugLogCategory.PROFILING] = false
            DebugLog.logBatch(DebugLogCategory.PROFILING, "disabled-batch")
            queue.submit {}.get(5, TimeUnit.SECONDS)
            assertFalse(output.contains("disabled-batch"))
        } finally {
            release.countDown()
            caller.shutdown()
            caller.awaitTermination(5, TimeUnit.SECONDS)
            queue.submit {}.get(5, TimeUnit.SECONDS)
            writerField.set(DebugLog, previousWriter)
            flags[DebugLogCategory.PROFILING] = previousFlag
        }
    }
}
