package com.dxxredux.app.multiplayer

import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.security.SecureRandom

private const val TAG = "ConnectivityChecker"

// Probe packet: 12 bytes [magic: 4][token: 4][seq: 2][flags: 2]
// The magic value lets us distinguish probes from game traffic.
private const val PROBE_MAGIC: Int = 0x44585043 // "DXPC"
private const val PROBE_SIZE = 12
private const val PROBE_FLAG_REQUEST: Short = 0x0001
private const val PROBE_FLAG_RESPONSE: Short = 0x0002

private const val PROBE_INTERVAL_MS = 200L
private const val TOTAL_TIMEOUT_MS = 3000L

/** Result of connectivity checking for a single peer pair. */
data class ConnectivityResult(
    val peerId: String,
    val winningCandidateType: String, // "host", "srflx", "predicted", "relay"
    val rttMs: Int,
)

object ConnectivityChecker {
    /**
     * Race UDP probes across all candidate pairs. Both sides run this
     * simultaneously: we send REQUEST probes and echo any incoming
     * REQUEST as a RESPONSE back to the sender. The first RESPONSE
     * matching one of our sent probes wins.
     *
     * Runs blocking I/O -- call from Dispatchers.IO.
     *
     * @param pairs Candidate pairs from CONNECTIVITY_CHECK_GO, sorted by priority descending
     * @param token A 4-byte token embedded in outgoing probes so peer can echo it back
     */
    fun probe(
        pairs: List<CandidatePair>,
        token: Int = SecureRandom().nextInt(),
    ): ConnectivityResult? {
        if (pairs.isEmpty()) return null

        val socket = DatagramSocket()
        socket.soTimeout = PROBE_INTERVAL_MS.toInt()
        try {
            val startTime = System.currentTimeMillis()
            var seq: Short = 0
            val sentTimes = mutableMapOf<Short, Pair<Long, CandidatePair>>()

            while (System.currentTimeMillis() - startTime < TOTAL_TIMEOUT_MS) {
                // Send a probe to the next pair in round-robin
                val pairIndex = (seq.toInt() and 0xFFFF) % pairs.size
                val pair = pairs[pairIndex]

                val probePacket = buildProbe(token, seq, PROBE_FLAG_REQUEST)
                val addr = parseAddr(pair.remoteAddr)
                if (addr != null) {
                    val sendTime = System.currentTimeMillis()
                    sentTimes[seq] = Pair(sendTime, pair)
                    try {
                        socket.send(DatagramPacket(probePacket, probePacket.size, addr))
                    } catch (e: Exception) {
                        Log.w(TAG, "Probe send failed to ${pair.remoteAddr}: ${e.message}")
                    }
                }

                // Receive loop: drain available packets until timeout, handling
                // both incoming requests (echo back) and responses (our win).
                val r = receiveAndEcho(socket, token, sentTimes)
                if (r != null) return r

                seq++
                if (seq < 0) seq = 0 // unsigned wrap
            }

            Log.i(TAG, "All ${pairs.size} pairs timed out after ${TOTAL_TIMEOUT_MS}ms")
            return null
        } finally {
            socket.close()
        }
    }

    // Read one packet from the socket. If it's a REQUEST, echo a RESPONSE
    // back to the sender. If it's a RESPONSE matching one of our probes,
    // return a ConnectivityResult.
    private fun receiveAndEcho(
        socket: DatagramSocket,
        ourToken: Int,
        sentTimes: Map<Short, Pair<Long, CandidatePair>>,
    ): ConnectivityResult? {
        try {
            val buf = ByteArray(PROBE_SIZE)
            val pkt = DatagramPacket(buf, buf.size)
            socket.receive(pkt)

            if (pkt.length < PROBE_SIZE) return null
            val bb = ByteBuffer.wrap(buf)
            val magic = bb.int
            if (magic != PROBE_MAGIC) return null
            val pktToken = bb.int
            val pktSeq = bb.short
            val flags = bb.short

            if (flags == PROBE_FLAG_REQUEST) {
                // Echo the peer's probe back as a response, preserving
                // their token and seq so they can match it.
                val resp = buildProbe(pktToken, pktSeq, PROBE_FLAG_RESPONSE)
                val replyAddr = InetSocketAddress(pkt.address, pkt.port)
                try {
                    socket.send(DatagramPacket(resp, resp.size, replyAddr))
                } catch (e: Exception) {
                    Log.w(TAG, "Echo send failed: ${e.message}")
                }
                return null
            }

            if (flags == PROBE_FLAG_RESPONSE && pktToken == ourToken) {
                val entry = sentTimes[pktSeq] ?: return null
                val rtt = (System.currentTimeMillis() - entry.first).toInt()
                val pair = entry.second
                Log.i(TAG, "Connectivity OK: peer=${pair.peerId} type=${pair.remoteType} rtt=${rtt}ms")
                return ConnectivityResult(
                    peerId = pair.peerId,
                    winningCandidateType = pair.remoteType,
                    rttMs = rtt,
                )
            }

            return null
        } catch (_: java.net.SocketTimeoutException) {
            return null
        }
    }

    /** Build a 12-byte probe packet. */
    private fun buildProbe(
        token: Int,
        seq: Short,
        flags: Short,
    ): ByteArray {
        val buf = ByteBuffer.allocate(PROBE_SIZE)
        buf.putInt(PROBE_MAGIC)
        buf.putInt(token)
        buf.putShort(seq)
        buf.putShort(flags)
        return buf.array()
    }

    private fun parseAddr(addr: String): InetSocketAddress? {
        val parts = addr.split(":")
        if (parts.size != 2) return null
        val port = parts[1].toIntOrNull() ?: return null
        return try {
            InetSocketAddress(parts[0], port)
        } catch (e: Exception) {
            Log.w(TAG, "Invalid address: $addr")
            null
        }
    }
}
