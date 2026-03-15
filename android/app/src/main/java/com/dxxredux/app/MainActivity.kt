package com.dxxredux.app

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.LayoutTransition
import android.animation.ObjectAnimator
import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.graphics.PointF
import android.graphics.Rect
import android.graphics.Typeface
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.ParcelFileDescriptor
import android.text.InputType
import android.util.Log
import android.util.TypedValue
import android.view.Gravity
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
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsAnimationCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.json.JSONObject
import java.io.File

class MainActivity :
    Activity(),
    SurfaceHolder.Callback {
    companion object {
        // Library is loaded dynamically in onCreate based on intent extra

        /** First virtual joystick button index for D-pad directions (Up=+0, Down=+1, Left=+2, Right=+3).
         *  Must match DPAD_BUTTON_BASE in joy.c. */
        const val DPAD_JOY_BUTTON_BASE = 22
    }

    // ── JNI declarations ────────────────────────────────────
    external fun helloFromNative(): String

    external fun startGame()

    external fun nativeSetSurface(surface: Surface?)

    external fun nativeTouchEvent(
        action: Int,
        normX: Float,
        normY: Float,
    )

    external fun nativeKeyEvent(
        action: Int,
        androidKeyCode: Int,
        unicodeChar: Int,
    )

    external fun nativeTextInput(unicodeChar: Int)

    external fun nativeOnPause()

    external fun nativeOnResume()

    external fun nativeQuit()

    external fun nativeGetGameState(): String

    external fun nativeRequestIntrospect()

    external fun nativeSetIntrospectPath(path: String)

    external fun nativeLoadAutomationScript(path: String)

    external fun nativeSetAutomationPath(path: String)

    external fun nativeSetMusicGain(gainDb: Float)

    external fun nativeSetMusicVoices(maxVoices: Int)

    external fun nativeJoystickAxis(
        axis: Int,
        value: Float,
    )

    external fun nativeJoystickButton(
        button: Int,
        pressed: Int,
    )

    external fun nativeIsInGame(): Boolean

    external fun nativeSetJoystickEnabled(enabled: Boolean)

    external fun nativeIsAutomapActive(): Boolean

    external fun nativeIsSkippableScreen(): Boolean

    external fun nativeAutomapInput(
        heading: Float,
        pitch: Float,
        thrust: Float,
        bank: Float = 0f,
        vertical: Float = 0f,
        sideways: Float = 0f,
    )

    external fun nativeAutomapCenter()

    external fun nativeAutomapSelectMarker(idx: Int)

    external fun nativeGetMarkerCount(): Int

    external fun nativeGetGameWidth(): Int

    external fun nativeGetGameHeight(): Int

    external fun nativeSetKeyboardHeight(
        keyboardHeightPx: Int,
        screenHeightPx: Int,
    )

    external fun nativeGetWeaponState(): IntArray

    // ── Admin tray (android_input.c) ────────────────────────────────
    external fun nativeCycleCockpit(direction: Int)

    external fun nativeToggleAutoLeveling()

    external fun nativeGetAutoLeveling(): Boolean

    external fun nativeGetCockpitMode(): Int

    // ── Music track control (jni_music_control.c) ────────────────────
    external fun nativeNextTrack(): Int

    external fun nativePrevTrack(): Int

    external fun nativePlaySpecificTrack(track: Int): Int

    external fun nativeGetTrackName(track: Int): String

    external fun nativeGetCurrentTrackNum(): Int

    external fun nativeGetNumAudioTracks(): Int

    external fun nativeGetCurrentTrackInfo(): String

    // ── SAF leave-in-place: called from native via JNI (jni_saf.c) ───
    @Suppress("unused") // Called from native code
    fun openSafFile(contentUri: String): Int {
        return try {
            if (contentUri.startsWith("/")) {
                // Test mode: direct filesystem path (for adb testing)
                val pfd =
                    ParcelFileDescriptor.open(
                        java.io.File(contentUri),
                        ParcelFileDescriptor.MODE_READ_ONLY,
                    )
                return pfd.detachFd()
            }
            // Production mode: SAF content URI
            val uri = Uri.parse(contentUri)
            val pfd = contentResolver.openFileDescriptor(uri, "r") ?: return -1
            pfd.detachFd() // transfers fd ownership to native
        } catch (e: Exception) {
            Log.e("MainActivity", "openSafFile failed for $contentUri", e)
            -1
        }
    }

    private var gameStarted = false
    private lateinit var gameSurfaceView: GameSurfaceView
    private lateinit var touchOverlay: TouchOverlayView
    private lateinit var skipButton: SkipButtonView
    private lateinit var overlayContainer: LinearLayout
    private var overlayEnabled = false
    private val overlayPoller = android.os.Handler(android.os.Looper.getMainLooper())
    private var musicPanel: MusicControlPanel? = null
    private var lastTrackNum = -1 // for detecting track changes in polling
    private var gyroManager: GyroInputManager? = null

    // Controller meta-action bindings: SDL button index → meta action ID
    private var buttonMetaBindings = mapOf<Int, Int>()

    // D-pad meta-action bindings: DPAD keycode → meta action ID
    private var dpadMetaBindings = mapOf<Int, Int>()

    // ── Left-edge fling detection (→ setup screen) ────────────────────
    private lateinit var edgeFlingDetector: android.view.GestureDetector
    private var edgeSwipeTracking = false
    private var edgeSwipeStartX = 0f

    // ── Automap gesture state (drag = pan/tilt, pinch = thrust/rotate/translate) ──
    private val automapPointers = mutableMapOf<Int, PointF>() // pointerId → last position
    private var automapPinchDist = 0f // last distance between two fingers
    private var automapPinchAngle = 0f // last angle between two fingers (radians)
    private var automapPinchMidX = 0f // last midpoint X between two fingers
    private var automapPinchMidY = 0f // last midpoint Y between two fingers

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Load the correct game library based on the launcher's selection
        val game = intent.getStringExtra("game") ?: "d2"
        val libName = if (game == "d1") "dxx-redux-d1" else "dxx-redux-d2"
        System.loadLibrary(libName)
        Log.i("MainActivity", "Loaded native library: $libName")

        loadMetaBindings()

        // Keep screen on while the game is running
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Apply orientation lock from launcher preference
        val prefs = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
        val orientPref = prefs.getString("game_orientation", "landscape")
        requestedOrientation =
            if (orientPref == "portrait") {
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            } else {
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            }

        // Allow rendering into the display cutout (notch) area
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }

        // Draw behind system bars
        WindowCompat.setDecorFitsSystemWindows(window, false)

        // Fling detector for left-edge swipe → open setup screen
        edgeFlingDetector =
            android.view.GestureDetector(
                this,
                object : android.view.GestureDetector.SimpleOnGestureListener() {
                    override fun onFling(
                        e1: MotionEvent?,
                        e2: MotionEvent,
                        velocityX: Float,
                        velocityY: Float,
                    ): Boolean {
                        if (e1 == null) return false
                        val density = resources.displayMetrics.density
                        val edgePx = 40 * density
                        if (e1.x > edgePx) return false // didn't start at left edge
                        if (velocityX < 600) return false // not fast enough rightward
                        val dx = e2.x - e1.x
                        val dy = kotlin.math.abs(e2.y - e1.y)
                        if (dx > 30 * density && dy < dx * 1.5f) { // roughly horizontal
                            openSetupScreen()
                            return true
                        }
                        return false
                    }
                },
            )

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
        val layout = TouchLayoutRepository.load(this)
        touchOverlay.setLayout(layout)
        touchOverlay.axisCallback = { axis, value -> nativeJoystickAxis(axis, value) }

        // Gyro aiming
        if (layout.gyro.enabled) {
            val gm = GyroInputManager(this)
            gm.setConfig(layout.gyro)
            gm.axisCallback = { axis, value -> nativeJoystickAxis(axis, value) }
            touchOverlay.gyroManager = gm
            gyroManager = gm
        }
        touchOverlay.buttonCallback = { button, pressed ->
            if (TouchBindings.isMetaAction(button)) {
                NativeMetaActions.nativeMetaAction(button, if (pressed) 1 else 0)
            } else {
                nativeJoystickButton(button + TouchBindings.TOUCH_BTN_OFFSET, if (pressed) 1 else 0)
            }
        }
        touchOverlay.metaActionCallback = { actionId, pressed ->
            NativeMetaActions.nativeMetaAction(actionId, if (pressed) 1 else 0)
        }
        touchOverlay.keyCallback = { action, keyCode, unicode ->
            nativeKeyEvent(action, keyCode, unicode)
        }
        touchOverlay.gameVariant = game
        touchOverlay.cheatCodeCallback = { code ->
            for (ch in code) nativeTextInput(ch.code)
        }
        touchOverlay.adminTrayCallback = { action ->
            when (action) {
                TouchOverlayView.ADMIN_INCREASE_VIEW -> nativeCycleCockpit(1)
                TouchOverlayView.ADMIN_DECREASE_VIEW -> nativeCycleCockpit(-1)
                TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL -> nativeToggleAutoLeveling()
                TouchOverlayView.ADMIN_QUICK_SAVE -> {
                    // Alt+F2
                    nativeKeyEvent(0, KeyEvent.KEYCODE_ALT_LEFT, 0)
                    nativeKeyEvent(0, KeyEvent.KEYCODE_F2, 0)
                    nativeKeyEvent(1, KeyEvent.KEYCODE_F2, 0)
                    nativeKeyEvent(1, KeyEvent.KEYCODE_ALT_LEFT, 0)
                }
                TouchOverlayView.ADMIN_QUICK_LOAD -> {
                    // Alt+F3
                    nativeKeyEvent(0, KeyEvent.KEYCODE_ALT_LEFT, 0)
                    nativeKeyEvent(0, KeyEvent.KEYCODE_F3, 0)
                    nativeKeyEvent(1, KeyEvent.KEYCODE_F3, 0)
                    nativeKeyEvent(1, KeyEvent.KEYCODE_ALT_LEFT, 0)
                }
                TouchOverlayView.ADMIN_OPEN_MENU -> {
                    nativeKeyEvent(0, KeyEvent.KEYCODE_ESCAPE, 0)
                    nativeKeyEvent(1, KeyEvent.KEYCODE_ESCAPE, 0)
                }
            }
        }
        touchOverlay.adminTrayAutoLevelingProvider = {
            try {
                nativeGetAutoLeveling()
            } catch (_: Throwable) {
                true
            }
        }
        touchOverlay.adminTrayCockpitModeProvider = {
            try {
                nativeGetCockpitMode()
            } catch (_: Throwable) {
                -1
            }
        }
        touchOverlay.weaponStateProvider = {
            try {
                WeaponState.fromArray(nativeGetWeaponState())
            } catch (_: Throwable) {
                null
            }
        }
        touchOverlay.automapInputCallback = { heading, pitch, thrust, bank, vertical, sideways ->
            nativeAutomapInput(heading, pitch, thrust, bank, vertical, sideways)
        }
        touchOverlay.automapCenterCallback = { nativeAutomapCenter() }
        touchOverlay.automapMarkerCallback = { idx -> nativeAutomapSelectMarker(idx) }
        touchOverlay.markerCountProvider =
            if (game == "d1") {
                { 0 } // D1 has no markers
            } else {
                {
                    try {
                        nativeGetMarkerCount()
                    } catch (_: Throwable) {
                        0
                    }
                }
            }
        touchOverlay.mapButtonCallback = { toggleAutomap() }
        touchOverlay.prevTrackCallback = {
            nativePrevTrack()
            updateTrackLabel()
        }
        touchOverlay.nextTrackCallback = {
            nativeNextTrack()
            updateTrackLabel()
        }
        touchOverlay.musicPanelCallback = { showMusicPanel() }
        touchOverlay.tapPassthroughCallback = {
            // Inject Enter key press so "press any key" screens respond to touch
            nativeKeyEvent(0, android.view.KeyEvent.KEYCODE_ENTER, '\r'.code)
            nativeKeyEvent(1, android.view.KeyEvent.KEYCODE_ENTER, 0)
        }
        touchOverlay.isActive = false

        // Skip button for movies/briefings (upper-right, hidden by default)
        skipButton =
            SkipButtonView(this).apply {
                keyCallback = { action, keyCode, unicode -> nativeKeyEvent(action, keyCode, unicode) }
                visibility = View.GONE
            }

        // Multi-line overlay container (upper-left, items fade independently)
        overlayContainer =
            LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                layoutTransition =
                    LayoutTransition().apply {
                        enableTransitionType(LayoutTransition.CHANGE_DISAPPEARING)
                    }
            }
        val overlayLp =
            FrameLayout
                .LayoutParams(
                    FrameLayout.LayoutParams.WRAP_CONTENT,
                    FrameLayout.LayoutParams.WRAP_CONTENT,
                ).apply {
                    gravity = Gravity.TOP or Gravity.START
                    val pad = (8 * resources.displayMetrics.density).toInt()
                    setMargins(pad, pad, 0, 0)
                }

        // Layer surface + overlay in a FrameLayout
        val frame = FrameLayout(this)
        frame.addView(
            gameSurfaceView,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
        frame.addView(
            touchOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
        frame.addView(
            skipButton,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
        frame.addView(overlayContainer, overlayLp)

        setContentView(frame)

        // Hide system bars after content view is set
        hideSystemBars()

        // Keyboard height detection: with adjustNothing, neither
        // setOnApplyWindowInsetsListener nor WindowInsetsAnimationCompat
        // fire reliably.  Instead we poll via getRootWindowInsets()/
        // WindowInsets.Type.ime() from showKeyboard().  The animation
        // callback is kept as a best-effort supplement.
        ViewCompat.setWindowInsetsAnimationCallback(
            window.decorView,
            object :
                WindowInsetsAnimationCompat.Callback(
                    WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_STOP,
                ) {
                override fun onProgress(
                    insets: WindowInsetsCompat,
                    runningAnimations: List<WindowInsetsAnimationCompat>,
                ): WindowInsetsCompat {
                    val imeBottom = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom
                    if (imeBottom > 0) {
                        Log.i("DXX-Keyboard", "imeAnim onProgress: imeBottom=$imeBottom")
                        nativeSetKeyboardHeight(imeBottom, window.decorView.height)
                    }
                    return insets
                }

                override fun onEnd(animation: WindowInsetsAnimationCompat) {
                    val insets = ViewCompat.getRootWindowInsets(window.decorView)
                    val imeBottom = insets?.getInsets(WindowInsetsCompat.Type.ime())?.bottom ?: 0
                    Log.i("DXX-Keyboard", "imeAnim onEnd: imeBottom=$imeBottom")
                    nativeSetKeyboardHeight(imeBottom, window.decorView.height)
                }
            },
        )

        // Prevent the system back/nav gestures from consuming edge swipes.
        // Left edge: we use it for the setup-screen swipe.
        // Right edge: prevent system "back" from interfering with gameplay.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            gameSurfaceView.addOnLayoutChangeListener { v, _, _, _, _, _, _, _, _ ->
                val edgePx = (40 * resources.displayMetrics.density).toInt()
                v.systemGestureExclusionRects =
                    listOf(
                        Rect(0, 0, edgePx, v.height), // left edge
                        Rect(v.width - edgePx, 0, v.width, v.height), // right edge
                    )
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
            Log.i("DXX-Automate", "Game surface created, gameStarted=true")

            Thread {
                startGame()
            }.start()
        }
    }

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int,
    ) {
        nativeSetSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSetSurface(null)
    }

    // ── Lifecycle ────────────────────────────────────────────
    override fun onStop() {
        super.onStop()
        gyroManager?.pause()
        overlayPoller.removeCallbacksAndMessages(null)
        // Inject Escape so the engine opens its pause / game menu.
        // This pauses a single-player game while the app is in the background.
        if (gameStarted) {
            nativeOnPause()
        }
    }

    override fun onResume() {
        super.onResume()
        gyroManager?.resume()
        // Resume music that was paused when backgrounded
        if (gameStarted) {
            nativeOnResume()
        }
        // Re-read preference (user may have toggled in SetupActivity)
        val prefs = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
        // Default to enabled when no physical controller is connected
        val hasController =
            InputDevice.getDeviceIds().any { id ->
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
        val pollRunnable =
            object : Runnable {
                override fun run() {
                    if (gameStarted) {
                        try {
                            val inGame = nativeIsInGame()
                            val automap =
                                try {
                                    nativeIsAutomapActive()
                                } catch (_: Exception) {
                                    false
                                }
                            val skippable =
                                try {
                                    nativeIsSkippableScreen()
                                } catch (_: Exception) {
                                    false
                                }
                            // Show overlay when in-game with overlay enabled, or when automap is active
                            val shouldShow = (inGame && overlayEnabled) || automap
                            val wasActive = touchOverlay.isActive
                            touchOverlay.isActive = shouldShow
                            touchOverlay.automapActive = automap
                            // Show/hide skip button (mutually exclusive with game overlay)
                            skipButton.visibility = if (skippable && !shouldShow) View.VISIBLE else View.GONE
                            // Enable/disable joystick input when overlay state changes
                            if (shouldShow && !wasActive) {
                                nativeSetJoystickEnabled(true)
                            } else if (!shouldShow && wasActive) {
                                nativeSetJoystickEnabled(false)
                            }
                            // Poll current track to update overlay label
                            if (shouldShow && !automap) pollTrackLabel()
                        } catch (_: Exception) {
                            touchOverlay.isActive = false
                            touchOverlay.automapActive = false
                            skipButton.visibility = View.GONE
                        }
                    } else {
                        if (touchOverlay.isActive) {
                            nativeSetJoystickEnabled(false)
                        }
                        touchOverlay.isActive = false
                        touchOverlay.automapActive = false
                        skipButton.visibility = View.GONE
                    }
                    overlayPoller.postDelayed(this, 100)
                }
            }
        overlayPoller.post(pollRunnable)
    }

    // ── Introspection (debug builds only) ────────────────────
    // Trigger a game state dump to a file readable via adb:
    //   adb shell am broadcast -a com.dxxredux.INTROSPECT -n com.dxxredux.app/.MainActivity
    //   adb shell run-as com.dxxredux.app cat files/introspect.json
    private val introspectReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                context: Context,
                intent: Intent,
            ) {
                if (!gameStarted) return
                nativeRequestIntrospect()
                Log.i("DXX-Introspect", "Introspection requested — will dump on next frame")
            }
        }

    // ── Automation (debug builds only) ───────────────────────
    // Load and run a JSON automation script:
    //   adb push script.json /data/local/tmp/script.json
    //   adb shell am broadcast -a com.dxxredux.AUTOMATE --es script /data/local/tmp/script.json
    // Or use the files dir:
    //   adb shell run-as com.dxxredux.app cp /data/local/tmp/script.json files/
    //   adb shell am broadcast -a com.dxxredux.AUTOMATE --es script files/automate.json
    private val automateReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                context: Context,
                intent: Intent,
            ) {
                if (!gameStarted) {
                    Log.w("DXX-Automate", "AUTOMATE broadcast ignored: game not started yet")
                    return
                }
                val scriptPath = intent.getStringExtra("script")
                if (scriptPath.isNullOrEmpty()) {
                    Log.e("DXX-Automate", "No 'script' extra in AUTOMATE broadcast")
                    return
                }
                // If path is relative, resolve against filesDir
                val resolvedPath =
                    if (scriptPath.startsWith("/")) {
                        scriptPath
                    } else {
                        filesDir.absolutePath + "/" + scriptPath
                    }
                Log.i("DXX-Automate", "Loading automation script: $resolvedPath")
                nativeLoadAutomationScript(resolvedPath)
            }
        }

    // ── Game command API (debug builds only) ─────────────────
    //   adb shell am broadcast -a com.dxxredux.GAME_COMMAND --es command gain --ef value -20.0
    //   adb shell am broadcast -a com.dxxredux.GAME_COMMAND --es command voices --ei value 32
    private val gameCommandReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                context: Context,
                intent: Intent,
            ) {
                if (!gameStarted) return
                val cmd = intent.getStringExtra("command") ?: return
                when (cmd) {
                    "gain" -> {
                        val db = intent.getFloatExtra("value", -10.0f)
                        Log.i("DXX-Command", "Setting music gain to $db dB")
                        nativeSetMusicGain(db)
                    }
                    "voices" -> {
                        val n = intent.getIntExtra("value", 48)
                        Log.i("DXX-Command", "Setting max voices to $n")
                        nativeSetMusicVoices(n)
                    }
                    else -> Log.w("DXX-Command", "Unknown command: $cmd")
                }
            }
        }

    override fun onStart() {
        super.onStart()
        if (BuildConfig.DEBUG) {
            // Set the file path for introspection dumps
            nativeSetIntrospectPath(filesDir.absolutePath + "/introspect.json")
            nativeSetAutomationPath(filesDir.absolutePath)

            val filter = IntentFilter("com.dxxredux.INTROSPECT")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(introspectReceiver, filter, RECEIVER_EXPORTED)
            } else {
                @Suppress("UnspecifiedRegisterReceiverFlag")
                registerReceiver(introspectReceiver, filter)
            }

            val automateFilter = IntentFilter("com.dxxredux.AUTOMATE")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(automateReceiver, automateFilter, RECEIVER_EXPORTED)
            } else {
                @Suppress("UnspecifiedRegisterReceiverFlag")
                registerReceiver(automateReceiver, automateFilter)
            }

            val cmdFilter = IntentFilter("com.dxxredux.GAME_COMMAND")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(gameCommandReceiver, cmdFilter, RECEIVER_EXPORTED)
            } else {
                @Suppress("UnspecifiedRegisterReceiverFlag")
                registerReceiver(gameCommandReceiver, cmdFilter)
            }
        }
    }

    override fun onDestroy() {
        if (BuildConfig.DEBUG) {
            try {
                unregisterReceiver(introspectReceiver)
            } catch (_: Exception) {
            }
            try {
                unregisterReceiver(automateReceiver)
            } catch (_: Exception) {
            }
            try {
                unregisterReceiver(gameCommandReceiver)
            } catch (_: Exception) {
            }
        }
        super.onDestroy()
    }

    // ── Touch → Mouse ───────────────────────────────────────
    private fun handleTouch(
        view: View,
        event: MotionEvent,
    ): Boolean {
        val density = resources.displayMetrics.density
        val edgeThresholdPx = 40 * density // 40 dp from left edge

        // ── Left-edge fling detection (→ setup screen) ──────
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                edgeSwipeTracking = event.x < edgeThresholdPx
                if (edgeSwipeTracking) edgeSwipeStartX = event.x
            }
            MotionEvent.ACTION_MOVE -> {
                if (edgeSwipeTracking) {
                    edgeFlingDetector.onTouchEvent(event)
                    return true
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (edgeSwipeTracking) {
                    edgeFlingDetector.onTouchEvent(event)
                    edgeSwipeTracking = false
                    return true
                }
            }
        }
        if (edgeSwipeTracking) {
            edgeFlingDetector.onTouchEvent(event)
            return true
        }

        // ── Automap gesture handling (when overlay is off) ────
        // When the automap is showing, all non-edge touches become
        // pan/tilt (drag) or forward/reverse (pinch).
        if (gameStarted) {
            try {
                if (nativeIsAutomapActive()) {
                    return handleAutomapTouch(event, view.width.toFloat(), view.height.toFloat())
                }
            } catch (_: Exception) {
                // engine not ready
            }
        }

        // ── Normal game touch handling ──────────────────────
        // Map touch to normalised 0.0–1.0 coordinates.  The native side
        // converts to engine resolution via grd_curscreen, so Kotlin
        // never needs to know the game resolution.
        val viewW = view.width.toFloat()
        val viewH = view.height.toFloat()
        if (viewW <= 0f || viewH <= 0f) return false

        val normX = (event.x / viewW).coerceIn(0f, 1f)
        val normY = (event.y / viewH).coerceIn(0f, 1f)

        val action =
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> 0
                MotionEvent.ACTION_MOVE -> 1
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> 2
                else -> return false
            }

        nativeTouchEvent(action, normX, normY)
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
        // Immediately flip the overlay to avoid polling lag
        touchOverlay.automapActive = !touchOverlay.automapActive
        nativeKeyEvent(0, KeyEvent.KEYCODE_TAB, '\t'.code)
        nativeKeyEvent(1, KeyEvent.KEYCODE_TAB, 0)
    }

    // ── Automap touch gestures ──────────────────────────────
    //  1-finger drag         →  pan / tilt  (heading_time, pitch_time)
    //  double-tap then drag  →  translate x/y (sideways_thrust, vertical_thrust)
    //  2-finger pinch        →  zoom (forward_thrust) + rotate (bank_time)
    //  2-finger pan          →  translate x/y (sideways_thrust, vertical_thrust)
    //
    // Called from handleTouch (overlay off) and from the overlay's
    // unmatched-touch callback (overlay on).

    /**
     * Feed a raw MotionEvent into the automap gesture tracker.
     * [screenW] / [screenH] are the view dimensions for normalisation.
     * Returns true if the event was consumed.
     */
    fun handleAutomapTouch(
        event: MotionEvent,
        screenW: Float,
        screenH: Float,
    ): Boolean {
        if (screenW <= 0f || screenH <= 0f) return false

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val idx = event.actionIndex
                val pid = event.getPointerId(idx)
                val px = event.getX(idx)
                val py = event.getY(idx)
                automapPointers[pid] = PointF(px, py)

                // When a second finger lands, initialise pinch distance + angle
                if (automapPointers.size == 2) {
                    automapPinchDist = automapFingerDistance(event)
                    automapPinchAngle = automapFingerAngle(event)
                    val mid = automapFingerMidpoint(event)
                    automapPinchMidX = mid.x
                    automapPinchMidY = mid.y
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (automapPointers.size == 1) {
                    val pid = automapPointers.keys.first()
                    val idx = event.findPointerIndex(pid)
                    if (idx >= 0) {
                        val prev = automapPointers[pid]!!
                        val dx = event.getX(idx) - prev.x
                        val dy = event.getY(idx) - prev.y
                        prev.set(event.getX(idx), event.getY(idx))

                        // Single-finger drag → pan / tilt
                        val heading = dx / screenW
                        val pitch = dy / screenH
                        if (heading != 0f || pitch != 0f) {
                            nativeAutomapInput(heading, pitch, 0f)
                        }
                    }
                } else if (automapPointers.size >= 2) {
                    // Pinch → zoom + rotate + translate
                    val dist = automapFingerDistance(event)
                    val angle = automapFingerAngle(event)
                    val mid = automapFingerMidpoint(event)

                    if (automapPinchDist > 0f) {
                        val delta = dist - automapPinchDist
                        val thrust = delta / screenW * 60f * 3f

                        var dAngle = angle - automapPinchAngle
                        while (dAngle > Math.PI.toFloat()) dAngle -= (2 * Math.PI).toFloat()
                        while (dAngle < -Math.PI.toFloat()) dAngle += (2 * Math.PI).toFloat()
                        val bank = dAngle / Math.PI.toFloat() * 3f

                        val sideways = -(mid.x - automapPinchMidX) / screenW
                        val vertical = (mid.y - automapPinchMidY) / screenH

                        if (thrust != 0f || bank != 0f || sideways != 0f || vertical != 0f) {
                            nativeAutomapInput(0f, 0f, thrust, bank, vertical, sideways)
                        }
                    }
                    automapPinchDist = dist
                    automapPinchAngle = angle
                    automapPinchMidX = mid.x
                    automapPinchMidY = mid.y

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
                if (automapPointers.size >= 2) {
                    automapPinchDist = automapFingerDistance(event)
                    automapPinchAngle = automapFingerAngle(event)
                    val mid = automapFingerMidpoint(event)
                    automapPinchMidX = mid.x
                    automapPinchMidY = mid.y
                } else {
                    automapPinchDist = 0f
                    automapPinchAngle = 0f
                    automapPinchMidX = 0f
                    automapPinchMidY = 0f
                    for ((id, pt) in automapPointers) {
                        val i = event.findPointerIndex(id)
                        if (i >= 0) pt.set(event.getX(i), event.getY(i))
                    }
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                automapPointers.clear()
                automapPinchDist = 0f
                automapPinchAngle = 0f
                automapPinchMidX = 0f
                automapPinchMidY = 0f
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

    /** Angle (radians) from the first tracked finger to the second. */
    private fun automapFingerAngle(event: MotionEvent): Float {
        val ids = automapPointers.keys.toList()
        if (ids.size < 2) return 0f
        val i0 = event.findPointerIndex(ids[0])
        val i1 = event.findPointerIndex(ids[1])
        if (i0 < 0 || i1 < 0) return 0f
        val dx = event.getX(i1) - event.getX(i0)
        val dy = event.getY(i1) - event.getY(i0)
        return kotlin.math.atan2(dy, dx)
    }

    /** Midpoint between the first two tracked automap fingers. */
    private fun automapFingerMidpoint(event: MotionEvent): PointF {
        val ids = automapPointers.keys.toList()
        if (ids.size < 2) return PointF(0f, 0f)
        val i0 = event.findPointerIndex(ids[0])
        val i1 = event.findPointerIndex(ids[1])
        if (i0 < 0 || i1 < 0) return PointF(0f, 0f)
        return PointF(
            (event.getX(i0) + event.getX(i1)) / 2f,
            (event.getY(i0) + event.getY(i1)) / 2f,
        )
    }

    /** Load controller meta-action bindings from controller_config.json. */
    private fun loadMetaBindings() {
        val file = File(filesDir, "controller_config.json")
        if (!file.exists()) return
        try {
            val json = JSONObject(file.readText())
            if (!json.has("meta_bindings")) return
            val meta = json.getJSONObject("meta_bindings")
            val btnMap = mutableMapOf<Int, Int>()
            val dpadMap = mutableMapOf<Int, Int>()
            for (key in meta.keys()) {
                val actionId = meta.getInt(key)
                if (key.startsWith("dpad_")) {
                    val dpadKeyCode = dpadControlToKeyCode(key.removePrefix("dpad_"))
                    if (dpadKeyCode > 0) dpadMap[dpadKeyCode] = actionId
                } else {
                    val sdlBtn = key.toIntOrNull()
                    if (sdlBtn != null) btnMap[sdlBtn] = actionId
                }
            }
            buttonMetaBindings = btnMap
            dpadMetaBindings = dpadMap
        } catch (e: Exception) {
            Log.w("MainActivity", "Failed to load meta bindings", e)
        }
    }

    /** Map d-pad control ID from config to Android KeyEvent keycode. */
    private fun dpadControlToKeyCode(controlId: String): Int =
        when (controlId) {
            "DUp" -> KeyEvent.KEYCODE_DPAD_UP
            "DDown" -> KeyEvent.KEYCODE_DPAD_DOWN
            "DLeft" -> KeyEvent.KEYCODE_DPAD_LEFT
            "DRight" -> KeyEvent.KEYCODE_DPAD_RIGHT
            else -> -1
        }

    /** Dispatch a d-pad event, using meta action if bound, else joystick button.
     *  D-pad virtual button indices: DUp=22, DDown=23, DLeft=24, DRight=25.
     *  Shared constant with joy.c D-pad button registration. */
    private fun dispatchDpad(
        keyCode: Int,
        action: Int,
    ) {
        val metaId = dpadMetaBindings[keyCode]
        if (metaId != null) {
            NativeMetaActions.nativeMetaAction(metaId, if (action == 0) 1 else 0)
        } else {
            val btnIdx = dpadKeyCodeToJoyButton(keyCode)
            if (btnIdx >= 0) {
                nativeJoystickButton(btnIdx, if (action == 0) 1 else 0)
            }
        }
    }

    /** Map d-pad keycode to virtual joystick button index (22-25). */
    private fun dpadKeyCodeToJoyButton(keyCode: Int): Int =
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_UP -> DPAD_JOY_BUTTON_BASE
            KeyEvent.KEYCODE_DPAD_DOWN -> DPAD_JOY_BUTTON_BASE + 1
            KeyEvent.KEYCODE_DPAD_LEFT -> DPAD_JOY_BUTTON_BASE + 2
            KeyEvent.KEYCODE_DPAD_RIGHT -> DPAD_JOY_BUTTON_BASE + 3
            else -> -1
        }

    /** Map Android gamepad KEYCODE_BUTTON_* to virtual joystick button index (0-9). */
    private fun gamepadButtonIndex(keyCode: Int): Int =
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> 0
            KeyEvent.KEYCODE_BUTTON_B -> 1
            KeyEvent.KEYCODE_BUTTON_X -> 2
            KeyEvent.KEYCODE_BUTTON_Y -> 3
            KeyEvent.KEYCODE_BUTTON_L1 -> 4
            KeyEvent.KEYCODE_BUTTON_R1 -> 5
            KeyEvent.KEYCODE_BUTTON_SELECT -> 6
            KeyEvent.KEYCODE_BUTTON_START -> 7
            KeyEvent.KEYCODE_BUTTON_THUMBL -> 8
            KeyEvent.KEYCODE_BUTTON_THUMBR -> 9
            else -> -1
        }

    override fun onKeyDown(
        keyCode: Int,
        event: KeyEvent,
    ): Boolean {
        // Let the system handle volume keys
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyDown(keyCode, event)
        }

        // Gamepad face / shoulder buttons → joystick button events
        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
            val metaId = buttonMetaBindings[joyBtn]
            if (metaId != null) {
                NativeMetaActions.nativeMetaAction(metaId, 1)
            } else {
                nativeJoystickButton(joyBtn, 1)
            }
            return true
        }

        // D-pad keys: route through dispatchDpad to avoid dual dispatch
        // (HAT axis path already sends joystick buttons 22-25)
        if (dpadKeyCodeToJoyButton(keyCode) >= 0) {
            dispatchDpad(keyCode, 0)
            return true
        }

        nativeKeyEvent(0, keyCode, event.unicodeChar)
        return true
    }

    override fun onKeyUp(
        keyCode: Int,
        event: KeyEvent,
    ): Boolean {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyUp(keyCode, event)
        }

        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
            val metaId = buttonMetaBindings[joyBtn]
            if (metaId != null) {
                NativeMetaActions.nativeMetaAction(metaId, 0)
            } else {
                nativeJoystickButton(joyBtn, 0)
            }
            return true
        }

        if (dpadKeyCodeToJoyButton(keyCode) >= 0) {
            dispatchDpad(keyCode, 1)
            return true
        }

        nativeKeyEvent(1, keyCode, 0)
        return true
    }

    // ── Gamepad analog axes ─────────────────────────────────
    private var hatXState = 0 // -1, 0, +1
    private var hatYState = 0

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK &&
            event.action == MotionEvent.ACTION_MOVE
        ) {
            nativeJoystickAxis(0, event.getAxisValue(MotionEvent.AXIS_X))
            nativeJoystickAxis(1, event.getAxisValue(MotionEvent.AXIS_Y))
            nativeJoystickAxis(2, event.getAxisValue(MotionEvent.AXIS_Z))
            nativeJoystickAxis(3, event.getAxisValue(MotionEvent.AXIS_RZ))
            nativeJoystickAxis(4, event.getAxisValue(MotionEvent.AXIS_LTRIGGER))
            nativeJoystickAxis(5, event.getAxisValue(MotionEvent.AXIS_RTRIGGER))

            // D-pad reported as HAT axes → synthesize keyboard arrow keys
            val hx = event.getAxisValue(MotionEvent.AXIS_HAT_X)
            val hy = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
            val newHatX =
                if (hx < -0.5f) {
                    -1
                } else if (hx > 0.5f) {
                    1
                } else {
                    0
                }
            val newHatY =
                if (hy < -0.5f) {
                    -1
                } else if (hy > 0.5f) {
                    1
                } else {
                    0
                }
            if (newHatX != hatXState) {
                if (hatXState == -1) dispatchDpad(KeyEvent.KEYCODE_DPAD_LEFT, 1)
                if (hatXState == 1) dispatchDpad(KeyEvent.KEYCODE_DPAD_RIGHT, 1)
                if (newHatX == -1) dispatchDpad(KeyEvent.KEYCODE_DPAD_LEFT, 0)
                if (newHatX == 1) dispatchDpad(KeyEvent.KEYCODE_DPAD_RIGHT, 0)
                hatXState = newHatX
            }
            if (newHatY != hatYState) {
                if (hatYState == -1) dispatchDpad(KeyEvent.KEYCODE_DPAD_UP, 1)
                if (hatYState == 1) dispatchDpad(KeyEvent.KEYCODE_DPAD_DOWN, 1)
                if (newHatY == -1) dispatchDpad(KeyEvent.KEYCODE_DPAD_UP, 0)
                if (newHatY == 1) dispatchDpad(KeyEvent.KEYCODE_DPAD_DOWN, 0)
                hatYState = newHatY
            }

            return true
        }
        return super.onGenericMotionEvent(event)
    }

    // ── Soft keyboard show/hide (called from JNI) ───────────
    private var keyboardPollRunnable: Runnable? = null

    /** Poll for IME height via rootWindowInsets.  With adjustNothing
     *  the insets callbacks don't fire, so we poll after requesting
     *  the keyboard and stop once we detect a non-zero IME height. */
    private fun pollKeyboardHeight(attemptsLeft: Int) {
        if (attemptsLeft <= 0) return
        val decorView = window.decorView
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val insets = decorView.rootWindowInsets
            if (insets != null) {
                val imeHeight =
                    insets
                        .getInsets(
                            android.view.WindowInsets.Type
                                .ime(),
                        ).bottom
                Log.i("DXX-Keyboard", "poll: imeHeight=$imeHeight viewH=${decorView.height} left=$attemptsLeft")
                if (imeHeight > 0) {
                    nativeSetKeyboardHeight(imeHeight, decorView.height)
                    return
                }
            }
        }
        val r = Runnable { pollKeyboardHeight(attemptsLeft - 1) }
        keyboardPollRunnable = r
        decorView.postDelayed(r, 100)
    }

    @Suppress("unused") // Called from native code
    fun showKeyboard(inputType: Int) {
        runOnUiThread {
            gameSurfaceView.currentInputType =
                when (inputType) {
                    2 -> InputType.TYPE_CLASS_NUMBER
                    else -> InputType.TYPE_CLASS_TEXT
                }
            gameSurfaceView.keyboardActive = true
            gameSurfaceView.requestFocus()
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.restartInput(gameSurfaceView)
            // Use WindowInsetsController API (works with setDecorFitsSystemWindows(false))
            WindowInsetsControllerCompat(window, gameSurfaceView)
                .show(WindowInsetsCompat.Type.ime())
            // Start polling for keyboard height (up to 2 seconds)
            pollKeyboardHeight(20)
        }
    }

    @Suppress("unused") // Called from native code
    fun hideKeyboard() {
        runOnUiThread {
            keyboardPollRunnable?.let { window.decorView.removeCallbacks(it) }
            keyboardPollRunnable = null
            gameSurfaceView.keyboardActive = false
            WindowInsetsControllerCompat(window, gameSurfaceView)
                .hide(WindowInsetsCompat.Type.ime())
            nativeSetKeyboardHeight(0, window.decorView.height)
        }
    }

    // ── Music overlay helpers ─────────────────────────────────
    private fun pollTrackLabel() {
        try {
            val trackNum = nativeGetCurrentTrackNum()
            if (trackNum != lastTrackNum) {
                lastTrackNum = trackNum
                updateTrackLabel()
            }
        } catch (_: Exception) {
            // not playing
        }
    }

    private fun updateTrackLabel() {
        try {
            val info = nativeGetCurrentTrackInfo()
            if (info.isNotEmpty()) {
                // Format: "trackNum|sourceIndex|trackName"
                val parts = info.split("|", limit = 3)
                val name = if (parts.size >= 3 && parts[2].isNotEmpty()) parts[2] else "Track ${parts[0]}"
                touchOverlay.trackLabel = "\u266B $name"
                touchOverlay.invalidate()
            } else {
                touchOverlay.trackLabel = ""
            }
        } catch (_: Exception) {
            touchOverlay.trackLabel = ""
        }
    }

    private fun showMusicPanel() {
        if (musicPanel != null) return // already showing
        val panel =
            MusicControlPanel(this, { track ->
                nativePlaySpecificTrack(track)
                updateTrackLabel()
            }, {
                musicPanel?.let { mp ->
                    (gameSurfaceView.parent as? FrameLayout)?.removeView(mp)
                }
                musicPanel = null
            })
        musicPanel = panel
        val frame = gameSurfaceView.parent as? FrameLayout ?: return
        frame.addView(
            panel,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
    }

    // ── Overlay toast lines (multi-line, each fades independently) ──
    private fun showOverlayLine(text: String) {
        runOnUiThread {
            val tv =
                TextView(this).apply {
                    this.text = text
                    setTextColor(Color.GREEN)
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
                    typeface = Typeface.MONOSPACE
                    alpha = 0f
                }
            overlayContainer.addView(tv)

            val fadeIn = ObjectAnimator.ofFloat(tv, "alpha", 0f, 1f).apply { duration = 500 }
            fadeIn.addListener(
                object : AnimatorListenerAdapter() {
                    override fun onAnimationEnd(animation: Animator) {
                        val fadeOut =
                            ObjectAnimator.ofFloat(tv, "alpha", 1f, 0f).apply {
                                startDelay = 3000
                                duration = 500
                            }
                        fadeOut.addListener(
                            object : AnimatorListenerAdapter() {
                                override fun onAnimationEnd(animation: Animator) {
                                    overlayContainer.removeView(tv)
                                }
                            },
                        )
                        fadeOut.start()
                    }
                },
            )
            fadeIn.start()
        }
    }

    // ── Track name overlay (called from JNI) ────────────────
    @Suppress("unused")
    fun showTrackName(name: String) {
        showOverlayLine(name)
    }

    // ── Level name overlay (called from JNI) ────────────────
    @Suppress("unused")
    fun showLevelName(name: String) {
        showOverlayLine(name)
    }

    // ── GameSurfaceView with InputConnection for soft keyboard ──
    private inner class GameSurfaceView(
        context: Context,
    ) : SurfaceView(context) {
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
    private inner class GameInputConnection(
        view: View,
    ) : BaseInputConnection(view, false) {
        override fun setComposingText(
            text: CharSequence,
            newCursorPosition: Int,
        ): Boolean {
            // Some IMEs still compose even with NO_SUGGESTIONS.
            // Finish composition immediately and commit the text so each
            // character appears in the game without waiting for a space.
            finishComposingText()
            return commitText(text, newCursorPosition)
        }

        override fun commitText(
            text: CharSequence,
            newCursorPosition: Int,
        ): Boolean {
            for (c in text) {
                nativeTextInput(c.code)
            }
            return true
        }

        override fun deleteSurroundingText(
            beforeLength: Int,
            afterLength: Int,
        ): Boolean {
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
