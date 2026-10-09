package com.dxxredux.app

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.RectF
import android.os.SystemClock
import android.util.Log
import android.view.KeyEvent
import android.view.MotionEvent
import org.json.JSONObject

/** Real panel gestures with the asynchronous native command queue replaced by a recording callback */
internal fun checkMusicVolumeTouches(context: Context) {
    checkMusicSourceTouches(context)
    checkMusicControllerNavigation(context)
    checkMusicPauseRefresh(context)
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

private fun checkMusicPauseRefresh(context: Context) {
    val queued = mutableListOf<Boolean>()
    var acceptCommands = true
    var refreshes = 0
    val panel =
        MusicControlPanel(
            context,
            onDismiss = { error("Pause dismissed the panel") },
            onStateChanged = { refreshes++ },
            onPauseChange = {
                if (acceptCommands) queued.add(it)
                acceptCommands
            },
        )

    fun flag(name: String): Boolean {
        val state =
            MusicControlPanel::class.java
                .getDeclaredField("state")
                .apply { isAccessible = true }
                .get(panel)!!
        return state.javaClass
            .getDeclaredField(name)
            .apply { isAccessible = true }
            .getBoolean(state)
    }

    fun refresh(
        paused: Boolean,
        oneTrack: Boolean,
    ) {
        // Match the JNI snapshot's numeric flags, including refreshes after a queued command
        val snapshot = JSONObject().put("paused", if (paused) 1 else 0).put("oneTrackPerLevel", if (oneTrack) 1 else 0)
        Log.i("DXX-MusicPauseTest", "Snapshot=$snapshot boolean_decoder=${snapshot.optBoolean("paused", false)}")
        panel.refreshState(snapshot.toString())
        check(flag("paused") == paused) { "Refresh changed the Pause/Play state" }
        check(flag("oneTrackPerLevel") == oneTrack) { "Refresh cleared One track per level" }
    }

    fun activatePause() {
        check(panel.handleControllerKey(KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.ACTION_DOWN))
        check(panel.handleControllerKey(KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.ACTION_UP))
    }

    refresh(false, true)
    repeat(3) {
        activatePause()
        check(queued.last() && flag("paused")) { "Pause did not immediately switch to Play" }
        repeat(3) { refresh(true, true) }
        activatePause()
        check(!queued.last() && !flag("paused")) { "Play queued another pause instead of resuming" }
        repeat(3) { refresh(false, false) }
    }
    check(queued == listOf(true, false, true, false, true, false))
    check(refreshes == queued.size)

    acceptCommands = false
    activatePause()
    check(!flag("paused") && refreshes == queued.size) { "Rejected pause changed the displayed state" }
    refresh(true, true)
    activatePause()
    check(flag("paused") && refreshes == queued.size) { "Rejected resume changed the displayed state" }
    Log.i("DXX-MusicPauseTest", "PASS: repeated pause, refresh, resume, numeric checkbox state and command rejection")
}

private fun checkMusicControllerNavigation(context: Context) {
    for ((width, height) in listOf(1000 to 600, 600 to 1000)) {
        val queued = mutableListOf<Int>()
        var dismissed = false
        val panel =
            MusicControlPanel(context, { dismissed = true }, {}) {
                queued.add(it)
                it
            }

        fun field(name: String) = MusicControlPanel::class.java.getDeclaredField(name).apply { isAccessible = true }

        fun key(code: Int) {
            check(panel.handleControllerKey(code, KeyEvent.ACTION_DOWN))
            check(panel.handleControllerKey(code, KeyEvent.ACTION_UP))
        }
        panel.layout(0, 0, width, height)
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        try {
            fun expectFocus(
                name: String,
                track: Int = -1,
            ) {
                panel.draw(Canvas(bitmap))
                val focused =
                    MusicControlPanel::class.java
                        .getDeclaredMethod("focusedRect")
                        .apply { isAccessible = true }
                        .invoke(panel)
                val expected =
                    if (track <
                        0
                    ) {
                        field(name).get(panel)
                    } else {
                        (field("trackRects").get(panel) as List<*>)[track]
                    }
                check(focused == expected) { "Expected $name track=$track, got $focused instead of $expected" }
            }
            val up = KeyEvent.KEYCODE_DPAD_UP
            val down = KeyEvent.KEYCODE_DPAD_DOWN
            val left = KeyEvent.KEYCODE_DPAD_LEFT
            val right = KeyEvent.KEYCODE_DPAD_RIGHT
            val activate = KeyEvent.KEYCODE_DPAD_CENTER

            // Empty lists leave focus on the header instead of entering a nonexistent row
            key(down)
            expectFocus("playRect")
            key(right)
            expectFocus("oneTrackRect")
            key(down)
            expectFocus("oneTrackRect")
            key(up)
            expectFocus("sourceRect")
            key(left)
            expectFocus("playRect")
            key(up)
            expectFocus("sourceRect")
            key(right)
            expectFocus("closeRect")
            key(right)
            expectFocus("volumeRect")
            check(queued.isEmpty()) { "Horizontal navigation changed the volume" }
            key(down)
            key(up)
            key(up)
            check(queued == listOf(7, 8)) { "Vertical volume adjustment or upper clamp failed: $queued" }
            repeat(10) { key(down) }
            check(queued == listOf(7, 8, 7, 6, 5, 4, 3, 2, 1, 0)) { "Volume key-up or lower clamp failed: $queued" }
            expectFocus("volumeRect")
            key(left)
            expectFocus("closeRect")
            key(down)
            expectFocus("oneTrackRect")
            key(right)
            expectFocus("volumeRect")
            key(right)
            expectFocus("oneTrackRect")
            key(left)
            expectFocus("playRect")
            key(left)
            expectFocus("volumeRect")
            key(right)
            expectFocus("playRect")

            val state = field("state").get(panel)!!
            state.javaClass
                .getDeclaredField("tracks")
                .apply { isAccessible = true }
                .set(state, List(30) { MusicControlPanel.TrackEntry(it, "Track $it") })
            key(down)
            expectFocus("trackRects", 0)
            key(up)
            expectFocus("oneTrackRect")
            key(down)
            repeat(29) { key(down) }
            expectFocus("trackRects", 29)
            check(field("scrollOffset").getFloat(panel) > 0f) { "Track navigation did not scroll the list" }
            key(down)
            expectFocus("trackRects", 29)
            for (exit in listOf(left, right)) {
                key(exit)
                expectFocus("volumeRect")
                key(if (exit == left) right else left)
                expectFocus("trackRects", 29)
            }
            key(up)
            expectFocus("trackRects", 28)
            repeat(29) { key(up) }
            expectFocus("oneTrackRect")
            check(queued.size == 10) { "Leaving volume changed its setting" }

            key(up)
            field(
                "sourceOptionsCache",
            ).set(panel, listOf(MusicOverlaySourceOption("cd", "CD"), MusicOverlaySourceOption("midi", "MIDI")))
            key(activate)
            check(field("sourceDropdownOpen").getBoolean(panel))
            key(down)
            check(field("sourceDropdownIndex").getInt(panel) == 1)
            key(up)
            check(field("sourceDropdownIndex").getInt(panel) == 0)
            key(right)
            check(!field("sourceDropdownOpen").getBoolean(panel))
            expectFocus("closeRect")
            key(left)
            expectFocus("sourceRect")
            key(activate)
            key(left)
            check(!field("sourceDropdownOpen").getBoolean(panel))
            expectFocus("playRect")
            key(up)
            key(down)
            expectFocus("oneTrackRect")
            key(up)
            key(right)
            key(activate)
            check(dismissed) { "Close could not be activated with the controller" }
            Log.i(
                "DXX-MusicControllerTest",
                "PASS: ${width}x$height spatial focus, volume, track scrolling, dropdown and Close",
            )
        } finally {
            bitmap.recycle()
        }
    }
}

private fun checkMusicSourceTouches(context: Context) {
    for ((width, height) in listOf(1000 to 600, 600 to 1000)) {
        var dismissed = false
        val panel =
            MusicControlPanel(context, { dismissed = true }, {}) {
                error("Open source dropdown passed a tap to the volume control")
            }

        fun field(name: String) = MusicControlPanel::class.java.getDeclaredField(name).apply { isAccessible = true }
        field("sourceOptionsCache").set(
            panel,
            listOf(MusicOverlaySourceOption("cd", "CD"), MusicOverlaySourceOption("midi", "Base game MIDI")),
        )
        panel.layout(0, 0, width, height)
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        try {
            panel.draw(Canvas(bitmap))
            val source = field("sourceRect").get(panel) as RectF
            val underlying = field("oneTrackRect").get(panel) as RectF

            @Suppress("UNCHECKED_CAST")
            val options = field("sourceOptionRects").get(panel) as List<RectF>

            fun tap(rect: RectF) {
                val now = SystemClock.uptimeMillis()
                for (action in listOf(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP)) {
                    val event = MotionEvent.obtain(now, now, action, rect.centerX(), rect.centerY(), 0)
                    try {
                        check(panel.dispatchTouchEvent(event))
                    } finally {
                        event.recycle()
                    }
                }
            }
            for ((index, option) in options.withIndex()) {
                tap(source)
                check(field("sourceDropdownOpen").getBoolean(panel))
                Log.i(
                    "DXX-MusicSourceTest",
                    "${width}x$height option=$index bounds=$option overlaps_one_track=${RectF.intersects(
                        option,
                        underlying,
                    )}",
                )
                tap(option)
                check(!field("sourceDropdownOpen").getBoolean(panel)) {
                    "Source option $index tap was intercepted by an underlying control"
                }
                check(field("sourceDropdownIndex").getInt(panel) == index)
            }
            for (name in listOf("volumeLaneRect", "closeRect", "sourceRect")) {
                tap(source)
                check(field("sourceDropdownOpen").getBoolean(panel))
                tap(field(name).get(panel) as RectF)
                check(!field("sourceDropdownOpen").getBoolean(panel) && !dismissed)
            }
            Log.i("DXX-MusicSourceTest", "PASS: ${width}x$height source selection and outside dismissal")
        } finally {
            bitmap.recycle()
        }
    }
}
