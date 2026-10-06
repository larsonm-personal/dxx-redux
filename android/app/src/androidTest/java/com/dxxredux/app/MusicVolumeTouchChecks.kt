package com.dxxredux.app

import android.content.Context
import android.graphics.RectF
import android.os.SystemClock
import android.util.Log
import android.view.MotionEvent

/** Real panel gestures with the asynchronous native command queue replaced by a recording callback */
internal fun checkMusicVolumeTouches(context: Context) {
    for ((width, height) in listOf(1000 to 600, 600 to 1000)) {
        val queued = mutableListOf<Int>()
        var refreshes = 0
        var dismissed = false
        var acceptCommands = true
        val panel =
            MusicControlPanel(context, { dismissed = true }, { refreshes++ }) {
                if (acceptCommands) {
                    queued.add(it)
                    it
                } else {
                    -1
                }
            }
        panel.layout(0, 0, width, height)

        fun field(name: String): Any =
            MusicControlPanel::class.java
                .getDeclaredField(name)
                .apply {
                    isAccessible = true
                }.get(panel)!!

        fun displayedVolume(): Int {
            val state = field("state")
            return state.javaClass
                .getDeclaredField("volume")
                .apply { isAccessible = true }
                .getInt(state)
        }
        val lane = field("volumeLaneRect") as RectF
        val slider = field("volumeRect") as RectF
        val top = slider.top + 8f
        val bottom = slider.bottom - 8f

        fun y(volume: Int): Float = bottom - (bottom - top) * volume / 8f

        fun touch(
            action: Int,
            py: Float,
            px: Float = lane.centerX(),
        ) {
            val now = SystemClock.uptimeMillis()
            val event = MotionEvent.obtain(now, now, action, px, py, 0)
            try {
                check(panel.dispatchTouchEvent(event))
            } finally {
                event.recycle()
            }
        }

        touch(MotionEvent.ACTION_DOWN, y(4))
        Log.i("DXX-MusicVolumeTest", "Tap: queued=$queued displayed=${displayedVolume()} refreshes=$refreshes")
        check(displayedVolume() == 4) { "Thumb did not update immediately on touch-down" }
        repeat(100) { touch(MotionEvent.ACTION_MOVE, y(4)) }
        check(queued == listOf(4) && refreshes == 1) { "Stationary finger flooded the native command queue" }
        // A captured drag remains active when the finger strays sideways out of the lane
        for (volume in 3 downTo 0) {
            touch(MotionEvent.ACTION_MOVE, y(volume), lane.left - 30f)
            check(displayedVolume() == volume)
        }
        touch(MotionEvent.ACTION_MOVE, bottom + 100f)
        check(displayedVolume() == 0)
        for (volume in 1..8) {
            touch(MotionEvent.ACTION_MOVE, y(volume))
            check(displayedVolume() == volume)
        }
        touch(MotionEvent.ACTION_UP, top - 100f)
        check(queued == listOf(4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8))
        check(refreshes == queued.size && !dismissed)

        // Release coordinates count even without a final MOVE
        touch(MotionEvent.ACTION_DOWN, y(6), lane.left + 1f)
        touch(MotionEvent.ACTION_UP, y(2), lane.left + 1f)
        check(displayedVolume() == 2)
        touch(MotionEvent.ACTION_DOWN, y(5))
        touch(MotionEvent.ACTION_CANCEL, y(0))
        check(displayedVolume() == 5) { "Cancellation changed the chosen volume" }
        touch(MotionEvent.ACTION_DOWN, y(7))
        touch(MotionEvent.ACTION_UP, y(7))
        check(displayedVolume() == 7)

        acceptCommands = false
        touch(MotionEvent.ACTION_DOWN, y(1))
        touch(MotionEvent.ACTION_UP, y(1))
        check(displayedVolume() == 7) { "Rejected native command changed the displayed volume" }
        check(refreshes == queued.size)
        Log.i("DXX-MusicVolumeTest", "PASS: ${width}x$height tap, drag, clamping, release, cancel and queue rejection")
    }
}
