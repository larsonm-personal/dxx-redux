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
    var packetStatsProvider: (() -> IntArray?)? = null
    var proxyStatsProvider: (() -> List<PeerProxyStats>)? = null
    var connectionInfoProvider: (() -> List<PeerConnectionInfoMsg>)? = null
    var isLan = false
    var localIp: String? = null

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false
    private var resumePollingAfterSuspend = false

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

    // Engine-side packet counters (from JNI)
    private var enginePktSent = 0
    private var enginePktRecv = 0
    private val playerLoss = IntArray(MAX_PLAYERS) // outbound loss % per slot
    private val playerRxLoss = IntArray(MAX_PLAYERS) // inbound loss % per slot

    // Loss history: ring buffer parallel to pingHistory (max loss% across peers per sample)
    private val lossHistory = IntArray(GRAPH_SAMPLES)

    // Paints
    private val bgPaint =
        Paint().apply {
            color = 0x66000000
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
            color = 0x22000000
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
    private val lossDotPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFFF4444u.toInt()
            style = Paint.Style.FILL
        }

    private val pollRunnable =
        object : Runnable {
            override fun run() {
                if (!polling) return
                DormancyDiagnostics.recordIndependentOverlayPoll()
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

    private fun clearHistory() {
        pingHistoryCount = 0
        pingHistoryIndex = 0
        emaInitialized = false
        pingEma = 0f
        playerPing.fill(0)
        playerLoss.fill(0)
        playerRxLoss.fill(0)
        lossHistory.fill(0)
        enginePktSent = 0
        enginePktRecv = 0
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
                // Ring buffer -- ping
                val idx = pingHistoryIndex
                pingHistory[idx] = avgPing
                // Ring buffer -- max loss% across peers for this sample
                var maxLoss = 0
                for (i in 0 until MAX_PLAYERS) {
                    if (i != playerNum && i < nPlayers) {
                        maxLoss = maxOf(maxLoss, playerLoss[i], playerRxLoss[i])
                    }
                }
                lossHistory[idx] = maxLoss
                pingHistoryIndex = (idx + 1) % GRAPH_SAMPLES
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

        // Engine-side packet stats (always available via JNI)
        val pktStats =
            try {
                packetStatsProvider?.invoke()
            } catch (_: Exception) {
                null
            }
        if (pktStats != null && pktStats.size >= 18) {
            enginePktSent = pktStats[0]
            enginePktRecv = pktStats[1]
            for (i in 0 until MAX_PLAYERS) {
                playerLoss[i] = pktStats[2 + i]
                playerRxLoss[i] = pktStats[10 + i]
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
        // Compute max loss across peers for the loss line
        var maxLossNow = 0
        for (i in 0 until MAX_PLAYERS) {
            if (i != playerNum && i < nPlayers) {
                maxLossNow = maxOf(maxLossNow, playerLoss[i], playerRxLoss[i])
            }
        }
        val hasLoss = maxLossNow > 0
        // Title + packets + loss(opt) + avg ping + per-peer pings + connections + graph
        val contentLines = 1 + 1 + (if (hasLoss) 1 else 0) + 1 + peerCount + connLines
        val panelH = pad * 2 + lineH * contentLines + graphH + pad
        val panelW = (w * 0.3f).coerceIn(180f * density, w * 0.45f)

        val panelLeft = pad
        val panelTop = pad

        // Background
        canvas.drawRoundRect(
            RectF(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH),
            pad,
            pad,
            bgPaint,
        )

        var y = panelTop + pad + baseTextSize

        // Title + IP right-aligned
        textPaint.color = Color.WHITE
        canvas.drawText("NET STATS", panelLeft + pad, y, textPaint)
        localIp?.let { ip ->
            labelPaint.color = 0xFFAAAAAAu.toInt()
            labelPaint.textSize = baseTextSize * 0.75f
            val ipW = labelPaint.measureText(ip)
            canvas.drawText(ip, panelLeft + panelW - pad - ipW, y, labelPaint)
            labelPaint.textSize = baseTextSize * 0.85f
        }
        y += lineH

        // Packet totals (engine counters, always available)
        labelPaint.color = 0xFFAAAAAAu.toInt()
        val proxySent = lastProxyStats.sumOf { it.packetsSent }
        val proxyRecv = lastProxyStats.sumOf { it.packetsReceived }
        val hasProxy = proxySent > 0 || proxyRecv > 0
        if (enginePktSent > 0 || enginePktRecv > 0) {
            val base = "Pkts: ${enginePktSent}tx/${enginePktRecv}rx"
            val suffix = if (hasProxy) " p:$proxySent/$proxyRecv" else ""
            canvas.drawText(base + suffix, panelLeft + pad, y, labelPaint)
        } else if (hasProxy) {
            canvas.drawText("Pkts: ${proxySent}tx/${proxyRecv}rx", panelLeft + pad, y, labelPaint)
        } else {
            canvas.drawText("Pkts: --", panelLeft + pad, y, labelPaint)
        }
        y += lineH

        // Loss line (only when loss detected)
        if (hasLoss) {
            labelPaint.color = 0xFFFF4444u.toInt()
            canvas.drawText("Loss: $maxLossNow%", panelLeft + pad, y, labelPaint)
            labelPaint.color = 0xFFAAAAAAu.toInt()
            y += lineH
        }

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
            val mode = if (lastProxyStats.isNotEmpty()) "LAN (proxy)" else "LAN (direct)"
            canvas.drawText("Mode: $mode", panelLeft + pad, y, labelPaint)
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

        // Draw ping history line (right-aligned: newest sample at right edge)
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
                val x = graphLeft + ((GRAPH_SAMPLES - count + j).toFloat() / (GRAPH_SAMPLES - 1)) * actualGraphW
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

            // Red dots at graph bottom for samples with packet loss
            val dotR = 3f * density
            for (j in 0 until count) {
                val ringIdx = (startIdx - count + j + GRAPH_SAMPLES) % GRAPH_SAMPLES
                if (lossHistory[ringIdx] > 0) {
                    val x = graphLeft + ((GRAPH_SAMPLES - count + j).toFloat() / (GRAPH_SAMPLES - 1)) * actualGraphW
                    canvas.drawCircle(x, graphBottom - dotR - 1f, dotR, lossDotPaint)
                }
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
