package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.IOException

class FileProviderGrantStoreTest {
    @Test
    fun growingLogExportsOnlyTheCapturedPrefix() {
        val root = testRoot("growing-log")
        val source = File(root.parentFile, "growing-debuglog.txt")
        val original = "log line\n".repeat(20_000)
        source.writeText(original)
        val published =
            FileProviderGrantStore.copyLogSnapshotFile(root, source) { progress ->
                if (progress.bytesDone == 0L) source.appendText("launcher still logging\n")
            }
        assertEquals(original, published.readText())
        source.appendText("later line\n")
        assertEquals(original, published.readText())
    }

    @Test
    fun emptyLogSnapshotDoesNotReadLaterAppends() {
        val root = testRoot("empty-log")
        val source = File(root.parentFile, "empty-debuglog.txt").apply { writeText("") }
        val published =
            FileProviderGrantStore.copyLogSnapshotFile(root, source) { progress ->
                if (progress.bytesDone == 0L) source.appendText("later line\n")
            }
        assertEquals(0L, published.length())
    }

    @Test
    fun truncatedLogSnapshotLeavesNoGrant() {
        val root = testRoot("truncated-log")
        val source = File(root.parentFile, "truncated-debuglog.txt").apply { writeText("original log\n") }
        val failure =
            runCatching {
                FileProviderGrantStore.copyLogSnapshotFile(root, source) { progress ->
                    if (progress.bytesDone == 0L) source.writeText("")
                }
            }.exceptionOrNull()
        assertTrue(failure is IOException)
        assertEquals(emptyList<File>(), root.listFiles().orEmpty().toList())
    }

    @Test
    fun everyPublicationGetsAnImmutableGeneration() {
        val root = testRoot("immutable")
        assertEquals("Aa/readme.pdf".hashCode(), "BB/readme.pdf".hashCode())
        val first = publish(root, "Aa/readme.pdf", "first")
        val second = publish(root, "BB/readme.pdf", "second")
        val repeated = publish(root, "Aa/readme.pdf", "third")
        val malicious = publish(root, "..", "contained")

        assertNotEquals(first.parentFile, second.parentFile)
        assertNotEquals(first.parentFile, repeated.parentFile)
        assertEquals("first", first.readText())
        assertEquals("second", second.readText())
        assertEquals("third", repeated.readText())
        assertEquals("document", malicious.name)
        assertEquals(root.canonicalFile, malicious.parentFile?.parentFile?.canonicalFile)
    }

    @Test
    fun liveGenerationsAreNeverEvictedToAdmitANewGrant() {
        val root = testRoot("budget")
        val first = publish(root, "first.txt", "a".repeat(40), maxBytes = 64)
        val failure =
            runCatching {
                publish(root, "second.txt", "b".repeat(40), maxBytes = 64)
            }.exceptionOrNull()

        assertTrue(failure is IOException)
        assertEquals("a".repeat(40), first.readText())
        assertEquals(1, root.listFiles().orEmpty().count { it.isDirectory })
    }

    @Test
    fun expiredGenerationsAreReclaimedBeforePublication() {
        val root = testRoot("expiry")
        val first = publish(root, "first.txt", "first", nowMs = 1_000)
        val second =
            publish(
                root,
                "second.txt",
                "second",
                maxBytes = 6,
                retentionMs = 100,
                nowMs = 1_101,
            )

        assertEquals(false, first.exists())
        assertEquals("second", second.readText())
    }

    @Test
    fun failedWritersLeaveNoGrantableGeneration() {
        val root = testRoot("failure")
        val failure =
            runCatching {
                FileProviderGrantStore.publishFile(root, "partial.txt", 7) { temporary ->
                    temporary.writeText("short")
                }
            }.exceptionOrNull()

        assertTrue(failure is IOException)
        assertEquals(emptyList<File>(), root.listFiles().orEmpty().toList())
    }

    private fun publish(
        root: File,
        name: String,
        text: String,
        maxBytes: Long = 1024,
        retentionMs: Long = FileProviderGrantStore.RETENTION_MS,
        nowMs: Long = 1_000,
    ): File =
        FileProviderGrantStore.publishFile(
            root = root,
            displayName = name,
            expectedBytes = text.toByteArray().size.toLong(),
            maxRootBytes = maxBytes,
            retentionMs = retentionMs,
            nowMs = nowMs,
        ) { temporary ->
            temporary.writeText(text)
        }

    private fun testRoot(name: String): File =
        File("build/test-file-provider-grants/$name").absoluteFile.also {
            it.deleteRecursively()
            it.mkdirs()
        }
}
