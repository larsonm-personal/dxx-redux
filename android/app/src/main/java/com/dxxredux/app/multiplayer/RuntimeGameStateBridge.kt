package com.dxxredux.app.multiplayer

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.os.RemoteException
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject

internal const val RUNTIME_IPC_REGISTER = 1
internal const val RUNTIME_IPC_GAME_STATE = 2
internal const val RUNTIME_IPC_GET_DIAGNOSTICS = 3
internal const val RUNTIME_IPC_DIAGNOSTICS = 4
internal const val RUNTIME_IPC_GAME_STOPPED = 5
internal const val RUNTIME_IPC_ACTIVITY_VISIBILITY = 6
internal const val RUNTIME_IPC_BACKGROUND_TIMEOUT = 7
internal const val RUNTIME_IPC_KEY_HOST = "host"
internal const val RUNTIME_IPC_KEY_STATE = "state"
internal const val RUNTIME_IPC_KEY_DIAGNOSTICS = "diagnostics"
internal const val RUNTIME_IPC_KEY_BACKGROUND = "background"

internal data class RuntimeDiagnosticsSnapshot(
    val proxyStats: List<PeerProxyStats> = emptyList(),
    val connectionInfo: List<PeerConnectionInfoMsg> = emptyList(),
)

internal object RuntimeGameStateCodec {
    private const val GM_NETWORK = 4

    fun encode(values: IntArray): String? {
        if (values.size < 5 || values[4] and GM_NETWORK == 0) return null
        return JSONObject()
            .put("type", "UPDATE_GAME_STATE")
            .put("player_count", values[1])
            .put("max_players", values[2])
            .put("current_level", values[3])
            .put("game_status", values[0])
            .toString()
    }
}

internal fun shouldEndHostedGame(
    wasHost: Boolean,
    updatesEnabled: Boolean,
    lobbyIsHost: Boolean,
): Boolean = wasHost && (updatesEnabled || lobbyIsHost)

internal class RuntimeIpcSession {
    private var connected = false
    private var host = false

    fun register(isHost: Boolean) {
        connected = true
        host = isHost
    }

    fun acceptsGameState(): Boolean = connected && host

    fun isConnected(): Boolean = connected

    fun disconnect(): Boolean {
        val wasHost = connected && host
        connected = false
        host = false
        return wasHost
    }
}

internal object RuntimeDiagnosticsCodec {
    fun encode(
        proxyStats: List<PeerProxyStats>,
        connectionInfo: List<PeerConnectionInfoMsg>,
    ): String {
        val root = JSONObject()
        root.put(
            "proxy_stats",
            JSONArray().apply {
                proxyStats.forEach { stats ->
                    put(
                        JSONObject()
                            .put("peer_slot", stats.peerSlot)
                            .put("packets_sent", stats.packetsSent)
                            .put("packets_received", stats.packetsReceived)
                            .put("bytes_sent", stats.bytesSent)
                            .put("bytes_received", stats.bytesReceived),
                    )
                }
            },
        )
        root.put(
            "connection_info",
            JSONArray().apply {
                connectionInfo.forEach { info ->
                    put(
                        JSONObject()
                            .put("peer_id", info.peerId)
                            .put("peer_callsign", info.peerCallsign)
                            .put("method", info.method)
                            .put("detail", info.detail ?: JSONObject.NULL)
                            .put("server_relay", info.serverRelay)
                            .put("estimated_latency_ms", info.estimatedLatencyMs ?: JSONObject.NULL),
                    )
                }
            },
        )
        return root.toString()
    }

    fun decode(value: String): RuntimeDiagnosticsSnapshot {
        val root = JSONObject(value)
        val stats = root.getJSONArray("proxy_stats")
        val connections = root.getJSONArray("connection_info")
        return RuntimeDiagnosticsSnapshot(
            proxyStats =
                List(stats.length()) { index ->
                    val item = stats.getJSONObject(index)
                    PeerProxyStats(
                        peerSlot = item.getInt("peer_slot"),
                        packetsSent = item.getLong("packets_sent"),
                        packetsReceived = item.getLong("packets_received"),
                        bytesSent = item.getLong("bytes_sent"),
                        bytesReceived = item.getLong("bytes_received"),
                    )
                },
            connectionInfo =
                List(connections.length()) { index ->
                    val item = connections.getJSONObject(index)
                    PeerConnectionInfoMsg(
                        peerId = item.getString("peer_id"),
                        peerCallsign = item.getString("peer_callsign"),
                        method = item.getString("method"),
                        detail = item.optString("detail").takeUnless { item.isNull("detail") },
                        serverRelay = item.getBoolean("server_relay"),
                        estimatedLatencyMs =
                            if (item.isNull("estimated_latency_ms")) {
                                null
                            } else {
                                item.getInt("estimated_latency_ms")
                            },
                    )
                },
        )
    }
}

/**
 * Game-process endpoint for the default-process multiplayer owner.
 *
 * Native state is read on the game process main thread, then copied over
 * Binder. Diagnostic snapshots travel in the opposite direction and are
 * cached for the synchronous overlay providers.
 */
object RuntimeGameStateBridge {
    private const val TAG = "RuntimeGameStateBridge"
    private const val INITIAL_UPDATE_DELAY_MS = 3_000L
    private const val UPDATE_INTERVAL_MS = 30_000L
    private const val DIAGNOSTIC_REQUEST_INTERVAL_MS = 500L

