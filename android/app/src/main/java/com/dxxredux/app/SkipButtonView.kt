package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

/**
 * Upper-right action for transient native screens and Android's launch-intro preference.
 */
class SkipButtonView(
    context: Context,
) : View(context) {
    init {
        isClickable = true
    }

    var keyCallback: ((action: Int, keyCode: Int, unicode: Int) -> Unit)? = null
    var screenAdvanceCallback: ((generation: Long) -> Unit)? = null
    var screenAdvanceGeneration: Long? = null
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
    private val measurePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
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
    private var pressedScreenAdvanceGeneration: Long? = null
    private val screenLocation = IntArray(2)

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

        val pillHeight = min(w, h) * 0.055f
        val pillMargin = pillHeight * 0.35f
        val pillBaseTextSize = pillHeight * 0.42f
        textPaint.textSize = pillBaseTextSize
        pillRect.set(computePillRect(w, h, label))
        pillCornerRadius = pillHeight / 2f
        pillTextSize = min(pillBaseTextSize, pillRect.width() / label.length.coerceAtLeast(1) * 1.8f)

        borderPaint.strokeWidth = min(radius * 0.04f, pillHeight * 0.06f)
    }

    override fun onDraw(canvas: Canvas) {
        if (bigLabel) {
            bgPaint.color = 0x33000000
            borderPaint.color = 0x4DFFFFFF
            textPaint.color = 0x6EFFFFFF
            pressedBgPaint.color = 0x44222222
            textPaint.textSize = pillTextSize
            canvas.drawRoundRect(pillRect, pillCornerRadius, pillCornerRadius, if (pressed) pressedBgPaint else bgPaint)
            canvas.drawRoundRect(pillRect, pillCornerRadius, pillCornerRadius, borderPaint)
            val textY = pillRect.centerY() - (textPaint.descent() + textPaint.ascent()) / 2f
            canvas.drawText(label, pillRect.centerX(), textY, textPaint)
        } else {
            bgPaint.color = 0x66000000
            borderPaint.color = 0x99FFFFFF.toInt()
            textPaint.color = 0xDDFFFFFF.toInt()
            pressedBgPaint.color = 0x88444444.toInt()
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
            pressed = false
            pressedScreenAdvanceGeneration = null
            invalidate()
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean = handlePointerEvent(event.x, event.y, event.actionMasked)

    fun handleGlobalTouch(event: MotionEvent): Boolean {
        if (visibility != VISIBLE) {
            return false
        }
        val (localX, localY) = screenToLocal(event)
        return handlePointerEvent(localX, localY, event.actionMasked)
    }

    fun handleSurfaceFallbackTouch(event: MotionEvent): Boolean {
        if (!bigLabel || visibility != VISIBLE) {
            return false
        }
        val (localX, localY) = screenToLocal(event)
        return handlePointerEvent(localX, localY, event.actionMasked, skipEveryLaunch = true)
    }

    fun handleIntroTouch(
        event: MotionEvent,
        hostView: View,
    ): Boolean {
        if (hostView.width <= 0 || hostView.height <= 0) {
            return false
        }
        hostView.getLocationOnScreen(screenLocation)
        val introRect = computePillRect(hostView.width, hostView.height, INTRO_LABEL)
        return handlePointerEvent(
            event.rawX - screenLocation[0],
            event.rawY - screenLocation[1],
            event.actionMasked,
            skipEveryLaunch = true,
            hitRect = introRect,
        )
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    private fun handlePointerEvent(
        x: Float,
        y: Float,
        action: Int,
        skipEveryLaunch: Boolean = bigLabel,
        hitRect: RectF? = null,
    ): Boolean {
        val inside = isInside(x, y, hitRect)

        when (action) {
            MotionEvent.ACTION_DOWN -> {
                if (inside) {
                    pressed = true
                    pressedScreenAdvanceGeneration = screenAdvanceGeneration
                    invalidate()
                    return true
                }
                return false
            }

            MotionEvent.ACTION_UP -> {
                if (pressed && inside) {
                    performClick()
                    triggerSkipAction(skipEveryLaunch, pressedScreenAdvanceGeneration)
                }
                pressed = false
                pressedScreenAdvanceGeneration = null
                invalidate()
            }

            MotionEvent.ACTION_CANCEL -> {
                pressed = false
                pressedScreenAdvanceGeneration = null
                invalidate()
            }
        }
        return pressed
    }

    private fun isInside(
        x: Float,
        y: Float,
        hitRect: RectF? = null,
    ): Boolean {
        val activeRect = hitRect ?: if (bigLabel) pillRect else null
        if (activeRect != null) {
            return activeRect.contains(x, y)
        }

        val dx = x - cx
        val dy = y - cy
        return dx * dx + dy * dy <= radius * radius * 1.5f
    }

    private fun computePillRect(
        w: Int,
        h: Int,
        labelText: String,
    ): RectF {
        val pillHeight = min(w, h) * 0.055f
        val pillMargin = pillHeight * 0.35f
        val pillBaseTextSize = pillHeight * 0.42f
        val pillHorizontalPadding = pillHeight * 0.45f
        measurePaint.textSize = pillBaseTextSize
        val pillWidth = measurePaint.measureText(labelText) + pillHorizontalPadding * 2f
        return RectF(w - pillWidth - pillMargin, pillMargin, w - pillMargin, pillMargin + pillHeight)
    }

    private fun screenToLocal(event: MotionEvent): Pair<Float, Float> {
        getLocationOnScreen(screenLocation)
        return Pair(event.rawX - screenLocation[0], event.rawY - screenLocation[1])
    }

    private fun triggerSkipAction(
        skipEveryLaunch: Boolean,
        generation: Long?,
    ) {
        if (skipEveryLaunch) {
            skipEveryLaunchCallback?.invoke()
        } else if (generation != null) {
            screenAdvanceCallback?.invoke(generation)
            return
        }
        keyCallback?.invoke(0, KeyEvent.KEYCODE_ESCAPE, 0)
        keyCallback?.invoke(1, KeyEvent.KEYCODE_ESCAPE, 0)
    }

    companion object {
        private const val INTRO_LABEL = "Skip every launch"
    }
}
