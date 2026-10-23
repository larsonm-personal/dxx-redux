package com.dxxredux.app.multiplayer

import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/** Protocol 3 relay framing, shared with server/src/relay.rs.
 * Each allocation has independent AES-256-GCM keys for each direction. The relay
 * terminates encryption. Header bytes are authenticated; sequence is also the
 * 96-bit GCM nonce (four zero bytes followed by the big-endian sequence).
 */
internal class RelayTransport(
    private val token: UInt,
    private val ownSlot: Int,
    private val peerSlot: Int,
    sendKey: String,
    receiveKey: String,
) {
    private val sendKey = key(sendKey)
    private val receiveKey = key(receiveKey)
    private var outgoing = 0L
    private var highest = 0L
    private var seen = 0uL

    @Volatile var lastAcknowledgementNanos = System.nanoTime()
        private set

    @Volatile var bound = false
        private set

    init {
        require(ownSlot in 0..7 && peerSlot in 0..7 && ownSlot != peerSlot)
        require(!this.sendKey.encoded.contentEquals(this.receiveKey.encoded))
        claimKeys(this.sendKey.encoded, this.receiveKey.encoded)
    }

    data class Packet(
        val kind: Int,
        val payload: ByteArray,
    )

    @Synchronized
    fun encode(
        kind: Int,
        payload: ByteArray,
    ): ByteArray {
        require(payload.size <= MAX_PAYLOAD && outgoing < Long.MAX_VALUE)
        outgoing++
        val header =
            ByteBuffer
                .allocate(HEADER_SIZE)
                .order(ByteOrder.LITTLE_ENDIAN)
                .put(MAGIC)
                .putInt(token.toInt())
                .put(kind.toByte())
                .put(ownSlot.toByte())
                .put(peerSlot.toByte())
                .putLong(outgoing)
                .array()
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, sendKey, GCMParameterSpec(128, nonce(outgoing)))
        cipher.updateAAD(header)
        return header + cipher.doFinal(payload)
    }

    @Synchronized
    fun decode(
        data: ByteArray,
        length: Int,
    ): Packet? {
        if (length !in (HEADER_SIZE + 16)..MAX_PACKET || length > data.size) return null
        if (!data.copyOfRange(0, 4).contentEquals(MAGIC)) return null
        val header = ByteBuffer.wrap(data, 0, HEADER_SIZE).order(ByteOrder.LITTLE_ENDIAN)
        header.position(4)
        if (header.int.toUInt() != token) return null
        val kind = header.get().toInt() and 255
        if ((header.get().toInt() and 255) != peerSlot || (header.get().toInt() and 255) != ownSlot) return null
        val sequence = header.long
        if (sequence <= 0) return null
        val plain =
            try {
                val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                cipher.init(Cipher.DECRYPT_MODE, receiveKey, GCMParameterSpec(128, nonce(sequence)))
                cipher.updateAAD(data, 0, HEADER_SIZE)
                cipher.doFinal(data, HEADER_SIZE, length - HEADER_SIZE)
            } catch (_: java.security.GeneralSecurityException) {
                return null
            }
        if (sequence > highest) {
            val shift = sequence - highest
            seen = if (shift >= 64) 1uL else (seen shl shift.toInt()) or 1uL
            highest = sequence
        } else {
            val delta = highest - sequence
            if (delta >= 64 || seen and (1uL shl delta.toInt()) != 0uL) return null
            seen = seen or (1uL shl delta.toInt())
        }
        if (kind == BOUND && plain.isEmpty()) {
            bound = true
            lastAcknowledgementNanos = System.nanoTime()
        }
        return Packet(kind, plain)
    }

    companion object {
        const val DATA = 0
        const val BIND = 1
        const val CHALLENGE = 2
        const val PROOF = 3
        const val BOUND = 4
        const val HEADER_SIZE = 19
        const val MAX_PAYLOAD = 4096
        const val MAX_PACKET = HEADER_SIZE + MAX_PAYLOAD + 16
        private val MAGIC = byteArrayOf(68, 88, 82, 51)

        // Never evict a used key: reconstructing an allocation must obtain fresh keys
        // A process restart also establishes a new authenticated control session
        private val usedKeys = HashSet<String>()

        @Synchronized
        private fun claimKeys(
            send: ByteArray,
            receive: ByteArray,
        ) {
            fun fingerprint(key: ByteArray) =
                java.security.MessageDigest
                    .getInstance("SHA-256")
                    .digest(key)
                    .joinToString("") { "%02x".format(it) }
            val sending = fingerprint(send)
            val receiving = fingerprint(receive)
            check(usedKeys.size <= 8190) { "Relay allocation limit reached; restart the application" }
            check(sending !in usedKeys && receiving !in usedKeys) {
                "Relay allocation cannot be reused; reconnect for fresh credentials"
            }
            usedKeys.add(sending)
            usedKeys.add(receiving)
        }

        fun senderSlot(
            data: ByteArray,
            length: Int,
        ): Int? =
            if (length >= HEADER_SIZE + 16 &&
                data.copyOfRange(0, 4).contentEquals(MAGIC)
            ) {
                data[9].toInt() and 255
            } else {
                null
            }

        private fun nonce(sequence: Long): ByteArray =
            ByteBuffer
                .allocate(12)
                .putInt(0)
                .putLong(sequence)
                .array()

        private fun key(hex: String): SecretKeySpec {
            require(hex.length == 64)
            return SecretKeySpec(ByteArray(32) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }, "AES")
        }
    }
}
