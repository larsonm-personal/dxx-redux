package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class NativeFatalErrorStoreTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun publishedFailureIsConsumedExactlyOnce() {
        val filesDir = temporaryFolder.newFolder("files")
        val message =
            "Error: Android content setup failed during mount active set " +
                "for /missing/selected: not found"

        NativeFatalErrorStore.publish(filesDir, message)

        assertEquals(message, NativeFatalErrorStore.consume(filesDir))
        assertNull(NativeFatalErrorStore.consume(filesDir))
        assertFalse(filesDir.listFiles().orEmpty().any { it.name.contains("native_fatal_error") })
    }
}
