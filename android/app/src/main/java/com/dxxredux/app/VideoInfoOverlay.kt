package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.View

/**
 * In-game overlay showing video/rendering diagnostics.
 *
 * Shows: FPS, hi-res texture count and percentage, max texture resolution,
 * GL texture size cap.
 *
 * Toggle via admin tray (ADMIN_VIDEO_INFO).
 */
class VideoInfoOverlay(
    context: Context,
) : View(context) {
    /** Provider that calls nativeGetVideoStats(). */
    var statsProvider: (() -> IntArray?)? = null

    /** Setter for C-side debug flags: (name, value) -> nativeSetDebugFlag. */
    var debugFlagSetter: ((String, Int) -> Unit)? = null

    private val handler = Handler(Looper.getMainLooper())
    private var polling = false

    // Cached stats from last poll
    private var fps = 0
    private var totalLoaded = 0
    private var hiresCount = 0
    private var maxHiresW = 0
    private var maxHiresH = 0
    private var glMaxTexSize = 0
    private var texMemoryKb = 0
    private var renderW = 0
    private var renderH = 0
    private var displayW = 0
    private var displayH = 0

    // Labels toggle button state and hit region
    private var labelsOn = false
    private var buttonPressed = false
    private val buttonRect = RectF()
    private val panelBounds = RectF()

    private val pollRunnable =
        object : Runnable {
            override fun run() {
                if (!polling) return
                try {
                    val stats = statsProvider?.invoke()
                    if (stats != null && stats.size >= 11) {
                        fps = stats[0]
                        totalLoaded = stats[1]
                        hiresCount = stats[2]
                        maxHiresW = stats[3]
                        maxHiresH = stats[4]
                        glMaxTexSize = stats[5]
                        texMemoryKb = stats[6]
                        renderW = stats[7]
                        renderH = stats[8]
                        displayW = stats[9]
                        displayH = stats[10]
                    }
                } catch (_: Exception) {
                    // JNI not ready yet
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

    fun toggle() {
        if (visibility == VISIBLE) hide() else show()
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
    private val fpsGoodPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFF44FF44u.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val fpsWarnPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFFFFF44u.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val fpsBadPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFFF4444u.toInt()
            typeface = android.graphics.Typeface.MONOSPACE
        }
    private val btnNormalPaint =
        Paint().apply {
            color = 0x44FFFFFF
            style = Paint.Style.FILL
        }
    private val btnPressedPaint =
        Paint().apply {
            color = 0x66FFFFFF
            style = Paint.Style.FILL
        }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (visibility != VISIBLE) return

        val density = resources.displayMetrics.density
        val w = width.toFloat()
        val baseTextSize = (11f * density).coerceAtMost(w * 0.014f)
        titlePaint.textSize = baseTextSize * 1.1f
        labelPaint.textSize = baseTextSize
        valuePaint.textSize = baseTextSize
        fpsGoodPaint.textSize = baseTextSize * 1.3f
        fpsWarnPaint.textSize = baseTextSize * 1.3f
        fpsBadPaint.textSize = baseTextSize * 1.3f

        val pad = 8f * density
        val lineH = baseTextSize * 1.5f
        val numLines = 8 // title + fps + tex mem + hires + max res + resolution + gl cap + labels
        val panelH = pad * 2 + lineH * numLines
        val panelW = baseTextSize * 20f

        // Position: top-right corner
        val panelLeft = w - panelW - pad
        val panelTop = pad

        // Background
        panelBounds.set(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH)
        canvas.drawRoundRect(panelBounds, pad, pad, bgPaint)

        var y = panelTop + pad + titlePaint.textSize

        // Title
        canvas.drawText("VIDEO", panelLeft + pad, y, titlePaint)
        y += lineH

        // FPS with color coding
        val fpsPaint =
            when {
                fps >= 30 -> fpsGoodPaint
                fps >= 20 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("FPS: $fps", panelLeft + pad, y, fpsPaint)
        y += lineH

        // Texture memory
        val texMb = texMemoryKb / 1024
        canvas.drawText("Tex mem:", panelLeft + pad, y, labelPaint)
        canvas.drawText("${texMb}MB", panelLeft + pad + baseTextSize * 6f, y, valuePaint)
        y += lineH

        // Hi-res textures
        val pct = if (totalLoaded > 0) (hiresCount * 100 / totalLoaded) else 0
        canvas.drawText("Hires:", panelLeft + pad, y, labelPaint)
        canvas.drawText(
            "$hiresCount/$totalLoaded ($pct%)",
            panelLeft + pad + baseTextSize * 6f,
            y,
            valuePaint,
        )
        y += lineH

        // Max hires texture resolution
        canvas.drawText("Max res:", panelLeft + pad, y, labelPaint)
        val resText = if (hiresCount > 0) "%dx%d".format(maxHiresW, maxHiresH) else "n/a"
        canvas.drawText(resText, panelLeft + pad + baseTextSize * 6f, y, valuePaint)
        y += lineH

        // GL texture cap
        canvas.drawText("GL cap:", panelLeft + pad, y, labelPaint)
        canvas.drawText("$glMaxTexSize" + "px", panelLeft + pad + baseTextSize * 6f, y, valuePaint)
        y += lineH

        // Render vs display resolution
        canvas.drawText("Render:", panelLeft + pad, y, labelPaint)
        canvas.drawText(
            "${renderW}x$renderH / ${displayW}x$displayH",
            panelLeft + pad + baseTextSize * 6f,
            y,
            valuePaint,
        )
        y += lineH

        // Labels toggle button (tappable, with background)
        val labelsText = if (labelsOn) "Labels: ON" else "Labels: OFF"
        val labelTogglePaint = if (labelsOn) fpsGoodPaint else fpsWarnPaint
        buttonRect.set(
            panelLeft + pad * 0.5f,
            y - baseTextSize,
            panelLeft + panelW - pad * 0.5f,
            y + lineH * 0.3f,
        )
        val btnBg = if (buttonPressed) btnPressedPaint else btnNormalPaint
        canvas.drawRoundRect(buttonRect, pad * 0.5f, pad * 0.5f, btnBg)
        canvas.drawText(labelsText, panelLeft + pad, y, labelTogglePaint)
    }

    @Suppress("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (visibility != VISIBLE) return super.onTouchEvent(event)
        val inPanel = panelBounds.contains(event.x, event.y)
        val inButton = buttonRect.contains(event.x, event.y)

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                if (inButton) {
                    buttonPressed = true
                    invalidate()
                    return true
                }
                if (inPanel) return true
            }
            MotionEvent.ACTION_UP -> {
                if (buttonPressed && inButton) {
                    labelsOn = !labelsOn
                    debugFlagSetter?.invoke("tex_overlay", if (labelsOn) 1 else 0)
                    performClick()
                }
                if (buttonPressed || inPanel) {
                    buttonPressed = false
                    invalidate()
                    return true
                }
            }
            MotionEvent.ACTION_CANCEL -> {
                buttonPressed = false
                invalidate()
            }
        }
        return super.onTouchEvent(event)
    }

    override fun performClick(): Boolean = super.performClick()

    companion object {
        private const val POLL_INTERVAL_MS = 500L
    }
}
