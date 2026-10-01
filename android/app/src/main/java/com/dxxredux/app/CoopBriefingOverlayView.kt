package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

/** Native-authoritative countdown and a distinct bottom-left host launch control. */
class CoopBriefingOverlayView(
    context: Context,
) : View(context) {
    var launchCallback: ((Long) -> Boolean)? = null
    private var generation = 0L
    private var canLaunch = false
    private var launchLabel = "Launch now"
    private var status = ""
    private var remainingMs = -1
    private var durationMs = 0
    private var pressedGeneration = 0L
    private var pointerId = -1
    private val launchBounds = RectF()
    private val background = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xDD151515.toInt() }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFFFC600.toInt() }

    fun update(nativeState: String) {
        // Mirrored by nativeGetCoopBriefingState in android_input.c
        val fields = nativeState.split('\n', limit = 5)
        val nextGeneration = fields.getOrNull(0)?.toLongOrNull() ?: 0L
        val nextLaunch = fields.getOrNull(1) in listOf("-1", "1", "2")
        launchLabel =
            when (fields.getOrNull(1)) {
                "-1" -> "Cancel join"
                "2" -> "Continue now"
                else -> "Launch now"
            }
        if (nextGeneration != generation || !nextLaunch) cancelPress()
        generation = nextGeneration
        canLaunch = nextLaunch
        remainingMs = fields.getOrNull(2)?.toIntOrNull() ?: -1
        durationMs = fields.getOrNull(3)?.toIntOrNull() ?: 0
        status = fields.getOrNull(4).orEmpty()
        visibility = if (status.isEmpty()) GONE else VISIBLE
        contentDescription = if (canLaunch) "$status. $launchLabel" else status
        invalidate()
    }

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldw: Int,
        oldh: Int,
    ) {
        cancelPress()
        val unit = min(w, h) * 0.05f
        text.textSize = unit * 0.65f
        // Skip/Next is upper-right. Keep this hit target bottom-left on every layout.
        launchBounds.set(unit, h - unit * 2.4f, unit * 8f, h - unit)
    }

    override fun onDraw(canvas: Canvas) {
        val padding = text.textSize * 0.5f
        val lines = status.lines()
        val lineHeight = text.textSize * 1.35f
        val maxWidth = lines.maxOfOrNull { text.measureText(it) } ?: 0f
        val left = (width - maxWidth) / 2f - padding
        val barHeight = if (durationMs != 0) padding else 0f
        val panelBottom = padding * 2 + lines.size * lineHeight
        canvas.drawRoundRect(
            left,
            padding,
            left + maxWidth + 2 * padding,
            panelBottom + barHeight * 2,
            padding,
            padding,
            background,
        )
        lines.forEachIndexed { index, line ->
            canvas.drawText(line, left + padding, padding * 2 + (index + 0.8f) * lineHeight, text)
        }
        if (durationMs != 0) {
            val fraction =
                if (remainingMs >= 0 && durationMs > 0) {
                    (remainingMs.toFloat() / durationMs).coerceIn(0f, 1f)
                } else {
                    0.2f
                }
            val offset =
                if (remainingMs >= 0 && durationMs > 0) {
                    0f
                } else {
                    (SystemClock.uptimeMillis() % 1600L) / 1600f * (1f - fraction)
                }
            canvas.drawRoundRect(
                left + padding + maxWidth * offset,
                panelBottom,
                left + padding + maxWidth * (offset + fraction),
                panelBottom + barHeight,
                barHeight / 2,
                barHeight / 2,
                text,
            )
            if (remainingMs < 0) postInvalidateOnAnimation()
        }
        if (canLaunch) {
            canvas.drawRoundRect(launchBounds, padding, padding, background)
            val label = launchLabel.uppercase()
            canvas.drawText(
                label,
                launchBounds.centerX() - text.measureText(label) / 2,
                launchBounds.centerY() - (text.ascent() + text.descent()) / 2,
                text,
            )
        }
    }

    private fun cancelPress() {
        pressedGeneration = 0
        pointerId = -1
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (!canLaunch || generation == 0L || !launchBounds.contains(event.x, event.y)) return false
                pressedGeneration = generation
                pointerId = event.getPointerId(0)
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                val index = event.findPointerIndex(pointerId)
                if (index < 0 || !launchBounds.contains(event.getX(index), event.getY(index))) cancelPress()
            }

            MotionEvent.ACTION_UP -> {
                val deliberate =
                    pointerId == event.getPointerId(event.actionIndex) &&
                        pressedGeneration != 0L && pressedGeneration == generation && canLaunch &&
                        launchBounds.contains(event.x, event.y)
                cancelPress()
                if (deliberate) performClick()
                return true
            }

            MotionEvent.ACTION_CANCEL, MotionEvent.ACTION_POINTER_DOWN -> {
                cancelPress()
            }
        }
        return pointerId >= 0
    }

    override fun performClick(): Boolean {
        super.performClick()
        if (canLaunch && generation != 0L && launchCallback?.invoke(generation) == true) {
            canLaunch = false
            cancelPress()
            invalidate()
        }
        return true
    }
}
