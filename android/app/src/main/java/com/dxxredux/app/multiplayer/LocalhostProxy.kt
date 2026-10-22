package com.dxxredux.app.multiplayer

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import java.io.IOException
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
// Must be >= UPID_MAX_SIZE (4096) in d1/main/net_udp.h and d2/main/net_udp.h
private const val MAX_PACKET_SIZE = 4096
private const val ENGINE_PORT = 42424
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
 * Relay packets use the authenticated protocol in RelayTransport.
 */
data class PeerProxyStats(
    val peerSlot: Int,
    val packetsSent: Long,
    val packetsReceived: Long,
    val bytesSent: Long,
    val bytesReceived: Long,
    val relayBound: Boolean = true,
    val gameplayReceived: Long = 0,
    val isRelay: Boolean = false,
)

class LocalhostProxy(
    private val scope: CoroutineScope,
    private val sharedRealSocket: DatagramSocket? = null,
    private val allowDynamicPeers: Boolean = false,
    private val onFailure: (String) -> Unit = {},
) {
    companion object {
        // Mirror D1/D2 main/net_udp.h; the host producer-conformance test reads both headers
        internal const val UPID_PDATA = 16
        internal const val UPID_MDATA_PNORM = 17
        internal const val UPID_MDATA_PNEEDACK = 18
        private val activeWorkers =
            java.util.concurrent.atomic
                .AtomicInteger()

        fun activeWorkerCount(): Int = activeWorkers.get()
    }

    private val diagnosticId = ProxyDiagnostics.nextId()
    private val diagnostics = ProxyDiagnostics("proxy=$diagnosticId")
    private val peerProxies = mutableListOf<PeerProxy>()
    private val jobs = mutableListOf<Job>()
    private val peerJobs = mutableMapOf<Int, Job>()
    private var peerIdentities = emptyMap<Int, String>()
    private val retiredPorts = mutableSetOf<Int>()
    private val stopped = AtomicBoolean(false)
    private val failureReported = AtomicBoolean(false)
    private var started = false

    // Demux maps for shared-socket mode (concurrent for thread safety with receiver)
    private val directPeersByAddr = ConcurrentHashMap<String, PeerProxy>()
    private val relayPeersBySlot = ConcurrentHashMap<Int, PeerProxy>()

    // Next slot for dynamic peer allocation when allowDynamicPeers is set
    private var nextDynamicSlot = 0
    private val maxDynamicPeers = 8

    // When active, the shared receive loop echoes DXPC probe requests from
    // unknown senders (for late-join NAT holepunching).
    private val lateJoinProbeActive = AtomicBoolean(false)

    init {
        diagnostics.event(
            "created dynamic=$allowDynamicPeers shared=[${sharedRealSocket?.let(ProxyDiagnostics::socketState)}]",
        )
        if (sharedRealSocket != null) {
            launchWorker("shared receiver") { sharedReceiveLoop() }
        }
    }

    @Synchronized
    fun start() {
        check(!stopped.get()) { "Proxy is closed" }
        started = true
        diagnostics.event("started peers=${peerProxies.size}")
        jobs.forEach { it.start() }
    }

    @Synchronized
    private fun launchWorker(
        name: String,
        expectedStop: () -> Boolean = { false },
        block: suspend () -> Unit,
    ): Job {
        val job =
            scope.launch(Dispatchers.IO, start = CoroutineStart.LAZY) {
                activeWorkers.incrementAndGet()
                try {
                    block()
                    if (!stopped.get() && !expectedStop()) throw IOException("$name stopped unexpectedly")
                } catch (e: kotlinx.coroutines.CancellationException) {
                    throw e
                } catch (e: Exception) {
                    Log.e(TAG, "Proxy $name failed", e)
                    diagnostics.failure("worker $name", sharedRealSocket, e)
                    if (!stopped.get() && !expectedStop() && failureReported.compareAndSet(false, true)) {
                        shutdown()
                        onFailure("Network forwarding stopped: ${e.message ?: name}. Return to the lobby and retry.")
                    }
                } finally {
                    activeWorkers.decrementAndGet()
                }
            }
        jobs.add(job)
        job.invokeOnCompletion { synchronized(this) { jobs.remove(job) } }
        if (started) job.start()
        return job
    }

    @Synchronized
    private fun isCurrentPeer(proxy: PeerProxy) = peerProxies.any { it === proxy }

    @Synchronized
    fun reconcileRoster(identities: Map<Int, String>) {
        require(identities.keys.all { it in 0..7 } && identities.values.toSet().size == identities.size)
        val departed =
            peerProxies
                .filter { proxy ->
                    proxy.slot !in identities ||
                        (peerIdentities[proxy.slot] != null && peerIdentities[proxy.slot] != identities[proxy.slot])
                }.map { it.slot }
        departed.forEach { removePeer(it) }
        peerIdentities = identities.toMap()
    }

    @Synchronized
    fun removePeer(slot: Int) {
        val proxy = peerProxies.firstOrNull { it.slot == slot } ?: return
        peerProxies.remove(proxy)
        retiredPorts.add(proxy.localPort)
        relayPeersBySlot.remove(slot, proxy)
        directPeersByAddr.entries.removeIf { it.value === proxy }
        peerJobs.remove(slot)?.let { job ->
            jobs.remove(job)
            job.cancel()
        }
        proxy.close("peer departed")
    }

    private fun createPeer(
        config: PeerProxyConfig,
        real: DatagramSocket,
        ownsReal: Boolean,
    ): PeerProxy {
        // Closing a socket wakes blocked receivers asynchronously on some JVMs
        val retryUntil = if (retiredPorts.remove(config.localPort)) System.nanoTime() + 250_000_000L else 0L

        fun bindLocal(): DatagramSocket {
            while (true) {
                try {
                    return DatagramSocket(config.localPort, InetAddress.getByName("127.0.0.1"))
                } catch (e: java.net.BindException) {
                    if (System.nanoTime() >= retryUntil) throw e
                    Thread.sleep(5)
                }
            }
        }
        val local = bindLocal()
        try {
            return PeerProxy(config, real, ownsReal, diagnosticId, local)
        } catch (e: Exception) {
            local.close()
            throw e
        }
    }

    @Synchronized
    fun addPeer(peerConfig: PeerProxyConfig) {
        check(!stopped.get()) { "Proxy is closed" }
        val existing = peerProxies.firstOrNull { it.slot == peerConfig.peerSlot }
        if (existing?.matches(peerConfig) == true) return
        removePeer(peerConfig.peerSlot)
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
                createPeer(peerConfig, realSocket, ownsRealSocket)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to bind port ${peerConfig.localPort} for slot ${peerConfig.peerSlot}: ${e.message}")
                if (ownsRealSocket) realSocket.close()
                throw IOException("Could not open local game port ${peerConfig.localPort}. Please retry.", e)
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

        peerJobs[peerConfig.peerSlot] =
            launchWorker("peer ${peerConfig.peerSlot}", { !isCurrentPeer(proxy) }) { proxy.run() }
        Log.i(
            TAG,
            "Added peer proxy: slot=${peerConfig.peerSlot} " +
                "local=127.0.0.1:${peerConfig.localPort} -> ${peerConfig.realAddr} " +
                "relay=${peerConfig.isRelay} shared=${sharedRealSocket != null}",
        )
    }

    /**
     * Host mode: dynamically register a PeerProxy for a new source address.
     * Called from sharedReceiveLoop when an unknown sender is detected.
     * Returns the new PeerProxy or null on failure.
     */
    @Synchronized
    private fun addDynamicPeer(realAddr: InetSocketAddress): PeerProxy? {
        if (stopped.get()) return null
        val slot = nextDynamicSlot++
        val localPort = NetworkConstants.PROXY_PORT_BASE + slot
        val config =
            PeerProxyConfig(
                peerSlot = slot,
                localPort = localPort,
                realAddr = realAddr,
                isRelay = false,
            )
        val proxy =
            try {
                createPeer(config, sharedRealSocket!!, false)
            } catch (e: java.net.BindException) {
                Log.e(TAG, "Host-mode: failed to bind port $localPort for dynamic peer $realAddr: ${e.message}")
                throw e
            }
        peerProxies.add(proxy)
        val addrKey = "${realAddr.address.hostAddress}:${realAddr.port}"
        directPeersByAddr[addrKey] = proxy
        // Start the local->real forwarding coroutine
        peerJobs[slot] = launchWorker("dynamic peer $slot", { !isCurrentPeer(proxy) }) { proxy.run() }
        Log.i(TAG, "Host-mode: dynamic peer slot=$slot local=127.0.0.1:$localPort -> $addrKey")
        return proxy
    }

    /**
     * Single receive loop for shared-socket mode. Reads all incoming packets
     * and dispatches to the correct PeerProxy by source address (direct) or
     * relay header from_slot (relay).
     */
    private suspend fun sharedReceiveLoop() {
        val socket = sharedRealSocket ?: return
        val buf = ByteArray(RelayTransport.MAX_PACKET + 1)
        val pkt = DatagramPacket(buf, buf.size)

        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                diagnostics.receive("shared-network-receive", socket, pkt)
                val senderKey = "${pkt.address.hostAddress}:${pkt.port}"

                // Direct peer: match by source address
                val directProxy = directPeersByAddr[senderKey]
                if (directProxy != null) {
                    directProxy.deliverIncoming(pkt.data, pkt.length)
                    continue
                }

                // Endpoint, allocation keys and replay window are checked before delivery
                val relaySlot = RelayTransport.senderSlot(pkt.data, pkt.length)
                val relayProxy = relaySlot?.let { relayPeersBySlot[it] }
                if (relayProxy != null) {
                    relayProxy.deliverIncoming(pkt.data, pkt.length, InetSocketAddress(pkt.address, pkt.port))
                    continue
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
                                diagnostics.send("probe-response-send", socket, DatagramPacket(resp, PROBE_SIZE, dest))
                            } catch (_: Exception) {
                            }
                            continue
                        }
                    }
                }

                // Dynamically create a PeerProxy for unknown senders
                if (allowDynamicPeers && peerProxies.size < maxDynamicPeers) {
                    val peer = addDynamicPeer(InetSocketAddress(pkt.address, pkt.port))
                    if (peer != null) {
                        peer.deliverIncoming(pkt.data, pkt.length)
                        continue
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

    @Synchronized
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
        launchWorker("late-join probes", { true }) {
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
                    diagnostics.send("probe-send", socket, DatagramPacket(buf, PROBE_SIZE, addr))
                } catch (e: Exception) {
                    Log.w(TAG, "Late-join blind probe failed to $addrStr: ${e.message}")
                }
            }
            Log.i(TAG, "Sent ${addrs.size} late-join blind probes")
        }
    }

    @Synchronized
    fun shutdown() {
        if (stopped.getAndSet(true)) return
        val trace = Throwable("shutdown caller").stackTraceToString()
        Log.w(TAG, "Proxy shutdown called, peers=${peerProxies.size} jobs=${jobs.size}\n$trace")
        diagnostics.event("shutdown peers=${peerProxies.size} jobs=${jobs.size}\n$trace")
        sharedRealSocket?.let { diagnostics.closing("proxy shutdown", it) }
        // C10: close sockets first to unblock receive() calls, then cancel jobs
        for (proxy in peerProxies) proxy.close("proxy shutdown")
        sharedRealSocket?.close()
        for (job in jobs.toList()) job.cancel()
        jobs.clear()
        peerJobs.clear()
        peerIdentities = emptyMap()
        retiredPorts.clear()
        peerProxies.clear()
        directPeersByAddr.clear()
        relayPeersBySlot.clear()
        Log.i(TAG, "Proxy shutdown complete")
        diagnostics.event("shutdown complete")
    }
}

