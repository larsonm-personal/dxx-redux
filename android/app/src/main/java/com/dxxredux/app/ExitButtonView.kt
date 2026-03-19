package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

/**
 * Small circular "EXIT" button shown in the upper-left corner of the game surface.
 * Always visible while the game activity is running (menus and gameplay alike).
 * Tapping it pushes SDL_QUIT via META_RETURN_TO_LAUNCHER to return to the launcher.
 */
class ExitButtonView(
    context: Context,
) : View(context) {
    var exitCallback: (() -> Unit)? = null

    private val bgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x44000000
            style = Paint.Style.FILL
        }
    private val borderPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x66FFFFFF
            style = Paint.Style.STROKE
            strokeWidth = 2f
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAAFFFFFF.toInt()
            textAlign = Paint.Align.CENTER
            isFakeBoldText = true
        }
    private val pressedBgPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x88444444.toInt()
            style = Paint.Style.FILL
        }

    private var cx = 0f
    private var cy = 0f
    private var radius = 0f
    private var pressed = false

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldW: Int,
        oldH: Int,
    ) {
        super.onSizeChanged(w, h, oldW, oldH)
        val diameter = min(w, h) * 0.07f
        radius = diameter / 2f
        val margin = radius * 0.5f
        cx = radius + margin
        cy = radius + margin
        textPaint.textSize = radius * 0.6f
        borderPaint.strokeWidth = radius * 0.04f
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawCircle(cx, cy, radius, if (pressed) pressedBgPaint else bgPaint)
        canvas.drawCircle(cx, cy, radius, borderPaint)
        val textY = cy - (textPaint.descent() + textPaint.ascent()) / 2f
        canvas.drawText("EXIT", cx, textY, textPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val dx = event.x - cx
        val dy = event.y - cy
        val inside = dx * dx + dy * dy <= radius * radius * 1.5f

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
                    exitCallback?.invoke()
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
