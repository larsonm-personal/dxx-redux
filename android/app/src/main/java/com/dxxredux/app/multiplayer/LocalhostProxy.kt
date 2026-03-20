package com.dxxredux.app.multiplayer

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.ConcurrentHashMap

private const val TAG = "LocalhostProxy"

// Constants shared between LocalhostProxy and PeerProxy
private const val MAX_PACKET_SIZE = 1500
private const val ENGINE_PORT = 42424
private const val RELAY_HEADER_LEN = 5
private const val KEEPALIVE_INTERVAL_MS = 15_000L

/**
 * UDP proxy between the game engine (localhost:42424) and remote peers.
 *
 * Each peer gets a dedicated local port (42430 + peerIndex). The engine
 * sees peers at 127.0.0.1:4243x and sends/recvs normally. The proxy
 * forwards traffic to real addresses (direct or via relay).
 *
 * When [sharedRealSocket] is provided, all peers send and receive through
 * a single shared socket. Incoming packets are demultiplexed by source
 * address (direct peers) or relay header from_slot (relay peers). This
 * keeps UPnP port mappings and NAT pinholes valid for all peers.
 *
 * Relay wrapping: [session_token:4LE][dest_slot:1][payload]
 * Relay unwrapping: strips the 5-byte [token:4LE][from_slot:1] header.
 */
data class PeerProxyStats(
    val peerSlot: Int,
    val packetsSent: Long,
    val packetsReceived: Long,
    val bytesSent: Long,
    val bytesReceived: Long,
)

class LocalhostProxy(
    private val scope: CoroutineScope,
    private val sharedRealSocket: DatagramSocket? = null,
) {
    private val peerProxies = mutableListOf<PeerProxy>()
    private val jobs = mutableListOf<Job>()

    // Demux maps for shared-socket mode (concurrent for thread safety with receiver)
    private val directPeersByAddr = ConcurrentHashMap<String, PeerProxy>()
    private val relayPeersBySlot = ConcurrentHashMap<Int, PeerProxy>()

    init {
        if (sharedRealSocket != null) {
            jobs.add(scope.launch(Dispatchers.IO) { sharedReceiveLoop() })
        }
    }

    fun addPeer(peerConfig: PeerProxyConfig) {
        val realSocket: DatagramSocket
        val ownsRealSocket: Boolean
        if (sharedRealSocket != null) {
            realSocket = sharedRealSocket
            ownsRealSocket = false
        } else {
            realSocket = DatagramSocket()
            ownsRealSocket = true
        }

        val proxy =
            try {
                PeerProxy(peerConfig, realSocket, ownsRealSocket)
            } catch (e: java.net.BindException) {
                Log.e(TAG, "Failed to bind port ${peerConfig.localPort} for slot ${peerConfig.peerSlot}: ${e.message}")
                if (ownsRealSocket) realSocket.close()
                return
            }
        peerProxies.add(proxy)

        // Register in demux maps when using shared socket
        if (sharedRealSocket != null) {
            if (peerConfig.isRelay) {
                relayPeersBySlot[peerConfig.peerSlot] = proxy
            } else {
                val addrKey = "${peerConfig.realAddr.address.hostAddress}:${peerConfig.realAddr.port}"
                directPeersByAddr[addrKey] = proxy
            }
        }

        jobs.add(scope.launch(Dispatchers.IO) { proxy.run() })
        Log.i(
            TAG,
            "Added peer proxy: slot=${peerConfig.peerSlot} " +
                "local=127.0.0.1:${peerConfig.localPort} -> ${peerConfig.realAddr} " +
                "relay=${peerConfig.isRelay} shared=${sharedRealSocket != null}",
        )
    }

    /**
     * Single receive loop for shared-socket mode. Reads all incoming packets
     * and dispatches to the correct PeerProxy by source address (direct) or
     * relay header from_slot (relay).
     */
    private suspend fun sharedReceiveLoop() {
        val socket = sharedRealSocket ?: return
        val buf = ByteArray(MAX_PACKET_SIZE)
        val pkt = DatagramPacket(buf, buf.size)

        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                socket.receive(pkt)
                val senderKey = "${pkt.address.hostAddress}:${pkt.port}"

                // Direct peer: match by source address
                val directProxy = directPeersByAddr[senderKey]
                if (directProxy != null) {
                    directProxy.deliverIncoming(pkt.data, pkt.length)
                    continue
                }

                // Relay peer: parse from_slot from relay header
                if (pkt.length >= RELAY_HEADER_LEN) {
                    val fromSlot = pkt.data[4].toInt() and 0xFF
                    val relayProxy = relayPeersBySlot[fromSlot]
                    if (relayProxy != null) {
                        relayProxy.deliverIncoming(pkt.data, pkt.length)
                        continue
                    }
                }

                // Unmatched: stale probe, connectivity echo, or unknown sender
            } catch (_: java.net.SocketTimeoutException) {
                // Socket may have a residual soTimeout from STUN/connectivity
                // probing. Just continue; the socket is still usable.
                continue
            } catch (e: java.net.SocketException) {
                if (e.message?.contains("closed") == true) break
                Log.w(TAG, "Shared receive error: ${e.message}")
            }
        }
    }

    fun getStats(): List<PeerProxyStats> = peerProxies.map { it.getStats() }

    fun shutdown() {
        // C10: close sockets first to unblock receive() calls, then cancel jobs
        for (proxy in peerProxies) proxy.close()
        sharedRealSocket?.close()
        for (job in jobs) job.cancel()
        jobs.clear()
        peerProxies.clear()
        directPeersByAddr.clear()
        relayPeersBySlot.clear()
        Log.i(TAG, "Proxy shutdown complete")
    }
}

