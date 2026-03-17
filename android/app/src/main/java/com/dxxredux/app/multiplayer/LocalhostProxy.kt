package com.dxxredux.app.multiplayer

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder

private const val TAG = "LocalhostProxy"

/**
 * UDP proxy between the game engine (localhost:42424) and remote peers.
 *
 * Each peer gets a dedicated local port (42430 + peerIndex). The engine
 * sees peers at 127.0.0.1:4243x and sends/recvs normally. The proxy
 * forwards traffic to real addresses (direct or via relay).
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
) {
    private val peerProxies = mutableListOf<PeerProxy>()
    private val jobs = mutableListOf<Job>()

    fun addPeer(peerConfig: PeerProxyConfig) {
        val proxy = PeerProxy(peerConfig)
        peerProxies.add(proxy)
        jobs.add(scope.launch(Dispatchers.IO) { proxy.run() })
        Log.i(
            TAG,
            "Added peer proxy: slot=${peerConfig.peerSlot} " +
                "local=127.0.0.1:${peerConfig.localPort} -> ${peerConfig.realAddr} " +
                "relay=${peerConfig.isRelay}",
        )
    }

    fun getStats(): List<PeerProxyStats> = peerProxies.map { it.getStats() }

    fun shutdown() {
        for (job in jobs) job.cancel()
        for (proxy in peerProxies) proxy.close()
        jobs.clear()
        peerProxies.clear()
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
 * realSocket: ephemeral port, exchanges with peer's real address (or relay)
 */
private class PeerProxy(
    private val config: PeerProxyConfig,
) {
    private val loopback: InetAddress = InetAddress.getByName("127.0.0.1")
    private val localSocket = DatagramSocket(config.localPort, loopback)
    private val realSocket = DatagramSocket()

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
        val scope =
            kotlinx.coroutines.coroutineScope {
                // local -> real: engine sends to our local port, we forward to peer
                launch { forwardLocalToReal() }
                // real -> local: peer sends to our real socket, we forward to engine
                launch { forwardRealToLocal() }
                // NAT keepalive for direct connections
                if (!config.isRelay) {
                    launch { keepalive() }
                }
            }
    }

    private suspend fun forwardLocalToReal() {
        val buf = ByteArray(MAX_PACKET_SIZE)
        val pkt = DatagramPacket(buf, buf.size)
        val scope = kotlinx.coroutines.currentCoroutineContext()
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
        val ping = byteArrayOf(0)
        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                kotlinx.coroutines.delay(KEEPALIVE_INTERVAL_MS)
                realSocket.send(DatagramPacket(ping, 1, config.realAddr))
            } catch (e: java.net.SocketException) {
                if (e.message?.contains("closed") == true) break
                Log.w(TAG, "keepalive error: ${e.message}")
            }
        }
    }

    fun close() {
        localSocket.close()
        realSocket.close()
    }

    companion object {
        const val MAX_PACKET_SIZE = 1500
        const val ENGINE_PORT = 42424
        const val RELAY_HEADER_LEN = 5
        const val KEEPALIVE_INTERVAL_MS = 15_000L
    }
}
