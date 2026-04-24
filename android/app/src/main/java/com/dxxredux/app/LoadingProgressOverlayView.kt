package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.view.View
import kotlin.math.max

internal data class LoadingProgressOverlayLayout(
    val left: Float,
    val top: Float,
    val right: Float,
    val bottom: Float,
    val cornerRadius: Float,
    val inset: Float,
    val phaseBaseline: Float,
    val phaseTextSize: Float,
    val itemTextSize: Float,
    val borderWidth: Float,
    val clampedPercent: Int,
)

internal fun computeLoadingProgressOverlayLayout(
    width: Int,
    height: Int,
    density: Float,
    percent: Int,
): LoadingProgressOverlayLayout {
    val barWidth = width * 0.8f
    val barHeight = max(24f * density, height * 0.04f)
    val barLeft = (width - barWidth) * 0.5f
    val barTop = height * 0.8f
    return LoadingProgressOverlayLayout(
        left = barLeft,
        top = barTop,
        right = barLeft + barWidth,
        bottom = barTop + barHeight,
        cornerRadius = 6f * density,
        inset = max(2f * density, 2f),
        phaseBaseline = barTop - (12f * density),
        phaseTextSize = max(14f * density, height * 0.022f),
        itemTextSize = max(15f * density, height * 0.024f),
        borderWidth = max(2f * density, 2f),
        clampedPercent = percent.coerceIn(0, 100),
    )
}

class LoadingProgressOverlayView(
    context: Context,
) : View(context) {
    private val hideHandler = Handler(Looper.getMainLooper())
    private val barRect = RectF()
    private val innerRect = RectF()
    private var phaseLabel = "Prepare for Descent"
    private var itemLabel = ""
    private var progressPercent = 0

    private val hideRunnable =
        Runnable {
            visibility = GONE
        }

    private val backgroundPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.argb(128, 0, 0, 0)
            style = Paint.Style.FILL
        }

    private val borderPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.argb(224, 255, 255, 255)
            style = Paint.Style.STROKE
        }

    private val fillPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.argb(192, 72, 194, 104)
            style = Paint.Style.FILL
        }

    private val phasePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.argb(210, 255, 255, 255)
            typeface = Typeface.MONOSPACE
            textAlign = Paint.Align.CENTER
            setShadowLayer(2f, 0f, 1f, Color.argb(180, 0, 0, 0))
        }

    private val itemPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.argb(235, 255, 255, 255)
            typeface = Typeface.MONOSPACE
            textAlign = Paint.Align.CENTER
            setShadowLayer(2f, 0f, 1f, Color.argb(200, 0, 0, 0))
        }

    init {
        visibility = GONE
        isClickable = false
        isFocusable = false
    }

    fun showProgress(
        phase: String,
        item: String,
        percent: Int,
    ) {
        hideHandler.removeCallbacks(hideRunnable)
        phaseLabel = phase.ifBlank { "Prepare for Descent" }
        itemLabel = item
        progressPercent = percent.coerceIn(0, 100)
        visibility = VISIBLE
        invalidate()
        if (progressPercent >= 100) {
            hideHandler.postDelayed(hideRunnable, COMPLETE_HIDE_DELAY_MS)
        }
    }

    fun hideProgress() {
        hideHandler.removeCallbacks(hideRunnable)
        hideHandler.postDelayed(hideRunnable, COMPLETE_HIDE_DELAY_MS)
    }

    override fun onDetachedFromWindow() {
        hideHandler.removeCallbacks(hideRunnable)
        super.onDetachedFromWindow()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (visibility != VISIBLE || width <= 0 || height <= 0) return

        val density = resources.displayMetrics.density
        val layout = computeLoadingProgressOverlayLayout(width, height, density, progressPercent)

        phasePaint.textSize = layout.phaseTextSize
        itemPaint.textSize = layout.itemTextSize
        borderPaint.strokeWidth = layout.borderWidth

        barRect.set(layout.left, layout.top, layout.right, layout.bottom)
        innerRect.set(
            barRect.left + layout.inset,
            barRect.top + layout.inset,
            barRect.right - layout.inset,
            barRect.bottom - layout.inset,
        )

        canvas.drawRoundRect(barRect, layout.cornerRadius, layout.cornerRadius, backgroundPaint)

        if (layout.clampedPercent > 0) {
            val progressRight = innerRect.left + innerRect.width() * (layout.clampedPercent / 100f)
            if (progressRight > innerRect.left) {
                val fillRect = RectF(innerRect.left, innerRect.top, progressRight, innerRect.bottom)
                canvas.drawRoundRect(fillRect, layout.cornerRadius, layout.cornerRadius, fillPaint)
            }
        }

        canvas.drawRoundRect(barRect, layout.cornerRadius, layout.cornerRadius, borderPaint)
        canvas.drawText(phaseLabel, width * 0.5f, layout.phaseBaseline, phasePaint)

        val itemText = if (itemLabel.isBlank()) phaseLabel else itemLabel
        val itemY = barRect.centerY() - (itemPaint.ascent() + itemPaint.descent()) * 0.5f
        canvas.drawText(itemText, width * 0.5f, itemY, itemPaint)
    }

    companion object {
        private const val COMPLETE_HIDE_DELAY_MS = 250L
    }
}
