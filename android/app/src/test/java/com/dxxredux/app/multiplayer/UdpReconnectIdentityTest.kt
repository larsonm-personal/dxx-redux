package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class UdpReconnectIdentityTest {
    @Test
    fun signsAndVerifiesWithProcessIdentity() {
        val message = "generation=17;counter=9;callsign=ace".toByteArray()
        val publicKey = UdpReconnectIdentity.publicKey()
        val signature = UdpReconnectIdentity.sign(message)

        assertEquals(65, publicKey.size)
        assertEquals(0x04, publicKey[0].toInt())
        assertTrue(signature.size <= 80)
        assertTrue(UdpReconnectIdentity.verify(publicKey, message, signature))
    }

    @Test
    fun rejectsChangedGenerationOrRequestPayload() {
        val request = "generation=17;counter=9;callsign=ace".toByteArray()
        val signature = UdpReconnectIdentity.sign(request)
        val publicKey = UdpReconnectIdentity.publicKey()

        assertFalse(
            UdpReconnectIdentity.verify(
                publicKey,
                "generation=18;counter=9;callsign=ace".toByteArray(),
                signature,
            ),
        )
        assertFalse(
            UdpReconnectIdentity.verify(
                publicKey,
                "generation=17;counter=9;callsign=other".toByteArray(),
                signature,
            ),
        )
    }

    @Test
    fun rejectsWrongPublicKeyAndChangedSignature() {
        val message = "challenge=1234".toByteArray()
        val signature = UdpReconnectIdentity.sign(message)
        val changedKey = UdpReconnectIdentity.publicKey().also { it[12] = (it[12].toInt() xor 1).toByte() }
        val changedSignature =
            signature.copyOf().also {
                it[it.lastIndex] = (it.last().toInt() xor 1).toByte()
            }

        assertFalse(UdpReconnectIdentity.verify(changedKey, message, signature))
        assertFalse(
            UdpReconnectIdentity.verify(
                UdpReconnectIdentity.publicKey(),
                message,
                changedSignature,
            ),
        )
    }

    @Test
    fun generatesRequestedChallengeSize() {
        assertEquals(32, UdpReconnectIdentity.randomBytes(32).size)
        assertEquals(0, UdpReconnectIdentity.randomBytes(-1).size)
    }
}
