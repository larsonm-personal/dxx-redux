package com.dxxredux.app.multiplayer

import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.NetworkInterface
import java.nio.ByteBuffer
import java.security.SecureRandom

private const val TAG = "StunClient"

// STUN message constants (RFC 5389)
private const val STUN_BINDING_REQUEST: Short = 0x0001
private const val STUN_BINDING_RESPONSE: Short = 0x0101
private const val STUN_MAGIC_COOKIE: Int = 0x2112A442.toInt()
private const val STUN_HEADER_SIZE = 20
private const val STUN_ATTR_XOR_MAPPED_ADDRESS: Int = 0x0020
private const val STUN_ATTR_MAPPED_ADDRESS: Int = 0x0001
private const val STUN_TIMEOUT_MS = 2000
private const val STUN_RETRIES = 2

/** Result of a single STUN query: the server-reflexive (public) address. */
data class StunResult(
    val reflexiveAddr: InetSocketAddress?,
    val localPort: Int,
)

/** Aggregated results from querying two STUN servers on the same local port. */
data class StunReport(
    val candidates: List<ConnectionCandidate>,
    val natType: String,
)

object StunClient {
    /**
     * Perform STUN queries from a single local UDP socket to two self-hosted
     * STUN servers and classify the NAT type based on the reflexive addresses.
     *
     * @param stunAddrs Self-hosted STUN server addresses from AUTH_OK
     *                  (e.g. ["1.2.3.4:3478", "1.2.3.4:3479"]).
     *                  Must contain at least 2 entries for NAT classification.
     *
     * Runs blocking I/O -- call from a background thread / Dispatchers.IO.
     */
    fun discover(stunAddrs: List<String>): StunReport {
        if (stunAddrs.size < 2) {
            Log.w(TAG, "Need at least 2 STUN addresses for NAT detection, got ${stunAddrs.size}")
            return StunReport(emptyList(), "unknown")
        }
        val servers =
            stunAddrs.take(2).map { addr ->
                val parts = addr.split(":")
                InetSocketAddress(parts[0], parts[1].toInt())
            }
        val socket = DatagramSocket()
        socket.soTimeout = STUN_TIMEOUT_MS
        try {
            val localPort = socket.localPort
            val results = servers.map { server -> queryStun(socket, server) }
            val candidates = mutableListOf<ConnectionCandidate>()

            // Add host candidates (local IPs)
            for (addr in getLocalIpv4Addresses()) {
                candidates.add(ConnectionCandidate("host", "$addr:$localPort"))
            }

            // Add srflx candidates from STUN responses
            val reflexiveAddrs = results.mapNotNull { it.reflexiveAddr }
            for (addr in reflexiveAddrs.distinctBy { "${it.address.hostAddress}:${it.port}" }) {
                candidates.add(ConnectionCandidate("srflx", "${addr.address.hostAddress}:${addr.port}"))
            }

            val natType = classifyNat(reflexiveAddrs)
            Log.i(TAG, "NAT type: $natType, candidates: ${candidates.size}")
            return StunReport(candidates, natType)
        } finally {
            socket.close()
        }
    }

    /**
     * Send a STUN Binding Request and parse the response.
     * Retries up to STUN_RETRIES times on timeout.
     */
    private fun queryStun(
        socket: DatagramSocket,
        server: InetSocketAddress,
    ): StunResult {
        val txId = ByteArray(12)
        SecureRandom().nextBytes(txId)
        val request = buildBindingRequest(txId)

        for (attempt in 0 until STUN_RETRIES) {
            try {
                val sendPacket = DatagramPacket(request, request.size, server)
                socket.send(sendPacket)

                val buf = ByteArray(512)
                val recvPacket = DatagramPacket(buf, buf.size)
                socket.receive(recvPacket)

                val mapped = parseBindingResponse(buf, recvPacket.length, txId)
                if (mapped != null) {
                    Log.d(TAG, "STUN $server -> ${mapped.address.hostAddress}:${mapped.port}")
                    return StunResult(mapped, socket.localPort)
                }
            } catch (e: java.net.SocketTimeoutException) {
                Log.w(TAG, "STUN timeout querying $server (attempt ${attempt + 1})")
            }
        }
        Log.w(TAG, "STUN query to $server failed after $STUN_RETRIES attempts")
        return StunResult(null, socket.localPort)
    }

    /** Build a 20-byte STUN Binding Request. */
    private fun buildBindingRequest(transactionId: ByteArray): ByteArray {
        val buf = ByteBuffer.allocate(STUN_HEADER_SIZE)
        buf.putShort(STUN_BINDING_REQUEST)
        buf.putShort(0) // message length (no attributes)
        buf.putInt(STUN_MAGIC_COOKIE)
        buf.put(transactionId)
        return buf.array()
    }

