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

    /** Setter for C-side graphics options: (name, value) -> nativeSetGraphicsOption.
     *  Works in all builds (not gated by INTROSPECT_ON). */
    var graphicsOptionSetter: ((String, Int) -> Unit)? = null

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
    private var texFiltLevel = 0

    // Labels toggle button state and hit region
    private var labelsOn = false
    private var metl154Mode = 0
    private var metl154ExperimentMode = 0
    private var buttonPressed = false
    private var anisoPressed = false
    private var msaaPressed = false
    private var texFiltPressed = false
    private var metl154Pressed = false
    private var metl154ExperimentPressed = false
    private val buttonRect = RectF()
    private val anisoRect = RectF()
    private val msaaRect = RectF()
    private val texFiltRect = RectF()
    private val metl154Rect = RectF()
    private val metl154ExperimentRect = RectF()
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
                    if (stats != null && stats.size >= 28) {
                        texFiltLevel = stats[27]
                    }
                    if (stats != null && stats.size >= 32) {
                        metl154Mode = stats[30]
                        metl154ExperimentMode = stats[31]
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
        fpsGoodPaint.textSize = baseTextSize
        fpsWarnPaint.textSize = baseTextSize
        fpsBadPaint.textSize = baseTextSize

        val pad = 8f * density
        val lineH = baseTextSize * 1.5f
        val numLines = 19
        val panelH = pad * 2 + lineH * numLines
        val panelW = baseTextSize * 20f

        // Position: top-right corner
        val panelLeft = w - panelW - pad
        val panelTop = pad

        // Background
        panelBounds.set(panelLeft, panelTop, panelLeft + panelW, panelTop + panelH)
        canvas.drawRoundRect(panelBounds, pad, pad, bgPaint)

        val valCol = panelLeft + pad + baseTextSize * 5f
        var y = panelTop + pad + titlePaint.textSize

        // Title + FPS on same line
        canvas.drawText("VIDEO", panelLeft + pad, y, titlePaint)
        val fpsPaint =
            when {
                fps >= 23 -> fpsGoodPaint
                fps >= 18 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("${fps}fps", panelLeft + pad + baseTextSize * 5f, y, fpsPaint)
        y += lineH

        // Frame time avg / max (integer ms)
        val avgMs = frameTimeAvg / 1000
        val maxMs = frameTimeMax / 1000
        val frameTimePaint =
            when {
                frameTimeAvg <= 45000 -> fpsGoodPaint
                frameTimeAvg <= 55000 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("frame", panelLeft + pad, y, labelPaint)
        canvas.drawText("${avgMs}ms avg / ${maxMs}ms max", valCol, y, frameTimePaint)
        y += lineH

        // Frame budget load bar (40ms = 100% at 25fps)
        run {
            val barLeft = panelLeft + pad
            val barRight = panelLeft + panelW - pad
            val barTop = y - baseTextSize * 0.6f
            val barBot = y + lineH * 0.1f
            val barW = barRight - barLeft
            val pctFill = (frameTimeAvg / 40000f).coerceIn(0f, 1.5f) / 1.5f
            val barColor =
                when {
                    frameTimeAvg <= 45000 -> 0xFF00CC00.toInt()
                    frameTimeAvg <= 55000 -> 0xFFCCCC00.toInt()
                    else -> 0xFFCC0000.toInt()
                }
            val barBg =
                Paint().apply {
                    color = 0xFF333333.toInt()
                    style = Paint.Style.FILL
                }
            val barFg =
                Paint().apply {
                    color = barColor
                    style = Paint.Style.FILL
                }
            canvas.drawRoundRect(barLeft, barTop, barRight, barBot, 2f, 2f, barBg)
            canvas.drawRoundRect(barLeft, barTop, barLeft + barW * pctFill, barBot, 2f, 2f, barFg)
        }
        y += lineH

        // GPU time (moved up near load metrics)
        val gpuText =
            if (gpuTimerAvailable != 0) {
                when {
                    gpuTimeUs >= 10000 -> "GPU: ${gpuTimeUs / 1000}ms"
                    gpuTimeUs >= 1000 -> "GPU: ${gpuTimeUs / 1000}.${(gpuTimeUs % 1000) / 100}ms"
                    gpuTimeUs >= 100 -> "GPU: 0.${"%02d".format(gpuTimeUs / 10)}ms"
                    gpuTimeUs >= 10 -> "GPU: 0.0${gpuTimeUs / 10}ms"
                    else -> "GPU: <0.01ms"
                }
            } else {
                "GPU: n/a"
            }
        canvas.drawText(gpuText, panelLeft + pad, y, valuePaint)
        y += lineH

        // Level cache time (color-coded)
        val cachePaint =
            when {
                cacheTimeMs <= 500 -> valuePaint
                cacheTimeMs <= 2000 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("Cache:", panelLeft + pad, y, labelPaint)
        canvas.drawText("${cacheTimeMs}ms", valCol, y, cachePaint)
        y += lineH

        // Texture memory
        val texMb = texMemoryKb / 1024
        canvas.drawText("Tex:", panelLeft + pad, y, labelPaint)
        canvas.drawText("${texMb}MB", valCol, y, valuePaint)
        y += lineH

        // Hi-res textures
        val pct = if (totalLoaded > 0) (hiresCount * 100 / totalLoaded) else 0
        canvas.drawText("Hires:", panelLeft + pad, y, labelPaint)
        canvas.drawText(
            "$hiresCount/$totalLoaded ($pct%)",
            valCol,
            y,
            valuePaint,
        )
        y += lineH

        // Max hires texture resolution
        canvas.drawText("Max:", panelLeft + pad, y, labelPaint)
        val resText = if (hiresCount > 0) "%dx%d".format(maxHiresW, maxHiresH) else "n/a"
        canvas.drawText(resText, valCol, y, valuePaint)
        y += lineH

        // GL texture cap
        canvas.drawText("GL cap:", panelLeft + pad, y, labelPaint)
        canvas.drawText("${glMaxTexSize}px", valCol, y, valuePaint)
        y += lineH

        // Render resolution
        canvas.drawText("Render:", panelLeft + pad, y, labelPaint)
        canvas.drawText(
            "${renderW}x$renderH / ${displayW}x$displayH",
            valCol,
            y,
            valuePaint,
        )
        y += lineH

        // Texture binds per frame
        val bindTotal = texBinds + texBindReuse
        val hitPct = if (bindTotal > 0) (texBindReuse * 100 / bindTotal) else 0
        canvas.drawText("Binds:", panelLeft + pad, y, labelPaint)
        canvas.drawText("$texBinds ($hitPct% cache)", valCol, y, valuePaint)
        y += lineH

        // Draw polygons + shader/mask stats
        canvas.drawText("Polys:", panelLeft + pad, y, labelPaint)
        canvas.drawText(
            "$drawPolys  shd:$shaderSwitches  mask:$maskDraws",
            valCol,
            y,
            valuePaint,
        )
        y += lineH

        // Color depth
        val cdLabel = if (colorDepth >= 24) "RGB888" else "RGB565"
        canvas.drawText("Color:", panelLeft + pad, y, labelPaint)
        canvas.drawText(cdLabel, valCol, y, valuePaint)
        y += lineH

        // Texture filtering cycle button
        val tfLabel =
            when (texFiltLevel) {
                0 -> "OFF"
                1 -> "Bilinear"
                else -> "Trilinear"
            }
        val tfText = "TexFilt: $tfLabel"
        val tfPaint = if (texFiltLevel > 0) fpsGoodPaint else fpsWarnPaint
        texFiltRect.set(
            panelLeft + pad * 0.5f,
            y - baseTextSize,
            panelLeft + panelW - pad * 0.5f,
            y + lineH * 0.3f,
        )
        val tfBg = if (texFiltPressed) btnPressedPaint else btnNormalPaint
        canvas.drawRoundRect(texFiltRect, pad * 0.5f, pad * 0.5f, tfBg)
        canvas.drawText(tfText, panelLeft + pad, y, tfPaint)
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

        // metl154 shader debug cycle button
        val metl154Text =
            when (metl154Mode) {
                1 -> "metl154: Alpha"
                2 -> "metl154: RGB"
                else -> "metl154: OFF"
            }
        val metl154Paint = if (metl154Mode == 0) fpsWarnPaint else fpsGoodPaint
        metl154Rect.set(
            panelLeft + pad * 0.5f,
            y - baseTextSize,
            panelLeft + panelW - pad * 0.5f,
            y + lineH * 0.3f,
        )
        val metl154Bg = if (metl154Pressed) btnPressedPaint else btnNormalPaint
        canvas.drawRoundRect(metl154Rect, pad * 0.5f, pad * 0.5f, metl154Bg)
        canvas.drawText(metl154Text, panelLeft + pad, y, metl154Paint)
        y += lineH

        // Keep labels in sync with METL154_EXPERIMENT_* in debug_tex_overlay.h.
        val metl154ExperimentText =
            when (metl154ExperimentMode) {
                1 -> "m154 exp: NoMips"
                2 -> "m154 exp: RGBA"
                3 -> "m154 exp: RGBA1"
                4 -> "m154 exp: Stock"
                else -> "m154 exp: Default"
            }
        val metl154ExperimentPaint = if (metl154ExperimentMode == 0) fpsWarnPaint else fpsGoodPaint
        metl154ExperimentRect.set(
            panelLeft + pad * 0.5f,
            y - baseTextSize,
            panelLeft + panelW - pad * 0.5f,
            y + lineH * 0.3f,
        )
        val metl154ExperimentBg = if (metl154ExperimentPressed) btnPressedPaint else btnNormalPaint
        canvas.drawRoundRect(metl154ExperimentRect, pad * 0.5f, pad * 0.5f, metl154ExperimentBg)
        canvas.drawText(metl154ExperimentText, panelLeft + pad, y, metl154ExperimentPaint)
        y += lineH

        // Labels toggle button
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
        val inTexFilt = texFiltRect.contains(event.x, event.y)
        val inMetl154 = metl154Rect.contains(event.x, event.y)
        val inMetl154Experiment = metl154ExperimentRect.contains(event.x, event.y)

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
                if (inTexFilt) {
                    texFiltPressed = true
                    invalidate()
                    return true
                }
                if (inMetl154) {
                    metl154Pressed = true
                    invalidate()
                    return true
                }
                if (inMetl154Experiment) {
                    metl154ExperimentPressed = true
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
                if (texFiltPressed && inTexFilt) {
                    cycleTexFilt()
                    performClick()
                }
                if (metl154Pressed && inMetl154) {
                    cycleMetl154Mode()
                    performClick()
                }
                if (metl154ExperimentPressed && inMetl154Experiment) {
                    cycleMetl154Experiment()
                    performClick()
                }
                if (buttonPressed ||
                    anisoPressed ||
                    msaaPressed ||
                    texFiltPressed ||
                    metl154Pressed ||
                    metl154ExperimentPressed ||
                    inPanel
                ) {
                    buttonPressed = false
                    anisoPressed = false
                    msaaPressed = false
                    texFiltPressed = false
                    metl154Pressed = false
                    metl154ExperimentPressed = false
                    invalidate()
                    return true
                }
            }
            MotionEvent.ACTION_CANCEL -> {
                buttonPressed = false
                anisoPressed = false
                msaaPressed = false
                texFiltPressed = false
                metl154Pressed = false
                metl154ExperimentPressed = false
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
        graphicsOptionSetter?.invoke("aniso_level", next)
    }

    private fun cycleMsaa() {
        // Cycle: 0 -> 2 -> 4 -> 0, capped by msaaMax
        val levels = intArrayOf(0, 2, 4).filter { it <= msaaMax || it == 0 }
        val idx = levels.indexOf(msaaLevel)
        val next = levels[(idx + 1) % levels.size]
        msaaLevel = next
        graphicsOptionSetter?.invoke("msaa_level", next)
    }

    private fun cycleTexFilt() {
        // Cycle: 0 (nearest) -> 1 (bilinear) -> 2 (trilinear) -> 0
        val next = (texFiltLevel + 1) % 3
        texFiltLevel = next
        graphicsOptionSetter?.invoke("tex_filt", next)
    }

    private fun cycleMetl154Mode() {
        val next = (metl154Mode + 1) % 3
        metl154Mode = next
        debugFlagSetter?.invoke("metl154_mode", next)
    }

    private fun cycleMetl154Experiment() {
        val next = (metl154ExperimentMode + 1) % 5
        metl154ExperimentMode = next
        debugFlagSetter?.invoke("metl154_experiment", next)
    }

    companion object {
        private const val POLL_INTERVAL_MS = 500L
    }
}
