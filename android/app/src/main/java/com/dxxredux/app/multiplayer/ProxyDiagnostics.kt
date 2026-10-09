package com.dxxredux.app.multiplayer

import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.SocketTimeoutException
import java.util.concurrent.atomic.AtomicLong

// Android proxy diagnostics go to the exported Network log, not just logcat
internal class ProxyDiagnostics(
    private val label: String,
) {
    private val started = System.nanoTime()
    private val sent = AtomicLong()
    private val received = AtomicLong()
    private val keepalives = AtomicLong()
    private val failures = mutableMapOf<String, Long>()

    @Volatile private var lastSend = 0L

    @Volatile private var lastReceive = 0L

    @Volatile private var lastKeepalive = 0L

    @Volatile private var closing = false

    fun event(message: String) {
        NetLog.log("PROXY", "$label ${snapshot()} $message")
    }

    fun snapshot(): String {
        val now = System.nanoTime()

        fun age(time: Long) = if (time == 0L) -1L else (now - time) / 1_000_000
        return "age_ms=${age(started)} tx_ok=${sent.get()} rx_ok=${received.get()} " +
            "keepalive_ok=${keepalives.get()} last_tx_ms=${age(lastSend)} " +
            "last_rx_ms=${age(lastReceive)} last_keepalive_ms=${age(lastKeepalive)}"
    }

    fun received() {
        lastReceive = System.nanoTime()
        received.incrementAndGet()
    }

    fun send(
        operation: String,
        socket: DatagramSocket,
        packet: DatagramPacket,
    ) {
        try {
            socket.send(packet)
            when (operation) {
                "gameplay-send" -> {
                    lastSend = System.nanoTime()
                    sent.incrementAndGet()
                }

                "keepalive-send" -> {
                    lastKeepalive = System.nanoTime()
                    keepalives.incrementAndGet()
                }
            }
        } catch (e: Exception) {
            failure(operation, socket, e, "remote=${packet.socketAddress} bytes=${packet.length}")
            throw e
        }
    }

    fun receive(
        operation: String,
        socket: DatagramSocket,
        packet: DatagramPacket,
    ) {
        try {
            socket.receive(packet)
            if (operation == "network-receive" || operation == "shared-network-receive") received()
        } catch (e: Exception) {
            // Preserve the caller's timeout/error policy and avoid logging routine probe timeouts
            if (e !is SocketTimeoutException) failure(operation, socket, e)
            throw e
        }
    }

    @Synchronized
    fun failure(
        operation: String,
        socket: DatagramSocket?,
        error: Throwable,
        detail: String = "",
    ) {
        if (closing) return
        val now = System.nanoTime()
        val previous = failures[operation]
        if (previous != null && now - previous < 5_000_000_000L) return
        failures[operation] = now
        // Full cause chain includes Android ErrnoException and its errno message
        event("failure operation=$operation $detail ${socket?.let(::socketState)}\n${error.stackTraceToString()}")
    }

    fun closing(
        reason: String,
        local: DatagramSocket,
        real: DatagramSocket? = null,
    ) {
        closing = true
        event("closing reason=$reason local=[${socketState(local)}] real=[${real?.let(::socketState)}]")
    }

    companion object {
        private val nextId = AtomicLong()

        fun nextId(): Long = nextId.incrementAndGet()

        fun socketState(socket: DatagramSocket): String =
            "socket=${System.identityHashCode(socket)} local=${socket.localSocketAddress} " +
                "bound=${socket.isBound} connected=${socket.isConnected} closed=${socket.isClosed}"
    }
}
