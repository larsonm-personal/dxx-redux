package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class AudioPreviewResourceOwnerTest {
    @Test
    fun initializationFailureReleasesPartiallyStartedResource() {
        val released = mutableListOf<Any>()
        val owner = AudioPreviewResourceOwner<Any>(released::add)
        val resource = Any()

        assertThrows(IllegalStateException::class.java) {
            initializeAudioPreviewResource(owner, resource) { throw IllegalStateException("start failed") }
        }

        assertNull(owner.current)
        assertEquals(listOf(resource), released)
    }

    @Test
    fun releaseIsIdentityCheckedAndIdempotent() {
        val released = mutableListOf<Any>()
        val owner = AudioPreviewResourceOwner<Any>(released::add)
        val first = owner.replace(Any())
        val second = owner.replace(Any())

        assertEquals(listOf(first), released)
        assertSame(second, owner.current)
        assertFalse(owner.release(first))
        assertTrue(owner.release(second))
        assertFalse(owner.release(second))
        assertEquals(listOf(first, second), released)
    }
}
