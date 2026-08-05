package com.dxxredux.app

import java.io.ByteArrayInputStream
import java.io.InputStream
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class DiscIdentifierHashTest {
    @Test
    fun completeRangeProducesHashOnlyAfterExactRead() {
        val result =
            DiscIdentifier.sha1Hash(
                ByteArrayInputStream("prefixabc".toByteArray()),
                offset = 6,
                length = 3,
            )

        assertEquals(
            "a9993e364706816aba3e25717850c26c9cd0d89d",
            (result as DiscIdentifier.Sha1HashResult.Complete).sha1,
        )
    }

    @Test
    fun shortAndNonProgressingStreamsFailWithoutHash() {
        val short = DiscIdentifier.sha1Hash(ByteArrayInputStream(byteArrayOf(1, 2)), 3)
        val stalled =
            DiscIdentifier.sha1Hash(
                object : InputStream() {
                    override fun read(): Int = 0

                    override fun read(
                        b: ByteArray,
                        off: Int,
                        len: Int,
                    ): Int = 0
                },
                1,
            )

        assertTrue(short is DiscIdentifier.Sha1HashResult.Failed)
        assertTrue(stalled is DiscIdentifier.Sha1HashResult.Failed)
    }

    @Test
    fun incompleteSkipAndCancellationDoNotProduceHashes() {
        val missingOffset = DiscIdentifier.sha1Hash(ByteArrayInputStream(byteArrayOf(1)), 2, 1)
        val canceled =
            DiscIdentifier.sha1Hash(ByteArrayInputStream(ByteArray(70_000)), 70_000) { _, _ -> true }

        assertTrue(missingOffset is DiscIdentifier.Sha1HashResult.Failed)
        assertSame(DiscIdentifier.Sha1HashResult.Canceled, canceled)
    }

    @Test
    fun cancellationAfterFinalByteCannotDiscardCompleteHash() {
        val result = DiscIdentifier.sha1Hash(ByteArrayInputStream(byteArrayOf(1)), 1) { _, _ -> true }

        assertTrue(result is DiscIdentifier.Sha1HashResult.Complete)
    }
}