    /** Parse a STUN Binding Response and extract the XOR-MAPPED-ADDRESS. */
    private fun parseBindingResponse(
        data: ByteArray,
        length: Int,
        expectedTxId: ByteArray,
    ): InetSocketAddress? {
        if (length < STUN_HEADER_SIZE) return null
        val buf = ByteBuffer.wrap(data, 0, length)
        val msgType = buf.short
        if (msgType != STUN_BINDING_RESPONSE) return null
        val msgLen = buf.short.toInt() and 0xFFFF
        val cookie = buf.int
        if (cookie != STUN_MAGIC_COOKIE) return null

        val txId = ByteArray(12)
        buf.get(txId)
        if (!txId.contentEquals(expectedTxId)) return null

        // Parse attributes
        val attrEnd = STUN_HEADER_SIZE + msgLen
        var pos = STUN_HEADER_SIZE
        while (pos + 4 <= attrEnd && pos + 4 <= length) {
            val attrType = ((data[pos].toInt() and 0xFF) shl 8) or (data[pos + 1].toInt() and 0xFF)
            val attrLen = ((data[pos + 2].toInt() and 0xFF) shl 8) or (data[pos + 3].toInt() and 0xFF)
            val attrDataStart = pos + 4

            if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS && attrLen >= 8) {
                return parseXorMappedAddress(data, attrDataStart, attrLen)
            }
            if (attrType == STUN_ATTR_MAPPED_ADDRESS && attrLen >= 8) {
                return parseMappedAddress(data, attrDataStart, attrLen)
            }

            // Attributes are padded to 4-byte boundaries
            pos = attrDataStart + ((attrLen + 3) and 0x7FFC.inv())
        }
        return null
    }

    /** Parse XOR-MAPPED-ADDRESS attribute (RFC 5389 section 15.2). */
    private fun parseXorMappedAddress(
        data: ByteArray,
        offset: Int,
        len: Int,
    ): InetSocketAddress? {
        if (len < 8) return null
        val family = data[offset + 1].toInt() and 0xFF
        if (family != 0x01) return null // IPv4 only for now
        val xPort = ((data[offset + 2].toInt() and 0xFF) shl 8) or (data[offset + 3].toInt() and 0xFF)
        val port = xPort xor (STUN_MAGIC_COOKIE ushr 16)
        val xIp = ByteArray(4)
        System.arraycopy(data, offset + 4, xIp, 0, 4)
        val cookieBytes = ByteBuffer.allocate(4).putInt(STUN_MAGIC_COOKIE).array()
        val ip = ByteArray(4)
        for (i in 0..3) ip[i] = (xIp[i].toInt() xor cookieBytes[i].toInt()).toByte()
        return InetSocketAddress(InetAddress.getByAddress(ip), port)
    }

    /** Parse MAPPED-ADDRESS attribute (RFC 5389 section 15.1, fallback). */
    private fun parseMappedAddress(
        data: ByteArray,
        offset: Int,
        len: Int,
    ): InetSocketAddress? {
        if (len < 8) return null
        val family = data[offset + 1].toInt() and 0xFF
        if (family != 0x01) return null
        val port = ((data[offset + 2].toInt() and 0xFF) shl 8) or (data[offset + 3].toInt() and 0xFF)
        val ip = ByteArray(4)
        System.arraycopy(data, offset + 4, ip, 0, 4)
        return InetSocketAddress(InetAddress.getByAddress(ip), port)
    }

    /**
     * Classify the NAT type based on reflexive addresses from two STUN servers.
     *
     * - Same IP + same port from both servers = full_cone (or no NAT)
     * - Same IP + different ports = port_restricted_cone or symmetric
     *   (we classify as symmetric since we can't distinguish without
     *    a full RFC 3489 test, and the server treats both the same way)
     * - Different IPs = symmetric (multiple NAT layers or load-balanced)
     * - Only one response = unknown
     * - No responses = unknown
     */
    private fun classifyNat(responses: List<InetSocketAddress>): String {
        if (responses.size < 2) return "unknown"
        val a = responses[0]
        val b = responses[1]
        val sameIp = a.address.hostAddress == b.address.hostAddress
        val samePort = a.port == b.port
        return when {
            sameIp && samePort -> "full_cone"
            sameIp && !samePort -> "symmetric"
            else -> "symmetric"
        }
    }

    /** Get non-loopback IPv4 addresses for host candidates. */
    private fun getLocalIpv4Addresses(): List<String> {
        val addrs = mutableListOf<String>()
        try {
            for (iface in NetworkInterface.getNetworkInterfaces().toList()) {
                if (iface.isLoopback || !iface.isUp) continue
                for (addr in iface.inetAddresses.toList()) {
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        addr.hostAddress?.let { addrs.add(it) }
                    }
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to enumerate network interfaces: ${e.message}")
        }
        return addrs
    }
}
