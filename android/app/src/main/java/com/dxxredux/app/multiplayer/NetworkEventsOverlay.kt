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
 * Positioned on the LEFT side to avoid overlapping MultiplayerStatsOverlay (right).
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
        lines.add(LineEntry("Status: ${state.status.name}", LineType.LABEL))

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

        // Panel sizing
        val panelW = (w * 0.32f).coerceIn(160f * density, w * 0.45f)
        val panelH = pad * 2 + lineH * lines.size
        val panelLeft = pad
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
    }
}
