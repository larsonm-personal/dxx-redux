package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.Log
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.widget.Toast
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

class MusicControlPanel(
    context: Context,
    private val onDismiss: () -> Unit,
    private val onStateChanged: () -> Unit,
) : View(context) {
    data class TrackEntry(
        val index: Int,
        val name: String,
    )

    private data class MusicState(
        val source: String = "cd",
        val oneTrackPerLevel: Boolean = false,
        val volume: Int = 8,
        val paused: Boolean = false,
        val currentTrack: Int = -1,
        val tracks: List<TrackEntry> = emptyList(),
    )

    private val activity: MainActivity? get() = context as? MainActivity
    private var state = MusicState()
    private var sourceOptionsCache = listOf(MusicOverlaySourceOption("midi", "Base game MIDI"))
    private var scrollOffset = 0f
    private var selectedIndex = 0
    private var panelRect = RectF()
    private var closeRect = RectF()
    private var playRect = RectF()
    private var oneTrackRect = RectF()
    private var sourceRect = RectF()
    private val sourceOptionRects = mutableListOf<RectF>()
    private var volumeLaneRect = RectF()
    private var volumeRect = RectF()
    private var trackListRect = RectF()
    private val trackRects = mutableListOf<RectF>()
    private var rowHeight = 0f
    private var headerHeight = 0f
    private var volumeTouchActive = false
    private var controlTouchActive = false
    private var sourceDropdownOpen = false
    private var sourceDropdownIndex = 0
    private var sourceTouchActive = false

    private var touchStartY = 0f
    private var touchStartScroll = 0f
    private var dragging = false

    private val bgPaint = Paint().apply { color = 0xCC000000.toInt() }
    private val panelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xE6222222.toInt() }
    private val cellPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x55333333 }
    private val sourceCellPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF303030.toInt() }
    private val sourceDropdownPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF202020.toInt() }
    private val sourceActivePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF315C3D.toInt() }
    private val activePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x6644AA66 }
    private val selectedPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            color = 0xFF66FF66.toInt()
            strokeWidth = 3f
        }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFE0E0E0.toInt()
            textAlign = Paint.Align.CENTER
        }
    private val smallTextPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFCCCCCC.toInt()
            textAlign = Paint.Align.CENTER
        }
    private val currentTextPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFF77FF77.toInt()
            textAlign = Paint.Align.LEFT
        }
    private val rowTextPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFCCCCCC.toInt()
            textAlign = Paint.Align.LEFT
        }
    private val rowHighlightPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x22FFFFFF }
    private val volumeOutlinePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            color = 0xAAFFFFFF.toInt()
            strokeWidth = 2f
        }
    private val volumeThumbPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xDDFFFFFF.toInt() }
    private val checkboxPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            color = 0xCCFFFFFF.toInt()
            strokeWidth = 2f
        }
    private val scrollPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x88FFFFFF.toInt() }
    private val scrollIconPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xCCFFFFFF.toInt() }

    init {
        isFocusable = true
        refreshSourceOptions()
        refreshState()
    }

    override fun onDetachedFromWindow() {
        removeCallbacks(sourceRefreshRunnable)
        super.onDetachedFromWindow()
    }

    private fun refreshState() {
        val a = activity ?: return
        state =
            runCatching {
                val obj = JSONObject(a.nativeGetMusicOverlayState())
                val tracksJson = obj.optJSONArray("tracks") ?: JSONArray()
                val tracks =
                    buildList {
                        for (i in 0 until tracksJson.length()) {
                            val track = tracksJson.getJSONObject(i)
                            val idx = track.getInt("index")
                            val name = track.optString("name", "")
                            add(TrackEntry(idx, if (name.isBlank()) "Track ${idx + 1}" else name))
                        }
                    }
                MusicState(
                    source = obj.optString("source", "cd"),
                    oneTrackPerLevel = obj.optBoolean("oneTrackPerLevel", false),
                    volume = obj.optInt("volume", 8).coerceIn(0, 8),
                    paused = obj.optBoolean("paused", false),
                    currentTrack = obj.optInt("currentTrack", -1),
                    tracks = tracks,
                )
            }.getOrElse {
                MusicState()
            }
        if (selectedIndex >= focusCount()) selectedIndex = (focusCount() - 1).coerceAtLeast(0)
        refreshSourceOptions()
        invalidate()
    }

    private fun refreshSourceOptions() {
        val a = activity ?: return
        sourceOptionsCache =
            musicOverlaySourceOptions(a.filesDir, a.gameVariantForMusicOverlay()) { uri, useFileDescriptor ->
                canAccessSafUri(a, android.net.Uri.parse(uri), useFileDescriptor)
            }
        sourceDropdownIndex = sourceDropdownIndex.coerceIn(0, sourceOptionsCache.lastIndex)
    }

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldw: Int,
        oldh: Int,
    ) {
        super.onSizeChanged(w, h, oldw, oldh)
        val base = min(w, h).toFloat()
        val margin = base * 0.06f
        panelRect.set(margin, margin, w - margin, h - margin)
        headerHeight = base * 0.2f
        rowHeight = (base * 0.058f).coerceAtLeast(34f)
        textPaint.textSize = base * 0.04f
        smallTextPaint.textSize = base * 0.028f
        rowTextPaint.textSize = base * 0.032f
        currentTextPaint.textSize = rowTextPaint.textSize
        layoutControls()
    }

    private fun layoutControls() {
        sourceOptionRects.clear()
        trackRects.clear()
        val pad = panelRect.width() * 0.025f
        val top = panelRect.top + pad
        val cellH = headerHeight * 0.26f
        val gap = pad * 0.55f
        val left = panelRect.left + pad
        val volumeLaneW = max(panelRect.width() * 0.12f, rowHeight * 1.9f)
        volumeLaneRect.set(panelRect.right - pad - volumeLaneW, top, panelRect.right - pad, panelRect.bottom - pad)
        val usableRight = volumeLaneRect.left - gap
        val titleWidth = panelRect.width() * 0.2f
        val topRowH = cellH * 1.12f
        val closeW = max(panelRect.width() * 0.14f, topRowH * 2.2f)
        closeRect.set(usableRight - closeW, top, usableRight, top + topRowH)
        sourceRect.set(left + titleWidth, top, closeRect.left - gap, top + topRowH)
        playRect.set(left, sourceRect.bottom + gap, left + panelRect.width() * 0.18f, sourceRect.bottom + gap + cellH)
        oneTrackRect.set(playRect.right + gap, playRect.top, usableRight, playRect.bottom)
        val optionH = sourceRect.height()
        for (i in sourceOptions().indices) {
            val y = sourceRect.bottom + i * optionH
            sourceOptionRects.add(RectF(sourceRect.left, y, sourceRect.right, y + optionH))
        }
        volumeRect.set(
            volumeLaneRect.left + volumeLaneRect.width() * 0.22f,
            volumeLaneRect.top + headerHeight * 0.28f,
            volumeLaneRect.right - volumeLaneRect.width() * 0.22f,
            volumeLaneRect.bottom - rowHeight * 0.35f,
        )
        trackListRect.set(
            panelRect.left + pad,
            panelRect.top + headerHeight,
            volumeLaneRect.left - pad,
            panelRect.bottom - pad,
        )
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), bgPaint)
        canvas.drawRoundRect(panelRect, 8f, 8f, panelPaint)
        layoutControls()
        drawHeader(canvas)
        drawTracks(canvas)
        drawVolume(canvas)
        drawSourceDropdown(canvas)
        drawFocus(canvas)
    }

    private fun drawHeader(canvas: Canvas) {
        textPaint.textSize = headerHeight * 0.18f
        textPaint.textAlign = Paint.Align.LEFT
        canvas.drawText(
            "Music",
            panelRect.left + panelRect.width() * 0.025f,
            sourceRect.centerY() + textPaint.textSize * 0.35f,
            textPaint,
        )
        textPaint.textAlign = Paint.Align.CENTER
        drawCell(canvas, closeRect, "Close", false)
        drawCell(canvas, playRect, if (state.paused) "Play" else "Pause", false)
        drawOneTrackCell(canvas)
        drawSourceCell(canvas)
    }

    private fun drawOneTrackCell(canvas: Canvas) {
        canvas.drawRect(oneTrackRect, if (state.oneTrackPerLevel) activePaint else cellPaint)
        val s = oneTrackRect.height() * 0.36f
        val box =
            RectF(
                oneTrackRect.left + s * 0.8f,
                oneTrackRect.centerY() - s / 2f,
                oneTrackRect.left + s * 1.8f,
                oneTrackRect.centerY() + s / 2f,
            )
        canvas.drawRect(box, checkboxPaint)
        if (state.oneTrackPerLevel) {
            canvas.drawLine(box.left + s * 0.2f, box.centerY(), box.centerX(), box.bottom - s * 0.15f, checkboxPaint)
            canvas.drawLine(
                box.centerX(),
                box.bottom - s * 0.15f,
                box.right - s * 0.12f,
                box.top + s * 0.15f,
                checkboxPaint,
            )
        }
        smallTextPaint.textSize = oneTrackRect.height() * 0.38f
        smallTextPaint.textAlign = Paint.Align.LEFT
        canvas.drawText(
            trimToWidth("One track per level", smallTextPaint, oneTrackRect.right - box.right - s * 1.1f),
            box.right + s * 0.5f,
            oneTrackRect.centerY() + smallTextPaint.textSize * 0.35f,
            smallTextPaint,
        )
    }

    private fun drawCell(
        canvas: Canvas,
        rect: RectF,
        label: String,
        active: Boolean,
    ) {
        canvas.drawRect(rect, if (active) activePaint else cellPaint)
        smallTextPaint.textSize = rect.height() * 0.42f
        smallTextPaint.textAlign = Paint.Align.CENTER
        canvas.drawText(label, rect.centerX(), rect.centerY() + smallTextPaint.textSize * 0.35f, smallTextPaint)
    }

    private fun drawSourceCell(canvas: Canvas) {
        canvas.drawRect(sourceRect, sourceCellPaint)
        smallTextPaint.textSize = sourceRect.height() * 0.42f
        smallTextPaint.textAlign = Paint.Align.LEFT
        val chevronWidth = smallTextPaint.measureText(" v")
        val currentLabel = sourceOptions().getOrNull(sourceIndex())?.label ?: "Base game MIDI"
        val label = trimToWidth("Source: $currentLabel", smallTextPaint, sourceRect.width() - chevronWidth - 20f)
        canvas.drawText(
            label,
            sourceRect.left + 10f,
            sourceRect.centerY() + smallTextPaint.textSize * 0.35f,
            smallTextPaint,
        )
        smallTextPaint.textAlign = Paint.Align.RIGHT
        canvas.drawText(
            if (sourceDropdownOpen) "^" else "v",
            sourceRect.right - 10f,
            sourceRect.centerY() + smallTextPaint.textSize * 0.35f,
            smallTextPaint,
        )
    }

    private fun drawSourceDropdown(canvas: Canvas) {
        if (!sourceDropdownOpen) return
        sourceOptions().forEachIndexed { i, option ->
            val rect = sourceOptionRects[i]
            val active = state.source == option.id
            canvas.drawRect(rect, if (active) sourceActivePaint else sourceDropdownPaint)
            smallTextPaint.textSize = rect.height() * 0.42f
            smallTextPaint.textAlign = Paint.Align.LEFT
            canvas.drawText(
                option.label,
                rect.left + 12f,
                rect.centerY() + smallTextPaint.textSize * 0.35f,
                smallTextPaint,
            )
            if (i == sourceDropdownIndex) {
                canvas.drawRect(rect, selectedPaint)
            }
        }
    }

    private fun drawTracks(canvas: Canvas) {
        trackRects.clear()
        canvas.save()
        canvas.clipRect(trackListRect)
        state.tracks.forEachIndexed { i, track ->
            val y = trackListRect.top + i * rowHeight - scrollOffset
            val rect = RectF(trackListRect.left, y, trackListRect.right, y + rowHeight)
            trackRects.add(rect)
            if (rect.bottom < trackListRect.top || rect.top > trackListRect.bottom) return@forEachIndexed
            if (track.index == state.currentTrack) {
                canvas.drawRect(rect, rowHighlightPaint)
            }
            val paint = if (track.index == state.currentTrack) currentTextPaint else rowTextPaint
            val label = "${i + 1}. ${track.name}"
            canvas.drawText(
                trimToWidth(label, paint, rect.width() - 20f),
                rect.left + 10f,
                rect.centerY() + paint.textSize * 0.35f,
                paint,
            )
        }
        canvas.restore()

        val max = maxScroll()
        if (max > 0f) {
            val barW = 4f
            val thumbH =
                (trackListRect.height() * trackListRect.height() / (state.tracks.size * rowHeight))
                    .coerceAtLeast(
                        24f,
                    )
            val thumbTop = trackListRect.top + (trackListRect.height() - thumbH) * (scrollOffset / max)
            canvas.drawRect(
                trackListRect.right - barW,
                trackListRect.top,
                trackListRect.right,
                trackListRect.bottom,
                cellPaint,
            )
            canvas.drawRect(trackListRect.right - barW, thumbTop, trackListRect.right, thumbTop + thumbH, scrollPaint)
            drawScrollIcon(canvas, true, scrollOffset > 1f)
            drawScrollIcon(canvas, false, scrollOffset < max - 1f)
        }
    }

    private fun drawVolume(canvas: Canvas) {
        canvas.drawRect(volumeLaneRect, cellPaint)
        canvas.drawLine(
            volumeLaneRect.left,
            volumeLaneRect.top,
            volumeLaneRect.left,
            volumeLaneRect.bottom,
            volumeOutlinePaint,
        )
        smallTextPaint.textSize = volumeRect.width() * 0.34f
        smallTextPaint.textAlign = Paint.Align.CENTER
        canvas.drawText(
            "Vol",
            volumeRect.centerX(),
            volumeLaneRect.top + smallTextPaint.textSize * 1.15f,
            smallTextPaint,
        )
        canvas.drawRoundRect(volumeRect, 5f, 5f, volumeOutlinePaint)
        val track =
            RectF(
                volumeRect.centerX() - 5f,
                volumeRect.top + 8f,
                volumeRect.centerX() + 5f,
                volumeRect.bottom - 8f,
            )
        canvas.drawRoundRect(track, 5f, 5f, cellPaint)
        val fraction = state.volume / 8f
        val thumbY = track.bottom - track.height() * fraction
        val thumb = RectF(track.left - 18f, thumbY - 12f, track.right + 18f, thumbY + 12f)
        canvas.drawRoundRect(thumb, 5f, 5f, volumeThumbPaint)
        canvas.drawRoundRect(thumb, 5f, 5f, volumeOutlinePaint)
        canvas.drawText(
            state.volume.toString(),
            volumeRect.centerX(),
            volumeRect.bottom + smallTextPaint.textSize * 1.1f,
            smallTextPaint,
        )
    }

    private fun drawFocus(canvas: Canvas) {
        focusedRect()?.let { canvas.drawRect(it, selectedPaint) }
    }

    private fun focusedRect(): RectF? =
        when {
            selectedIndex == 0 -> {
                playRect
            }

            selectedIndex == 1 -> {
                oneTrackRect
            }

            selectedIndex == sourceFocusIndex() -> {
                sourceRect
            }

            selectedIndex == volumeFocusIndex() -> {
                volumeRect
            }

            selectedIndex >= firstTrackFocusIndex() -> {
                val trackIndex = selectedIndex - firstTrackFocusIndex()
                trackRects.getOrNull(trackIndex)
            }

            else -> {
                null
            }
        }

    fun handleControllerKey(
        keyCode: Int,
        action: Int,
    ): Boolean {
        val handled =
            keyCode == KeyEvent.KEYCODE_DPAD_UP ||
                keyCode == KeyEvent.KEYCODE_DPAD_DOWN ||
                keyCode == KeyEvent.KEYCODE_DPAD_LEFT ||
                keyCode == KeyEvent.KEYCODE_DPAD_RIGHT ||
                keyCode == KeyEvent.KEYCODE_BUTTON_A ||
                keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
                keyCode == KeyEvent.KEYCODE_BUTTON_B ||
                keyCode == KeyEvent.KEYCODE_BUTTON_Y ||
                keyCode == KeyEvent.KEYCODE_BACK ||
                keyCode == KeyEvent.KEYCODE_ESCAPE
        if (!handled) return false
        if (action != 0) return true

        if (sourceDropdownOpen) {
            return handleSourceDropdownKey(keyCode)
        }

        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_B,
            KeyEvent.KEYCODE_BUTTON_Y,
            KeyEvent.KEYCODE_BACK,
            KeyEvent.KEYCODE_ESCAPE,
            -> {
                onDismiss()
            }

            KeyEvent.KEYCODE_DPAD_LEFT,
            -> {
                if (selectedIndex == volumeFocusIndex()) {
                    setVolume(state.volume - 1)
                } else {
                    moveFocus(-1)
                }
            }

            KeyEvent.KEYCODE_DPAD_RIGHT,
            -> {
                if (selectedIndex == volumeFocusIndex()) {
                    setVolume(state.volume + 1)
                } else {
                    moveFocus(1)
                }
            }

            KeyEvent.KEYCODE_DPAD_UP,
            -> {
                moveFocus(-1)
            }

            KeyEvent.KEYCODE_DPAD_DOWN,
            -> {
                moveFocus(1)
            }

            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_DPAD_CENTER,
            -> {
                activateSelection()
            }
        }
        invalidate()
        return true
    }

    private fun moveFocus(delta: Int) {
        selectedIndex = (selectedIndex + delta).coerceIn(0, focusCount() - 1)
        ensureFocusedTrackVisible()
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
    }

    private fun activateSelection() {
        when {
            selectedIndex == 0 -> {
                setPaused(!state.paused)
            }

            selectedIndex == 1 -> {
                setOneTrackPerLevel(!state.oneTrackPerLevel)
            }

            selectedIndex == sourceFocusIndex() -> {
                openSourceDropdown()
            }

            selectedIndex == volumeFocusIndex() -> {
                setVolume(state.volume + 1)
            }

            selectedIndex >= firstTrackFocusIndex() -> {
                val trackIndex = selectedIndex - firstTrackFocusIndex()
                state.tracks.getOrNull(trackIndex)?.let { playTrack(it.index) }
            }
        }
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val px = event.x
        val py = event.y
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (sourceDropdownOpen && sourceDropdownHitRect().contains(px, py)) {
                    sourceTouchActive = true
                    return true
                }
                if (volumeHitRect().contains(px, py)) {
                    volumeTouchActive = true
                    setVolumeFromTouch(py)
                    return true
                }
                if (closeRect.contains(px, py) ||
                    playRect.contains(px, py) ||
                    oneTrackRect.contains(px, py) ||
                    sourceRect.contains(px, py)
                ) {
                    controlTouchActive = true
                    return true
                }
                touchStartY = py
                touchStartScroll = scrollOffset
                dragging = false
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                if (sourceTouchActive) return true
                if (controlTouchActive) return true
                if (volumeTouchActive) {
                    setVolumeFromTouch(py)
                    return true
                }
                val dy = py - touchStartY
                if (!dragging && abs(dy) > 10f) dragging = true
                if (dragging) {
                    scrollOffset = (touchStartScroll - dy).coerceIn(0f, maxScroll())
                    invalidate()
                }
                return true
            }

            MotionEvent.ACTION_UP -> {
                if (volumeTouchActive) {
                    setVolumeFromTouch(py)
                    volumeTouchActive = false
                    return true
                }
                if (sourceTouchActive) {
                    sourceTouchActive = false
                    handleTap(px, py)
                    return true
                }
                if (controlTouchActive) {
                    controlTouchActive = false
                    handleTap(px, py)
                    return true
                }
                if (dragging) return true
                handleTap(px, py)
                return true
            }

            MotionEvent.ACTION_CANCEL -> {
                volumeTouchActive = false
                controlTouchActive = false
                sourceTouchActive = false
                return true
            }
        }
        return true
    }

    private fun handleTap(
        px: Float,
        py: Float,
    ) {
        when {
            closeRect.contains(px, py) || !panelRect.contains(px, py) -> {
                onDismiss()
            }

            playRect.contains(px, py) -> {
                setPaused(!state.paused)
            }

            oneTrackRect.contains(px, py) -> {
                setOneTrackPerLevel(!state.oneTrackPerLevel)
            }

            sourceRect.contains(px, py) -> {
                if (sourceDropdownOpen) closeSourceDropdown() else openSourceDropdown()
            }

            else -> {
                if (sourceDropdownOpen && sourceDropdownHitRect().contains(px, py)) {
                    sourceOptionRects.forEachIndexed { i, rect ->
                        if (rect.contains(px, py)) {
                            sourceDropdownIndex = i
                            chooseSourceDropdownOption()
                            return
                        }
                    }
                    closeSourceDropdown()
                    return
                }
                if (sourceDropdownOpen) {
                    closeSourceDropdown()
                    return
                }
                if (trackListRect.contains(px, py)) {
                    val idx = ((py - trackListRect.top + scrollOffset) / rowHeight).toInt()
                    state.tracks.getOrNull(idx)?.let {
                        selectedIndex = firstTrackFocusIndex() + idx
                        playTrack(it.index)
                    }
                }
            }
        }
    }

    private fun openSourceDropdown() {
        sourceDropdownIndex = sourceIndex()
        sourceDropdownOpen = true
        invalidate()
    }

    private fun closeSourceDropdown() {
        sourceDropdownOpen = false
        invalidate()
    }

    private fun chooseSourceDropdownOption() {
        val source = sourceOptions().getOrNull(sourceDropdownIndex)?.id ?: return
        sourceDropdownOpen = false
        if (source != state.source) {
            setSource(source)
        } else {
            invalidate()
        }
    }

    private fun handleSourceDropdownKey(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_UP,
            -> {
                sourceDropdownIndex = (sourceDropdownIndex - 1).coerceAtLeast(0)
            }

            KeyEvent.KEYCODE_DPAD_DOWN,
            -> {
                sourceDropdownIndex = (sourceDropdownIndex + 1).coerceAtMost(sourceOptions().lastIndex)
            }

            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_DPAD_CENTER,
            -> {
                chooseSourceDropdownOption()
            }

            KeyEvent.KEYCODE_DPAD_LEFT,
            -> {
                closeSourceDropdown()
                moveFocus(-1)
            }

            KeyEvent.KEYCODE_DPAD_RIGHT,
            -> {
                closeSourceDropdown()
                moveFocus(1)
            }

            KeyEvent.KEYCODE_BUTTON_B,
            KeyEvent.KEYCODE_BUTTON_Y,
            KeyEvent.KEYCODE_BACK,
            KeyEvent.KEYCODE_ESCAPE,
            -> {
                closeSourceDropdown()
            }
        }
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        invalidate()
        return true
    }

    private fun setSource(source: String) {
        val a = activity ?: return
        if (source == "files") {
            CustomAudioSetManager(a.filesDir).writeM3U(a)
        }
        if (source == "cd") {
            try {
                AudioSourceManager(a.filesDir).writePlaylist(a.contentResolver)
            } catch (e: Exception) {
                Log.e("DXX-MusicPanel", "Could not select CD audio", e)
                Toast.makeText(a, "Could not select CD audio: ${e.message}", Toast.LENGTH_LONG).show()
                refreshSourceOptions()
                invalidate()
                return
            }
        }
        state = state.copy(source = source)
        invalidate()
        val prefs = a.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        val edit = prefs.edit()
        when (source) {
            "mission" -> {
                edit.putBoolean(PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE, true)
            }

            "files" -> {
                edit.putBoolean(PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE, false)
                edit.putString("music_mode", "files")
            }

            "cd" -> {
                edit.putBoolean(PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE, false)
                edit.putString("music_mode", "cd")
            }

            "midi" -> {
                edit.putBoolean(PREF_USE_MISSION_SOUNDTRACK_WHEN_AVAILABLE, false)
                edit.putString("music_mode", "midi")
            }
        }
        edit.apply()
        if (a.nativeSetMusicSource(source)) {
            onStateChanged()
            scheduleSourceRefresh()
        }
    }

    private fun setOneTrackPerLevel(enabled: Boolean) {
        activity?.nativeSetMusicOneTrackPerLevel(enabled)
        afterNativeChange()
    }

    private fun setVolume(volume: Int) {
        activity?.nativeSetMusicVolume(volume.coerceIn(0, 8))
        afterNativeChange()
    }

    private fun setVolumeFromTouch(py: Float) {
        val fraction = ((volumeRect.bottom - py) / volumeRect.height()).coerceIn(0f, 1f)
        setVolume((fraction * 8f).roundToInt())
    }

    private fun setPaused(paused: Boolean) {
        if (activity?.nativeSetMusicPaused(paused) == true) {
            state = state.copy(paused = paused)
            invalidate()
            onStateChanged()
        }
    }

    private fun playTrack(track: Int) {
        activity?.nativePlaySpecificTrack(track)
        afterNativeChange()
    }

    private fun afterNativeChange() {
        refreshState()
        onStateChanged()
    }

    private val sourceRefreshRunnable =
        object : Runnable {
            private var remainingRefreshes = 0

            fun start() {
                remainingRefreshes = 40
                this@MusicControlPanel.removeCallbacks(this)
                this@MusicControlPanel.postDelayed(this, 120L)
            }

            override fun run() {
                if (activity?.nativeIsMusicSourceChangePending() == true) {
                    remainingRefreshes--
                    if (remainingRefreshes > 0) {
                        this@MusicControlPanel.postDelayed(this, 120L)
                    } else {
                        Log.w("DXX-MusicPanel", "Music source change still pending after refresh window")
                        refreshState()
                        onStateChanged()
                    }
                    return
                }
                refreshState()
                onStateChanged()
            }
        }

    private fun scheduleSourceRefresh() {
        sourceRefreshRunnable.start()
    }

    private fun ensureFocusedTrackVisible() {
        if (selectedIndex < firstTrackFocusIndex()) return
        val trackIndex = selectedIndex - firstTrackFocusIndex()
        scrollOffset =
            scrollOffsetToKeepRowVisible(
                currentOffset = scrollOffset,
                selectedIndex = trackIndex,
                rowHeight = rowHeight,
                viewportHeight = trackListRect.height(),
            ).coerceIn(0f, maxScroll())
    }

    private fun focusCount(): Int = firstTrackFocusIndex() + state.tracks.size

    private fun sourceFocusIndex(): Int = 2

    private fun volumeFocusIndex(): Int = 3

    private fun firstTrackFocusIndex(): Int = volumeFocusIndex() + 1

    private fun maxScroll(): Float = (state.tracks.size * rowHeight - trackListRect.height()).coerceAtLeast(0f)

    private fun sourceIndex(): Int = sourceOptions().indexOfFirst { it.id == state.source }.coerceAtLeast(0)

    private fun sourceOptions(): List<MusicOverlaySourceOption> = sourceOptionsCache

    private fun volumeHitRect(): RectF = RectF(volumeLaneRect)

    private fun sourceDropdownHitRect(): RectF {
        val bounds = RectF(sourceRect)
        sourceOptionRects.forEach { bounds.union(it) }
        return bounds
    }

    private fun trimToWidth(
        text: String,
        paint: Paint,
        maxWidth: Float,
    ): String {
        if (paint.measureText(text) <= maxWidth) return text
        var end = text.length
        while (end > 1 && paint.measureText(text.substring(0, end) + "...") > maxWidth) end--
        return text.substring(0, end) + "..."
    }

    private fun drawScrollIcon(
        canvas: Canvas,
        up: Boolean,
        enabled: Boolean,
    ) {
        val size = rowHeight * 0.22f
        val cx = trackListRect.right - size * 1.8f
        val cy = if (up) trackListRect.top + size * 1.4f else trackListRect.bottom - size * 1.4f
        val path = Path()
        if (up) {
            path.moveTo(cx, cy - size)
            path.lineTo(cx - size, cy + size * 0.6f)
            path.lineTo(cx + size, cy + size * 0.6f)
        } else {
            path.moveTo(cx, cy + size)
            path.lineTo(cx - size, cy - size * 0.6f)
            path.lineTo(cx + size, cy - size * 0.6f)
        }
        path.close()
        scrollIconPaint.alpha = if (enabled) 210 else 70
        canvas.drawPath(path, scrollIconPaint)
    }
}
