package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

/**
 * "START GAME" pill button shown at bottom-center of the screen when the host
 * is on the "select players / press enter to begin" screen.
 * Tapping it injects an Enter key event to confirm and launch the game.
 */
class StartGameButtonView(
    context: Context,
) : View(context) {
    var startCallback: (() -> Unit)? = null

    private val bgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x88006600.toInt()
            style = Paint.Style.FILL
        }
    private val borderPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAA00CC00.toInt()
            style = Paint.Style.STROKE
            strokeWidth = 3f
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFCCFFCC.toInt()
            textAlign = Paint.Align.CENTER
            isFakeBoldText = true
        }
    private val pressedBgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAA009900.toInt()
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
        val pillW = unit * 6f
        val pillH = unit * 1.4f
        cornerRadius = pillH / 2f
        bounds.set(
            w / 2f - pillW / 2f,
            h - pillH - unit * 0.8f,
            w / 2f + pillW / 2f,
            h - unit * 0.8f,
        )
        textPaint.textSize = pillH * 0.45f
        borderPaint.strokeWidth = unit * 0.06f
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, if (pressed) pressedBgPaint else bgPaint)
        canvas.drawRoundRect(bounds, cornerRadius, cornerRadius, borderPaint)
        val textY = bounds.centerY() - (textPaint.descent() + textPaint.ascent()) / 2f
        canvas.drawText("START GAME", bounds.centerX(), textY, textPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
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
                if (pressed && inside) {
                    startCallback?.invoke()
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
}
