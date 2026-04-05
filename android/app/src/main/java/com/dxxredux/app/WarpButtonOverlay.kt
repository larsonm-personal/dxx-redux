package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.View

/**
 * Overlay button that appears during coop multiplayer when warp-to-player
 * is available.  Tapping warps to the displayed target.  Long-pressing
 * cycles to the next eligible target.
 *
 * android port: coop QoL -- Phase 5 warp button
 */
class WarpButtonOverlay(
    context: Context,
) : View(context) {
    /** Provides [available, target_pnum, cooldown_secs, engaged]. */
    var warpStatusProvider: (() -> IntArray?)? = null

    /** Returns the target player's callsign. */
    var warpTargetNameProvider: (() -> String?)? = null

    /** Called to execute the warp. Returns 1 on success. */
    var warpExecuteCallback: (() -> Int)? = null

    /** Called to cycle to the next warp target. */
    var warpCycleCallback: (() -> Unit)? = null

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false
    private var warpAvailable = false
    private var targetName = ""
    private var cooldownSecs = 0
    private var engaged = false

    private val btnRect = RectF()
    private var pressed = false
    private var pressStartMs = 0L

    private val bgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x88004400.toInt()
            style = Paint.Style.FILL
        }
    private val bgPressedPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAA006600.toInt()
            style = Paint.Style.FILL
        }
    private val borderPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xCC00FF00.toInt()
            style = Paint.Style.STROKE
            strokeWidth = 2f
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xDD00FF00.toInt()
            textAlign = Paint.Align.CENTER
            isFakeBoldText = true
        }
    private val subTextPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAA00CC00.toInt()
            textAlign = Paint.Align.CENTER
        }

    private val pollRunnable =
        object : Runnable {
            override fun run() {
                if (!polling) return
                try {
                    val st = warpStatusProvider?.invoke()
                    if (st != null && st.size >= 4) {
                        warpAvailable = st[0] != 0
                        cooldownSecs = st[2]
                        engaged = st[3] != 0
                    }
                    if (warpAvailable) {
                        targetName = warpTargetNameProvider?.invoke() ?: ""
                    }
                } catch (_: Exception) {
                    // JNI not ready
                }

                visibility = if (warpAvailable) VISIBLE else GONE
                invalidate()
                handler.postDelayed(this, POLL_INTERVAL_MS)
            }
        }

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldW: Int,
        oldH: Int,
    ) {
        super.onSizeChanged(w, h, oldW, oldH)
        val btnW = w * 0.18f
        val btnH = h * 0.08f
        val margin = w * 0.02f
        // Position: left side, vertically centered
        btnRect.set(margin, h * 0.45f, margin + btnW, h * 0.45f + btnH)
        textPaint.textSize = btnH * 0.35f
        subTextPaint.textSize = btnH * 0.25f
        borderPaint.strokeWidth = btnH * 0.03f
    }

    override fun onDraw(canvas: Canvas) {
        if (!warpAvailable) return
        val r = btnRect.height() * 0.15f
        canvas.drawRoundRect(btnRect, r, r, if (pressed) bgPressedPaint else bgPaint)
        canvas.drawRoundRect(btnRect, r, r, borderPaint)

        val cx = btnRect.centerX()
        val topY = btnRect.top + btnRect.height() * 0.42f
        canvas.drawText("WARP", cx, topY, textPaint)

        if (targetName.isNotEmpty()) {
            val botY = btnRect.top + btnRect.height() * 0.75f
            canvas.drawText("to $targetName", cx, botY, subTextPaint)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                if (warpAvailable && btnRect.contains(event.x, event.y)) {
                    pressed = true
                    pressStartMs = System.currentTimeMillis()
                    invalidate()
                    return true
                }
                return false
            }
            MotionEvent.ACTION_UP -> {
                if (pressed && btnRect.contains(event.x, event.y)) {
                    val holdMs = System.currentTimeMillis() - pressStartMs
                    if (holdMs > LONG_PRESS_MS) {
                        // Long press: cycle target
                        try {
                            warpCycleCallback?.invoke()
                        } catch (_: Exception) {
                        }
                    } else {
                        // Short press: execute warp
                        try {
                            warpExecuteCallback?.invoke()
                        } catch (_: Exception) {
                        }
                    }
                }
                pressed = false
                invalidate()
            }
            MotionEvent.ACTION_CANCEL -> {
                pressed = false
                invalidate()
            }
        }
        return pressed
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
        visibility = GONE
    }

    companion object {
        private const val POLL_INTERVAL_MS = 500L
        private const val LONG_PRESS_MS = 600L
    }
}
