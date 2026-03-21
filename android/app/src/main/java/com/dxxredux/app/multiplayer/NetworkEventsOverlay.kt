package com.dxxredux.app.multiplayer

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.View

/**
 * In-game overlay showing network connection events: per-peer connection method,
 * relay/direct status, estimated latency, NAT type, and STUN addresses.
 *
 * Positioned on the RIGHT side to avoid overlapping MultiplayerStatsOverlay (left).
 * Toggled via admin tray (ADMIN_NET_EVENTS).
 */
class NetworkEventsOverlay(
    context: Context,
) : View(context) {
    // Data provider set by MainActivity (reads MatchmakingStateHolder snapshot)
    var stateProvider: (() -> MatchmakingState?)? = null

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false
    private var cachedState: MatchmakingState? = null

    private val pollRunnable =
        object : Runnable {
            override fun run() {
                if (!polling) return
                cachedState =
                    try {
                        stateProvider?.invoke()
                    } catch (_: Exception) {
                        null
                    }
                invalidate()
                handler.postDelayed(this, POLL_INTERVAL_MS)
            }
        }

    fun show() {
        visibility = VISIBLE
        if (!polling) {
            polling = true
            handler.post(pollRunnable)
        }
    }

    fun hide() {
        visibility = GONE
        polling = false
        handler.removeCallbacks(pollRunnable)
    }

    fun toggle() {
        if (visibility == VISIBLE) hide() else show()
    }

    // Paints
    private val bgPaint =
        Paint().apply {
            color = 0x66000000
            style = Paint.Style.FILL
        }
    private val titlePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val labelPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFAAAAAAu.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val valuePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            typeface = android.graphics.Typeface.MONOSPACE
        }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (visibility != VISIBLE) return
        val state = cachedState ?: return

        val density = resources.displayMetrics.density
        val w = width.toFloat()
        val baseTextSize = (12f * density).coerceAtMost(w * 0.016f)
        titlePaint.textSize = baseTextSize * 1.1f
        labelPaint.textSize = baseTextSize * 0.9f
        valuePaint.textSize = baseTextSize * 0.9f

        val pad = 8f * density
        val lineH = baseTextSize * 1.4f

        // Collect lines to render
        val lines = mutableListOf<LineEntry>()
        lines.add(LineEntry("NET EVENTS", LineType.TITLE))

        // Status line: "Connected (direct/relay) - host/client"
        val connType =
            when {
                state.connectionInfo.any { it.serverRelay } -> "relay"
                state.connectionInfo.isNotEmpty() -> "direct"
                else -> null
            }
        val method = state.connectionInfo.firstOrNull()?.method
        val role =
            when {
                state.gameLaunchInfo?.isHost == true -> "host"
                state.currentLobby?.isHost == true -> "host"
                state.gameLaunchInfo != null -> "client"
                state.currentLobby != null -> "client"
                else -> null
            }
        val statusParts = mutableListOf(state.status.name)
        connType?.let { statusParts[0] = "${state.status.name} ($it)" }
        method?.let { statusParts.add(it) }
        role?.let { statusParts.add(it) }
        lines.add(LineEntry(statusParts.joinToString(" - "), LineType.LABEL))

        if (state.stunAddrs.isNotEmpty()) {
            lines.add(LineEntry("STUN: ${state.stunAddrs.joinToString(", ")}", LineType.LABEL))
        }
        state.relayInfo?.let { relay ->
            lines.add(LineEntry("Relay: ${relay.relayAddr}", LineType.LABEL))
        }

        // Per-peer connection info
        if (state.connectionInfo.isNotEmpty()) {
            lines.add(LineEntry("Connections:", LineType.LABEL))
            for (ci in state.connectionInfo) {
                val latency = ci.estimatedLatencyMs?.let { " ${it}ms" } ?: ""
                val relay = if (ci.serverRelay) " [relay]" else " [direct]"
                val color = if (ci.serverRelay) 0xFFFFAA44u.toInt() else 0xFF44FF44u.toInt()
                lines.add(LineEntry("  ${ci.peerCallsign}: ${ci.method}$relay$latency", LineType.VALUE, color))
            }
        }

        // Peer candidates and NAT types
        if (state.peerCandidates.isNotEmpty()) {
            lines.add(LineEntry("Candidates:", LineType.LABEL))
            for ((_, info) in state.peerCandidates) {
                val types =
                    info.candidates
                        .map { it.candidateType }
                        .distinct()
                        .joinToString(",")
                lines.add(LineEntry("  ${info.peerId.take(8)}: NAT=${info.natType} [$types]", LineType.VALUE))
            }
        }

        // Connectivity pairs
        if (state.connectivityPairs.isNotEmpty()) {
            lines.add(LineEntry("Connectivity:", LineType.LABEL))
            for (pair in state.connectivityPairs) {
                lines.add(
                    LineEntry(
                        "  ${pair.peerId.take(8)}: ${pair.localType}->${pair.remoteType}",
                        LineType.VALUE,
                    ),
                )
            }
        }

        // ICE status summary
        val ice = state.iceStatus
        if (ice.phase != IcePhase.IDLE) {
            val iceLabel =
                when (ice.phase) {
                    IcePhase.STUN_DISCOVERY -> "ICE: discovering..."
                    IcePhase.STUN_COMPLETE -> "ICE: STUN done (${ice.stunNatType}, ${ice.stunCandidateCount} cands)"
                    IcePhase.PROBING -> "ICE: probing..."
                    IcePhase.COMPLETE -> {
                        val rtt = ice.probeRttMs?.let { " ${it}ms" } ?: ""
                        "ICE: ${ice.probeResult ?: "done"}$rtt"
                    }
                    IcePhase.FAILED -> "ICE: FAILED - ${ice.errorMessage ?: "unknown"}"
                    IcePhase.IDLE -> "ICE: idle"
                }
            val iceColor =
                when (ice.phase) {
                    IcePhase.COMPLETE -> if (ice.probeResult == "relay") 0xFFFFAA44u.toInt() else 0xFF44FF44u.toInt()
                    IcePhase.FAILED -> 0xFFFF4444u.toInt()
                    else -> 0xFFAAAAFFu.toInt()
                }
            lines.add(LineEntry(iceLabel, LineType.VALUE, iceColor))
        }

        // Recent status log (C-side MPDIAG + Kotlin events)
        if (state.statusLog.isNotEmpty()) {
            lines.add(LineEntry("Log:", LineType.LABEL))
            for (msg in state.statusLog.takeLast(STATUS_LOG_LINES)) {
                lines.add(LineEntry("  ${msg.take(MAX_LOG_LINE_LEN)}", LineType.VALUE))
            }
        }

        // Panel sizing
        val panelW = (w * 0.32f).coerceIn(160f * density, w * 0.45f)
        val panelH = pad * 2 + lineH * lines.size
        val panelLeft = w - panelW - pad
        val panelTop = pad

        // Background
        canvas.drawRoundRect(
            RectF(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH),
            pad,
            pad,
            bgPaint,
        )

        // Render lines
        var y = panelTop + pad + baseTextSize
        for (entry in lines) {
            val paint =
                when (entry.type) {
                    LineType.TITLE -> titlePaint
                    LineType.LABEL -> labelPaint
                    LineType.VALUE -> valuePaint
                }
            if (entry.color != 0) paint.color = entry.color
            canvas.drawText(entry.text, panelLeft + pad, y, paint)
            // Reset color
            when (entry.type) {
                LineType.TITLE -> paint.color = Color.WHITE
                LineType.LABEL -> paint.color = 0xFFAAAAAAu.toInt()
                LineType.VALUE -> paint.color = Color.WHITE
            }
            y += lineH
        }
    }

    private enum class LineType { TITLE, LABEL, VALUE }

    private data class LineEntry(
        val text: String,
        val type: LineType,
        val color: Int = 0,
    )

    companion object {
        const val POLL_INTERVAL_MS = 1000L
        private const val STATUS_LOG_LINES = 8
        private const val MAX_LOG_LINE_LEN = 60
    }
}
