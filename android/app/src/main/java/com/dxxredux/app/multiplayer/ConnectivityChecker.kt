package com.dxxredux.app.multiplayer

import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
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
private const val MAX_PROBE_REQUESTS = 15
private const val MAX_PROBE_RECEIVES = 128
private const val MAX_PROBE_ECHOES = 32

/** Numeric IPv4 only: parsing an untrusted probe target must never trigger DNS */
internal fun parseProbeEndpoint(value: String): InetSocketAddress? {
    if (value.length > 21) return null
    val parts = value.split(':')
    if (parts.size != 2) return null
    val port = parts[1].toIntOrNull()?.takeIf { it in 1..65535 } ?: return null
    val octets = parts[0].split('.')
    if (octets.size != 4) return null
    val bytes = ByteArray(4)
    for (index in octets.indices) {
        bytes[index] = (octets[index].toIntOrNull()?.takeIf { it in 0..255 } ?: return null).toByte()
    }
    val address = InetAddress.getByAddress(bytes)
    if (address.isAnyLocalAddress || address.isMulticastAddress || bytes.all { it == (-1).toByte() }) return null
    return InetSocketAddress(address, port)
}

/** Result of connectivity checking for a single peer pair. */
data class ConnectivityResult(
    val peerId: String,
    val winningCandidateType: String, // "host", "srflx", "predicted", "relay"
    val rttMs: Int,
    val checkId: String,
    val remoteAddr: String,
)

object ConnectivityChecker {
    /**
     * Race UDP probes across all candidate pairs. Both sides run this
     * simultaneously: we send REQUEST probes and echo any incoming
     * REQUEST as a RESPONSE back to the sender. The first RESPONSE
     * matching one of our sent probes wins. Keep echoing until the same bounded
     * check deadline so a peer with a later winning candidate can also finish.
     *
     * Runs blocking I/O -- call from Dispatchers.IO.
     *
     * @param pairs Candidate pairs from CONNECTIVITY_CHECK_GO, sorted by priority descending
     * @param token A 4-byte token embedded in outgoing probes so peer can echo it back
     * @param existingSocket If provided, used for probing and NOT closed. Otherwise a
     *   temporary socket is created and closed when done.
     */
    fun probe(
        pairs: List<CandidatePair>,
        token: Int = SecureRandom().nextInt(),
        existingSocket: DatagramSocket? = null,
        checkActive: () -> Unit = {},
        diagnostics: (Map<String, Int>) -> Unit = {},
    ): ConnectivityResult? {
        if (pairs.isEmpty() || pairs.size > 7 * NetworkConstants.MAX_CANDIDATE_PAIRS_PER_PEER) return null

        val ownsSocket = existingSocket == null
        val socket = existingSocket ?: DatagramSocket()
        socket.soTimeout = PROBE_INTERVAL_MS.toInt()
        val counts = mutableMapOf<String, Int>()

        fun count(name: String) {
            counts[name] = (counts[name] ?: 0) + 1
        }
        try {
            val deadline = System.nanoTime() + TOTAL_TIMEOUT_MS * 1_000_000L
            var winner: ConnectivityResult? = null
            val offeredEndpoints = pairs.mapNotNull { parseProbeEndpoint(it.remoteAddr) }.toSet()
            var seq: Short = 0
            val sentTimes = mutableMapOf<Short, Pair<Long, CandidatePair>>()
            var nextRequest = System.nanoTime()

            while (System.nanoTime() < deadline) {
                checkActive()
                if (winner == null && seq < MAX_PROBE_REQUESTS && System.nanoTime() >= nextRequest) {
                    // Send a probe to the next pair in round-robin
                    val pairIndex = (seq.toInt() and 0xFFFF) % pairs.size
                    val pair = pairs[pairIndex]

                    val probePacket = buildProbe(token, seq, PROBE_FLAG_REQUEST)
                    val addr = parseProbeEndpoint(pair.remoteAddr)
                    if (addr != null) {
                        val sendTime = System.nanoTime()
                        sentTimes[seq] = Pair(sendTime, pair)
                        try {
                            socket.send(DatagramPacket(probePacket, probePacket.size, addr))
                            count("sent_$pairIndex")
                        } catch (e: Exception) {
                            count("send_errors")
                            Log.w(TAG, "Probe send failed to ${pair.remoteAddr}: ${e.message}")
                        }
                    }
                    seq++
                    nextRequest = System.nanoTime() + PROBE_INTERVAL_MS * 1_000_000L
                }

                // A local win does not prove the peer has received its own response
                // Continue bounded echo service without sending more requests
                val remainingMs = ((deadline - System.nanoTime()) / 1_000_000L).coerceAtLeast(1L)
                socket.soTimeout = minOf(PROBE_INTERVAL_MS, remainingMs).toInt()
                if ((counts["received"] ?: 0) >= MAX_PROBE_RECEIVES) {
                    Thread.sleep(minOf(PROBE_INTERVAL_MS, remainingMs))
                    continue
                }
                val r =
                    receiveAndEcho(
                        socket,
                        token,
                        sentTimes,
                        offeredEndpoints,
                        winner != null,
                        (counts["echoes"] ?: 0) < MAX_PROBE_ECHOES,
                        ::count,
                    )
                if (winner == null && r != null) winner = r
            }

            checkActive()
            if (winner == null) Log.i(TAG, "All ${pairs.size} pairs timed out after ${TOTAL_TIMEOUT_MS}ms")
            return winner
        } finally {
            if (ownsSocket) socket.close()
            diagnostics(counts.toMap())
        }
    }

