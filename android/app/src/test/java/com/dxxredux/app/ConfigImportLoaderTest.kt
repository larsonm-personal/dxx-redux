package com.dxxredux.app

import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.yield
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.IOException
import java.io.InputStream
import java.util.concurrent.CountDownLatch
import java.util.concurrent.atomic.AtomicBoolean

class ConfigImportLoaderTest {
    @Test
    fun boundedReaderAcceptsExactLimitAndRejectsNextByte() {
        assertEquals("12345678", readBoundedConfigText(ByteArrayInputStream("12345678".toByteArray()), 8))

        val error =
            runCatching {
                readBoundedConfigText(ByteArrayInputStream("123456789".toByteArray()), 8)
            }.exceptionOrNull()
        assertTrue(error is ConfigImportLimitException)
    }

    @Test
    fun boundedReaderHandlesShortAndZeroLengthReads() {
        val source = "short reads remain complete".toByteArray()
        var zeroPending = true
        val input =
            object : InputStream() {
                var offset = 0

                override fun read(): Int = if (offset < source.size) source[offset++].toInt() and 0xff else -1

                override fun read(
                    buffer: ByteArray,
                    off: Int,
                    len: Int,
                ): Int {
                    if (zeroPending) {
                        zeroPending = false
                        return 0
                    }
                    if (offset >= source.size) return -1
                    val count = minOf(3, len, source.size - offset)
                    source.copyInto(buffer, off, offset, offset + count)
                    offset += count
                    return count
                }
            }

        assertEquals(String(source), readBoundedConfigText(input))
    }

    @Test
    fun preparationRejectsReadFailuresMalformedJsonAndDeepNesting() {
        val failing =
            prepareConfigImport(
                object : InputStream() {
                    override fun read(): Int = throw IOException("provider failed")
                },
            )
        assertTrue((failing as ConfigImportPreparation.Error).message.contains("provider failed"))

        val malformed = prepareConfigImport(ByteArrayInputStream("{".toByteArray()))
        assertTrue((malformed as ConfigImportPreparation.Error).message.contains("invalid configuration"))

        val deep = "[".repeat(MAX_CONFIG_IMPORT_DEPTH + 1) + "]".repeat(MAX_CONFIG_IMPORT_DEPTH + 1)
        val nested = prepareConfigImport(ByteArrayInputStream(deep.toByteArray()))
        assertTrue((nested as ConfigImportPreparation.Error).message.contains("nesting"))
    }

    @Test
    fun preparationRejectsOversizedStringsArraysAndSlots() {
        val longString =
            """{"type":"touch_layout","name":"${"x".repeat(MAX_CONFIG_STRING_CHARS + 1)}"}"""
        assertTrue(
            (prepareConfigImport(ByteArrayInputStream(longString.toByteArray())) as ConfigImportPreparation.Error)
                .message
                .contains("string"),
        )

        val largeArray =
            """{"type":"touch_layout","buttons":[${"{}".plus(",").repeat(MAX_CONFIG_CONTAINER_ENTRIES)}{}]}"""
        assertTrue(
            (prepareConfigImport(ByteArrayInputStream(largeArray.toByteArray())) as ConfigImportPreparation.Error)
                .message
                .contains("array"),
        )

        val slots =
            """{"type":"combined_config","touch_layout_slots":[${
                """{"layout":{}},""".repeat(MAX_CONFIG_SLOTS)
            }{"layout":{}}]}"""
        assertTrue(
            (prepareConfigImport(ByteArrayInputStream(slots.toByteArray())) as ConfigImportPreparation.Error)
                .message
                .contains("slots"),
        )
    }

    @Test
    fun validPreparationDetectsTypeAndWorkerLeavesCallerThread() =
        runBlocking {
            val prepared =
                prepareConfigImport(
                    ByteArrayInputStream("""{"type":"controller_config","bindings":{}}""".toByteArray()),
                )
            assertEquals("controller_config", (prepared as ConfigImportPreparation.Ready).config.type)

            val callerThread = Thread.currentThread().threadId()
            val workerThread = onConfigImportWorker { Thread.currentThread().threadId() }
            assertNotEquals(callerThread, workerThread)
        }

    @Test
    fun cancellingWorkerInterruptsBlockingProviderWork() =
        runBlocking {
            val started = CountDownLatch(1)
            val interrupted = AtomicBoolean(false)
            val job =
                launch {
                    onConfigImportWorker {
                        started.countDown()
                        try {
                            Thread.sleep(30_000)
                        } catch (error: InterruptedException) {
                            interrupted.set(true)
                            throw error
                        }
                    }
                }

            while (started.count != 0L) yield()
            job.cancelAndJoin()
            assertTrue(interrupted.get())
        }
}
