package com.dxxredux.app

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.PointF
import android.graphics.Rect
import android.os.Build
import android.os.Bundle
import android.text.InputType
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager
import android.widget.FrameLayout
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

class MainActivity : Activity(), SurfaceHolder.Callback {

    companion object {
        init {
            System.loadLibrary("d2x-redux")
        }
        /** Game canvas resolution (must match gr_set_mode in the engine) */
        const val GAME_W = 640
        const val GAME_H = 480
    }

    // ── JNI declarations ────────────────────────────────────
    external fun helloFromNative(): String
    external fun startGame()
    external fun nativeSetSurface(surface: Surface?)
    external fun nativeTouchEvent(action: Int, gameX: Int, gameY: Int)
    external fun nativeKeyEvent(action: Int, androidKeyCode: Int, unicodeChar: Int)
    external fun nativeTextInput(unicodeChar: Int)
    external fun nativeOnPause()
    external fun nativeQuit()
    external fun nativeGetGameState(): String
    external fun nativeRequestIntrospect()
    external fun nativeSetIntrospectPath(path: String)
    external fun nativeJoystickAxis(axis: Int, value: Float)
    external fun nativeJoystickButton(button: Int, pressed: Int)
    external fun nativeIsInGame(): Boolean
    external fun nativeSetJoystickEnabled(enabled: Boolean)
    external fun nativeIsAutomapActive(): Boolean
    external fun nativeAutomapInput(heading: Float, pitch: Float, thrust: Float)

    private var gameStarted = false
    private lateinit var gameSurfaceView: GameSurfaceView
    private lateinit var touchOverlay: TouchOverlayView
    private var overlayEnabled = false
    private val overlayPoller = android.os.Handler(android.os.Looper.getMainLooper())

    // ── Edge-swipe state ────────────────────────────────────────────────
    private var edgeSwipeTracking = false      // left-edge → setup screen
    private var rightEdgeSwipeTracking = false  // right-edge → toggle automap
    private var edgeSwipeStartX = 0f
    private var edgeSwipeStartY = 0f

