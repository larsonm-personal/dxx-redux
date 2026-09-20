package com.dxxredux.app

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup

internal const val PREF_SHOW_TAP_FEEDBACK = "show_tap_feedback"

/** Android diagnostic: observe window input before any overlay can consume it */
internal class TapFeedbackOverlay(
    private val host: ViewGroup,
) : View(host.context) {
    private data class Tap(
        val x: Float,
        val y: Float,
        val expiresAt: Long,
    )

    private val taps = mutableListOf<Tap>()
    private val screenLocation = IntArray(2)
    private val density = resources.displayMetrics.density
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val expire = Runnable { expireTaps() }

    init {
        importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
        // ViewOverlay draws above the game controls without participating in hit testing
        host.overlay.add(this)
    }

    fun observe(event: MotionEvent) {
        if (event.actionMasked != MotionEvent.ACTION_DOWN &&
            event.actionMasked != MotionEvent.ACTION_POINTER_DOWN
        ) {
            return
        }
        layout(0, 0, host.width, host.height)
        host.getLocationOnScreen(screenLocation)
        val index = event.actionIndex
        // rawX/rawY address pointer zero on older Android versions
        val x = event.rawX + event.getX(index) - event.x - screenLocation[0]
        val y = event.rawY + event.getY(index) - event.y - screenLocation[1]
        if (taps.size >= 32) taps.removeAt(0)
        taps.add(Tap(x, y, SystemClock.uptimeMillis() + 500L))
        expireTaps()
    }

    private fun expireTaps() {
        host.removeCallbacks(expire)
        val now = SystemClock.uptimeMillis()
        taps.removeAll { it.expiresAt <= now }
        invalidate()
        taps.firstOrNull()?.let { host.postDelayed(expire, (it.expiresAt - now).coerceAtLeast(1L)) }
    }

    override fun onDraw(canvas: Canvas) {
        for (tap in taps) {
            paint.style = Paint.Style.FILL
            paint.color = 0x66FF00FF
            canvas.drawCircle(tap.x, tap.y, 24f * density, paint)
            paint.style = Paint.Style.STROKE
            paint.color = Color.BLACK
            paint.strokeWidth = 7f * density
            canvas.drawCircle(tap.x, tap.y, 24f * density, paint)
            paint.color = Color.WHITE
            paint.strokeWidth = 4f * density
            canvas.drawCircle(tap.x, tap.y, 24f * density, paint)
            paint.style = Paint.Style.FILL
            paint.color = Color.MAGENTA
            canvas.drawCircle(tap.x, tap.y, 4f * density, paint)
        }
    }

    fun dispose() {
        host.removeCallbacks(expire)
        taps.clear()
        host.overlay.remove(this)
    }
}
