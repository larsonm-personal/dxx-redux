package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import org.json.JSONArray
import kotlin.math.min

/**
 * Semi-transparent full-screen overlay showing the track list.
 * Tap a track to play it; tap the X or outside the panel to dismiss.
 *
 * @param onPlayTrack called with the 1-based combined track number when a track is tapped
 * @param onDismiss called when the panel should be removed
 */
class MusicControlPanel(
    context: Context,
    private val onPlayTrack: (Int) -> Unit,
    private val onDismiss: () -> Unit,
) : View(context) {
    data class TrackEntry(
        val index: Int,
        val name: String,
    )

    private val tracks = mutableListOf<TrackEntry>()
    private var currentTrack = -1
    private var scrollOffset = 0f
    private var panelRect = RectF()
    private var rowHeight = 0f
    private var closeRect = RectF()
    private var titleHeight = 0f
    private var selectedTrackIndex = -1

    // Touch tracking
    private var touchStartY = 0f
    private var touchStartScroll = 0f
    private var dragging = false

    private val bgPaint = Paint().apply { color = 0xCC000000.toInt() }
    private val panelPaint = Paint().apply { color = 0xE6222222.toInt() }
    private val textPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFCCCCCC.toInt()
            textAlign = Paint.Align.LEFT
        }
    private val currentPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFF44FF44.toInt()
            textAlign = Paint.Align.LEFT
        }
    private val titlePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFFFFFFF.toInt()
            textAlign = Paint.Align.CENTER
        }
    private val closePaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFFF4444.toInt()
            textAlign = Paint.Align.CENTER
        }
    private val highlightPaint = Paint().apply { color = 0x22FFFFFF }
    private val selectedPaint = Paint().apply { color = 0x33FFFFFF }
    private val scrollHintPaint =
        Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xAAFFFFFF.toInt()
            textAlign = Paint.Align.CENTER
        }

    init {
        loadTracks()
    }

    private fun loadTracks() {
        tracks.clear()
        try {
            val activity = context as? MainActivity ?: return
            val json = activity.nativeGetTrackList()
            val info = activity.nativeGetCurrentTrackInfo()
            // Current track index from "musicType|trackIndex|totalTracks|trackName"
            if (info.isNotEmpty()) {
                val parts = info.split("|", limit = 4)
                if (parts.size >= 2) currentTrack = parts[1].toIntOrNull() ?: -1
            }
            val arr = JSONArray(json)
            for (i in 0 until arr.length()) {
                val obj = arr.getJSONObject(i)
                val idx = obj.getInt("index")
                val name = obj.optString("name", "")
                val label = if (name.isNotEmpty()) "Track ${idx + 1}: $name" else "Track ${idx + 1}"
                tracks.add(TrackEntry(idx, label))
            }
            syncSelectedTrackIndex()
        } catch (_: Exception) {
            // engine not ready or no tracks
        }
    }

    private fun syncSelectedTrackIndex() {
        selectedTrackIndex =
            when {
                tracks.isEmpty() -> {
                    -1
                }

                else -> {
                    tracks.indexOfFirst { it.index == currentTrack }.takeIf { it >= 0 } ?: 0
                }
            }
    }

    private fun listViewportHeight(): Float = (panelRect.height() - titleHeight - 8f).coerceAtLeast(0f)

    private fun ensureSelectedTrackVisible() {
        scrollOffset =
            scrollOffsetToKeepRowVisible(
                currentOffset = scrollOffset,
                selectedIndex = selectedTrackIndex,
                rowHeight = rowHeight,
                viewportHeight = listViewportHeight(),
            ).coerceIn(0f, maxScroll())
    }

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldw: Int,
        oldh: Int,
    ) {
        super.onSizeChanged(w, h, oldw, oldh)
        val base = min(w, h).toFloat()
        val margin = base * 0.08f
        panelRect.set(margin, margin, w - margin, h - margin)
        textPaint.textSize = base * 0.035f
        currentPaint.textSize = base * 0.035f
        titlePaint.textSize = base * 0.05f
        closePaint.textSize = base * 0.045f
        rowHeight = base * 0.06f
        titleHeight = base * 0.08f
        val closeSize = base * 0.06f
        closeRect.set(
            panelRect.right - closeSize - base * 0.02f,
            panelRect.top + base * 0.01f,
            panelRect.right - base * 0.02f,
            panelRect.top + base * 0.01f + closeSize,
        )
    }

    override fun onDraw(canvas: Canvas) {
        // Dim background
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), bgPaint)

        // Panel background
        canvas.drawRoundRect(panelRect, 16f, 16f, panelPaint)

        // Title
        canvas.drawText(
            "Music Tracks",
            panelRect.centerX(),
            panelRect.top + titleHeight * 0.7f,
            titlePaint,
        )

        // Close button
        canvas.drawText(
            "\u2715",
            closeRect.centerX(),
            closeRect.centerY() + closePaint.textSize * 0.35f,
            closePaint,
        )

        // Track list (clipped to panel)
        canvas.save()
        val listTop = panelRect.top + titleHeight
        val listBottom = panelRect.bottom - 8f
        canvas.clipRect(panelRect.left, listTop, panelRect.right, listBottom)

        for ((i, track) in tracks.withIndex()) {
            val y = listTop + i * rowHeight - scrollOffset
            if (y + rowHeight < listTop || y > listBottom) continue

            if (i == selectedTrackIndex) {
                canvas.drawRect(panelRect.left + 4f, y, panelRect.right - 4f, y + rowHeight, selectedPaint)
            }

            // Highlight current track row
            if (track.index == currentTrack) {
                canvas.drawRect(panelRect.left + 4f, y, panelRect.right - 4f, y + rowHeight, highlightPaint)
            }

            val paint = if (track.index == currentTrack) currentPaint else textPaint
            val textY = y + rowHeight * 0.65f
            canvas.drawText(track.name, panelRect.left + 20f, textY, paint)
        }
        canvas.restore()

        // Scroll indicators
        scrollHintPaint.textSize = titlePaint.textSize * 0.7f
        val max = maxScroll()
        if (scrollOffset > 0f) {
            canvas.drawText(
                "\u25B2",
                panelRect.centerX(),
                listTop + scrollHintPaint.textSize,
                scrollHintPaint,
            )
        }
        if (max > 0f && scrollOffset < max - 1f) {
            canvas.drawText(
                "\u25BC",
                panelRect.centerX(),
                listBottom - 4f,
                scrollHintPaint,
            )
        }
    }

    fun handleControllerKey(
        keyCode: Int,
        action: Int,
    ): Boolean {
        val handledKey =
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
        if (!handledKey) return false
        if (action != 0) return true

        if (tracks.isNotEmpty() && selectedTrackIndex !in tracks.indices) {
            syncSelectedTrackIndex()
        }

        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_B,
            KeyEvent.KEYCODE_BUTTON_Y,
            KeyEvent.KEYCODE_BACK,
            KeyEvent.KEYCODE_ESCAPE,
            -> {
                onDismiss()
            }

            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_RIGHT,
            -> {
                if (tracks.isNotEmpty()) {
                    val nextIndex = moveLinearSelection(selectedTrackIndex, tracks.size, keyCode)
                    if (nextIndex != selectedTrackIndex) {
                        selectedTrackIndex = nextIndex
                        ensureSelectedTrackVisible()
                        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                    }
                    invalidate()
                }
            }

            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_DPAD_CENTER,
            -> {
                if (selectedTrackIndex in tracks.indices) {
                    currentTrack = tracks[selectedTrackIndex].index
                    onPlayTrack(currentTrack)
                    ensureSelectedTrackVisible()
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                    invalidate()
                }
            }
        }

        return true
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                touchStartY = event.y
                touchStartScroll = scrollOffset
                dragging = false
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                val dy = event.y - touchStartY
                if (!dragging && kotlin.math.abs(dy) > 10f) dragging = true
                if (dragging) {
                    scrollOffset = (touchStartScroll - dy).coerceIn(0f, maxScroll())
                    invalidate()
                }
                return true
            }

            MotionEvent.ACTION_UP -> {
                if (dragging) return true
                val px = event.x
                val py = event.y

                // Close button
                if (closeRect.contains(px, py)) {
                    onDismiss()
                    return true
                }

                // Outside panel
                if (!panelRect.contains(px, py)) {
                    onDismiss()
                    return true
                }

                // Track tap
                val listTop = panelRect.top + titleHeight
                if (py >= listTop && py <= panelRect.bottom) {
                    val idx = ((py - listTop + scrollOffset) / rowHeight).toInt()
                    if (idx in tracks.indices) {
                        selectedTrackIndex = idx
                        currentTrack = tracks[idx].index
                        onPlayTrack(currentTrack)
                        invalidate()
                    }
                }
                return true
            }
        }
        return true
    }

    private fun maxScroll(): Float {
        val contentHeight = tracks.size * rowHeight
        val viewHeight = panelRect.height() - titleHeight
        return (contentHeight - viewHeight).coerceAtLeast(0f)
    }
}
