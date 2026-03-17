package com.dxxredux.app.multiplayer

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.View
import kotlin.math.exp

/**
 * In-game overlay for multiplayer network stats.
 *
 * Shows: packet counts, ping EMA + color-coded graph, connection type per peer.
 * Toggle via admin tray (ADMIN_NET_STATS).
 *
 * Shared constant: MAX_PLAYERS = 8 (matches C #define in player.h)
 */
class MultiplayerStatsOverlay(
    context: Context,
) : View(context) {
    // Data providers set by MainActivity
    var pingProvider: (() -> IntArray?)? = null
    var proxyStatsProvider: (() -> List<PeerProxyStats>)? = null
    var connectionInfoProvider: (() -> List<PeerConnectionInfoMsg>)? = null
    var isLan = false

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false

    // Ping history: ring buffer of 30 samples, averaged across all peers
    private val pingHistory = IntArray(GRAPH_SAMPLES)
    private var pingHistoryIndex = 0
    private var pingHistoryCount = 0

    // EMA ping (milliseconds, float for smoothing)
    private var pingEma = 0f
    private var emaInitialized = false

    // Per-player current ping cache
    private val playerPing = IntArray(MAX_PLAYERS)
    private var nPlayers = 0
    private var playerNum = 0

    // Cached stats
    private var lastProxyStats = emptyList<PeerProxyStats>()
    private var lastConnectionInfo = emptyList<PeerConnectionInfoMsg>()

    // Paints
    private val bgPaint =
        Paint().apply {
            color = 0xCC000000.toInt()
            style = Paint.Style.FILL
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val labelPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFAAAAAAu.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val graphBgPaint =
        Paint().apply {
            color = 0x44000000
            style = Paint.Style.FILL
        }
    private val graphLinePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = 3f
            strokeJoin = Paint.Join.ROUND
            strokeCap = Paint.Cap.ROUND
        }
    private val graphGridPaint =
        Paint().apply {
            color = 0x33FFFFFF
            style = Paint.Style.STROKE
            strokeWidth = 1f
        }

    private val pollRunnable =
        object : Runnable {
            override fun run() {
                if (!polling) return
                pollStats()
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
        clearHistory()
    }

    fun toggle() {
        if (visibility == VISIBLE) hide() else show()
    }

    private fun clearHistory() {
        pingHistoryCount = 0
        pingHistoryIndex = 0
        emaInitialized = false
        pingEma = 0f
        playerPing.fill(0)
    }

    private fun pollStats() {
        val pings =
            try {
                pingProvider?.invoke()
            } catch (_: Exception) {
                null
            }
        if (pings != null && pings.size >= 2 + MAX_PLAYERS) {
            nPlayers = pings[0]
            playerNum = pings[1]
            var peerCount = 0
            var pingSum = 0
            for (i in 0 until MAX_PLAYERS) {
                playerPing[i] = pings[2 + i]
                if (i != playerNum && i < nPlayers) {
                    pingSum += playerPing[i].coerceIn(0, PING_CAP_MS)
                    peerCount++
                }
            }
            if (peerCount > 0) {
                val avgPing = pingSum / peerCount
                // Ring buffer
                pingHistory[pingHistoryIndex] = avgPing
                pingHistoryIndex = (pingHistoryIndex + 1) % GRAPH_SAMPLES
                if (pingHistoryCount < GRAPH_SAMPLES) pingHistoryCount++
                // EMA
                if (!emaInitialized) {
                    pingEma = avgPing.toFloat()
                    emaInitialized = true
                } else {
                    pingEma = EMA_ALPHA * avgPing + (1f - EMA_ALPHA) * pingEma
                }
            }
        }

        lastProxyStats =
            try {
                proxyStatsProvider?.invoke() ?: emptyList()
            } catch (_: Exception) {
                emptyList()
            }
        lastConnectionInfo =
            try {
                connectionInfoProvider?.invoke() ?: emptyList()
            } catch (_: Exception) {
                emptyList()
            }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (visibility != VISIBLE) return

        val density = resources.displayMetrics.density
        val w = width.toFloat()
        val h = height.toFloat()

        val baseTextSize = (13f * density).coerceAtMost(w * 0.018f)
        textPaint.textSize = baseTextSize
        labelPaint.textSize = baseTextSize * 0.85f

        val pad = 8f * density
        val lineH = baseTextSize * 1.5f
        val graphH = 70f * density

        // Count content lines
        val peerCount = (nPlayers - 1).coerceAtLeast(0)
        val connLines =
            if (lastConnectionInfo.isNotEmpty()) {
                lastConnectionInfo.size.coerceAtMost(MAX_PLAYERS)
            } else if (isLan) {
                1
            } else {
                0
            }
        // Title + packets + avg ping + per-peer pings + connections + graph
        val contentLines = 1 + 1 + 1 + peerCount + connLines
        val panelH = pad * 2 + lineH * contentLines + graphH + pad
        val panelW = (w * 0.3f).coerceIn(180f * density, w * 0.45f)

        val panelLeft = w - panelW - pad
        val panelTop = pad

        // Background
        canvas.drawRoundRect(
            RectF(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH),
            pad,
            pad,
            bgPaint,
        )

        var y = panelTop + pad + baseTextSize

        // Title
        textPaint.color = Color.WHITE
        canvas.drawText("NET STATS", panelLeft + pad, y, textPaint)
        y += lineH

        // Packet totals
        val totalSent = lastProxyStats.sumOf { it.packetsSent }
        val totalRecv = lastProxyStats.sumOf { it.packetsReceived }
        labelPaint.color = 0xFFAAAAAAu.toInt()
        if (totalSent > 0 || totalRecv > 0) {
            canvas.drawText("Pkts: ${totalSent}tx / ${totalRecv}rx", panelLeft + pad, y, labelPaint)
        } else {
            canvas.drawText("Pkts: --", panelLeft + pad, y, labelPaint)
        }
        y += lineH

        // Average ping
        val emaMs = if (emaInitialized) pingEma.toInt() else 0
        textPaint.color = pingColor(emaMs)
        canvas.drawText("Avg ping: ${emaMs}ms", panelLeft + pad, y, textPaint)
        y += lineH

        // Per-peer pings
        for (i in 0 until MAX_PLAYERS) {
            if (i == playerNum || i >= nPlayers) continue
            val ms = playerPing[i]
            textPaint.color = pingColor(ms.coerceAtMost(PING_CAP_MS))
            canvas.drawText("  P$i: ${ms}ms", panelLeft + pad, y, textPaint)
            y += lineH
        }

        // Connection types
        if (lastConnectionInfo.isNotEmpty()) {
            for (ci in lastConnectionInfo) {
                labelPaint.color = 0xFFAAAAAAu.toInt()
                val label = "${ci.peerCallsign}: ${ci.method}"
                canvas.drawText(label, panelLeft + pad, y, labelPaint)
                y += lineH
            }
        } else if (isLan) {
            labelPaint.color = 0xFFAAAAAAu.toInt()
            canvas.drawText("Mode: LAN", panelLeft + pad, y, labelPaint)
            y += lineH
        }

        // Ping graph
        y += pad * 0.5f
        val graphLeft = panelLeft + pad
        val graphTop = y
        val graphRight = panelLeft + panelW - pad
        val actualGraphW = graphRight - graphLeft
        val graphBottom = graphTop + graphH

        canvas.drawRoundRect(
            RectF(graphLeft, graphTop, graphRight, graphBottom),
            4f * density,
            4f * density,
            graphBgPaint,
        )

        // Grid at 250 and 500ms
        for (gridMs in intArrayOf(250, 500)) {
            val gridY = graphBottom - (gridMs.toFloat() / PING_CAP_MS) * graphH
            canvas.drawLine(graphLeft, gridY, graphRight, gridY, graphGridPaint)
        }

        // Draw ping history line
        if (pingHistoryCount >= 2) {
            val count = pingHistoryCount
            val startIdx = pingHistoryIndex

            graphLinePaint.color = pingColor(emaMs)
            graphLinePaint.alpha = 220

            var prevX = 0f
            var prevY = 0f
            for (j in 0 until count) {
                val ringIdx = (startIdx - count + j + GRAPH_SAMPLES) % GRAPH_SAMPLES
                val ms = pingHistory[ringIdx].coerceAtMost(PING_CAP_MS)
                val x = graphLeft + (j.toFloat() / (GRAPH_SAMPLES - 1)) * actualGraphW
                val gy = graphBottom - (ms.toFloat() / PING_CAP_MS) * graphH
                // Color each segment
                if (j > 0) {
                    graphLinePaint.color = pingColor(ms)
                    graphLinePaint.alpha = 220
                    canvas.drawLine(prevX, prevY, x, gy, graphLinePaint)
                }
                prevX = x
                prevY = gy
            }
        }

        // Y-axis labels
        labelPaint.textSize = baseTextSize * 0.7f
        labelPaint.color = 0x88FFFFFF.toInt()
        canvas.drawText("0", graphLeft, graphBottom + labelPaint.textSize, labelPaint)
        canvas.drawText("500ms", graphRight - labelPaint.measureText("500ms"), graphTop - 2f * density, labelPaint)
        labelPaint.textSize = baseTextSize * 0.85f
    }

    companion object {
        const val MAX_PLAYERS = 8
        const val GRAPH_SAMPLES = 30
        const val PING_CAP_MS = 500
        const val POLL_INTERVAL_MS = 1000L
        val EMA_ALPHA = (1.0 - exp(-1.0 / 10.0)).toFloat()

        fun pingColor(ms: Int): Int =
            when {
                ms > 400 -> 0xFFFF4444u.toInt()
                ms > 250 -> 0xFFFFFF00u.toInt()
                else -> 0xFF44FF44u.toInt()
            }
    }
}