data class PeerProxyConfig(
    val peerSlot: Int,
    val localPort: Int,
    val realAddr: InetSocketAddress,
    val isRelay: Boolean,
    val relayToken: UInt = 0u,
    val relayDestSlot: Int = 0,
)

/**
 * Bi-directional UDP forwarder for one peer.
 *
 * localSocket: bound to 127.0.0.1:localPort, exchanges with engine at :42424
 * realSocket: shared or per-proxy socket, exchanges with peer's real address (or relay)
 *
 * When [ownsRealSocket] is false, the parent LocalhostProxy handles incoming
 * packets via [deliverIncoming] and closes the socket on shutdown.
 */
private class PeerProxy(
    private val config: PeerProxyConfig,
    private val realSocket: DatagramSocket,
    private val ownsRealSocket: Boolean,
) {
    private val loopback: InetAddress = InetAddress.getByName("127.0.0.1")
    private val localSocket = DatagramSocket(config.localPort, loopback)

    @Volatile
    var packetsSent: Long = 0L

    @Volatile
    var packetsReceived: Long = 0L

    @Volatile
    var bytesSent: Long = 0L

    @Volatile
    var bytesReceived: Long = 0L

    fun getStats() = PeerProxyStats(config.peerSlot, packetsSent, packetsReceived, bytesSent, bytesReceived)

    suspend fun run() {
        kotlinx.coroutines.coroutineScope {
            // local -> real: engine sends to our local port, we forward to peer
            launch { forwardLocalToReal() }
            // real -> local: only when we own the socket (non-shared mode)
            if (ownsRealSocket) {
                launch { forwardRealToLocal() }
            }
            // NAT keepalive (direct and relay)
            launch { keepalive() }
        }
    }

    /**
     * Called by LocalhostProxy's shared receive loop to deliver an incoming
     * packet that has already been demuxed to this peer.
     */
    fun deliverIncoming(
        data: ByteArray,
        length: Int,
    ) {
        try {
            val payload: ByteArray
            val payloadLen: Int
            if (config.isRelay) {
                if (length < RELAY_HEADER_LEN) return
                payloadLen = length - RELAY_HEADER_LEN
                payload = data.copyOfRange(RELAY_HEADER_LEN, length)
            } else {
                payloadLen = length
                payload = data.copyOfRange(0, length)
            }
            localSocket.send(DatagramPacket(payload, payloadLen, InetSocketAddress(loopback, ENGINE_PORT)))
            packetsReceived++
            bytesReceived += payloadLen
        } catch (e: java.net.SocketException) {
            if (e.message?.contains("closed") != true) {
                Log.w(TAG, "deliverIncoming error slot=${config.peerSlot}: ${e.message}")
            }
        }
    }

    private suspend fun forwardLocalToReal() {
        val buf = ByteArray(MAX_PACKET_SIZE)
        val pkt = DatagramPacket(buf, buf.size)
        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                localSocket.receive(pkt)
                packetsSent++
                bytesSent += pkt.length
                if (config.isRelay) {
                    // Wrap: [token:4LE][dest_slot:1][payload]
                    val wrapped =
                        ByteBuffer
                            .allocate(RELAY_HEADER_LEN + pkt.length)
                            .apply {
                                order(ByteOrder.LITTLE_ENDIAN)
                                putInt(config.relayToken.toInt())
                                put(config.relayDestSlot.toByte())
                                put(pkt.data, 0, pkt.length)
                            }.array()
                    realSocket.send(DatagramPacket(wrapped, wrapped.size, config.realAddr))
                } else {
                    realSocket.send(DatagramPacket(pkt.data, pkt.length, config.realAddr))
                }
            } catch (e: java.net.SocketException) {
                if (e.message?.contains("closed") == true) break
                Log.w(TAG, "local->real error: ${e.message}")
            }
        }
    }

    private suspend fun forwardRealToLocal() {
        val buf = ByteArray(MAX_PACKET_SIZE)
        val pkt = DatagramPacket(buf, buf.size)
        val engineAddr = InetSocketAddress(loopback, ENGINE_PORT)
        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                realSocket.receive(pkt)
                val payload: ByteArray
                val payloadLen: Int
                if (config.isRelay) {
                    if (pkt.length < RELAY_HEADER_LEN) continue
                    // Unwrap: skip [token:4][from_slot:1]
                    payloadLen = pkt.length - RELAY_HEADER_LEN
                    payload = pkt.data.copyOfRange(RELAY_HEADER_LEN, pkt.length)
                } else {
                    payloadLen = pkt.length
                    payload = pkt.data.copyOfRange(0, pkt.length)
                }
                localSocket.send(DatagramPacket(payload, payloadLen, engineAddr))
                packetsReceived++
                bytesReceived += payloadLen
            } catch (e: java.net.SocketException) {
                if (e.message?.contains("closed") == true) break
                Log.w(TAG, "real->local error: ${e.message}")
            }
        }
    }

    private suspend fun keepalive() {
        val ping =
            if (config.isRelay) {
                // Relay-wrapped keepalive: [token:4LE][dest_slot:1][0x00]
                ByteBuffer
                    .allocate(RELAY_HEADER_LEN + 1)
                    .apply {
                        order(ByteOrder.LITTLE_ENDIAN)
                        putInt(config.relayToken.toInt())
                        put(config.relayDestSlot.toByte())
                        put(0.toByte())
                    }.array()
            } else {
                byteArrayOf(0)
            }
        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                kotlinx.coroutines.delay(KEEPALIVE_INTERVAL_MS)
                realSocket.send(DatagramPacket(ping, ping.size, config.realAddr))
            } catch (e: java.net.SocketException) {
                if (e.message?.contains("closed") == true) break
                Log.w(TAG, "keepalive error: ${e.message}")
            }
        }
    }

    fun close() {
        localSocket.close()
        if (ownsRealSocket) {
            realSocket.close()
        }
    }
}
