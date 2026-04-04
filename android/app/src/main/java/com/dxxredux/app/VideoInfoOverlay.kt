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

    /** Persists graphics option changes: (name, value) -> save to SharedPreferences. */
    var settingsSaver: ((String, Int) -> Unit)? = null

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
    private var frameTimeUs = 0
    private var frameTimeAvg = 0
    private var frameTimeMax = 0
    private var texBinds = 0
    private var texBindReuse = 0
    private var drawPolys = 0
    private var cacheTimeMs = 0
    private var anisoLevel = 0
    private var anisoMax = 0
    private var msaaLevel = 0
    private var msaaMax = 0
    private var gpuTimeUs = 0
    private var gpuTimerAvailable = 0
    private var shaderSwitches = 0
    private var maskDraws = 0
    private var colorDepth = 16

    // Labels toggle button state and hit region
    private var labelsOn = false
    private var buttonPressed = false
    private var anisoPressed = false
    private var msaaPressed = false
    private val buttonRect = RectF()
    private val anisoRect = RectF()
    private val msaaRect = RectF()
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
                    if (stats != null && stats.size >= 18) {
                        frameTimeUs = stats[11]
                        frameTimeAvg = stats[12]
                        frameTimeMax = stats[13]
                        texBinds = stats[14]
                        texBindReuse = stats[15]
                        drawPolys = stats[16]
                        cacheTimeMs = stats[17]
                    }
                    if (stats != null && stats.size >= 20) {
                        anisoLevel = stats[18]
                        anisoMax = stats[19]
                    }
                    if (stats != null && stats.size >= 22) {
                        msaaLevel = stats[20]
                        msaaMax = stats[21]
                    }
                    if (stats != null && stats.size >= 24) {
                        gpuTimeUs = stats[22]
                        gpuTimerAvailable = stats[23]
                    }
                    if (stats != null && stats.size >= 26) {
                        shaderSwitches = stats[24]
                        maskDraws = stats[25]
                    }
                    if (stats != null && stats.size >= 27) {
                        colorDepth = stats[26]
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
        // fps frame texmem hires maxres render glcap binds polys cache aniso msaa labels (+ title + gap)
        val numLines = 17
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

        // FPS with color coding (target 25fps)
        val fpsPaint =
            when {
                fps >= 23 -> fpsGoodPaint
                fps >= 18 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("FPS: $fps", panelLeft + pad, y, fpsPaint)
        y += lineH

        // Frame time avg / max with color coding
        val avgMs = "%.1f".format(frameTimeAvg / 1000f)
        val maxMs = "%.1f".format(frameTimeMax / 1000f)
        val frameTimePaint = when {
            frameTimeAvg <= 45000 -> fpsGoodPaint
            frameTimeAvg <= 55000 -> fpsWarnPaint
            else -> fpsBadPaint
        }
        canvas.drawText("Frame:", panelLeft + pad, y, labelPaint)
        canvas.drawText("${avgMs}ms avg / ${maxMs}ms max", panelLeft + pad + baseTextSize * 6f, y, frameTimePaint)
        y += lineH

        // Frame budget load bar (40ms = 100% at 25fps)
        run {
            val barLeft = panelLeft + pad
            val barRight = panelLeft + panelW - pad
            val barTop = y - baseTextSize * 0.6f
            val barBot = y + lineH * 0.1f
            val barW = barRight - barLeft
            val pctFill = (frameTimeAvg / 40000f).coerceIn(0f, 1.5f) / 1.5f
            val barColor = when {
                frameTimeAvg <= 45000 -> 0xFF00CC00.toInt()  // green: at or under 25fps budget
                frameTimeAvg <= 55000 -> 0xFFCCCC00.toInt()  // yellow: over budget
                else -> 0xFFCC0000.toInt()                   // red: severe
            }
            val barBg = Paint().apply { color = 0xFF333333.toInt(); style = Paint.Style.FILL }
            val barFg = Paint().apply { color = barColor; style = Paint.Style.FILL }
            canvas.drawRoundRect(barLeft, barTop, barRight, barBot, 2f, 2f, barBg)
            canvas.drawRoundRect(barLeft, barTop, barLeft + barW * pctFill, barBot, 2f, 2f, barFg)
        }
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

        // Render vs display resolution + color depth
        val cdLabel = if (colorDepth >= 24) "RGB888" else "RGB565"
        canvas.drawText("Render:", panelLeft + pad, y, labelPaint)
        canvas.drawText(
            "${renderW}x$renderH / ${displayW}x$displayH  $cdLabel",
            panelLeft + pad + baseTextSize * 6f,
            y,
            valuePaint,
        )
        y += lineH

        // Texture binds per frame
        val bindTotal = texBinds + texBindReuse
        val hitPct = if (bindTotal > 0) (texBindReuse * 100 / bindTotal) else 0
        canvas.drawText("Binds:", panelLeft + pad, y, labelPaint)
        canvas.drawText("$texBinds ($hitPct% cache)", panelLeft + pad + baseTextSize * 6f, y, valuePaint)
        y += lineH

        // Draw polygons + shader/mask stats
        canvas.drawText("Polys:", panelLeft + pad, y, labelPaint)
        canvas.drawText("$drawPolys  shd:$shaderSwitches  mask:$maskDraws", panelLeft + pad + baseTextSize * 6f, y, valuePaint)
        y += lineH

        // Level cache time (color-coded: >500ms = warn, >2000ms = bad)
        val cachePaint = when {
            cacheTimeMs <= 500 -> valuePaint
            cacheTimeMs <= 2000 -> fpsWarnPaint
            else -> fpsBadPaint
        }
        canvas.drawText("Cache:", panelLeft + pad, y, labelPaint)
        canvas.drawText("${cacheTimeMs}ms", panelLeft + pad + baseTextSize * 6f, y, cachePaint)
        y += lineH

        // Anisotropic filtering cycle button
        val anisoText = if (anisoLevel > 0) "AF: ${anisoLevel}x" else "AF: OFF"
        val anisoPaint = if (anisoLevel > 0) fpsGoodPaint else fpsWarnPaint
        anisoRect.set(
            panelLeft + pad * 0.5f,
            y - baseTextSize,
            panelLeft + panelW - pad * 0.5f,
            y + lineH * 0.3f,
        )
        val anisoBg = if (anisoPressed) btnPressedPaint else btnNormalPaint
        canvas.drawRoundRect(anisoRect, pad * 0.5f, pad * 0.5f, anisoBg)
        val maxText = if (anisoMax > 0) " (max ${anisoMax}x)" else ""
        canvas.drawText(anisoText + maxText, panelLeft + pad, y, anisoPaint)
        y += lineH

        // MSAA cycle button
        val msaaText = if (msaaLevel > 0) "MSAA: ${msaaLevel}x" else "MSAA: OFF"
        val msaaPaint = if (msaaLevel > 0) fpsGoodPaint else fpsWarnPaint
        msaaRect.set(
            panelLeft + pad * 0.5f,
            y - baseTextSize,
            panelLeft + panelW - pad * 0.5f,
            y + lineH * 0.3f,
        )
        val msaaBg = if (msaaPressed) btnPressedPaint else btnNormalPaint
        canvas.drawRoundRect(msaaRect, pad * 0.5f, pad * 0.5f, msaaBg)
        val msaaMaxText = if (msaaMax > 0) " (max ${msaaMax}x)" else ""
        canvas.drawText(msaaText + msaaMaxText, panelLeft + pad, y, msaaPaint)
        y += lineH

        // GPU time
        val gpuText = if (gpuTimerAvailable != 0) "GPU: ${gpuTimeUs / 1000}.${(gpuTimeUs % 1000) / 100}ms" else "GPU: n/a"
        canvas.drawText(gpuText, panelLeft + pad, y, valuePaint)
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
        val inAniso = anisoRect.contains(event.x, event.y)
        val inMsaa = msaaRect.contains(event.x, event.y)

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                if (inButton) {
                    buttonPressed = true
                    invalidate()
                    return true
                }
                if (inAniso) {
                    anisoPressed = true
                    invalidate()
                    return true
                }
                if (inMsaa) {
                    msaaPressed = true
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
                if (anisoPressed && inAniso) {
                    cycleAnisotropy()
                    performClick()
                }
                if (msaaPressed && inMsaa) {
                    cycleMsaa()
                    performClick()
                }
                if (buttonPressed || anisoPressed || msaaPressed || inPanel) {
                    buttonPressed = false
                    anisoPressed = false
                    msaaPressed = false
                    invalidate()
                    return true
                }
            }
            MotionEvent.ACTION_CANCEL -> {
                buttonPressed = false
                anisoPressed = false
                msaaPressed = false
                invalidate()
            }
        }
        return super.onTouchEvent(event)
    }

    override fun performClick(): Boolean = super.performClick()

    private fun cycleAnisotropy() {
        // Cycle: 0 -> 2 -> 4 -> 8 -> 16 -> 0, capped by anisoMax
        val levels = intArrayOf(0, 2, 4, 8, 16).filter { it <= anisoMax || it == 0 }
        val idx = levels.indexOf(anisoLevel)
        val next = levels[(idx + 1) % levels.size]
        anisoLevel = next
        debugFlagSetter?.invoke("aniso_level", next)
        settingsSaver?.invoke("aniso_level", next)
    }

    private fun cycleMsaa() {
        // Cycle: 0 -> 2 -> 4 -> 0, capped by msaaMax
        val levels = intArrayOf(0, 2, 4).filter { it <= msaaMax || it == 0 }
        val idx = levels.indexOf(msaaLevel)
        val next = levels[(idx + 1) % levels.size]
        msaaLevel = next
        debugFlagSetter?.invoke("msaa_level", next)
        settingsSaver?.invoke("msaa_level", next)
    }

    companion object {
        private const val POLL_INTERVAL_MS = 500L
    }
}