    // ── Automap gesture state (drag = pan/tilt, pinch = thrust) ─────────
    private val automapPointers = mutableMapOf<Int, PointF>()  // pointerId → last position
    private var automapPinchDist = 0f                          // last distance between two fingers

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Keep screen on while the game is running
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Allow rendering into the display cutout (notch) area
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }

        // Draw behind system bars
        WindowCompat.setDecorFitsSystemWindows(window, false)

        gameSurfaceView = GameSurfaceView(this)
        gameSurfaceView.holder.addCallback(this)
        gameSurfaceView.isFocusable = true
        gameSurfaceView.isFocusableInTouchMode = true
        gameSurfaceView.requestFocus()

        // Handle touch on the SurfaceView so coordinates are view-relative
        gameSurfaceView.setOnTouchListener { view, event ->
            handleTouch(view, event)
        }

        // Touch overlay
        touchOverlay = TouchOverlayView(this)
        touchOverlay.axisCallback = { axis, value -> nativeJoystickAxis(axis, value) }
        touchOverlay.buttonCallback = { button, pressed ->
            nativeJoystickButton(button, if (pressed) 1 else 0)
        }
        touchOverlay.automapInputCallback = { heading, pitch, thrust ->
            nativeAutomapInput(heading, pitch, thrust)
        }
        touchOverlay.isActive = false

        // Layer surface + overlay in a FrameLayout
        val frame = FrameLayout(this)
        frame.addView(gameSurfaceView, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))
        frame.addView(touchOverlay, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        setContentView(frame)

        // Hide system bars after content view is set
        hideSystemBars()

        // Prevent the system back-gesture from consuming left-edge swipes;
        // we use them to open the setup screen.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            gameSurfaceView.addOnLayoutChangeListener { v, _, _, _, _, _, _, _, _ ->
                val edgePx = (20 * resources.displayMetrics.density).toInt()
                v.systemGestureExclusionRects = listOf(Rect(0, 0, edgePx, v.height))
            }
        }
    }

    // ── Immersive fullscreen helper ─────────────────────────
    private fun hideSystemBars() {
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    // ── SurfaceHolder.Callback ──────────────────────────────
    override fun surfaceCreated(holder: SurfaceHolder) {
        nativeSetSurface(holder.surface)

        // Start the engine only once, after the surface is ready
        if (!gameStarted) {
            gameStarted = true

            Thread {
                startGame()
            }.start()
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        nativeSetSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSetSurface(null)
    }

    // ── Lifecycle ────────────────────────────────────────────
    override fun onStop() {
        super.onStop()
        overlayPoller.removeCallbacksAndMessages(null)
        // Inject Escape so the engine opens its pause / game menu.
        // This pauses a single-player game while the app is in the background.
        if (gameStarted) {
            nativeOnPause()
        }
    }

    override fun onResume() {
        super.onResume()
        // Re-read preference (user may have toggled in SetupActivity)
        val prefs = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
        // Default to enabled when no physical controller is connected
        val hasController = InputDevice.getDeviceIds().any { id ->
            val dev = InputDevice.getDevice(id) ?: return@any false
            val src = dev.sources
            src and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            src and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
        }
        overlayEnabled = prefs.getBoolean("touch_overlay_enabled", !hasController)
        // Start polling in-game state to show/hide overlay
        startOverlayPolling()
    }

    private fun startOverlayPolling() {
        overlayPoller.removeCallbacksAndMessages(null)
        val pollRunnable = object : Runnable {
            override fun run() {
                if (gameStarted && overlayEnabled) {
                    try {
                        val inGame = nativeIsInGame()
                        val wasActive = touchOverlay.isActive
                        touchOverlay.isActive = inGame
                        // Tell the overlay whether the automap is showing
                        touchOverlay.automapActive = try { nativeIsAutomapActive() } catch (_: Exception) { false }
                        // Enable/disable joystick input when overlay state changes
                        if (inGame && !wasActive) {
                            nativeSetJoystickEnabled(true)
                        } else if (!inGame && wasActive) {
                            nativeSetJoystickEnabled(false)
                        }
                    } catch (_: Exception) {
                        touchOverlay.isActive = false
                        touchOverlay.automapActive = false
                    }
                } else {
                    if (touchOverlay.isActive) {
                        nativeSetJoystickEnabled(false)
                    }
                    touchOverlay.isActive = false
                    touchOverlay.automapActive = false
                }
                overlayPoller.postDelayed(this, 500)
            }
        }
        overlayPoller.post(pollRunnable)
    }

    // ── Introspection (debug builds only) ────────────────────
    // Trigger a game state dump to a file readable via adb:
    //   adb shell am broadcast -a com.dxxredux.INTROSPECT -n com.dxxredux.app/.MainActivity
    //   adb shell run-as com.dxxredux.app cat files/introspect.json
    private val introspectReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (!gameStarted) return
            nativeRequestIntrospect()
            Log.i("DXX-Introspect", "Introspection requested — will dump on next frame")
        }
    }

    override fun onStart() {
        super.onStart()
        if (BuildConfig.DEBUG) {
            // Set the file path for introspection dumps
            nativeSetIntrospectPath(filesDir.absolutePath + "/introspect.json")

            val filter = IntentFilter("com.dxxredux.INTROSPECT")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(introspectReceiver, filter, RECEIVER_EXPORTED)
            } else {
                @Suppress("UnspecifiedRegisterReceiverFlag")
                registerReceiver(introspectReceiver, filter)
            }
        }
    }

    override fun onDestroy() {
        if (BuildConfig.DEBUG) {
            try { unregisterReceiver(introspectReceiver) } catch (_: Exception) {}
        }
        super.onDestroy()
    }

    // ── Touch → Mouse ───────────────────────────────────────
    private fun handleTouch(view: View, event: MotionEvent): Boolean {
        val density = resources.displayMetrics.density
        val edgeThresholdPx = 20 * density   // 20 dp from left edge
        val swipeMinPx      = 80 * density   // minimum 80 dp horizontal drag

        // ── Edge-swipe detection ────────────────────────────
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (event.x < edgeThresholdPx) {
                    edgeSwipeTracking = true
                    edgeSwipeStartX = event.x
                    edgeSwipeStartY = event.y
                    return true           // consume – don't forward to game
                }
                if (event.x > view.width - edgeThresholdPx) {
                    rightEdgeSwipeTracking = true
                    edgeSwipeStartX = event.x
                    edgeSwipeStartY = event.y
                    return true
                }
                edgeSwipeTracking = false
                rightEdgeSwipeTracking = false
            }
            MotionEvent.ACTION_MOVE -> {
                if (edgeSwipeTracking || rightEdgeSwipeTracking) return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (edgeSwipeTracking) {
                    edgeSwipeTracking = false
                    if (event.actionMasked == MotionEvent.ACTION_UP) {
                        val dx = event.x - edgeSwipeStartX
                        val dy = kotlin.math.abs(event.y - edgeSwipeStartY)
                        if (dx > swipeMinPx && dy < dx) {
                            openSetupScreen()
                        }
                    }
                    return true
                }
                if (rightEdgeSwipeTracking) {
                    rightEdgeSwipeTracking = false
                    if (event.actionMasked == MotionEvent.ACTION_UP) {
                        val dx = edgeSwipeStartX - event.x   // positive = swiped left
                        val dy = kotlin.math.abs(event.y - edgeSwipeStartY)
                        if (dx > swipeMinPx && dy < dx) {
                            toggleAutomap()
                        }
                    }
                    return true
                }
            }
        }

        // ── Automap gesture handling (when overlay is off) ────
        // When the automap is showing, all non-edge touches become
        // pan/tilt (drag) or forward/reverse (pinch).
        if (gameStarted) {
            try {
                if (nativeIsAutomapActive()) {
                    return handleAutomapTouch(event, view.width.toFloat(), view.height.toFloat())
                }
            } catch (_: Exception) { /* engine not ready */ }
        }

        // ── Normal game touch handling ──────────────────────
        // The engine renders 640×480 into ANativeWindow which the compositor
        // stretches to fill the entire SurfaceView.  Map proportionally.
        val viewW = view.width.toFloat()
        val viewH = view.height.toFloat()
        if (viewW <= 0f || viewH <= 0f) return false

        val gameX = (event.x / viewW * GAME_W).toInt().coerceIn(0, GAME_W - 1)
        val gameY = (event.y / viewH * GAME_H).toInt().coerceIn(0, GAME_H - 1)

        val action = when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> 0
            MotionEvent.ACTION_MOVE -> 1
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> 2
            else -> return false
        }

        nativeTouchEvent(action, gameX, gameY)
        return true
    }

    // ── Keyboard & Gamepad buttons ────────────────────────────

    /** Open the setup screen (game pauses automatically via onStop). */
    private fun openSetupScreen() {
        val intent = Intent(this, SetupActivity::class.java)
        intent.putExtra("gameRunning", true)
        startActivity(intent)
    }

    /** Toggle the automap by injecting a TAB key press/release. */
    private fun toggleAutomap() {
        nativeKeyEvent(0, KeyEvent.KEYCODE_TAB, '\t'.code)
        nativeKeyEvent(1, KeyEvent.KEYCODE_TAB, 0)
    }

    // ── Automap touch gestures ──────────────────────────────
    //  1-finger drag  →  pan / tilt  (heading_time, pitch_time)
    //  2-finger pinch →  forward / reverse thrust (uncapped)
    //
    // Called from handleTouch (overlay off) and from the overlay's
    // unmatched-touch callback (overlay on).

    /**
     * Feed a raw MotionEvent into the automap gesture tracker.
     * [screenW] / [screenH] are the view dimensions for normalisation.
     * Returns true if the event was consumed.
     */
    fun handleAutomapTouch(event: MotionEvent, screenW: Float, screenH: Float): Boolean {
        if (screenW <= 0f || screenH <= 0f) return false

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val idx = event.actionIndex
                val pid = event.getPointerId(idx)
                automapPointers[pid] = PointF(event.getX(idx), event.getY(idx))

                // When a second finger lands, initialise pinch distance
                if (automapPointers.size == 2) {
                    automapPinchDist = automapFingerDistance(event)
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (automapPointers.size == 1) {
                    // ── Single-finger drag → pan / tilt ─────────
                    val pid = automapPointers.keys.first()
                    val idx = event.findPointerIndex(pid)
                    if (idx >= 0) {
                        val prev = automapPointers[pid]!!
                        val dx = event.getX(idx) - prev.x
                        val dy = event.getY(idx) - prev.y
                        prev.set(event.getX(idx), event.getY(idx))

                        // Normalise to fraction of screen dimension
                        val heading = dx / screenW
                        val pitch   = dy / screenH
                        if (heading != 0f || pitch != 0f) {
                            nativeAutomapInput(heading, pitch, 0f)
                        }
                    }
                } else if (automapPointers.size >= 2) {
                    // ── Pinch → forward / reverse thrust ────────
                    val dist = automapFingerDistance(event)
                    if (automapPinchDist > 0f) {
                        val delta = dist - automapPinchDist
                        // Normalise to fraction of screen width; positive = expand = forward
                        val thrust = delta / screenW
                        if (thrust != 0f) {
                            nativeAutomapInput(0f, 0f, thrust)
                        }
                    }
                    automapPinchDist = dist

                    // Also update stored positions so a lift→single-finger
                    // transition doesn't jump.
                    for ((pid, pt) in automapPointers) {
                        val idx = event.findPointerIndex(pid)
                        if (idx >= 0) pt.set(event.getX(idx), event.getY(idx))
                    }
                }
            }

            MotionEvent.ACTION_POINTER_UP -> {
                val idx = event.actionIndex
                val pid = event.getPointerId(idx)
                automapPointers.remove(pid)
                // Recalculate pinch distance with remaining fingers
                if (automapPointers.size >= 2) {
                    automapPinchDist = automapFingerDistance(event)
                } else {
                    automapPinchDist = 0f
                    // Update the remaining pointer's position to avoid a jump
                    for ((id, pt) in automapPointers) {
                        val i = event.findPointerIndex(id)
                        if (i >= 0) pt.set(event.getX(i), event.getY(i))
                    }
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                automapPointers.clear()
                automapPinchDist = 0f
            }
        }
        return true
    }

    /** Euclidean distance between the first two tracked automap fingers. */
    private fun automapFingerDistance(event: MotionEvent): Float {
        val ids = automapPointers.keys.toList()
        if (ids.size < 2) return 0f
        val i0 = event.findPointerIndex(ids[0])
        val i1 = event.findPointerIndex(ids[1])
        if (i0 < 0 || i1 < 0) return 0f
        val dx = event.getX(i0) - event.getX(i1)
        val dy = event.getY(i0) - event.getY(i1)
        return kotlin.math.hypot(dx, dy)
    }

    /** Map Android gamepad KEYCODE_BUTTON_* to virtual joystick button index (0-9). */
    private fun gamepadButtonIndex(keyCode: Int): Int = when (keyCode) {
        KeyEvent.KEYCODE_BUTTON_A      -> 0
        KeyEvent.KEYCODE_BUTTON_B      -> 1
        KeyEvent.KEYCODE_BUTTON_X      -> 2
        KeyEvent.KEYCODE_BUTTON_Y      -> 3
        KeyEvent.KEYCODE_BUTTON_L1     -> 4
        KeyEvent.KEYCODE_BUTTON_R1     -> 5
        KeyEvent.KEYCODE_BUTTON_SELECT -> 6
        KeyEvent.KEYCODE_BUTTON_START  -> 7
        KeyEvent.KEYCODE_BUTTON_THUMBL -> 8
        KeyEvent.KEYCODE_BUTTON_THUMBR -> 9
        else -> -1
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        // Let the system handle volume keys
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN)
            return super.onKeyDown(keyCode, event)

        // Gamepad face / shoulder buttons → joystick button events
        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
            nativeJoystickButton(joyBtn, 1)
            return true
        }

        nativeKeyEvent(0, keyCode, event.unicodeChar)
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN)
            return super.onKeyUp(keyCode, event)

        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
            nativeJoystickButton(joyBtn, 0)
            return true
        }

        nativeKeyEvent(1, keyCode, 0)
        return true
    }

    // ── Gamepad analog axes ─────────────────────────────────
    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
            && event.action == MotionEvent.ACTION_MOVE
        ) {
            nativeJoystickAxis(0, event.getAxisValue(MotionEvent.AXIS_X))
            nativeJoystickAxis(1, event.getAxisValue(MotionEvent.AXIS_Y))
            nativeJoystickAxis(2, event.getAxisValue(MotionEvent.AXIS_Z))
            nativeJoystickAxis(3, event.getAxisValue(MotionEvent.AXIS_RZ))
            nativeJoystickAxis(4, event.getAxisValue(MotionEvent.AXIS_LTRIGGER))
            nativeJoystickAxis(5, event.getAxisValue(MotionEvent.AXIS_RTRIGGER))
            return true
        }
        return super.onGenericMotionEvent(event)
    }

    // ── Soft keyboard show/hide (called from JNI) ───────────
    @Suppress("unused")   // Called from native code
    fun showKeyboard(inputType: Int) {
        runOnUiThread {
            gameSurfaceView.currentInputType = when (inputType) {
                2    -> InputType.TYPE_CLASS_NUMBER
                else -> InputType.TYPE_CLASS_TEXT
            }
            gameSurfaceView.keyboardActive = true
            gameSurfaceView.requestFocus()
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.restartInput(gameSurfaceView)
            imm.showSoftInput(gameSurfaceView, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    @Suppress("unused")   // Called from native code
    fun hideKeyboard() {
        runOnUiThread {
            gameSurfaceView.keyboardActive = false
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.hideSoftInputFromWindow(gameSurfaceView.windowToken, 0)
        }
    }

    // ── GameSurfaceView with InputConnection for soft keyboard ──
    private inner class GameSurfaceView(context: Context) : SurfaceView(context) {
        var currentInputType = InputType.TYPE_CLASS_TEXT
        var keyboardActive = false

        override fun onCheckIsTextEditor(): Boolean = keyboardActive

        override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
            // Disable word prediction / autocorrect so each keystroke arrives
            // immediately via commitText instead of being buffered in composition.
            outAttrs.inputType = currentInputType or
                    InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
                    InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE or
                    EditorInfo.IME_FLAG_NO_EXTRACT_UI
            return GameInputConnection(this)
        }
    }

    /**
     * Routes soft-keyboard text input into the engine via JNI.
     * commitText → nativeTextInput (one SDL key pair per character)
     * performEditorAction(DONE) → Enter key
     * deleteSurroundingText → Backspace key(s)
     */
    private inner class GameInputConnection(view: View) : BaseInputConnection(view, false) {

        override fun setComposingText(text: CharSequence, newCursorPosition: Int): Boolean {
            // Some IMEs still compose even with NO_SUGGESTIONS.
            // Finish composition immediately and commit the text so each
            // character appears in the game without waiting for a space.
            finishComposingText()
            return commitText(text, newCursorPosition)
        }

        override fun commitText(text: CharSequence, newCursorPosition: Int): Boolean {
            for (c in text) {
                nativeTextInput(c.code)
            }
            return true
        }

        override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
            // Each "before" character = one Backspace press
            repeat(beforeLength) {
                nativeKeyEvent(0, KeyEvent.KEYCODE_DEL, 0)
                nativeKeyEvent(1, KeyEvent.KEYCODE_DEL, 0)
            }
            return true
        }

        override fun performEditorAction(actionCode: Int): Boolean {
            // "Done" / Enter on the soft keyboard → inject Enter key
            nativeKeyEvent(0, KeyEvent.KEYCODE_ENTER, '\r'.code)
            nativeKeyEvent(1, KeyEvent.KEYCODE_ENTER, 0)
            return true
        }
    }
}
