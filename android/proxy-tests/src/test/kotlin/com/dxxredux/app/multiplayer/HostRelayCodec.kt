package com.dxxredux.app.multiplayer

import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/** Independent server-role fixture, without constructing or bypassing a client transport */
internal class HostRelayCodec(
    private val token: UInt,
    private val own: Int,
    private val peer: Int,
    sending: String,
    receiving: String,
) {
    private val sending = SecretKeySpec(hex(sending), "AES")
    private val receiving = SecretKeySpec(hex(receiving), "AES")
    private var outgoing = 0L
    private var highest = 0L
    private val received = HashSet<Long>()

    @Synchronized fun encode(
        kind: Int,
        payload: ByteArray,
    ): ByteArray {
        val seq = ++outgoing
        val header =
            ByteBuffer
                .allocate(19)
                .order(ByteOrder.LITTLE_ENDIAN)
                .put(byteArrayOf(68, 88, 82, 51))
                .putInt(token.toInt())
                .put(kind.toByte())
                .put(own.toByte())
                .put(peer.toByte())
                .putLong(seq)
                .array()
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, sending, GCMParameterSpec(128, nonce(seq)))
        cipher.updateAAD(header)
        return header + cipher.doFinal(payload)
    }

    @Synchronized fun decode(
        data: ByteArray,
        length: Int,
    ): RelayTransport.Packet? {
        if (length !in 35..RelayTransport.MAX_PACKET || length > data.size) return null
        val h = ByteBuffer.wrap(data, 0, 19).order(ByteOrder.LITTLE_ENDIAN)
        if (h.int != 0x33525844 || h.int.toUInt() != token) return null
        val kind = h.get().toInt() and 255
        if ((h.get().toInt() and 255) != peer || (h.get().toInt() and 255) != own) return null
        val seq = h.long
        if (seq <= 0 || seq <= highest - 64 || seq in received) return null
        val plain =
            try {
                val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                cipher.init(Cipher.DECRYPT_MODE, receiving, GCMParameterSpec(128, nonce(seq)))
                cipher.updateAAD(data, 0, 19)
                cipher.doFinal(data, 19, length - 19)
            } catch (_: java.security.GeneralSecurityException) {
                return null
            }
        highest = maxOf(highest, seq)
        received.removeAll { it <= highest - 64 }
        received.add(seq)
        return RelayTransport.Packet(kind, plain)
    }

    private fun nonce(seq: Long) =
        ByteBuffer
            .allocate(12)
            .putInt(0)
            .putLong(seq)
            .array()

    private fun hex(value: String) = ByteArray(32) { value.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
}
