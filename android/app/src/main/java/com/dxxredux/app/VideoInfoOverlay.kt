package com.dxxredux.app

import android.content.Context
import android.content.SharedPreferences
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View

internal enum class VideoInfoControllerAction {
    TEX_FILT,
    ANISO,
    MSAA,
    MERGED_WALL,
    MERGED_WALL_EXPERIMENT,
    MERGED_WALL_TAP,
    LABELS,
}

internal fun videoInfoControllerActions(showDebugControls: Boolean): List<VideoInfoControllerAction> =
    buildList {
        add(VideoInfoControllerAction.TEX_FILT)
        add(VideoInfoControllerAction.ANISO)
        add(VideoInfoControllerAction.MSAA)
        if (showDebugControls) {
            add(VideoInfoControllerAction.MERGED_WALL)
            add(VideoInfoControllerAction.MERGED_WALL_EXPERIMENT)
            add(VideoInfoControllerAction.MERGED_WALL_TAP)
            add(VideoInfoControllerAction.LABELS)
        }
    }

internal data class VideoInfoOverlayLayout(
    val outerPad: Float,
    val panelPad: Float,
    val panelHeight: Float,
    val panelCornerRadius: Float,
    val buttonCornerRadius: Float,
    val focusStrokeWidth: Float,
    val infoTextSize: Float,
    val titleTextSize: Float,
    val buttonTextSize: Float,
    val infoLineHeight: Float,
    val buttonLineHeight: Float,
)

internal fun computeVideoInfoOverlayLayout(
    height: Int,
    density: Float,
    baseTextSize: Float,
    actionRows: Int,
): VideoInfoOverlayLayout {
    val safeHeight = height.coerceAtLeast(1).toFloat()
    val safeDensity = density.coerceAtLeast(0.1f)
    val safeTextSize = baseTextSize.coerceAtLeast(1f)
    val safeActionRows = actionRows.coerceAtLeast(0)
    val basePad = VIDEO_INFO_PAD_DP * safeDensity
    val baseLineHeight = safeTextSize * VIDEO_INFO_LINE_HEIGHT_SCALE

    fun screenHeightFor(
        infoScale: Float,
        buttonScale: Float,
        padScale: Float,
    ): Float =
        basePad * 4f * padScale +
            VIDEO_INFO_INFO_ROW_COUNT * baseLineHeight * infoScale +
            safeActionRows * baseLineHeight * buttonScale

    val fullButtonAndPadHeight = screenHeightFor(0f, 1f, 1f)
    val infoScaleForFit =
        (
            (safeHeight - fullButtonAndPadHeight) /
                (VIDEO_INFO_INFO_ROW_COUNT * baseLineHeight)
        ).coerceAtMost(1f)

    var infoScale: Float
    var buttonScale = 1f
    var padScale = 1f
    if (infoScaleForFit >= VIDEO_INFO_SOFT_INFO_SCALE) {
        infoScale = infoScaleForFit
    } else {
        infoScale = VIDEO_INFO_SOFT_INFO_SCALE
        val buttonHeight = safeActionRows * baseLineHeight
        val softInfoAndFullPadHeight = screenHeightFor(infoScale, 0f, 1f)
        val buttonScaleForFit =
            if (buttonHeight > 0f) {
                ((safeHeight - softInfoAndFullPadHeight) / buttonHeight).coerceAtMost(1f)
            } else {
                1f
            }

        if (buttonScaleForFit >= VIDEO_INFO_SOFT_BUTTON_SCALE) {
            buttonScale = buttonScaleForFit
        } else {
            val softHeight = screenHeightFor(VIDEO_INFO_SOFT_INFO_SCALE, VIDEO_INFO_SOFT_BUTTON_SCALE, 1f)
            val emergencyScale = (safeHeight / softHeight).coerceAtMost(1f)
            infoScale = VIDEO_INFO_SOFT_INFO_SCALE * emergencyScale
            buttonScale = VIDEO_INFO_SOFT_BUTTON_SCALE * emergencyScale
            padScale = emergencyScale
        }
    }

    infoScale = infoScale.coerceAtLeast(0f)
    buttonScale = buttonScale.coerceAtLeast(0f)
    padScale = padScale.coerceAtLeast(0f)

    val panelPad = basePad * padScale
    val outerPad = basePad * padScale
    val infoTextSize = safeTextSize * infoScale
    val buttonTextSize = safeTextSize * buttonScale
    val infoLineHeight = baseLineHeight * infoScale
    val buttonLineHeight = baseLineHeight * buttonScale
    return VideoInfoOverlayLayout(
        outerPad = outerPad,
        panelPad = panelPad,
        panelHeight =
            panelPad * 2f +
                VIDEO_INFO_INFO_ROW_COUNT * infoLineHeight +
                safeActionRows * buttonLineHeight,
        panelCornerRadius = panelPad,
        buttonCornerRadius = panelPad * 0.5f,
        focusStrokeWidth = (3f * padScale).coerceAtLeast(1f),
        infoTextSize = infoTextSize,
        titleTextSize = infoTextSize * VIDEO_INFO_TITLE_TEXT_SCALE,
        buttonTextSize = buttonTextSize,
        infoLineHeight = infoLineHeight,
        buttonLineHeight = buttonLineHeight,
    )
}