    private val handler = Handler(Looper.getMainLooper())
    private val incoming =
        Messenger(
            Handler(Looper.getMainLooper()) { message ->
                if (message.what == RUNTIME_IPC_DIAGNOSTICS) {
                    val encoded = message.data.getString(RUNTIME_IPC_KEY_DIAGNOSTICS)
                    diagnostics =
                        try {
                            encoded?.let(RuntimeDiagnosticsCodec::decode) ?: RuntimeDiagnosticsSnapshot()
                        } catch (e: Exception) {
                            Log.w(TAG, "Rejected malformed diagnostic snapshot", e)
                            RuntimeDiagnosticsSnapshot()
                        }
                    true
                } else if (message.what == RUNTIME_IPC_BACKGROUND_TIMEOUT) {
                    backgroundTimeoutHandler?.invoke()
                    true
                } else {
                    false
                }
            },
        )

    @Volatile
    private var diagnostics = RuntimeDiagnosticsSnapshot()
    private var appContext: Context? = null
    private var service: Messenger? = null
    private var bound = false
    private var isHost = false
    private var nativeStateProvider: (() -> IntArray)? = null
    private var backgroundTimeoutHandler: (() -> Unit)? = null
    private var activityBackgrounded = false
    private var lastDiagnosticRequestMs = 0L

    private val statePublisher =
        object : Runnable {
            override fun run() {
                if (!isHost) return
                val state =
                    try {
                        nativeStateProvider?.invoke()
                    } catch (e: Exception) {
                        Log.w(TAG, "Native game state poll failed", e)
                        null
                    }
                if (state != null) {
                    send(RUNTIME_IPC_GAME_STATE, Bundle().apply { putIntArray(RUNTIME_IPC_KEY_STATE, state) })
                }
                handler.postDelayed(this, UPDATE_INTERVAL_MS)
            }
        }

    private val connection =
        object : ServiceConnection {
            override fun onServiceConnected(
                name: ComponentName,
                binder: IBinder,
            ) {
                service = Messenger(binder)
                send(
                    RUNTIME_IPC_REGISTER,
                    Bundle().apply { putBoolean(RUNTIME_IPC_KEY_HOST, isHost) },
                )
                requestDiagnostics(force = true)
                noteActivityVisibility(activityBackgrounded)
            }

            override fun onServiceDisconnected(name: ComponentName) {
                service = null
                diagnostics = RuntimeDiagnosticsSnapshot()
                Log.w(TAG, "Multiplayer owner process disconnected")
            }

            override fun onBindingDied(name: ComponentName) {
                onServiceDisconnected(name)
            }

            override fun onNullBinding(name: ComponentName) {
                onServiceDisconnected(name)
            }
        }

    fun connect(
        context: Context,
        host: Boolean,
        stateProvider: () -> IntArray,
        onBackgroundTimeout: () -> Unit,
    ) {
        disconnect()
        appContext = context.applicationContext
        isHost = host
        nativeStateProvider = stateProvider
        backgroundTimeoutHandler = onBackgroundTimeout
        bound =
            context.bindService(
                Intent(context, MultiplayerForegroundService::class.java),
                connection,
                Context.BIND_AUTO_CREATE,
            )
        if (!bound) Log.e(TAG, "Could not bind multiplayer owner process")
        if (host) handler.postDelayed(statePublisher, INITIAL_UPDATE_DELAY_MS)
    }

    fun disconnect() {
        handler.removeCallbacks(statePublisher)
        if (bound) {
            send(RUNTIME_IPC_GAME_STOPPED, Bundle().apply { putBoolean(RUNTIME_IPC_KEY_HOST, isHost) })
            try {
                appContext?.unbindService(connection)
            } catch (_: IllegalArgumentException) {
            }
        }
        bound = false
        service = null
        appContext = null
        isHost = false
        nativeStateProvider = null
        backgroundTimeoutHandler = null
        activityBackgrounded = false
        diagnostics = RuntimeDiagnosticsSnapshot()
        lastDiagnosticRequestMs = 0L
    }

    fun noteActivityVisibility(background: Boolean) {
        activityBackgrounded = background
        send(
            RUNTIME_IPC_ACTIVITY_VISIBILITY,
            Bundle().apply { putBoolean(RUNTIME_IPC_KEY_BACKGROUND, background) },
        )
    }

    fun getProxyStats(): List<PeerProxyStats> {
        requestDiagnostics()
        return diagnostics.proxyStats
    }

    fun getConnectionInfo(): List<PeerConnectionInfoMsg> {
        requestDiagnostics()
        return diagnostics.connectionInfo
    }

    private fun requestDiagnostics(force: Boolean = false) {
        val now = android.os.SystemClock.elapsedRealtime()
        if (!force && now - lastDiagnosticRequestMs < DIAGNOSTIC_REQUEST_INTERVAL_MS) return
        lastDiagnosticRequestMs = now
        send(RUNTIME_IPC_GET_DIAGNOSTICS)
    }

    private fun send(
        what: Int,
        data: Bundle = Bundle.EMPTY,
    ) {
        val destination = service ?: return
        try {
            destination.send(
                Message.obtain(null, what).apply {
                    this.data = data
                    replyTo = incoming
                },
            )
        } catch (e: RemoteException) {
            service = null
            diagnostics = RuntimeDiagnosticsSnapshot()
            Log.w(TAG, "Multiplayer owner IPC failed", e)
        }
    }
}
