package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ImportStorageGuardTest {
    @Test
    fun unknownFilesystemCapacityDoesNotProduceFalseNoSpaceFailure() {
        assertTrue(!ImportStorageGuard.shouldReject(0L, 100L))
        assertTrue(ImportStorageGuard.shouldReject(99L, 100L))
    }

    @Test
    fun archiveEntryBytesIgnoresUnknownSizes() {
        val total = ImportStorageGuard.archiveEntryBytes(listOf(10L, -1L, 0L, 25L))

        assertEquals(35L, total)
    }

    @Test
    fun storageFailureMessageUsesSharedPopupText() {
        val error =
            InsufficientStorageException(
                requiredFreeBytes = 75L * 1024L * 1024L,
                availableBytes = 10L * 1024L * 1024L,
                target = "extract demo.zip",
            )

        val message = ImportStorageGuard.messageForFailure(error)

        assertTrue(message.contains("Not enough free space"))
        assertTrue(message.contains("Required: 75 MiB"))
        assertTrue(message.contains("Available: 10 MiB"))
        assertTrue(message.contains("stopped before writing"))
    }
}
