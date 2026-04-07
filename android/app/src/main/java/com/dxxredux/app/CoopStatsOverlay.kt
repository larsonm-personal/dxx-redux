package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.View

/**
 * In-game overlay showing coop robot kill stats and teammate status.
 *
 * Shows: robot kill count (killed/total), per-player score %, teammate
 * shields/energy/secondary weapon.
 *
 * Only visible in coop multiplayer (GM_MULTI_COOP).
 *
 * Shared constants: MAX_PLAYERS = 8 (duplicated from player.h)
 */
class CoopStatsOverlay(
    context: Context,
) : View(context) {
    /** Provider that calls nativeGetCoopRobotStats(). */
    var robotStatsProvider: (() -> IntArray?)? = null

    /** Provider that calls nativeGetTeammateStatus(). */
    var teammateStatusProvider: (() -> IntArray?)? = null

    /** Provider that calls nativeGetEscortOwnerPlayer(). */
    var escortOwnerProvider: (() -> Int)? = null

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false

    // Kill stats from last poll
    private var totalKilled = 0
    private var robotCount = 0
    private var totalRobotScore = 0
    private var nPlayers = 0
    private var localPlayerNum = 0
    private var perPlayerKills = IntArray(MAX_PLAYERS)
    private var perPlayerScore = IntArray(MAX_PLAYERS)

    // Teammate status from last poll
    private var gameMode = 0
    private var connected = IntArray(MAX_PLAYERS)
    private var shieldsPct = IntArray(MAX_PLAYERS)
    private var energyPct = IntArray(MAX_PLAYERS)
    private var secondaryWeapon = IntArray(MAX_PLAYERS)
    private var secondaryAmmo = IntArray(MAX_PLAYERS)

    // Escort (Guide-Bot) owner from last poll (-1 = none)
    private var escortOwnerPlayer = -1

    private val pollRunnable =
        object : Runnable {
            override fun run() {
                if (!polling) return
                try {
                    val rs = robotStatsProvider?.invoke()
                    if (rs != null && rs.size >= 5 + 2 * MAX_PLAYERS) {
                        totalKilled = rs[0]
                        robotCount = rs[1]
                        totalRobotScore = rs[2]
                        nPlayers = rs[3]
                        localPlayerNum = rs[4]
                        for (i in 0 until MAX_PLAYERS) {
                            perPlayerKills[i] = rs[5 + i * 2]
                            perPlayerScore[i] = rs[5 + i * 2 + 1]
                        }
                    }
                    val ts = teammateStatusProvider?.invoke()
                    if (ts != null && ts.size >= 3 + 5 * MAX_PLAYERS) {
                        // nPlayers and localPlayerNum already set from robot stats
                        gameMode = ts[2]
                        for (i in 0 until MAX_PLAYERS) {
                            val base = 3 + i * 5
                            connected[i] = ts[base]
                            shieldsPct[i] = ts[base + 1]
                            energyPct[i] = ts[base + 2]
                            secondaryWeapon[i] = ts[base + 3]
                            secondaryAmmo[i] = ts[base + 4]
                        }
                    }
                } catch (_: Exception) {
                    // JNI not ready yet
                }
                // Poll escort owner
                try {
                    escortOwnerPlayer = escortOwnerProvider?.invoke() ?: -1
                } catch (_: Exception) {
                    escortOwnerPlayer = -1
                }
                // Auto-hide if not in coop
                if (gameMode and GM_MULTI_COOP == 0) {
                    if (visibility == VISIBLE) visibility = GONE
                } else {
                    if (visibility == GONE && polling) visibility = VISIBLE
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

    fun startPolling() {
        if (!polling) {
            polling = true
            handler.post(pollRunnable)
        }
    }

    fun stopPolling() {
        polling = false
        handler.removeCallbacks(pollRunnable)
    }

    // Paints
    private val bgPaint =
        Paint().apply {
            color = 0x88000000.toInt()
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
    private val goodPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFF44FF44u.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val warnPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFFFFF44u.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val badPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFFF4444u.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val barBgPaint =
        Paint().apply {
            color = 0x44FFFFFF
            style = Paint.Style.FILL
        }
    private val shieldBarPaint =
        Paint().apply {
            color = 0xFF4488FFu.toInt()
            style = Paint.Style.FILL
        }
    private val energyBarPaint =
        Paint().apply {
            color = 0xFFFFDD44u.toInt()
            style = Paint.Style.FILL
        }

    private val panelBounds = RectF()

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (visibility != VISIBLE) return
        if (gameMode and GM_MULTI_COOP == 0) return
        if (nPlayers < 2) return

        val density = resources.displayMetrics.density
        val w = width.toFloat()
        val baseTextSize = (10f * density).coerceAtMost(w * 0.013f)
        titlePaint.textSize = baseTextSize * 1.1f
        labelPaint.textSize = baseTextSize
        valuePaint.textSize = baseTextSize
        goodPaint.textSize = baseTextSize
        warnPaint.textSize = baseTextSize
        badPaint.textSize = baseTextSize

        val pad = 6f * density
        val lineH = baseTextSize * 1.4f
        val barH = baseTextSize * 0.6f

        // Count active players for sizing
        var activePlayers = 0
        for (i in 0 until MAX_PLAYERS) {
            if (connected[i] != 0) activePlayers++
        }
        if (activePlayers < 1) activePlayers = 1

        // Lines: title + kill count + per-player (2 lines each: score + bars) + escort owner
        val hasEscort = escortOwnerPlayer >= 0
        val numLines = 2 + activePlayers * 2 + if (hasEscort) 1 else 0
        val panelH = pad * 2 + lineH * numLines
        val panelW = baseTextSize * 16f

        // Position: top-left corner (video overlay is top-right)
        val panelLeft = pad
        val panelTop = pad

        panelBounds.set(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH)
        canvas.drawRoundRect(panelBounds, pad, pad, bgPaint)

        var y = panelTop + pad + titlePaint.textSize

        // Title: "COOP 30/130"
        val killStr = "$totalKilled/$robotCount"
        canvas.drawText("COOP $killStr", panelLeft + pad, y, titlePaint)
        y += lineH

        // Score summary line
        val scoreEarned = perPlayerScore.sum()
        val scorePctStr = if (totalRobotScore > 0) "${scoreEarned * 100 / totalRobotScore}%" else "--"
        canvas.drawText("Score: $scorePctStr earned", panelLeft + pad, y, labelPaint)
        y += lineH

        // Per-player rows
        val barLeft = panelLeft + pad
        val barMaxW = panelW - pad * 2

        for (i in 0 until MAX_PLAYERS) {
            if (connected[i] == 0) continue

            val isLocal = i == localPlayerNum
            val namePaint = if (isLocal) valuePaint else labelPaint

            // Player line: "P0: 15 kills  42%"
            val pKills = perPlayerKills[i]
            val pScore = perPlayerScore[i]
            val pPct = if (totalRobotScore > 0) pScore * 100 / totalRobotScore else 0
            val marker = if (isLocal) "*" else " "
            val label = "${marker}P$i: $pKills kills  $pPct%"
            canvas.drawText(label, barLeft, y, namePaint)
            y += lineH

            // Shield/energy bars
            val shPct = shieldsPct[i].coerceIn(0, 200)
            val enPct = energyPct[i].coerceIn(0, 200)
            val halfW = (barMaxW - pad) / 2

            // Shield bar
            canvas.drawRect(barLeft, y - barH, barLeft + halfW, y, barBgPaint)
            val shW = halfW * shPct / 200
            canvas.drawRect(barLeft, y - barH, barLeft + shW, y, shieldBarPaint)

            // Energy bar
            val enLeft = barLeft + halfW + pad
            canvas.drawRect(enLeft, y - barH, enLeft + halfW, y, barBgPaint)
            val enW = halfW * enPct / 200
            canvas.drawRect(enLeft, y - barH, enLeft + enW, y, energyBarPaint)

            // Secondary weapon label
            val secName = SECONDARY_NAMES.getOrElse(secondaryWeapon[i]) { "?" }
            val secStr = "$secName:${secondaryAmmo[i]}"
            val secPaint = if (secondaryAmmo[i] > 0) goodPaint else badPaint
            canvas.drawText(secStr, enLeft, y - barH - 2f * density, secPaint)

            y += lineH
        }

        // Escort (Guide-Bot) owner line
        if (hasEscort) {
            val ownerLabel = if (escortOwnerPlayer == localPlayerNum) "You" else "P$escortOwnerPlayer"
            canvas.drawText("Guide-Bot: $ownerLabel", panelLeft + pad, y, labelPaint)
            y += lineH
        }
    }

    companion object {
        private const val POLL_INTERVAL_MS = 1000L
        private const val MAX_PLAYERS = 8

        // Game_mode flags (from game.h)
        private const val GM_MULTI_COOP = 16

        // Secondary weapon short names (from weapon.h order)
        private val SECONDARY_NAMES =
            arrayOf(
                "Conc", // 0: CONCUSSION_INDEX
                "Hom", // 1: HOMING_INDEX
                "Prox", // 2: PROXIMITY_INDEX
                "Smt", // 3: SMART_INDEX
                "Meg", // 4: MEGA_INDEX
                "Flsh", // 5: SMISSILE1_INDEX (Flash)
                "Guid", // 6: GUIDED_INDEX
                "SBmb", // 7: SMART_MINE_INDEX
                "Merc", // 8: SMISSILE4_INDEX (Mercury)
                "ShkR", // 9: SMISSILE5_INDEX (Earthshaker)
            )
    }
}