private const val VIDEO_INFO_INFO_ROW_COUNT = 15
private const val VIDEO_INFO_PAD_DP = 8f
private const val VIDEO_INFO_LINE_HEIGHT_SCALE = 1.5f
private const val VIDEO_INFO_TITLE_TEXT_SCALE = 1.1f
private const val VIDEO_INFO_SOFT_INFO_SCALE = 0.6f
private const val VIDEO_INFO_SOFT_BUTTON_SCALE = 0.8f

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
    private var swapTimeUs = 0
    private var resolveTimeUs = 0
    private var glErrorTimeUs = 0
    private var cacheKtxMs = 0
    private var cachePngMs = 0
    private var cacheUploadMs = 0
    private var cacheMaskMs = 0

    // Labels toggle button state and hit region
    private var labelsOn = false
    private var mergedWallMode = 0
    private var mergedWallExperimentMode = 0
    private var showDebugControls = false
    private var buttonPressed = false
    private var anisoPressed = false
    private var msaaPressed = false
    private var texFiltPressed = false
    private var mergedWallPressed = false
    private var mergedWallExperimentPressed = false
    private var mergedWallTapPressed = false
    private var mergedWallTapFlashUntilMs = 0L
    private var selectedControllerAction: VideoInfoControllerAction? = null
    private val buttonRect = RectF()
    private val anisoRect = RectF()
    private val msaaRect = RectF()
    private val texFiltRect = RectF()
    private val mergedWallRect = RectF()
    private val mergedWallExperimentRect = RectF()
    private val mergedWallTapRect = RectF()
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
                        mergedWallMode = stats[30]
                        mergedWallExperimentMode = stats[31]
                    }
                    if (stats != null && stats.size >= 39) {
                        swapTimeUs = stats[32]
                        resolveTimeUs = stats[33]
                        glErrorTimeUs = stats[34]
                        cacheKtxMs = stats[35]
                        cachePngMs = stats[36]
                        cacheUploadMs = stats[37]
                        cacheMaskMs = stats[38]
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
        selectedControllerAction = activeControllerActions().firstOrNull()
        if (!polling) {
            polling = true
            handler.post(pollRunnable)
        }
    }

    fun hide() {
        visibility = GONE
        selectedControllerAction = null
        polling = false
        handler.removeCallbacks(pollRunnable)
    }

    fun toggle() {
        if (visibility == VISIBLE) hide() else show()
    }

    fun applyLauncherPrefs(prefs: SharedPreferences) {
        showDebugControls = prefs.getBoolean(PREF_SHOW_VIDEO_INFO_DEBUG_OPTIONS, false)
        ensureControllerSelection()
        invalidate()
    }

    private fun formatMillisTenths(us: Int): String {
        if (us <= 0) return "0.0"
        return "${us / 1000}.${(us % 1000) / 100}"
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
    private val btnFocusedPaint =
        Paint().apply {
            color = 0x55FFFFFF
            style = Paint.Style.FILL
        }
    private val btnFocusOutlinePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFF00E676u.toInt()
            style = Paint.Style.STROKE
            strokeWidth = 3f
        }

    private fun drawFocusOutline(
        canvas: Canvas,
        rect: RectF,
        radius: Float,
        focused: Boolean,
    ) {
        if (focused) {
            canvas.drawRoundRect(rect, radius, radius, btnFocusOutlinePaint)
        }
    }

    private fun applyTextSizes(
        textSize: Float,
        titleTextSize: Float,
    ) {
        titlePaint.textSize = titleTextSize
        labelPaint.textSize = textSize
        valuePaint.textSize = textSize
        fpsGoodPaint.textSize = textSize
        fpsWarnPaint.textSize = textSize
        fpsBadPaint.textSize = textSize
    }

    private fun setButtonBounds(
        rect: RectF,
        panelLeft: Float,
        panelWidth: Float,
        baselineY: Float,
        layout: VideoInfoOverlayLayout,
    ) {
        rect.set(
            panelLeft + layout.panelPad * 0.5f,
            baselineY - layout.buttonTextSize,
            panelLeft + panelWidth - layout.panelPad * 0.5f,
            baselineY + layout.buttonLineHeight * 0.3f,
        )
    }

    private fun activeControllerActions(): List<VideoInfoControllerAction> =
        videoInfoControllerActions(showDebugControls)

    private fun ensureControllerSelection(): VideoInfoControllerAction? {
        val actions = activeControllerActions()
        val currentSelection = selectedControllerAction
        val resolved = if (currentSelection in actions) currentSelection else actions.firstOrNull()
        selectedControllerAction = resolved
        return resolved
    }

    private fun activateControllerAction(action: VideoInfoControllerAction) {
        when (action) {
            VideoInfoControllerAction.TEX_FILT -> {
                cycleTexFilt()
            }

            VideoInfoControllerAction.ANISO -> {
                cycleAnisotropy()
            }

            VideoInfoControllerAction.MSAA -> {
                cycleMsaa()
            }

            VideoInfoControllerAction.MERGED_WALL -> {
                cycleMergedWallMode()
            }

            VideoInfoControllerAction.MERGED_WALL_EXPERIMENT -> {
                cycleMergedWallExperiment()
            }

            VideoInfoControllerAction.MERGED_WALL_TAP -> {
                triggerMergedWallTap()
            }

            VideoInfoControllerAction.LABELS -> {
                labelsOn = !labelsOn
                debugFlagSetter?.invoke("tex_overlay", if (labelsOn) 1 else 0)
            }
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (visibility != VISIBLE) return
        if (width <= 0 || height <= 0) return
        ensureControllerSelection()

        val density = resources.displayMetrics.density
        val viewWidth = width.toFloat()
        val baseTextSize = (11f * density).coerceAtMost(viewWidth * 0.014f)
        val layout = computeVideoInfoOverlayLayout(height, density, baseTextSize, activeControllerActions().size)
        applyTextSizes(layout.infoTextSize, layout.titleTextSize)
        btnFocusOutlinePaint.strokeWidth = layout.focusStrokeWidth

        val panelWidth = maxOf(layout.infoTextSize, layout.buttonTextSize) * 20f
        val panelLeft = viewWidth - panelWidth - layout.outerPad
        val panelTop = layout.outerPad

        panelBounds.set(panelLeft, panelTop, panelLeft + panelWidth, panelTop + layout.panelHeight)
        canvas.drawRoundRect(panelBounds, layout.panelCornerRadius, layout.panelCornerRadius, bgPaint)

        val valueColumn = panelLeft + layout.panelPad + layout.infoTextSize * 5f
        var baselineY = panelTop + layout.panelPad + titlePaint.textSize

        // Title + FPS on same line
        canvas.drawText("VIDEO", panelLeft + layout.panelPad, baselineY, titlePaint)
        val fpsPaint =
            when {
                fps >= 23 -> fpsGoodPaint
                fps >= 18 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("${fps}fps", valueColumn, baselineY, fpsPaint)
        baselineY += layout.infoLineHeight

        // Frame time avg / max (integer ms)
        val avgMs = frameTimeAvg / 1000
        val maxMs = frameTimeMax / 1000
        val frameTimePaint =
            when {
                frameTimeAvg <= 45000 -> fpsGoodPaint
                frameTimeAvg <= 55000 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("frame", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText("${avgMs}ms avg / ${maxMs}ms max", valueColumn, baselineY, frameTimePaint)
        baselineY += layout.infoLineHeight

        // Frame budget load bar (40ms = 100% at 25fps)
        run {
            val barLeft = panelLeft + layout.panelPad
            val barRight = panelLeft + panelWidth - layout.panelPad
            val barTop = baselineY - layout.infoTextSize * 0.6f
            val barBot = baselineY + layout.infoLineHeight * 0.1f
            val barWidth = barRight - barLeft
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
            canvas.drawRoundRect(barLeft, barTop, barLeft + barWidth * pctFill, barBot, 2f, 2f, barFg)
        }
        baselineY += layout.infoLineHeight

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
        canvas.drawText(gpuText, panelLeft + layout.panelPad, baselineY, valuePaint)
        baselineY += layout.infoLineHeight

        canvas.drawText("Flip:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText(
            "sw ${formatMillisTenths(
                swapTimeUs,
            )} rs ${formatMillisTenths(resolveTimeUs)} er ${formatMillisTenths(glErrorTimeUs)}",
            valueColumn,
            baselineY,
            valuePaint,
        )
        baselineY += layout.infoLineHeight

        // Level cache time (color-coded)
        val cachePaint =
            when {
                cacheTimeMs <= 500 -> valuePaint
                cacheTimeMs <= 2000 -> fpsWarnPaint
                else -> fpsBadPaint
            }
        canvas.drawText("Cache:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText("${cacheTimeMs}ms", valueColumn, baselineY, cachePaint)
        baselineY += layout.infoLineHeight

        canvas.drawText("Load:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText("k$cacheKtxMs p$cachePngMs u$cacheUploadMs m$cacheMaskMs", valueColumn, baselineY, valuePaint)
        baselineY += layout.infoLineHeight

        // Texture memory
        val texMb = texMemoryKb / 1024
        canvas.drawText("Tex:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText("${texMb}MB", valueColumn, baselineY, valuePaint)
        baselineY += layout.infoLineHeight

        // Hi-res textures
        val pct = if (totalLoaded > 0) (hiresCount * 100 / totalLoaded) else 0
        canvas.drawText("Hires:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText(
            "$hiresCount/$totalLoaded ($pct%)",
            valueColumn,
            baselineY,
            valuePaint,
        )
        baselineY += layout.infoLineHeight

        // Max hires texture resolution
        canvas.drawText("Max:", panelLeft + layout.panelPad, baselineY, labelPaint)
        val resText = if (hiresCount > 0) "%dx%d".format(maxHiresW, maxHiresH) else "n/a"
        canvas.drawText(resText, valueColumn, baselineY, valuePaint)
        baselineY += layout.infoLineHeight

        // GL texture cap
        canvas.drawText("GL cap:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText("${glMaxTexSize}px", valueColumn, baselineY, valuePaint)
        baselineY += layout.infoLineHeight

        // Render resolution
        canvas.drawText("Render:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText(
            "${renderW}x$renderH / ${displayW}x$displayH",
            valueColumn,
            baselineY,
            valuePaint,
        )
        baselineY += layout.infoLineHeight

        // Texture binds per frame
        val bindTotal = texBinds + texBindReuse
        val hitPct = if (bindTotal > 0) (texBindReuse * 100 / bindTotal) else 0
        canvas.drawText("Binds:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText("$texBinds ($hitPct% cache)", valueColumn, baselineY, valuePaint)
        baselineY += layout.infoLineHeight

        // Draw polygons + shader/mask stats
        canvas.drawText("Polys:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText(
            "$drawPolys  shd:$shaderSwitches  mask:$maskDraws",
            valueColumn,
            baselineY,
            valuePaint,
        )
        baselineY += layout.infoLineHeight

        // Color depth
        val cdLabel = if (colorDepth >= 24) "RGB888" else "RGB565"
        canvas.drawText("Color:", panelLeft + layout.panelPad, baselineY, labelPaint)
        canvas.drawText(cdLabel, valueColumn, baselineY, valuePaint)
        baselineY += layout.infoLineHeight

        applyTextSizes(layout.buttonTextSize, layout.buttonTextSize)

        // Texture filtering cycle button
        val tfLabel =
            when (texFiltLevel) {
                0 -> "OFF"
                1 -> "Bilinear"
                else -> "Trilinear"
            }
        val tfText = "TexFilt: $tfLabel"
        val tfPaint = if (texFiltLevel > 0) fpsGoodPaint else fpsWarnPaint
        setButtonBounds(texFiltRect, panelLeft, panelWidth, baselineY, layout)
        val tfBg =
            when {
                texFiltPressed -> btnPressedPaint
                selectedControllerAction == VideoInfoControllerAction.TEX_FILT -> btnFocusedPaint
                else -> btnNormalPaint
            }
        canvas.drawRoundRect(texFiltRect, layout.buttonCornerRadius, layout.buttonCornerRadius, tfBg)
        drawFocusOutline(
            canvas,
            texFiltRect,
            layout.buttonCornerRadius,
            selectedControllerAction == VideoInfoControllerAction.TEX_FILT,
        )
        canvas.drawText(tfText, panelLeft + layout.panelPad, baselineY, tfPaint)
        baselineY += layout.buttonLineHeight

        // Anisotropic filtering cycle button
        val anisoText = if (anisoLevel > 0) "AF: ${anisoLevel}x" else "AF: OFF"
        val anisoPaint = if (anisoLevel > 0) fpsGoodPaint else fpsWarnPaint
        setButtonBounds(anisoRect, panelLeft, panelWidth, baselineY, layout)
        val anisoBg =
            when {
                anisoPressed -> btnPressedPaint
                selectedControllerAction == VideoInfoControllerAction.ANISO -> btnFocusedPaint
                else -> btnNormalPaint
            }
        canvas.drawRoundRect(anisoRect, layout.buttonCornerRadius, layout.buttonCornerRadius, anisoBg)
        drawFocusOutline(
            canvas,
            anisoRect,
            layout.buttonCornerRadius,
            selectedControllerAction == VideoInfoControllerAction.ANISO,
        )
        val maxText = if (anisoMax > 0) " (max ${anisoMax}x)" else ""
        canvas.drawText(anisoText + maxText, panelLeft + layout.panelPad, baselineY, anisoPaint)
        baselineY += layout.buttonLineHeight

        // MSAA cycle button
        val msaaText = if (msaaLevel > 0) "MSAA: ${msaaLevel}x" else "MSAA: OFF"
        val msaaPaint = if (msaaLevel > 0) fpsGoodPaint else fpsWarnPaint
        setButtonBounds(msaaRect, panelLeft, panelWidth, baselineY, layout)
        val msaaBg =
            when {
                msaaPressed -> btnPressedPaint
                selectedControllerAction == VideoInfoControllerAction.MSAA -> btnFocusedPaint
                else -> btnNormalPaint
            }
        canvas.drawRoundRect(msaaRect, layout.buttonCornerRadius, layout.buttonCornerRadius, msaaBg)
        drawFocusOutline(
            canvas,
            msaaRect,
            layout.buttonCornerRadius,
            selectedControllerAction == VideoInfoControllerAction.MSAA,
        )
        val msaaMaxText = if (msaaMax > 0) " (max ${msaaMax}x)" else ""
        canvas.drawText(msaaText + msaaMaxText, panelLeft + layout.panelPad, baselineY, msaaPaint)
        baselineY += layout.buttonLineHeight

        if (showDebugControls) {
            // Merged-wall overlay debug cycle button
            val mergedWallText =
                when (mergedWallMode) {
                    1 -> "overlay: Alpha"
                    2 -> "overlay: RGB"
                    else -> "overlay: OFF"
                }
            val mergedWallPaint = if (mergedWallMode == 0) fpsWarnPaint else fpsGoodPaint
            setButtonBounds(mergedWallRect, panelLeft, panelWidth, baselineY, layout)
            val mergedWallBg =
                when {
                    mergedWallPressed -> btnPressedPaint
                    selectedControllerAction == VideoInfoControllerAction.MERGED_WALL -> btnFocusedPaint
                    else -> btnNormalPaint
                }
            canvas.drawRoundRect(mergedWallRect, layout.buttonCornerRadius, layout.buttonCornerRadius, mergedWallBg)
            canvas.drawText(mergedWallText, panelLeft + layout.panelPad, baselineY, mergedWallPaint)
            baselineY += layout.buttonLineHeight

            // Surface the small set of explicit merged-wall experiment modes.
            val mergedWallExperimentText =
                when (mergedWallExperimentMode) {
                    MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE_VALUE -> "mwall exp: Legacy"
                    MERGED_WALL_EXPERIMENT_CLEAR_SECONDARY_UNITS_SINGLE_VALUE -> "mwall exp: Clear TU1/2"
                    0 -> "mwall exp: Default"
                    else -> "mwall exp: Compat $mergedWallExperimentMode"
                }
            val mergedWallExperimentPaint = if (mergedWallExperimentMode == 0) fpsWarnPaint else fpsGoodPaint
            setButtonBounds(mergedWallExperimentRect, panelLeft, panelWidth, baselineY, layout)
            val mergedWallExperimentBg =
                when {
                    mergedWallExperimentPressed -> btnPressedPaint
                    selectedControllerAction == VideoInfoControllerAction.MERGED_WALL_EXPERIMENT -> btnFocusedPaint
                    else -> btnNormalPaint
                }
            canvas.drawRoundRect(
                mergedWallExperimentRect,
                layout.buttonCornerRadius,
                layout.buttonCornerRadius,
                mergedWallExperimentBg,
            )
            canvas.drawText(mergedWallExperimentText, panelLeft + layout.panelPad, baselineY, mergedWallExperimentPaint)
            baselineY += layout.buttonLineHeight

            val tapActive = android.os.SystemClock.uptimeMillis() < mergedWallTapFlashUntilMs
            val mergedWallTapText = if (tapActive) "mwall tap: Sent" else "mwall tap: Tap"
            val mergedWallTapPaint = if (tapActive) fpsGoodPaint else valuePaint
            setButtonBounds(mergedWallTapRect, panelLeft, panelWidth, baselineY, layout)
            val mergedWallTapBg =
                when {
                    mergedWallTapPressed -> btnPressedPaint
                    selectedControllerAction == VideoInfoControllerAction.MERGED_WALL_TAP -> btnFocusedPaint
                    else -> btnNormalPaint
                }
            canvas.drawRoundRect(
                mergedWallTapRect,
                layout.buttonCornerRadius,
                layout.buttonCornerRadius,
                mergedWallTapBg,
            )
            canvas.drawText(mergedWallTapText, panelLeft + layout.panelPad, baselineY, mergedWallTapPaint)
            baselineY += layout.buttonLineHeight

            // Labels toggle button
            val labelsText = if (labelsOn) "Labels: ON" else "Labels: OFF"
            val labelTogglePaint = if (labelsOn) fpsGoodPaint else fpsWarnPaint
            setButtonBounds(buttonRect, panelLeft, panelWidth, baselineY, layout)
            val btnBg =
                when {
                    buttonPressed -> btnPressedPaint
                    selectedControllerAction == VideoInfoControllerAction.LABELS -> btnFocusedPaint
                    else -> btnNormalPaint
                }
            canvas.drawRoundRect(buttonRect, layout.buttonCornerRadius, layout.buttonCornerRadius, btnBg)
            canvas.drawText(labelsText, panelLeft + layout.panelPad, baselineY, labelTogglePaint)
        } else {
            mergedWallRect.setEmpty()
            mergedWallExperimentRect.setEmpty()
            mergedWallTapRect.setEmpty()
            buttonRect.setEmpty()
        }
    }

    @Suppress("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (visibility != VISIBLE) return super.onTouchEvent(event)
        val inPanel = panelBounds.contains(event.x, event.y)
        val inButton = buttonRect.contains(event.x, event.y)
        val inAniso = anisoRect.contains(event.x, event.y)
        val inMsaa = msaaRect.contains(event.x, event.y)
        val inTexFilt = texFiltRect.contains(event.x, event.y)
        val inMergedWall = mergedWallRect.contains(event.x, event.y)
        val inMergedWallExperiment = mergedWallExperimentRect.contains(event.x, event.y)
        val inMergedWallTap = mergedWallTapRect.contains(event.x, event.y)

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                if (inButton) {
                    selectedControllerAction = VideoInfoControllerAction.LABELS
                    buttonPressed = true
                    invalidate()
                    return true
                }
                if (inAniso) {
                    selectedControllerAction = VideoInfoControllerAction.ANISO
                    anisoPressed = true
                    invalidate()
                    return true
                }
                if (inMsaa) {
                    selectedControllerAction = VideoInfoControllerAction.MSAA
                    msaaPressed = true
                    invalidate()
                    return true
                }
                if (inTexFilt) {
                    selectedControllerAction = VideoInfoControllerAction.TEX_FILT
                    texFiltPressed = true
                    invalidate()
                    return true
                }
                if (inMergedWall) {
                    selectedControllerAction = VideoInfoControllerAction.MERGED_WALL
                    mergedWallPressed = true
                    invalidate()
                    return true
                }
                if (inMergedWallExperiment) {
                    selectedControllerAction = VideoInfoControllerAction.MERGED_WALL_EXPERIMENT
                    mergedWallExperimentPressed = true
                    invalidate()
                    return true
                }
                if (inMergedWallTap) {
                    selectedControllerAction = VideoInfoControllerAction.MERGED_WALL_TAP
                    mergedWallTapPressed = true
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
                if (mergedWallPressed && inMergedWall) {
                    cycleMergedWallMode()
                    performClick()
                }
                if (mergedWallExperimentPressed && inMergedWallExperiment) {
                    cycleMergedWallExperiment()
                    performClick()
                }
                if (mergedWallTapPressed && inMergedWallTap) {
                    triggerMergedWallTap()
                    performClick()
                }
                if (buttonPressed ||
                    anisoPressed ||
                    msaaPressed ||
                    texFiltPressed ||
                    mergedWallPressed ||
                    mergedWallExperimentPressed ||
                    mergedWallTapPressed ||
                    inPanel
                ) {
                    buttonPressed = false
                    anisoPressed = false
                    msaaPressed = false
                    texFiltPressed = false
                    mergedWallPressed = false
                    mergedWallExperimentPressed = false
                    mergedWallTapPressed = false
                    invalidate()
                    return true
                }
            }

            MotionEvent.ACTION_CANCEL -> {
                buttonPressed = false
                anisoPressed = false
                msaaPressed = false
                texFiltPressed = false
                mergedWallPressed = false
                mergedWallExperimentPressed = false
                mergedWallTapPressed = false
                invalidate()
            }
        }
        return super.onTouchEvent(event)
    }

    fun handleControllerKey(
        keyCode: Int,
        action: Int,
    ): Boolean {
        if (visibility != VISIBLE) return false

        val handledKey =
            keyCode == KeyEvent.KEYCODE_DPAD_UP ||
                keyCode == KeyEvent.KEYCODE_DPAD_DOWN ||
                keyCode == KeyEvent.KEYCODE_DPAD_LEFT ||
                keyCode == KeyEvent.KEYCODE_DPAD_RIGHT ||
                keyCode == KeyEvent.KEYCODE_BUTTON_A ||
                keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
                keyCode == KeyEvent.KEYCODE_BUTTON_B ||
                keyCode == KeyEvent.KEYCODE_BACK ||
                keyCode == KeyEvent.KEYCODE_ESCAPE
        if (!handledKey) return false
        if (action != 0) return true

        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_B,
            KeyEvent.KEYCODE_BACK,
            KeyEvent.KEYCODE_ESCAPE,
            -> {
                hide()
                return true
            }

            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_RIGHT,
            -> {
                val actions = activeControllerActions()
                if (actions.isNotEmpty()) {
                    val currentAction = ensureControllerSelection()
                    val currentIndex = actions.indexOf(currentAction).let { if (it >= 0) it else 0 }
                    val nextIndex = moveLinearSelection(currentIndex, actions.size, keyCode)
                    if (nextIndex != currentIndex) {
                        selectedControllerAction = actions[nextIndex]
                        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                    }
                    invalidate()
                }
                return true
            }

            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_DPAD_CENTER,
            -> {
                val actionToRun = ensureControllerSelection()
                if (actionToRun != null) {
                    activateControllerAction(actionToRun)
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                    invalidate()
                }
                return true
            }
        }

        return false
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

    private fun cycleMergedWallMode() {
        val next = (mergedWallMode + 1) % 3
        mergedWallMode = next
        debugFlagSetter?.invoke("merged_wall_mode", next)
    }

    private fun cycleMergedWallExperiment() {
        val levels =
            intArrayOf(
                0,
                MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE_VALUE,
                MERGED_WALL_EXPERIMENT_CLEAR_SECONDARY_UNITS_SINGLE_VALUE,
            )
        val idx = levels.indexOf(mergedWallExperimentMode).let { if (it >= 0) it else 0 }
        val next = levels[(idx + 1) % levels.size]
        mergedWallExperimentMode = next
        debugFlagSetter?.invoke("merged_wall_experiment", next)
    }

    private fun triggerMergedWallTap() {
        debugFlagSetter?.invoke("merged_wall_snapshot", MERGED_WALL_REQUEST_CAPTURE_VALUE)
        mergedWallTapFlashUntilMs = android.os.SystemClock.uptimeMillis() + SNAPSHOT_FLASH_MS
        invalidate()
        handler.postDelayed(
            { if (visibility == VISIBLE) invalidate() },
            SNAPSHOT_FLASH_MS,
        )
    }

    companion object {
        private const val POLL_INTERVAL_MS = 500L
        private const val SNAPSHOT_FLASH_MS = 900L
        private const val MERGED_WALL_REQUEST_CAPTURE_VALUE = 2
    }
}
