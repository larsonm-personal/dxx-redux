package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

/**
 * "ACCEPT: callsign" pill button shown when a player requests to join
 * a game in progress (RefusePlayers=1, WaitForRefuseAnswer=1).
 * Tapping it calls nativeAcceptJoinRequest() which sets RefuseThisPlayer=1.
 */
class AcceptJoinButtonView(
    context: Context,
) : View(context) {
    var acceptCallback: (() -> Unit)? = null
    var callsign: String = ""
        set(value) {
            if (field != value) {
                field = value
                invalidate()
            }
        }

    private val bgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x880066AA.toInt()
            style = Paint.Style.FILL
        }
    private val borderPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAA00AAFF.toInt()
            style = Paint.Style.STROKE
            strokeWidth = 3f
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFCCDDFF.toInt()
            textAlign = Paint.Align.CENTER
            isFakeBoldText = true
        }
    private val pressedBgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAA0088DD.toInt()
            style = Paint.Style.FILL
        }

    private val bounds = RectF()
    private var cornerRadius = 0f
    private var pressed = false

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldW: Int,
        oldH: Int,
    ) {
        super.onSizeChanged(w, h, oldW, oldH)
        val unit = min(w, h) * 0.05f
        val pillW = unit * 8f
        val pillH = unit * 1.4f
        cornerRadius = pillH / 2f
        // Position above center to avoid conflicting with bottom controls
        bounds.set(
            w / 2f - pillW / 2f,
            h * 0.35f - pillH / 2f,
            w / 2f + pillW / 2f,
            h * 0.35f + pillH / 2f,
        )
        textPaint.textSize = pillH * 0.40f
        borderPaint.strokeWidth = unit * 0.06f
    }

    override fun onDraw(canvas: Canvas) {
        if (callsign.isEmpty()) return
        canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, if (pressed) pressedBgPaint else bgPaint)
        canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, borderPaint)
        val label = "ACCEPT: $callsign"
        val textY = bounds.centerY() - (textPaint.descent() + textPaint.ascent()) / 2f
        canvas.drawText(label, bounds.centerX(), textY, textPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (callsign.isEmpty()) return false
        val inside = bounds.contains(event.x, event.y)
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                if (inside) {
                    pressed = true
                    invalidate()
                    return true
                }
                return false
            }

            MotionEvent.ACTION_UP -> {
                if (pressed && inside) acceptCallback?.invoke()
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
}