data class PeerProxyConfig(
    val peerSlot: Int,
    val localPort: Int,
    val realAddr: InetSocketAddress,
    val isRelay: Boolean,
    val relayToken: UInt = 0u,
    val relayDestSlot: Int = 0,
    val relayOwnSlot: Int = -1,
    val relaySendKey: String? = null,
    val relayReceiveKey: String? = null,
    val admissionId: String? = null,
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
    diagnosticId: Long,
    private val localSocket: DatagramSocket,
) {
    val localPort: Int get() = config.localPort
    val slot: Int get() = config.peerSlot

    fun matches(other: PeerProxyConfig): Boolean = config == other

    private val relay =
        if (config.isRelay) {
            RelayTransport(
                config.relayToken,
                config.relayOwnSlot,
                config.peerSlot,
                requireNotNull(config.relaySendKey),
                requireNotNull(config.relayReceiveKey),
            )
        } else {
            null
        }

    private val loopback: InetAddress = InetAddress.getByName("127.0.0.1")
    private val diagnostics = ProxyDiagnostics("proxy=$diagnosticId peer=${config.peerSlot} remote=${config.realAddr}")

    init {
        diagnostics.event(
            "peer created relay=${config.isRelay} owns_real=$ownsRealSocket " +
                "engine=[${ProxyDiagnostics.socketState(
                    localSocket,
                )}] network=[${ProxyDiagnostics.socketState(realSocket)}]",
        )
    }

    @Volatile
    var packetsSent: Long = 0L

    @Volatile
    var packetsReceived: Long = 0L

    @Volatile
    var bytesSent: Long = 0L

    @Volatile
    var bytesReceived: Long = 0L

    @Volatile
    var pdataForwarded: Long = 0L

    @Volatile
    var mdataForwarded: Long = 0L

    fun getStats() =
        PeerProxyStats(
            config.peerSlot,
            packetsSent,
            packetsReceived,
            bytesSent,
            bytesReceived,
            relayBound =
                relay?.bound ?: true,
            gameplayReceived = pdataForwarded + mdataForwarded,
            isRelay = config.isRelay,
        )

    suspend fun run() {
        val firstFailure =
            java.util.concurrent.atomic
                .AtomicReference<Exception>()
        try {
            kotlinx.coroutines.coroutineScope {
                suspend fun forward(
                    name: String,
                    block: suspend () -> Unit,
                ) {
                    try {
                        block()
                        throw IOException("Peer ${config.peerSlot} forwarding loop stopped")
                    } catch (e: kotlinx.coroutines.CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        // Record the cause before socket closure can fail a sibling worker
                        firstFailure.compareAndSet(null, e)
                        diagnostics.failure("worker $name", realSocket, e)
                        throw e
                    } finally {
                        // Unblock sibling receivers before coroutineScope waits for them
                        close("worker $name finished")
                    }
                }
                // local -> real: engine sends to our local port, we forward to peer
                launch { forward("local-to-real") { forwardLocalToReal() } }
                // real -> local: only when we own the socket (non-shared mode)
                if (ownsRealSocket) {
                    launch { forward("real-to-local") { forwardRealToLocal() } }
                }
                // NAT keepalive (direct and relay)
                launch { forward("keepalive") { keepalive() } }
            }
        } catch (e: kotlinx.coroutines.CancellationException) {
            throw e
        } catch (e: Exception) {
            throw firstFailure.get() ?: e
        }
    }

    /**
     * Called by LocalhostProxy's shared receive loop to deliver an incoming
     * packet that has already been demuxed to this peer.
     */
    fun deliverIncoming(
        data: ByteArray,
        length: Int,
        source: InetSocketAddress? = null,
    ) {
        diagnostics.received()
        try {
            val payload: ByteArray
            val payloadLen: Int
            if (config.isRelay) {
                if (source != config.realAddr) return
                val decoded = relay!!.decode(data, length) ?: return
                when (decoded.kind) {
                    RelayTransport.CHALLENGE -> {
                        if (decoded.payload.size != 16) return
                        val proof = relay.encode(RelayTransport.PROOF, decoded.payload)
                        diagnostics.send(
                            "relay-bind-proof",
                            realSocket,
                            DatagramPacket(proof, proof.size, config.realAddr),
                        )
                        return
                    }

                    RelayTransport.DATA -> {
                        payload = decoded.payload
                        payloadLen = payload.size
                    }

                    else -> {
                        return
                    }
                }
            } else {
                payloadLen = length
                payload = data.copyOfRange(0, length)
            }
            if (isTransportKeepalive(payload, payloadLen)) return
            val delivered = OnlineAdmissionEnvelope.wrap(payload, config.admissionId, config.peerSlot)
            diagnostics.send(
                "engine-delivery-send",
                localSocket,
                DatagramPacket(delivered, delivered.size, InetSocketAddress(loopback, ENGINE_PORT)),
            )
            packetsReceived++
            bytesReceived += payloadLen
            if (payloadLen > 0) {
                val upid = payload[0].toInt() and 0xFF
                if (upid == LocalhostProxy.UPID_PDATA) {
                    pdataForwarded++
                } else if (upid == LocalhostProxy.UPID_MDATA_PNORM || upid == LocalhostProxy.UPID_MDATA_PNEEDACK) {
                    mdataForwarded++
                }
            }
            if (packetsReceived <= 5 ||
                (packetsReceived <= 100 && packetsReceived % 20 == 0L) ||
                packetsReceived % 500 == 0L
            ) {
                Log.i(
                    TAG,
                    "real->local slot=${config.peerSlot} #$packetsReceived ${payloadLen}b" +
                        " pdata=$pdataForwarded mdata=$mdataForwarded relay=${config.isRelay}",
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
                diagnostics.receive("engine-receive", localSocket, pkt)
                packetsSent++
                bytesSent += pkt.length
                if (packetsSent <= 5 || (packetsSent <= 100 && packetsSent % 20 == 0L) || packetsSent % 500 == 0L) {
                    Log.i(
                        TAG,
                        "local->real slot=${config.peerSlot} #$packetsSent ${pkt.length}b -> ${config.realAddr} relay=${config.isRelay}",
                    )
                }
                if (config.isRelay) {
                    if (!relay!!.bound) continue
                    val wrapped = relay.encode(RelayTransport.DATA, pkt.data.copyOfRange(0, pkt.length))
                    diagnostics.send(
                        "gameplay-send",
                        realSocket,
                        DatagramPacket(wrapped, wrapped.size, config.realAddr),
                    )
                } else {
                    diagnostics.send("gameplay-send", realSocket, DatagramPacket(pkt.data, pkt.length, config.realAddr))
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
        val buf = ByteArray(RelayTransport.MAX_PACKET + 1)
        val pkt = DatagramPacket(buf, buf.size)
        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            try {
                diagnostics.receive("network-receive", realSocket, pkt)
                if (InetSocketAddress(pkt.address, pkt.port) != config.realAddr) continue
                deliverIncoming(pkt.data, pkt.length, InetSocketAddress(pkt.address, pkt.port))
            } catch (e: java.net.SocketException) {
                if (realSocket.isClosed) break
                Log.w(TAG, "network receive failed: ${e.message}")
            } catch (e: Exception) {
                Log.e(TAG, "network receive failed", e)
            }
        }
    }

    private suspend fun keepalive() {
        var keepaliveCount = 0L
        while (kotlinx.coroutines.currentCoroutineContext()[Job]?.isActive == true) {
            if (relay != null && System.nanoTime() - relay.lastAcknowledgementNanos > 10_000_000_000L) {
                throw IOException("Relay did not acknowledge the route within 10 seconds")
            }
            try {
                if (relay == null) kotlinx.coroutines.delay(KEEPALIVE_INTERVAL_MS)
                val ping = relay?.encode(RelayTransport.BIND, ByteArray(16)) ?: byteArrayOf(0)
                diagnostics.send("keepalive-send", realSocket, DatagramPacket(ping, ping.size, config.realAddr))
                if (relay != null) kotlinx.coroutines.delay(1000L)
                keepaliveCount++
                Log.i(TAG, "keepalive slot=${config.peerSlot} #$keepaliveCount sent=$packetsSent recv=$packetsReceived")
            } catch (e: java.net.SocketException) {
                val msg = e.message ?: ""
                if ("closed" in msg || "EPERM" in msg || "Operation not permitted" in msg) {
                    Log.w(TAG, "keepalive slot=${config.peerSlot} socket closed after #$keepaliveCount")
                    break
                }
                Log.w(TAG, "keepalive error: $msg")
            } catch (e: IOException) {
                // Android can report transient route loss as a plain IOException
                // Keep the forwarding workers alive so native retries can recover
                Log.w(TAG, "keepalive send failed slot=${config.peerSlot}: ${e.message}")
            }
        }
        Log.w(TAG, "keepalive loop EXITED slot=${config.peerSlot} count=$keepaliveCount")
    }

    private fun isTransportKeepalive(
        payload: ByteArray,
        payloadLen: Int,
    ): Boolean = payloadLen == 1 && payload[0] == 0.toByte()

    fun close(reason: String) {
        diagnostics.closing(reason, localSocket, realSocket)
        localSocket.close()
        if (ownsRealSocket) {
            realSocket.close()
        }
    }
}
