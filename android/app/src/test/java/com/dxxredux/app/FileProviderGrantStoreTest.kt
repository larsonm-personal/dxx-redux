package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.IOException

class FileProviderGrantStoreTest {
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
