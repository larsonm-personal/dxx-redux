package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

/**
 * Circular "SKIP" button shown in the upper-right corner during movies and briefings.
 * Tapping it injects an ESC key event to skip the current skippable screen.
 */
class SkipButtonView(
    context: Context,
) : View(context) {
    var keyCallback: ((action: Int, keyCode: Int, unicode: Int) -> Unit)? = null

    private val bgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x66000000 // semi-transparent black
            style = Paint.Style.FILL
        }
    private val borderPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x99FFFFFF.toInt()
            style = Paint.Style.STROKE
            strokeWidth = 2f
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xDDFFFFFF.toInt()
            textAlign = Paint.Align.CENTER
            isFakeBoldText = true
        }

    private var cx = 0f
    private var cy = 0f
    private var radius = 0f
    private var pressed = false

    private val pressedBgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x88444444.toInt()
            style = Paint.Style.FILL
        }

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldW: Int,
        oldH: Int,
    ) {
        super.onSizeChanged(w, h, oldW, oldH)
        val diameter = min(w, h) * 0.10f
        radius = diameter / 2f
        val margin = radius * 0.4f
        cx = w - radius - margin
        cy = radius + margin
        textPaint.textSize = radius * 0.55f
        borderPaint.strokeWidth = radius * 0.04f
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawCircle(cx, cy, radius, if (pressed) pressedBgPaint else bgPaint)
        canvas.drawCircle(cx, cy, radius, borderPaint)
        val textY = cy - (textPaint.descent() + textPaint.ascent()) / 2f
        canvas.drawText("SKIP", cx, textY, textPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val dx = event.x - cx
        val dy = event.y - cy
        val inside = dx * dx + dy * dy <= radius * radius * 1.5f // slight tolerance

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
                    injectEscape()
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

    private fun injectEscape() {
        keyCallback?.invoke(0, KeyEvent.KEYCODE_ESCAPE, 0)
        keyCallback?.invoke(1, KeyEvent.KEYCODE_ESCAPE, 0)
    }
}
