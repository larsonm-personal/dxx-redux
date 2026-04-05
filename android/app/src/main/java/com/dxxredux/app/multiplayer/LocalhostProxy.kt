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
import java.util.concurrent.atomic.AtomicBoolean

private const val TAG = "LocalhostProxy"

// Constants shared between LocalhostProxy and PeerProxy
private const val MAX_PACKET_SIZE = 1500
private const val ENGINE_PORT = 42424
private const val RELAY_HEADER_LEN = 5
private const val KEEPALIVE_INTERVAL_MS = 15_000L

// Connectivity probe constants (must match ConnectivityChecker)
private const val PROBE_MAGIC: Int = 0x44585043 // "DXPC"
private const val PROBE_SIZE = 12
private const val PROBE_FLAG_REQUEST: Short = 0x0001
private const val PROBE_FLAG_RESPONSE: Short = 0x0002

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

    // When active, the shared receive loop echoes DXPC probe requests from
    // unknown senders (for late-join NAT holepunching).
    private val lateJoinProbeActive = AtomicBoolean(false)

    init {
        if (sharedRealSocket != null) {
            jobs.add(
                scope.launch(Dispatchers.IO) {
                    try {
                        sharedReceiveLoop()
                        Log.w(TAG, "sharedReceiveLoop returned normally")
                    } catch (e: kotlinx.coroutines.CancellationException) {
                        Log.w(TAG, "sharedReceiveLoop cancelled")
                        throw e
                    } catch (e: Exception) {
                        Log.e(TAG, "sharedReceiveLoop CRASHED: ${e.javaClass.simpleName}: ${e.message}", e)
                    }
                },
            )
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

        jobs.add(
            scope.launch(Dispatchers.IO) {
                try {
                    proxy.run()
                    Log.w(TAG, "proxy.run() returned normally for slot=${peerConfig.peerSlot}")
                } catch (e: kotlinx.coroutines.CancellationException) {
                    Log.w(TAG, "proxy.run() cancelled for slot=${peerConfig.peerSlot}")
                    throw e
                } catch (e: Exception) {
                    Log.e(
                        TAG,
                        "proxy.run() CRASHED for slot=${peerConfig.peerSlot}: ${e.javaClass.simpleName}: ${e.message}",
                        e,
                    )
                }
            },
        )
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

                // Unmatched: check for late-join probe echo
                if (lateJoinProbeActive.get() && pkt.length == PROBE_SIZE) {
                    val bb = ByteBuffer.wrap(pkt.data, 0, PROBE_SIZE)
                    if (bb.int == PROBE_MAGIC) {
                        val token = bb.int
                        val seq = bb.short
                        val flags = bb.short
                        if (flags == PROBE_FLAG_REQUEST) {
                            // Echo back as RESPONSE
                            val resp = ByteArray(PROBE_SIZE)
                            val rb = ByteBuffer.wrap(resp)
                            rb.putInt(PROBE_MAGIC)
                            rb.putInt(token)
                            rb.putShort(seq)
                            rb.putShort(PROBE_FLAG_RESPONSE)
                            val dest = InetSocketAddress(pkt.address, pkt.port)
                            try {
                                socket.send(DatagramPacket(resp, PROBE_SIZE, dest))
                            } catch (_: Exception) {
                            }
                            continue
                        }
                    }
                }
                // Unmatched: stale probe, connectivity echo, or unknown sender
                Log.w(
                    TAG,
                    "Unmatched packet from $senderKey len=${pkt.length} relaySlots=${relayPeersBySlot.keys} directAddrs=${directPeersByAddr.keys}",
                )
            } catch (_: java.net.SocketTimeoutException) {
                // Socket may have a residual soTimeout from STUN/connectivity
                // probing. Just continue; the socket is still usable.
                continue
            } catch (e: java.net.SocketException) {
                if (e.message?.contains("closed") == true) {
                    Log.w(TAG, "sharedReceiveLoop: socket closed, exiting")
                    break
                }
                Log.w(TAG, "Shared receive error: ${e.message}")
            }
        }
        Log.w(
            TAG,
            "sharedReceiveLoop: while-loop exited (job.isActive=${kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive})",
        )
    }

    fun getStats(): List<PeerProxyStats> = peerProxies.map { it.getStats() }

    /**
     * Enable probe echo on the shared socket and send blind probes to the
     * given addresses. This opens NAT pinholes for a late-joining player
     * and lets their ConnectivityChecker receive echo responses.
     */
    fun sendLateJoinProbes(addrs: List<String>) {
        val socket = sharedRealSocket ?: return
        lateJoinProbeActive.set(true)
        val token = java.security.SecureRandom().nextInt()
        scope.launch(Dispatchers.IO) {
            for (addrStr in addrs) {
                val parts = addrStr.split(":")
                if (parts.size != 2) continue
                val port = parts[1].toIntOrNull() ?: continue
                try {
                    val addr = InetSocketAddress(InetAddress.getByName(parts[0]), port)
                    val buf = ByteArray(PROBE_SIZE)
                    val bb = ByteBuffer.wrap(buf)
                    bb.putInt(PROBE_MAGIC)
                    bb.putInt(token)
                    bb.putShort(0) // seq
                    bb.putShort(PROBE_FLAG_REQUEST) // flags
                    socket.send(DatagramPacket(buf, PROBE_SIZE, addr))
                } catch (e: Exception) {
                    Log.w(TAG, "Late-join blind probe failed to $addrStr: ${e.message}")
                }
            }
            Log.i(TAG, "Sent ${addrs.size} late-join blind probes")
        }
    }

    fun shutdown() {
        val trace = Throwable("shutdown caller").stackTraceToString()
        Log.w(TAG, "Proxy shutdown called, peers=${peerProxies.size} jobs=${jobs.size}\n$trace")
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
            if (packetsReceived <= 5 ||
                (packetsReceived <= 100 && packetsReceived % 20 == 0L) ||
                packetsReceived % 500 == 0L
            ) {
                Log.i(
                    TAG,
                    "real->local slot=${config.peerSlot} #$packetsReceived ${payloadLen}b relay=${config.isRelay}",
                )
            }
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
                if (packetsSent <= 5 || (packetsSent <= 100 && packetsSent % 20 == 0L) || packetsSent % 500 == 0L) {
                    Log.i(
                        TAG,
                        "local->real slot=${config.peerSlot} #$packetsSent ${pkt.length}b -> ${config.realAddr} relay=${config.isRelay}",
                    )
                }
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
                val msg = e.message ?: ""
                if ("closed" in msg || "EPERM" in msg || "Operation not permitted" in msg) break
                Log.w(TAG, "local->real error: $msg")
            } catch (e: Exception) {
                Log.e(
                    TAG,
                    "local->real UNEXPECTED slot=${config.peerSlot} #$packetsSent: ${e.javaClass.simpleName}: ${e.message}",
                )
            }
        }
        Log.w(TAG, "forwardLocalToReal EXITED slot=${config.peerSlot} sent=$packetsSent")
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
                val msg = e.message ?: ""
                if ("closed" in msg || "EPERM" in msg || "Operation not permitted" in msg) break
                Log.w(TAG, "real->local error: $msg")
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
        var keepaliveCount = 0L
        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                kotlinx.coroutines.delay(KEEPALIVE_INTERVAL_MS)
                realSocket.send(DatagramPacket(ping, ping.size, config.realAddr))
                keepaliveCount++
                Log.i(TAG, "keepalive slot=${config.peerSlot} #$keepaliveCount sent=$packetsSent recv=$packetsReceived")
            } catch (e: java.net.SocketException) {
                val msg = e.message ?: ""
                if ("closed" in msg || "EPERM" in msg || "Operation not permitted" in msg) {
                    Log.w(TAG, "keepalive slot=${config.peerSlot} socket closed after #$keepaliveCount")
                    break
                }
                Log.w(TAG, "keepalive error: $msg")
            }
        }
        Log.w(TAG, "keepalive loop EXITED slot=${config.peerSlot} count=$keepaliveCount")
    }

    fun close() {
        localSocket.close()
        if (ownsRealSocket) {
            realSocket.close()
        }
    }
}
