package com.dxxredux.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

/**
 * Semi-transparent full-screen overlay showing the track list.
 * Tap a track to play it; tap the X or outside the panel to dismiss.
 *
 * @param onPlayTrack called with the combined track index when a track is tapped
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

    init {
        loadTracks()
    }

    private fun loadTracks() {
        tracks.clear()
        try {
            val activity = context as? MainActivity ?: return
            val total = activity.nativeGetNumAudioTracks()
            currentTrack = activity.nativeGetCurrentTrackNum()
            for (i in 0 until total) {
                val name = activity.nativeGetTrackName(i)
                tracks.add(TrackEntry(i, if (name.isNotEmpty()) name else "Track ${i + 1}"))
            }
        } catch (_: Exception) {
            // engine not ready
        }
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

            // Highlight current track row
            if (track.index == currentTrack) {
                canvas.drawRect(panelRect.left + 4f, y, panelRect.right - 4f, y + rowHeight, highlightPaint)
            }

            val paint = if (track.index == currentTrack) currentPaint else textPaint
            val textY = y + rowHeight * 0.65f
            val label = "${track.index + 1}. ${track.name}"
            canvas.drawText(label, panelRect.left + 20f, textY, paint)
        }
        canvas.restore()
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
