package com.dxxredux.app.multiplayer

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.View
import com.dxxredux.app.DormancyDiagnostics

internal fun wrapNetworkEventText(
    text: String,
    maxColumns: Int,
    continuationIndent: String = "    ",
): List<String> {
    require(maxColumns > continuationIndent.length)
    if (text.length <= maxColumns) return listOf(text)

    val lines = mutableListOf<String>()
    var remaining = text
    var prefix = ""
    while (prefix.length + remaining.length > maxColumns) {
        val available = maxColumns - prefix.length
        val candidate = remaining.take(available)
        val whitespace = candidate.indexOfLast { it.isWhitespace() }
        val splitAt = if (whitespace > 0) whitespace else available
        lines.add(prefix + remaining.take(splitAt).trimEnd())
        remaining = remaining.drop(splitAt).trimStart()
        prefix = continuationIndent
    }
    lines.add(prefix + remaining)
    return lines
}

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
    private var resumePollingAfterSuspend = false
    private var cachedState: MatchmakingState? = null

    private val pollRunnable =
        object : Runnable {
            override fun run() {
                if (!polling) return
                DormancyDiagnostics.recordIndependentOverlayPoll()
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

    fun suspendPolling() {
        resumePollingAfterSuspend = polling
        polling = false
        handler.removeCallbacks(pollRunnable)
    }

    fun resumePolling() {
        if (resumePollingAfterSuspend && visibility == VISIBLE) {
            polling = true
            handler.post(pollRunnable)
        }
        resumePollingAfterSuspend = false
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
        val baseTextSize = (10f * density).coerceAtMost(w * 0.014f)
        titlePaint.textSize = baseTextSize * 1.1f
        labelPaint.textSize = baseTextSize * 0.9f
        valuePaint.textSize = baseTextSize * 0.9f

        val pad = 8f * density
        val lineH = baseTextSize * 1.4f
        val panelW = (w * 0.32f).coerceIn(160f * density, w * 0.45f)
        val maxColumns =
            ((panelW - pad * 2) / valuePaint.measureText("M").coerceAtLeast(1f))
                .toInt()
                .coerceAtLeast(MIN_LINE_COLUMNS)

        // Collect lines to render
        val unwrappedLines = mutableListOf<LineEntry>()
        unwrappedLines.add(LineEntry("NET EVENTS", LineType.TITLE))

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
        unwrappedLines.add(LineEntry(statusParts.joinToString(" - "), LineType.LABEL))

        if (state.stunAddrs.isNotEmpty()) {
            unwrappedLines.add(LineEntry("STUN: ${state.stunAddrs.joinToString(", ")}", LineType.LABEL))
        }
        state.relayInfo?.let { relay ->
            unwrappedLines.add(LineEntry("Relay: ${relay.relayAddr}", LineType.LABEL))
        }

        // Per-peer connection info
        if (state.connectionInfo.isNotEmpty()) {
            unwrappedLines.add(LineEntry("Connections:", LineType.LABEL))
            for (ci in state.connectionInfo) {
                val latency = ci.estimatedLatencyMs?.let { " ${it}ms" } ?: ""
                val relay = if (ci.serverRelay) " [relay]" else " [direct]"
                val color = if (ci.serverRelay) 0xFFFFAA44u.toInt() else 0xFF44FF44u.toInt()
                unwrappedLines.add(
                    LineEntry("  ${ci.peerCallsign}: ${ci.method}$relay$latency", LineType.VALUE, color),
                )
            }
        }

        // Peer candidates and NAT types
        if (state.peerCandidates.isNotEmpty()) {
            unwrappedLines.add(LineEntry("Candidates:", LineType.LABEL))
            for ((_, info) in state.peerCandidates) {
                val types =
                    info.candidates
                        .map { it.candidateType }
                        .distinct()
                        .joinToString(",")
                unwrappedLines.add(
                    LineEntry("  ${info.peerId.take(8)}: NAT=${info.natType} [$types]", LineType.VALUE),
                )
            }
        }

        // Connectivity pairs
        if (state.connectivityPairs.isNotEmpty()) {
            unwrappedLines.add(LineEntry("Connectivity:", LineType.LABEL))
            for (pair in state.connectivityPairs) {
                unwrappedLines.add(
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
                    IcePhase.STUN_DISCOVERY -> {
                        "ICE: discovering..."
                    }

                    IcePhase.STUN_COMPLETE -> {
                        "ICE: STUN done (${ice.stunNatType}, ${ice.stunCandidateCount} cands)"
                    }

                    IcePhase.PROBING -> {
                        "ICE: probing..."
                    }

                    IcePhase.COMPLETE -> {
                        val rtt = ice.probeRttMs?.let { " ${it}ms" } ?: ""
                        "ICE: ${ice.probeResult ?: "done"}$rtt"
                    }

                    IcePhase.FAILED -> {
                        "ICE: FAILED - ${ice.errorMessage ?: "unknown"}"
                    }

                    IcePhase.IDLE -> {
                        "ICE: idle"
                    }
                }
            val iceColor =
                when (ice.phase) {
                    IcePhase.COMPLETE -> if (ice.probeResult == "relay") 0xFFFFAA44u.toInt() else 0xFF44FF44u.toInt()
                    IcePhase.FAILED -> 0xFFFF4444u.toInt()
                    else -> 0xFFAAAAFFu.toInt()
                }
            unwrappedLines.add(LineEntry(iceLabel, LineType.VALUE, iceColor))
        }

        // Recent status log (C-side MPDIAG + Kotlin events)
        if (state.statusLog.isNotEmpty()) {
            unwrappedLines.add(LineEntry("Log:", LineType.LABEL))
            for (msg in state.statusLog.takeLast(STATUS_LOG_LINES)) {
                unwrappedLines.add(LineEntry("  $msg", LineType.VALUE))
            }
        }

        val lines =
            unwrappedLines.flatMap { entry ->
                wrapNetworkEventText(entry.text, maxColumns).map { entry.copy(text = it) }
            }

        // Panel sizing
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
        private const val MIN_LINE_COLUMNS = 12
    }
}
