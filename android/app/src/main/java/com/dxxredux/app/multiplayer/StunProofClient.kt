package com.dxxredux.app.multiplayer

import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException
import java.nio.ByteBuffer
import java.security.SecureRandom
import java.util.UUID

internal data class StunProofObservation(
    val endpoint: InetSocketAddress,
    val receipt: UUID,
)

/** The control server must redeem receipts before any returned endpoint is used for peer probes */
internal object StunProofClient {
    // Private discovery extension, kept in sync with server/src/stun.rs
    internal const val REQUEST_BYTES = 40
    internal const val RESPONSE_BYTES = 52
    private const val MAGIC = 0x2112A442
    private const val TIMEOUT_MS = 2000L
    private const val RETRIES = 2
    private const val MAX_RECEIVES_PER_ATTEMPT = 16

    /** Runs blocking I/O, without closing the caller's socket or retaining its timeout */
    fun discover(
        servers: List<InetSocketAddress>,
        credential: UUID,
        socket: DatagramSocket,
        checkActive: () -> Unit = {},
    ): List<StunProofObservation> {
        require(servers.size <= 2 && servers.distinct().size == servers.size)
        require(servers.all { !it.isUnresolved && it.port in 1..65535 })
        val previousTimeout = socket.soTimeout
        try {
            return servers
                .mapNotNull { server ->
                    query(server, credential, socket, checkActive)
                }.distinctBy { it.receipt }
        } finally {
            runCatching { socket.soTimeout = previousTimeout }
        }
    }

    private fun query(
        server: InetSocketAddress,
        credential: UUID,
        socket: DatagramSocket,
        checkActive: () -> Unit,
    ): StunProofObservation? {
        val transaction = ByteArray(12).also { SecureRandom().nextBytes(it) }
        val request = buildRequest(transaction, credential)
        val buffer = ByteArray(RESPONSE_BYTES + 1)
        repeat(RETRIES) {
            checkActive()
            socket.send(DatagramPacket(request, request.size, server))
            val deadline = System.nanoTime() + TIMEOUT_MS * 1_000_000L
            var received = 0
            while (true) {
                checkActive()
                val remaining = deadline - System.nanoTime()
                if (remaining <= 0L) break
                if (received >= MAX_RECEIVES_PER_ATTEMPT) {
                    // Junk cannot accelerate retries, extend the deadline or create unbounded receive work
                    Thread.sleep(minOf(100L, (remaining + 999_999L) / 1_000_000L))
                    continue
                }
                socket.soTimeout = ((remaining + 999_999L) / 1_000_000L).toInt().coerceAtLeast(1)
                val packet = DatagramPacket(buffer, buffer.size)
                try {
                    socket.receive(packet)
                    received++
                    if (packet.socketAddress != server) continue
                    parseResponse(buffer, packet.length, transaction)?.let { return it }
                } catch (_: SocketTimeoutException) {
                    // Keep the original absolute deadline even if the platform wakes early
                }
            }
        }
        return null
    }

    internal fun buildRequest(
        transaction: ByteArray,
        credential: UUID,
    ): ByteArray {
        require(transaction.size == 12)
        return ByteBuffer
            .allocate(REQUEST_BYTES)
            .putShort(0x0001)
            .putShort(20)
            .putInt(MAGIC)
            .put(transaction)
            .putShort(0xC001.toShort())
            .putShort(16)
            .putLong(credential.mostSignificantBits)
            .putLong(credential.leastSignificantBits)
            .array()
    }

    internal fun parseResponse(
        data: ByteArray,
        length: Int,
        transaction: ByteArray,
    ): StunProofObservation? {
        if (length != RESPONSE_BYTES || data.size < length || transaction.size != 12) return null
        val buffer = ByteBuffer.wrap(data, 0, length)
        if (buffer.short.toInt() != 0x0101 || buffer.short.toInt() != 32 || buffer.int != MAGIC) return null
        val actualTransaction = ByteArray(12).also { buffer.get(it) }
        if (!transaction.contentEquals(actualTransaction)) return null
        if (buffer.short.toInt() != 0x0020 || buffer.short.toInt() != 8 || buffer.short.toInt() != 1) return null
        val port = (buffer.short.toInt() and 0xFFFF) xor (MAGIC ushr 16)
        val ip = ByteBuffer.allocate(4).putInt(buffer.int xor MAGIC).array()
        val address = InetAddress.getByAddress(ip)
        if (port == 0 || address.isAnyLocalAddress || address.isMulticastAddress ||
            ip.all { it == (-1).toByte() }
        ) {
            return null
        }
        if ((buffer.short.toInt() and 0xFFFF) != 0xC002 || buffer.short.toInt() != 16) return null
        return StunProofObservation(InetSocketAddress(address, port), UUID(buffer.long, buffer.long))
    }
}