    // Read one packet from the socket. If it's a REQUEST, echo a RESPONSE
    // back to the sender. If it's a RESPONSE matching one of our probes,
    // return a ConnectivityResult.
    private fun receiveAndEcho(
        socket: DatagramSocket,
        ourToken: Int,
        sentTimes: Map<Short, Pair<Long, CandidatePair>>,
        offeredEndpoints: Set<InetSocketAddress>,
        alreadyQualified: Boolean,
        canEcho: Boolean,
        count: (String) -> Unit,
    ): ConnectivityResult? {
        try {
            val buf = ByteArray(PROBE_SIZE + 1)
            val pkt = DatagramPacket(buf, buf.size)
            socket.receive(pkt)
            count("received")

            if (pkt.length != PROBE_SIZE) {
                count("short_packets")
                return null
            }
            val bb = ByteBuffer.wrap(buf, 0, PROBE_SIZE)
            val magic = bb.int
            if (magic != PROBE_MAGIC) {
                count("other_packets")
                return null
            }
            val pktToken = bb.int
            val pktSeq = bb.short
            val flags = bb.short

            if (flags == PROBE_FLAG_REQUEST) {
                // An offer must never qualify by reflecting our own request
                if (pktToken == ourToken) {
                    count("self_requests")
                    return null
                }
                if (pkt.socketAddress !in offeredEndpoints || !canEcho) {
                    count("echo_rejected")
                    return null
                }
                count("requests")
                // Echo the peer's probe back as a response, preserving
                // their token and seq so they can match it.
                val resp = buildProbe(pktToken, pktSeq, PROBE_FLAG_RESPONSE)
                val replyAddr = InetSocketAddress(pkt.address, pkt.port)
                try {
                    socket.send(DatagramPacket(resp, resp.size, replyAddr))
                    count("echoes")
                    if (alreadyQualified) count("echoes_after_match")
                } catch (e: Exception) {
                    count("echo_errors")
                    Log.w(TAG, "Echo send failed: ${e.message}")
                }
                return null
            }

            if (flags == PROBE_FLAG_RESPONSE) count("responses")
            if (flags == PROBE_FLAG_RESPONSE && pktToken == ourToken) {
                val entry = sentTimes[pktSeq] ?: return null
                if (pkt.socketAddress != parseProbeEndpoint(entry.second.remoteAddr)) {
                    count("wrong_source")
                    count(
                        if (pkt.socketAddress in
                            offeredEndpoints
                        ) {
                            "wrong_source_offered"
                        } else {
                            "wrong_source_unoffered"
                        },
                    )
                    return null
                }
                count("matched")
                val rtt = ((System.nanoTime() - entry.first) / 1_000_000L).toInt()
                val pair = entry.second
                Log.i(TAG, "Connectivity OK: peer=${pair.peerId} type=${pair.remoteType} rtt=${rtt}ms")
                return ConnectivityResult(
                    peerId = pair.peerId,
                    winningCandidateType = pair.remoteType,
                    rttMs = rtt,
                    checkId = pair.checkId,
                    remoteAddr = pair.remoteAddr,
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
}
