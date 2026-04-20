package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.os.SystemClock
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
    var skipEveryLaunchCallback: (() -> Unit)? = null
    var label: String = "SKIP"
        set(value) {
            if (field != value) {
                field = value
                if (width > 0 && height > 0) updateGeometry(width, height)
                invalidate()
            }
        }
    var bigLabel: Boolean = false
        set(value) {
            if (field != value) {
                field = value
                if (width > 0 && height > 0) updateGeometry(width, height)
                invalidate()
            }
        }

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
    private val pillRect = RectF()
    private var pillCornerRadius = 0f
    private var circleTextSize = 0f
    private var pillTextSize = 0f
    private var pressed = false
    private var armAtMs = 0L

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
        updateGeometry(w, h)
    }

    private fun updateGeometry(
        w: Int,
        h: Int,
    ) {
        val diameter = min(w, h) * 0.10f
        radius = diameter / 2f
        val margin = radius * 0.4f
        cx = w - radius - margin
        cy = radius + margin
        circleTextSize = radius * 0.55f

        val pillHeight = min(w, h) * 0.11f
        val pillMargin = pillHeight * 0.35f
        val pillWidth = w * 0.42f
        pillRect.set(w - pillWidth - pillMargin, pillMargin, w - pillMargin, pillMargin + pillHeight)
        pillCornerRadius = pillHeight / 2f
        pillTextSize = min(pillHeight * 0.42f, pillRect.width() / label.length.coerceAtLeast(1) * 1.8f)

        borderPaint.strokeWidth = min(radius * 0.04f, pillHeight * 0.06f)
    }

    override fun onDraw(canvas: Canvas) {
        if (bigLabel) {
            textPaint.textSize = pillTextSize
            canvas.drawRoundRect(pillRect, pillCornerRadius, pillCornerRadius, if (pressed) pressedBgPaint else bgPaint)
            canvas.drawRoundRect(pillRect, pillCornerRadius, pillCornerRadius, borderPaint)
            val textY = pillRect.centerY() - (textPaint.descent() + textPaint.ascent()) / 2f
            canvas.drawText(label, pillRect.centerX(), textY, textPaint)
        } else {
            textPaint.textSize = circleTextSize
            canvas.drawCircle(cx, cy, radius, if (pressed) pressedBgPaint else bgPaint)
            canvas.drawCircle(cx, cy, radius, borderPaint)
            val textY = cy - (textPaint.descent() + textPaint.ascent()) / 2f
            canvas.drawText(label, cx, textY, textPaint)
        }
    }

    override fun onVisibilityChanged(
        changedView: View,
        visibility: Int,
    ) {
        super.onVisibilityChanged(changedView, visibility)
        if (changedView === this && visibility == VISIBLE) {
            // Ignore stale touch-up events during state transitions
            armAtMs = SystemClock.uptimeMillis() + ARM_DELAY_MS
            pressed = false
            invalidate()
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val inside = isInside(event.x, event.y)

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                if (SystemClock.uptimeMillis() < armAtMs) {
                    return false
                }
                if (inside) {
                    pressed = true
                    invalidate()
                    return true
                }
                return false
            }
            MotionEvent.ACTION_UP -> {
                if (pressed && inside) {
                    triggerSkipAction()
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

    private fun isInside(
        x: Float,
        y: Float,
    ): Boolean {
        if (bigLabel) {
            return pillRect.contains(x, y)
        }

        val dx = x - cx
        val dy = y - cy
        return dx * dx + dy * dy <= radius * radius * 1.5f
    }

    private fun triggerSkipAction() {
        if (bigLabel) {
            skipEveryLaunchCallback?.invoke()
        }
        keyCallback?.invoke(0, KeyEvent.KEYCODE_ESCAPE, 0)
        keyCallback?.invoke(1, KeyEvent.KEYCODE_ESCAPE, 0)
    }

    companion object {
        private const val ARM_DELAY_MS = 500L
    }
}
