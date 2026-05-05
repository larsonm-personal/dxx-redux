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
import com.dxxredux.app.multiplayer.MatchmakingService
import org.json.JSONObject
import java.io.File

internal fun shouldShowTouchOverlay(
    inGame: Boolean,
    overlayEnabled: Boolean,
    playerDead: Boolean,
    endlevel: Boolean,
    automap: Boolean,
    settingsTrayVisible: Boolean,
): Boolean {
    val gameplayOverlay = overlayEnabled && !playerDead && !endlevel && (inGame || settingsTrayVisible)
    return gameplayOverlay || automap
}

internal fun settingsTrayVisibleForOverlay(
    adminTrayOpen: Boolean,
    adminTrayPausedGame: Boolean,
    adminTrayCloseGraceActive: Boolean,
): Boolean = adminTrayOpen || adminTrayPausedGame || adminTrayCloseGraceActive

internal fun shouldHideStandaloneAdminOverlays(
    inGame: Boolean,
    settingsTrayVisible: Boolean,
): Boolean = !inGame && !settingsTrayVisible

internal fun shouldEnableNetStatsControl(
    isMultiplayerGame: Boolean,
    hasPendingLaunchInfo: Boolean,
): Boolean = isMultiplayerGame || hasPendingLaunchInfo

internal fun shouldEnableNetEventsControl(
    isMultiplayerGame: Boolean,
    hasPendingLaunchInfo: Boolean,
): Boolean = shouldEnableNetStatsControl(isMultiplayerGame, hasPendingLaunchInfo)

// Keep text-only flags off numeric editors so number-pad IMEs still commit digits
internal fun buildKeyboardEditorInputType(baseInputType: Int): Int =
    when (baseInputType and InputType.TYPE_MASK_CLASS) {
        InputType.TYPE_CLASS_NUMBER -> {
            baseInputType
        }

        else -> {
            baseInputType or
                InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
                InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
        }
    }

internal fun shouldUseControllerSettingsTrayShortcuts(
    gamepadOnlyMode: Boolean,
    touchOverlayActive: Boolean,
    automapActive: Boolean,
    adminTrayOpen: Boolean,
    adminTrayPausedGame: Boolean,
): Boolean = gamepadOnlyMode || adminTrayOpen || adminTrayPausedGame || (touchOverlayActive && !automapActive)

internal fun shouldDispatchGamepadButtonDown(
    isInGame: Boolean,
    repeatCount: Int,
    edgeDispatchAllowed: Boolean,
): Boolean = if (isInGame) edgeDispatchAllowed else repeatCount == 0

internal fun shouldDispatchGamepadButtonUp(
    isInGame: Boolean,
    edgeDispatchAllowed: Boolean,
): Boolean = if (isInGame) edgeDispatchAllowed else true

class MainActivity :
    Activity(),
    SurfaceHolder.Callback {
    companion object {
        private const val ADMIN_TRAY_CLOSE_GRACE_MS = 400L

        // Library is loaded dynamically in onCreate based on intent extra

        /** First virtual joystick button index for D-pad directions (Up=+0, Down=+1, Left=+2, Right=+3).
         *  Must match DPAD_BUTTON_BASE in joy.c. */
        const val DPAD_JOY_BUTTON_BASE = 22

        /** Static check if main() is already executing (guards double-launch). */
        @JvmStatic
        external fun nativeIsGameRunning(): Boolean
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

    external fun nativeSetAutomationStartStep(step: Int)

    external fun nativeSetMusicGain(gainDb: Float)

    external fun nativeSetMusicVoices(maxVoices: Int)

    external fun nativeGetConsoleSince(sinceSeq: Long): String

    external fun nativeSetDebugFlag(
        name: String,
        value: Int,
    )

    external fun nativeSetGraphicsOption(
        name: String,
        value: Int,
    )

    external fun nativeSetCoopIndicatorOptions(
        showNearestPlayerLine: Boolean,
        showGuidebotLine: Boolean,
    )

    external fun nativeSetDebugLogEnabled(
        category: Int,
        on: Boolean,
    )

    external fun nativeJoystickAxis(
        axis: Int,
        value: Float,
        touchActive: Boolean,
    )

    external fun nativeJoystickButton(
        button: Int,
        pressed: Int,
    )

    external fun nativeIsInGame(): Boolean

    external fun nativeSetJoystickEnabled(enabled: Boolean)

    external fun nativeIsAutomapActive(): Boolean

    external fun nativeIsSkippableScreen(): Boolean

    external fun nativeIsIntroActive(): Boolean

    external fun nativeSetSkipIntroMovie(enabled: Boolean)

    external fun nativeIsSaveLoadMenuActive(): Boolean

    external fun nativeIsPlayerDead(): Boolean

    external fun nativeIsEndlevelSequence(): Boolean

    external fun nativeIsHostSelectingPlayers(): Boolean

    external fun nativeIsLevelCompleteActive(): Boolean

    /** Returns the callsign of a player requesting to join, or "" if none. */
    external fun nativeGetJoinRequest(): String

    /** Accept the pending join request (equivalent to pressing F6). */
    external fun nativeAcceptJoinRequest()

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

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (::skipButton.isInitialized && skipButton.handleGlobalTouch(event)) {
            return true
        }
        return super.dispatchTouchEvent(event)
    }

    // ── Admin tray (android_input.c) ────────────────────────────────
    external fun nativeCycleCockpit(direction: Int)

    external fun nativeToggleAutoLeveling()

    external fun nativeGetAutoLeveling(): Boolean

    external fun nativeGetCockpitMode(): Int

    external fun nativeOpenSinglePlayerPauseIfSafe(): Boolean

    external fun nativeClosePauseIfFront(): Boolean

    external fun nativeOpenSaveMenuIfSafe(): Boolean

    external fun nativeOpenLoadMenuIfSafe(): Boolean

    external fun nativeOpenGameMenuIfSafe(): Boolean

    // ── Music track control (jni_music_control.c) ────────────────────
    external fun nativeNextTrack(): Int

    external fun nativePrevTrack(): Int

    external fun nativePlaySpecificTrack(track: Int): Int

    external fun nativeGetTrackName(track: Int): String

    external fun nativeGetCurrentTrackNum(): Int

    external fun nativeGetNumAudioTracks(): Int

    external fun nativeGetTotalTracks(): Int

    external fun nativeIsAudioTrack(track: Int): Boolean

    // -- Matchmaking auto-join/host (jni_main.c) --
    external fun nativeSetCallsign(callsign: String)

    external fun nativeSetClientId(clientId: String)

    external fun nativeGetMultiplayerPings(): IntArray

    external fun nativeGetMultiplayerPacketStats(): IntArray

    // Netgame state: [game_status, numconnected, max_numplayers, levelnum, gamemode]
    // android port: game state polling for matchmaking server updates
    external fun nativeGetNetgameState(): IntArray

    // Video stats: [fps, total_loaded, hires_count, max_hires_w, max_hires_h,
    //   gl_max_tex_size, tex_memory_kb, render_w, render_h, display_w, display_h]
    // android port: video diagnostics overlay
    external fun nativeGetVideoStats(): IntArray

    // android port: coop QoL overlay -- robot kill stats per player
    external fun nativeGetCoopRobotStats(): IntArray

    // android port: coop QoL overlay -- teammate shields/energy/secondary
    external fun nativeGetTeammateStatus(): IntArray

    // android port: coop QoL -- warp to player
    external fun nativeGetCoopWarpStatus(): IntArray

    external fun nativeGetCoopWarpTargetName(): String

    external fun nativeCoopWarpExecute(): Int

    external fun nativeCoopWarpCycleTarget()

    // android port: coop guidebot multiplayer support
    external fun nativeGetEscortOwnerPlayer(): Int

    external fun nativeIsEscortOwner(): Boolean

    external fun nativeGetEscortOwnerCallsign(): String

    external fun nativeIsBuddyReleased(): Boolean

    external fun nativeSetAutoJoin(
        hostAddr: String,
        hostPort: Int,
        myPort: Int,
    )

    external fun nativeSetAutoHost(
        myPort: Int,
        mission: String,
        mode: Int,
        maxPlayers: Int,
        levelNum: Int,
        difficulty: Int,
        coopQol: Boolean,
    )

    external fun nativeGetCurrentTrackInfo(): String

    external fun nativeGetMusicType(): Int

    external fun nativeGetTrackList(): String

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
    private lateinit var keyboardInputView: KeyboardInputView
    private lateinit var touchOverlay: TouchOverlayView
    private lateinit var skipButton: SkipButtonView
    private lateinit var exitButton: ExitButtonView
    private lateinit var startGameButton: StartGameButtonView
    private lateinit var acceptJoinButton: AcceptJoinButtonView
    private lateinit var overlayContainer: LinearLayout
    private var overlayEnabled = false
    private val overlayPoller = android.os.Handler(android.os.Looper.getMainLooper())
    private var musicPanel: MusicControlPanel? = null
    private var netStatsOverlay: com.dxxredux.app.multiplayer.MultiplayerStatsOverlay? = null
    private var netEventsOverlay: com.dxxredux.app.multiplayer.NetworkEventsOverlay? = null
    private var videoInfoOverlay: VideoInfoOverlay? = null
    private var loadingProgressOverlay: LoadingProgressOverlayView? = null
    private var coopStatsOverlay: CoopStatsOverlay? = null
    private var warpButtonOverlay: WarpButtonOverlay? = null
    private var netEventsManualToggle = false
    private var adminTrayPausedGame = false
    private var adminTrayCloseGraceUntilMs = 0L
    private var isMultiplayerGame = false
    private var imeNavigationDispatchDepth = 0
    private var lastTrackNum = -1 // for detecting track changes in polling
    private var gyroManager: GyroInputManager? = null
    private var activeTouchLayout = TouchLayoutRepository.defaultLayout()
    private var isActivityResumed = false
    private var gameVariantId = "d2" // "d1" or "d2", set in onCreate
    private var lastAppliedGraphicsSettingsGeneration = -1L

    // True when no touchscreen is available (Android TV / gamepad-only)
    private var gamepadOnlyMode = false

    // Controller meta-action bindings: SDL button index -> meta action ID
    private var buttonMetaBindings = mapOf<Int, Int>()

    // D-pad meta-action bindings: DPAD keycode → meta action ID
    private var dpadMetaBindings = mapOf<Int, Int>()

    // Half-axis combiners: (virtualAxis, posSourceAxis, negSourceAxis)
    // Loaded from controller_config.json; used in onGenericMotionEvent()
    private var halfAxisCombiners = emptyList<Triple<Int, Int, Int>>()
    private val rawAxisValues = FloatArray(6) // LX, LY, RX, RY, LT, RT

    // Input mixer: combines button/axis from touch, controller, gyro
    private lateinit var inputMixer: InputMixer

    // Mixer button map: SDL button index → list of kc_joystick action indices
    // Loaded from controller_config.json mixer_button_map_d1/d2
    private var mixerButtonMap = mapOf<Int, List<Int>>()

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

        CrashLog.install(this)
        // Append to the main-process log file if a path was passed, otherwise create new
        val netlogPath = intent.getStringExtra("netlog_path")
        if (netlogPath != null) {
            DebugLog.initAppend(this, netlogPath)
        } else {
            DebugLog.init(this)
        }
        MatchmakingService.setActivity(this)

        // Load the correct game library based on the launcher's selection
        val game = intent.getStringExtra("game") ?: "d2"
        gameVariantId = game
        gamepadOnlyMode =
            !packageManager.hasSystemFeature(
                android.content.pm.PackageManager.FEATURE_TOUCHSCREEN,
            )
        val libName = if (game == "d1") "dxx-redux-d1" else "dxx-redux-d2"
        System.loadLibrary(libName)
        Log.i("MainActivity", "Loaded native library: $libName")
        CrashLog.installNativeHandler(this)

        // Sync C-side per-category enable flags with Kotlin prefs
        syncDebugLogPrefs()

        // Rewrite audio playlist in the game process so SAF fds are valid.
        // SetupActivity runs in the default process; this activity runs in
        // :game.  PFDs opened there have fd numbers that don't exist here.
        AudioSourceManager(filesDir).writePlaylist(contentResolver)

        // Check for multiplayer auto-join/host from the matchmaking lobby
        val mpMode = intent.getStringExtra("mp_mode")
        isMultiplayerGame = mpMode != null
        resetSinglePlayerNetEventsIfNeeded()
        // Seed game-process MatchmakingStateHolder so overlay shows "CONNECTED" not "DISCONNECTED"
        if (mpMode != null) {
            com.dxxredux.app.multiplayer.MatchmakingStateHolder.update {
                it.copy(status = com.dxxredux.app.multiplayer.ConnectionStatus.CONNECTED)
            }
        }
        if (mpMode != null) {
            val callsign = intent.getStringExtra("mp_callsign") ?: ""
            if (callsign.isNotEmpty()) {
                nativeSetCallsign(callsign)
            }
            nativeSetClientId(
                com.dxxredux.app.multiplayer.ClientIdentity
                    .getInstallationId(this),
            )
        }
        if (mpMode == "join") {
            val hostAddr = intent.getStringExtra("mp_host_addr") ?: "127.0.0.1"
            val hostPort = intent.getIntExtra("mp_host_port", 42430)
            val myPort = intent.getIntExtra("mp_my_port", 42424)
            nativeSetAutoJoin(hostAddr, hostPort, myPort)
        } else if (mpMode == "host") {
            val myPort = intent.getIntExtra("mp_my_port", 42424)
            val mission = intent.getStringExtra("mp_mission") ?: "descent2"
            val mode = intent.getIntExtra("mp_game_mode", 0)
            val maxPlayers = intent.getIntExtra("mp_max_players", 4)
            val levelNum = intent.getIntExtra("mp_level_num", 1)
            val difficulty = intent.getIntExtra("mp_difficulty", 1)
            val coopQol = intent.getBooleanExtra("mp_coop_qol", true)
            nativeSetAutoHost(myPort, mission, mode, maxPlayers, levelNum, difficulty, coopQol)
        }

        // Start foreground service during multiplayer to prevent process kill
        if (mpMode != null) {
            com.dxxredux.app.multiplayer.MultiplayerForegroundService
                .start(this)
        }

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
        applySkipIntroPref(prefs)
        applyCoopIndicatorPrefs(prefs)

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
        keyboardInputView =
            KeyboardInputView(this).apply {
                isFocusable = true
                isFocusableInTouchMode = true
                alpha = 0f
                isCursorVisible = false
                setBackgroundColor(Color.TRANSPARENT)
                setTextColor(Color.TRANSPARENT)
                highlightColor = Color.TRANSPARENT
            }

        // Handle touch on the SurfaceView so coordinates are view-relative
        gameSurfaceView.setOnTouchListener { view, event ->
            handleTouch(view, event)
        }

        // Touch overlay
        touchOverlay = TouchOverlayView(this)
        activeTouchLayout = TouchLayoutRepository.load(this)
        touchOverlay.setLayout(activeTouchLayout)

        // Input mixer: combines button/axis inputs from touch, controller, gyro
        inputMixer =
            InputMixer(
                buttonCallback = { btn, pressed ->
                    nativeJoystickButton(TouchBindings.MIXER_BTN_BASE + btn, pressed)
                },
                axisCallback = { axis, value, touchActive -> nativeJoystickAxis(axis, value, touchActive) },
            )
        touchOverlay.inputMixer = inputMixer
        touchOverlay.axisCallback = { axis, value ->
            inputMixer.setAxis(axis, "touch", value)
        }

        applyGyroConfig(activeTouchLayout.gyro)
        touchOverlay.metaActionCallback = { actionId, pressed ->
            dispatchMetaAction(actionId, pressed)
        }
        touchOverlay.keyCallback = { action, keyCode, unicode ->
            nativeKeyEvent(action, keyCode, unicode)
        }
        touchOverlay.gameVariant = game
        touchOverlay.gamepadOnlyMode = gamepadOnlyMode
        touchOverlay.isEscortOwnerProvider = {
            try {
                nativeIsEscortOwner()
            } catch (_: Exception) {
                true // default to showing Guide controls
            }
        }
        touchOverlay.escortOwnerCallsignProvider = {
            try {
                nativeGetEscortOwnerCallsign()
            } catch (_: Exception) {
                ""
            }
        }
        touchOverlay.isBuddyReleasedProvider = {
            try {
                nativeIsBuddyReleased()
            } catch (_: Exception) {
                true // default to showing Guide controls
            }
        }
        touchOverlay.cheatCodeCallback = { code ->
            for (ch in code) nativeTextInput(ch.code)
        }
        touchOverlay.adminTrayOpenedCallback = { syncAdminTrayPause(open = true) }
        touchOverlay.adminTrayClosedCallback = { syncAdminTrayPause(open = false) }
        touchOverlay.adminTrayToggleStateProvider = { action ->
            when (action) {
                TouchOverlayView.ADMIN_NET_STATS -> netStatsOverlay?.visibility == View.VISIBLE
                TouchOverlayView.ADMIN_NET_EVENTS -> netEventsManualToggle
                TouchOverlayView.ADMIN_VIDEO_INFO -> videoInfoOverlay?.visibility == View.VISIBLE
                else -> false
            }
        }
        touchOverlay.adminTrayEnabledStateProvider = { action ->
            when (action) {
                TouchOverlayView.ADMIN_NET_STATS -> isNetStatsControlEnabled()
                TouchOverlayView.ADMIN_NET_EVENTS -> isNetEventsControlEnabled()
                else -> true
            }
        }
        touchOverlay.adminTrayCallback = { action ->
            when (action) {
                TouchOverlayView.ADMIN_INCREASE_VIEW -> {
                    nativeCycleCockpit(1)
                }

                TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL -> {
                    nativeToggleAutoLeveling()
                }

                TouchOverlayView.ADMIN_QUICK_SAVE -> {
                    openSaveLoadMenu(openSave = true)
                }

                TouchOverlayView.ADMIN_QUICK_LOAD -> {
                    openSaveLoadMenu(openSave = false)
                }

                TouchOverlayView.ADMIN_OPEN_MENU -> {
                    openGameMenuSafely()
                }

                TouchOverlayView.ADMIN_NET_STATS -> {
                    if (!isNetStatsControlEnabled()) {
                        resetSinglePlayerNetStatsIfNeeded()
                    } else {
                        netStatsOverlay?.toggle()
                    }
                }

                TouchOverlayView.ADMIN_NET_EVENTS -> {
                    if (!isNetEventsControlEnabled()) {
                        resetSinglePlayerNetEventsIfNeeded()
                    } else {
                        netEventsManualToggle = !netEventsManualToggle
                        if (netEventsManualToggle) netEventsOverlay?.show() else netEventsOverlay?.hide()
                    }
                }

                TouchOverlayView.ADMIN_EXIT_LAUNCHER -> {
                    NativeMetaActions.nativeMetaAction(TouchBindings.META_RETURN_TO_LAUNCHER, 1)
                }

                TouchOverlayView.ADMIN_VIDEO_INFO -> {
                    videoInfoOverlay?.toggle()
                }

                TouchOverlayView.ADMIN_AUTOMAP -> {
                    nativeKeyEvent(0, KeyEvent.KEYCODE_TAB, '\t'.code)
                    nativeKeyEvent(1, KeyEvent.KEYCODE_TAB, 0)
                }

                TouchOverlayView.ADMIN_HEADLIGHT -> {
                    nativeKeyEvent(0, KeyEvent.KEYCODE_H, 'h'.code)
                    nativeKeyEvent(1, KeyEvent.KEYCODE_H, 0)
                }

                TouchOverlayView.ADMIN_WARP -> {
                    try {
                        nativeCoopWarpExecute()
                    } catch (_: Exception) {
                    }
                }

                TouchOverlayView.ADMIN_MUSIC -> {
                    showMusicPanel()
                }

                TouchOverlayView.ADMIN_ACCEPT_JOIN -> {
                    try {
                        nativeAcceptJoinRequest()
                    } catch (_: Exception) {
                    }
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
        if (gamepadOnlyMode) {
            touchOverlay.adminTrayWarpLabelProvider = {
                try {
                    val st = nativeGetCoopWarpStatus()
                    if (st.isNotEmpty() && st[0] != 0) {
                        val name = nativeGetCoopWarpTargetName()
                        if (name.isNotEmpty()) "Warp: $name" else "Warp"
                    } else {
                        "Warp: --"
                    }
                } catch (_: Exception) {
                    "Warp: --"
                }
            }
            touchOverlay.adminTrayAcceptLabelProvider = {
                try {
                    val cs = nativeGetJoinRequest()
                    if (cs.isNotEmpty()) "Accept: $cs" else "Accept: --"
                } catch (_: Exception) {
                    "Accept: --"
                }
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
                skipEveryLaunchCallback = {
                    persistSkipIntroMoviePreference()
                    nativeSetSkipIntroMovie(true)
                }
                visibility = View.GONE
            }

        // Always-visible exit button (upper-left, returns to launcher from any screen)
        exitButton =
            ExitButtonView(this).apply {
                exitCallback = {
                    try {
                        NativeMetaActions.nativeMetaAction(TouchBindings.META_RETURN_TO_LAUNCHER, 1)
                    } catch (_: Exception) {
                        // Native side is dead or dying (e.g. Error() was called during init).
                        // Kill the process directly so the user isn't stuck on a frozen screen.
                        android.os.Process.killProcess(android.os.Process.myPid())
                    }
                }
            }

        // "START GAME" button for host player selection screen (hidden by default)
        startGameButton =
            StartGameButtonView(this).apply {
                keyCallback = { action, keyCode, unicode -> nativeKeyEvent(action, keyCode, unicode) }
                visibility = View.GONE
            }

        // "ACCEPT: callsign" button for mid-game join requests (hidden by default)
        acceptJoinButton =
            AcceptJoinButtonView(this).apply {
                acceptCallback = {
                    try {
                        nativeAcceptJoinRequest()
                    } catch (_: Exception) {
                    }
                }
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
            keyboardInputView,
            FrameLayout.LayoutParams(
                1,
                1,
                Gravity.TOP or Gravity.START,
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
        frame.addView(
            exitButton,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
        frame.addView(
            startGameButton,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
        // In gamepad-only mode, accept/warp are handled via admin tray items
        if (!gamepadOnlyMode) {
            frame.addView(
                acceptJoinButton,
                FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT,
                ),
            )
        }
        frame.addView(overlayContainer, overlayLp)

        // Network stats overlay (hidden by default, toggled via admin tray)
        val statsOverlay =
            com.dxxredux.app.multiplayer.MultiplayerStatsOverlay(this).apply {
                visibility = View.GONE
                isLan = intent.getBooleanExtra("mp_is_lan", false)
                pingProvider = {
                    try {
                        nativeGetMultiplayerPings()
                    } catch (_: Exception) {
                        null
                    }
                }
                packetStatsProvider = {
                    try {
                        nativeGetMultiplayerPacketStats()
                    } catch (_: Exception) {
                        null
                    }
                }
                proxyStatsProvider = {
                    com.dxxredux.app.multiplayer.MatchmakingService
                        .getProxyStats()
                }
                connectionInfoProvider = {
                    com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value.connectionInfo
                }
                localIp = resolveLocalIp()
            }
        netStatsOverlay = statsOverlay
        frame.addView(
            statsOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

        // Network events overlay (hidden by default, toggled via admin tray)
        val eventsOverlay =
            com.dxxredux.app.multiplayer.NetworkEventsOverlay(this).apply {
                visibility = View.GONE
                stateProvider = {
                    com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value
                }
            }
        netEventsOverlay = eventsOverlay
        frame.addView(
            eventsOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

        // Video info overlay (hidden by default, toggled via admin tray)
        val vidOverlay =
            VideoInfoOverlay(this).apply {
                visibility = View.GONE
                statsProvider = {
                    try {
                        nativeGetVideoStats()
                    } catch (_: Exception) {
                        null
                    }
                }
                debugFlagSetter = { name, value ->
                    try {
                        nativeSetDebugFlag(name, value)
                    } catch (_: Exception) {
                        // JNI not ready yet
                    }
                }
                graphicsOptionSetter = { name, value ->
                    try {
                        nativeSetGraphicsOption(name, value)
                    } catch (_: Exception) {
                        // JNI not ready yet
                    }
                }
            }
        videoInfoOverlay = vidOverlay
        frame.addView(
            vidOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
        applyGraphicsDebugPrefs(prefs)

        // android port: coop QoL overlay -- robot kill stats + teammate status
        val coopOverlay =
            CoopStatsOverlay(this).apply {
                visibility = View.GONE
                robotStatsProvider = {
                    try {
                        nativeGetCoopRobotStats()
                    } catch (_: Exception) {
                        null
                    }
                }
                teammateStatusProvider = {
                    try {
                        nativeGetTeammateStatus()
                    } catch (_: Exception) {
                        null
                    }
                }
                escortOwnerProvider = {
                    try {
                        nativeGetEscortOwnerPlayer()
                    } catch (_: Exception) {
                        -1
                    }
                }
            }
        coopStatsOverlay = coopOverlay
        frame.addView(
            coopOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

        // android port: coop QoL -- warp-to-player button overlay
        val warpOverlay =
            WarpButtonOverlay(this).apply {
                visibility = View.GONE
                warpStatusProvider = {
                    try {
                        nativeGetCoopWarpStatus()
                    } catch (_: Exception) {
                        null
                    }
                }
                warpTargetNameProvider = {
                    try {
                        nativeGetCoopWarpTargetName()
                    } catch (_: Exception) {
                        null
                    }
                }
                warpExecuteCallback = {
                    try {
                        nativeCoopWarpExecute()
                    } catch (_: Exception) {
                        0
                    }
                }
                warpCycleCallback = {
                    try {
                        nativeCoopWarpCycleTarget()
                    } catch (_: Exception) {
                    }
                }
            }
        warpButtonOverlay = warpOverlay
        if (!gamepadOnlyMode) {
            frame.addView(
                warpOverlay,
                FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT,
                ),
            )
        }

        val progressOverlay = LoadingProgressOverlayView(this)
        loadingProgressOverlay = progressOverlay
        frame.addView(
            progressOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

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
                    val imeHeight =
                        sampleKeyboardHeightPx(
                            imeBottomHint = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom,
                        )
                    if (imeHeight > 0) {
                        nativeSetKeyboardHeight(imeHeight, keyboardReferenceHeightPx())
                    }
                    return insets
                }

                override fun onEnd(animation: WindowInsetsAnimationCompat) {
                    val imeHeight = sampleKeyboardHeightPx()
                    nativeSetKeyboardHeight(imeHeight, keyboardReferenceHeightPx())
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

            // If launched with automation intent extras (from launcher script
            // executor), load the script automatically once the engine starts
            if (BuildConfig.DEBUG) {
                val autoScript = intent.getStringExtra("automation_script")
                val autoStep = intent.getIntExtra("automation_start_step", 0)
                if (!autoScript.isNullOrEmpty()) {
                    val resolved =
                        if (autoScript.startsWith("/")) {
                            autoScript
                        } else {
                            filesDir.absolutePath + "/" + autoScript
                        }
                    Log.i(
                        "DXX-Automate",
                        "Intent automation: script=$resolved start_step=$autoStep",
                    )
                    nativeSetAutomationStartStep(autoStep)
                    nativeLoadAutomationScript(resolved)
                }
            }

            // AF/MSAA now persist in descent.cfg and are loaded by
            // ReadConfigFile() -> ogl_aniso_level / ogl_msaa_samples

            Thread {
                startGame()
            }.start()

            // android port: coop QoL -- begin polling (auto-shows in coop)
            coopStatsOverlay?.startPolling()
            warpButtonOverlay?.startPolling()
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
        isActivityResumed = false
        gyroManager?.pause()
        overlayPoller.removeCallbacksAndMessages(null)
        gamepadButtonEdgeTracker.clear()
        // Inject Escape so the engine opens its pause / game menu.
        // This pauses a single-player game while the app is in the background.
        if (gameStarted) {
            nativeOnPause()
        }
    }

    override fun onResume() {
        super.onResume()
        isActivityResumed = true
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
        syncDebugLogPrefs()
        applySkipIntroPref(prefs)
        applyCoopIndicatorPrefs(prefs)
        applyGraphicsDebugPrefs(prefs)
        applyGraphicsSettingsPrefs(prefs)
        // Start polling in-game state to show/hide overlay
        startOverlayPolling()
    }

    private fun syncDebugLogPrefs() {
        for (cat in 0 until DebugLogCategory.COUNT) {
            val enabled = DebugLog.isCategoryEnabled(this, cat)
            DebugLog.setCategoryEnabled(this, cat, enabled)
            nativeSetDebugLogEnabled(cat, enabled)
        }
    }

    private fun applyGraphicsDebugPrefs(prefs: android.content.SharedPreferences) {
        videoInfoOverlay?.applyLauncherPrefs(prefs)
        try {
            nativeSetDebugFlag(
                "merged_wall_experiment",
                if (prefs.getBoolean(PREF_FORCE_LEGACY_MERGED_WALL_TEXMERGE, false)) {
                    MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE_VALUE
                } else {
                    0
                },
            )
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    private fun applyGraphicsSettingsPrefs(prefs: android.content.SharedPreferences) {
        val generation = prefs.getLong(PREF_GRAPHICS_SETTINGS_GENERATION, 0L)
        if (generation == lastAppliedGraphicsSettingsGeneration) return
        lastAppliedGraphicsSettingsGeneration = generation
        if (!gameStarted) return

        fun cfgInt(key: String): Int? = readConfigValueForGame(filesDir, gameVariantId, key)?.toIntOrNull()
        try {
            cfgInt("TexFilt")?.let { nativeSetGraphicsOption("tex_filt", it) }
            cfgInt("MenuTexFilt")?.let { nativeSetGraphicsOption("menu_tex_filt", it) }
            cfgInt("HudTexFilt")?.let { nativeSetGraphicsOption("hud_tex_filt", it) }
            cfgInt("AnisoLevel")?.let { nativeSetGraphicsOption("aniso_level", it) }
            cfgInt("MsaaLevel")?.let { nativeSetGraphicsOption("msaa_level", it) }
            cfgInt("ClassicDepth")?.let { nativeSetGraphicsOption("classic_depth", it) }
            if (gameVariantId == "d2") cfgInt("MovieTexFilt")?.let { nativeSetGraphicsOption("movie_tex_filt", it) }
            if (prefs.contains(PREF_GRAPHICS_ALPHA_EFFECTS)) {
                nativeSetGraphicsOption(
                    "alpha_effects",
                    if (prefs.getBoolean(PREF_GRAPHICS_ALPHA_EFFECTS, false)) 1 else 0,
                )
            }
            if (prefs.contains(PREF_GRAPHICS_DYNLIGHT_COLOR)) {
                nativeSetGraphicsOption(
                    "dynlight_color",
                    if (prefs.getBoolean(PREF_GRAPHICS_DYNLIGHT_COLOR, false)) 1 else 0,
                )
            }
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    private fun applySkipIntroPref(prefs: android.content.SharedPreferences) {
        try {
            nativeSetSkipIntroMovie(prefs.getBoolean(PREF_SKIP_INTRO_MOVIE, false))
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    private fun applyCoopIndicatorPrefs(prefs: android.content.SharedPreferences) {
        try {
            nativeSetCoopIndicatorOptions(
                prefs.getBoolean(PREF_NEAREST_PLAYER_LINE, true),
                prefs.getBoolean(PREF_GUIDEBOT_HELPER_LINE, true),
            )
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    private fun syncAdminTrayPause(open: Boolean) {
        if (open) {
            adminTrayCloseGraceUntilMs = 0L
            if (adminTrayPausedGame) return
            adminTrayPausedGame =
                try {
                    nativeOpenSinglePlayerPauseIfSafe()
                } catch (_: Exception) {
                    false
                }
            return
        }

        if (!adminTrayPausedGame) return
        try {
            nativeClosePauseIfFront()
        } catch (_: Exception) {
        }
        adminTrayPausedGame = false
        adminTrayCloseGraceUntilMs = android.os.SystemClock.uptimeMillis() + ADMIN_TRAY_CLOSE_GRACE_MS
    }

    private fun isAdminTrayCloseGraceActive(nowMs: Long = android.os.SystemClock.uptimeMillis()): Boolean =
        nowMs < adminTrayCloseGraceUntilMs

    private fun isNetEventsControlEnabled(): Boolean {
        val mpState = com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value
        return shouldEnableNetEventsControl(
            isMultiplayerGame = isMultiplayerGame,
            hasPendingLaunchInfo = mpState.gameLaunchInfo != null,
        )
    }

    private fun isNetStatsControlEnabled(): Boolean {
        val mpState = com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value
        return shouldEnableNetStatsControl(
            isMultiplayerGame = isMultiplayerGame,
            hasPendingLaunchInfo = mpState.gameLaunchInfo != null,
        )
    }

    private fun resetSinglePlayerNetStatsIfNeeded() {
        if (isNetStatsControlEnabled()) return
        netStatsOverlay?.hide()
    }

    private fun resetSinglePlayerNetEventsIfNeeded() {
        if (isNetEventsControlEnabled()) return
        netEventsManualToggle = false
        netEventsOverlay?.hide()
    }

    private fun openSaveLoadMenu(openSave: Boolean) {
        val opened =
            try {
                if (openSave) nativeOpenSaveMenuIfSafe() else nativeOpenLoadMenuIfSafe()
            } catch (_: Exception) {
                false
            }
        if (opened) adminTrayPausedGame = false
    }

    private fun openGameMenuSafely() {
        if (adminTrayPausedGame) {
            try {
                nativeClosePauseIfFront()
            } catch (_: Exception) {
            }
            adminTrayPausedGame = false
        }
        val opened =
            try {
                nativeOpenGameMenuIfSafe()
            } catch (_: Exception) {
                false
            }
        if (!opened) {
            nativeKeyEvent(0, KeyEvent.KEYCODE_ESCAPE, 0)
            nativeKeyEvent(1, KeyEvent.KEYCODE_ESCAPE, 0)
        }
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
                            val introActive =
                                try {
                                    nativeIsIntroActive()
                                } catch (_: Exception) {
                                    false
                                }
                            val playerDead =
                                try {
                                    nativeIsPlayerDead()
                                } catch (_: Exception) {
                                    false
                                }
                            val endlevel =
                                try {
                                    nativeIsEndlevelSequence()
                                } catch (_: Exception) {
                                    false
                                }
                            val saveloadMenu =
                                try {
                                    nativeIsSaveLoadMenuActive()
                                } catch (_: Exception) {
                                    false
                                }
                            val nowMs = android.os.SystemClock.uptimeMillis()
                            val settingsTrayVisible =
                                settingsTrayVisibleForOverlay(
                                    adminTrayOpen = touchOverlay.isAdminTrayOpen(),
                                    adminTrayPausedGame = adminTrayPausedGame,
                                    adminTrayCloseGraceActive = isAdminTrayCloseGraceActive(nowMs),
                                )
                            // During death or endlevel, show skip/continue button instead of controls
                            val showCutsceneButton = playerDead || endlevel || skippable
                            // Keep the overlay visible while the settings tray owns, or is still
                            // unwinding, its pause state so standalone overlays do not flicker off.
                            val shouldShow =
                                shouldShowTouchOverlay(
                                    inGame = inGame,
                                    overlayEnabled = overlayEnabled,
                                    playerDead = playerDead,
                                    endlevel = endlevel,
                                    automap = automap,
                                    settingsTrayVisible = settingsTrayVisible,
                                )
                            val wasActive = touchOverlay.isActive
                            touchOverlay.isActive = shouldShow
                            touchOverlay.automapActive = automap
                            // Show/hide skip button for cutscenes, death, save/load, or level complete
                            val levelComplete =
                                try {
                                    nativeIsLevelCompleteActive()
                                } catch (_: Exception) {
                                    false
                                }
                            if (saveloadMenu) {
                                skipButton.bigLabel = false
                                skipButton.label = "BACK"
                                skipButton.visibility = View.VISIBLE
                            } else if (levelComplete) {
                                skipButton.bigLabel = false
                                skipButton.label = "NEXT"
                                skipButton.visibility = View.VISIBLE
                            } else if (introActive) {
                                skipButton.bigLabel = true
                                skipButton.label = "Skip every launch"
                                skipButton.visibility = View.VISIBLE
                            } else if (showCutsceneButton && !shouldShow) {
                                skipButton.bigLabel = false
                                skipButton.label = if (playerDead) "CONTINUE" else "SKIP"
                                skipButton.visibility = View.VISIBLE
                            } else {
                                skipButton.bigLabel = false
                                skipButton.visibility = View.GONE
                            }
                            // Enable/disable joystick input when overlay state changes
                            if (shouldShow && !wasActive) {
                                nativeSetJoystickEnabled(true)
                            } else if (!shouldShow && wasActive) {
                                nativeSetJoystickEnabled(false)
                            }
                            // Poll current track to update overlay label
                            if (shouldShow && !automap) pollTrackLabel()
                            // Hide standalone exit when touch overlay is active (admin tray has Exit)
                            exitButton.visibility = if (shouldShow) View.GONE else View.VISIBLE
                            if (shouldHideStandaloneAdminOverlays(inGame, settingsTrayVisible)) {
                                netStatsOverlay?.hide()
                                videoInfoOverlay?.hide()
                            }
                            resetSinglePlayerNetStatsIfNeeded()
                            // Show "START GAME" button when host is on player selection screen
                            val hostSelecting =
                                try {
                                    nativeIsHostSelectingPlayers()
                                } catch (_: Exception) {
                                    false
                                }
                            startGameButton.visibility =
                                if (hostSelecting) View.VISIBLE else View.GONE
                            // Show "ACCEPT" button when a player requests to join mid-game
                            val joinCallsign =
                                try {
                                    nativeGetJoinRequest()
                                } catch (_: Exception) {
                                    ""
                                }
                            if (joinCallsign.isNotEmpty()) {
                                acceptJoinButton.visibility = View.GONE
                            }
                            // Auto-show/hide network events overlay during MP phases
                            val mpState = com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value
                            val netEventsEnabled =
                                shouldEnableNetEventsControl(
                                    isMultiplayerGame = isMultiplayerGame,
                                    hasPendingLaunchInfo = mpState.gameLaunchInfo != null,
                                )
                            if (!netEventsEnabled) {
                                resetSinglePlayerNetEventsIfNeeded()
                            } else {
                                val showNetEvents =
                                    netEventsManualToggle ||
                                        hostSelecting ||
                                        (isMultiplayerGame && !inGame) ||
                                        (mpState.gameLaunchInfo != null && !inGame)
                                if (showNetEvents) {
                                    netEventsOverlay?.show()
                                } else {
                                    netEventsOverlay?.hide()
                                }
                            }
                        } catch (_: Exception) {
                            touchOverlay.isActive = false
                            touchOverlay.automapActive = false
                            skipButton.visibility = View.GONE
                            exitButton.visibility = View.VISIBLE
                            startGameButton.visibility = View.GONE
                            acceptJoinButton.visibility = View.GONE
                            // Still try to show net events overlay during MP connecting
                            val mpState2 = com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value
                            if (isMultiplayerGame || mpState2.gameLaunchInfo != null || netEventsManualToggle) {
                                netEventsOverlay?.show()
                            }
                        }
                    } else {
                        if (touchOverlay.isActive) {
                            nativeSetJoystickEnabled(false)
                        }
                        touchOverlay.isActive = false
                        touchOverlay.automapActive = false
                        skipButton.visibility = View.GONE
                        startGameButton.visibility = View.GONE
                        acceptJoinButton.visibility = View.GONE
                        netStatsOverlay?.hide()
                        netEventsOverlay?.hide()
                        videoInfoOverlay?.hide()
                        coopStatsOverlay?.hide()
                        warpButtonOverlay?.stopPolling()
                        netEventsManualToggle = false
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

                    "debug" -> {
                        val field = intent.getStringExtra("field") ?: ""
                        val v = intent.getIntExtra("value", 0)
                        Log.i("DXX-Command", "Setting debug flag $field = $v")
                        nativeSetDebugFlag(field, v)
                    }

                    else -> {
                        Log.w("DXX-Command", "Unknown command: $cmd")
                    }
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
        AudioSourceManager.closeActivePfds()
        com.dxxredux.app.multiplayer.MultiplayerForegroundService
            .stop(this)
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

        val introActive =
            if (gameStarted) {
                try {
                    nativeIsIntroActive()
                } catch (_: Exception) {
                    false
                }
            } else {
                false
            }

        if (introActive && skipButton.handleIntroTouch(event, window.decorView)) {
            return true
        }

        if (skipButton.handleSurfaceFallbackTouch(event)) {
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

    /** Resolve the best local IP for display in the net stats overlay. */
    private fun resolveLocalIp(): String? =
        try {
            val ifaces =
                java.net.NetworkInterface
                    .getNetworkInterfaces()
                    ?.toList() ?: emptyList()
            ifaces
                .filter { it.isUp && !it.isLoopback }
                .flatMap { it.inetAddresses.toList() }
                .filterIsInstance<java.net.Inet4Address>()
                .firstOrNull()
                ?.hostAddress
        } catch (_: Exception) {
            null
        }

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
            if (json.has("meta_bindings")) {
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
            }
            // Half-axis combiners: [[virtualAxis, posSource, negSource], ...]
            if (json.has("half_axis_combiners")) {
                val arr = json.getJSONArray("half_axis_combiners")
                val list = mutableListOf<Triple<Int, Int, Int>>()
                for (i in 0 until arr.length()) {
                    val e = arr.getJSONArray(i)
                    list.add(Triple(e.getInt(0), e.getInt(1), e.getInt(2)))
                }
                halfAxisCombiners = list
            }
            // Mixer button map: SDL button → list of kc_joystick action indices
            val mapKey =
                if (gameVariantId == "d1") "mixer_button_map_d1" else "mixer_button_map_d2"
            if (json.has(mapKey)) {
                val mapObj = json.getJSONObject(mapKey)
                val result = mutableMapOf<Int, List<Int>>()
                for (key in mapObj.keys()) {
                    val sdlBtn = key.toIntOrNull() ?: continue
                    val arr = mapObj.getJSONArray(key)
                    val indices = (0 until arr.length()).map { arr.getInt(it) }
                    result[sdlBtn] = indices
                }
                mixerButtonMap = result
            }
        } catch (e: Exception) {
            Log.w("MainActivity", "Failed to load meta bindings", e)
        }
    }

    private fun ensureGyroManager(): GyroInputManager {
        gyroManager?.let { return it }
        val manager = GyroInputManager(this)
        manager.axisCallback = { axis, value ->
            inputMixer.setAxis(axis, "gyro", value)
        }
        manager.diagnosticCallback = { yaw, pitch, roll ->
            touchOverlay.updateGyroDiagnostic(yaw, pitch, roll)
        }
        touchOverlay.gyroManager = manager
        gyroManager = manager
        return manager
    }

    private fun applyGyroConfig(config: GyroConfig) {
        val manager = ensureGyroManager()
        manager.setConfig(config)
        touchOverlay.updateGyroEnabled(config.enabled)
        if (isActivityResumed) {
            if (config.enabled) {
                manager.resume()
            } else {
                manager.pause()
            }
        }
    }

    private fun setGyroEnabled(enabled: Boolean) {
        if (activeTouchLayout.gyro.enabled == enabled) return
        activeTouchLayout = activeTouchLayout.copy(gyro = activeTouchLayout.gyro.copy(enabled = enabled))
        TouchLayoutRepository.save(this, activeTouchLayout)
        applyGyroConfig(activeTouchLayout.gyro)
    }

    private fun dispatchMetaAction(
        actionId: Int,
        pressed: Boolean,
    ) {
        if (actionId == TouchBindings.META_GYRO_TOGGLE) {
            if (pressed) setGyroEnabled(!activeTouchLayout.gyro.enabled)
            return
        }
        NativeMetaActions.nativeMetaAction(actionId, if (pressed) 1 else 0)
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

    /** Dispatch a d-pad event, using meta action if bound, else mixer.
     *  D-pad virtual button indices: DUp=22, DDown=23, DLeft=24, DRight=25.
     *  Shared constant with joy.c D-pad button registration. */
    private fun dispatchDpad(
        keyCode: Int,
        action: Int,
    ) {
        val pressed = action == 0
        val metaId = dpadMetaBindings[keyCode]
        logGamepadInput(
            "dispatchDpad kc=$keyCode action=${if (pressed) "down" else "up"} meta=${metaId ?: -1}",
        )
        if (metaId != null) {
            dispatchMetaAction(metaId, pressed)
        } else {
            val btnIdx = dpadKeyCodeToJoyButton(keyCode)
            if (btnIdx >= 0) {
                val tag = "ctrl:dpad$keyCode"
                val kcIndices = mixerButtonMap[btnIdx]
                if (kcIndices != null) {
                    for (kc in kcIndices) inputMixer.setButton(kc, tag, pressed)
                }
                // Always fire the virtual joystick button too. In-game menus
                // translate joy buttons 22-25 to KEY_UP/DOWN/LEFT/RIGHT via
                // the ANDROID EVENT_JOYSTICK_BUTTON_DOWN block in newmenu.c.
                // During gameplay this is a no-op because the launcher binds
                // actions via keyboard through the mixer above; joy buttons
                // 22-25 stay unbound in kconfig
                nativeJoystickButton(btnIdx, if (pressed) 1 else 0)
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

    private fun isControllerSource(source: Int): Boolean =
        source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
            source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD

    private fun isImeReroutedEvent(): Boolean = imeNavigationDispatchDepth > 0

    private inline fun dispatchImeNavigationEvent(dispatch: () -> Boolean): Boolean {
        imeNavigationDispatchDepth += 1
        return try {
            dispatch()
        } finally {
            imeNavigationDispatchDepth -= 1
        }
    }

    private fun imeNavigationTargetView(): View {
        if (!keyboardInputView.hasFocus()) {
            keyboardInputView.requestFocus()
        }
        return keyboardInputView
    }

    private fun dispatchImeNavigationKey(reroutedEvent: KeyEvent): Boolean =
        dispatchImeNavigationEvent {
            val targetView = imeNavigationTargetView()
            val viewHandled = targetView.dispatchKeyEvent(reroutedEvent)
            if (viewHandled) {
                true
            } else if (reroutedEvent.keyCode == KeyEvent.KEYCODE_BACK) {
                false
            } else {
                super.dispatchKeyEvent(reroutedEvent)
            }
        }

    private fun imeNavigationSource(keyCode: Int): Int =
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_UP,
            KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_RIGHT,
            KeyEvent.KEYCODE_DPAD_CENTER,
            KeyEvent.KEYCODE_BACK,
            -> InputDevice.SOURCE_KEYBOARD or InputDevice.SOURCE_DPAD

            else -> InputDevice.SOURCE_KEYBOARD
        }

    private fun dispatchImeNavigationKey(
        originalEvent: KeyEvent,
        keyCode: Int = originalEvent.keyCode,
    ): Boolean {
        val reroutedEvent =
            KeyEvent(
                originalEvent.downTime,
                originalEvent.eventTime,
                originalEvent.action,
                keyCode,
                originalEvent.repeatCount,
                originalEvent.metaState,
                originalEvent.deviceId,
                if (keyCode == originalEvent.keyCode) originalEvent.scanCode else 0,
                originalEvent.flags,
                imeNavigationSource(keyCode),
            )
        return dispatchImeNavigationKey(reroutedEvent)
    }

    private fun logGamepadInput(message: String) {
        DebugLog.log(DebugLogCategory.GAME, message)
        Log.d("DXX-Input", message)
    }

    private fun logSelectRouting(message: String) {
        DebugLog.log(DebugLogCategory.GAME, "[select-route] $message")
        Log.d("DXX-Select", message)
    }

    private fun motionAxisDirection(value: Float): Int =
        when {
            value < -0.5f -> -1
            value > 0.5f -> 1
            else -> 0
        }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (gameSurfaceView.keyboardActive) {
            when (event.keyCode) {
                KeyEvent.KEYCODE_BUTTON_A -> {
                    return dispatchImeNavigationKey(event, KeyEvent.KEYCODE_DPAD_CENTER)
                }

                KeyEvent.KEYCODE_BUTTON_SELECT -> {
                    return dispatchImeNavigationKey(event, KeyEvent.KEYCODE_DPAD_CENTER)
                }

                KeyEvent.KEYCODE_BUTTON_B,
                KeyEvent.KEYCODE_BACK,
                -> {
                    if (event.action == KeyEvent.ACTION_DOWN) {
                        hideKeyboard()
                    }
                    return true
                }

                KeyEvent.KEYCODE_DPAD_UP,
                KeyEvent.KEYCODE_DPAD_DOWN,
                KeyEvent.KEYCODE_DPAD_LEFT,
                KeyEvent.KEYCODE_DPAD_RIGHT,
                KeyEvent.KEYCODE_DPAD_CENTER,
                -> {
                    return super.dispatchKeyEvent(event)
                }

                KeyEvent.KEYCODE_ENTER,
                KeyEvent.KEYCODE_NUMPAD_ENTER,
                -> {
                    if (isControllerSource(event.source)) {
                        return dispatchImeNavigationKey(event, KeyEvent.KEYCODE_DPAD_CENTER)
                    }
                    return super.dispatchKeyEvent(event)
                }
            }
            if (gamepadButtonIndex(event.keyCode) >= 0 || isControllerSource(event.source)) {
                return true
            }
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onKeyDown(
        keyCode: Int,
        event: KeyEvent,
    ): Boolean {
        if (!isImeReroutedEvent() &&
            (
                isControllerSource(event.source) ||
                    dpadKeyCodeToJoyButton(keyCode) >= 0 ||
                    gamepadButtonIndex(keyCode) >= 0
            )
        ) {
            logGamepadInput(
                "onKeyDown kc=$keyCode src=${event.source} repeat=${event.repeatCount} " +
                    "admin=${touchOverlay.isAdminTrayOpen()} focus=${gameSurfaceView.hasFocus()}",
            )
        }
        // Let the system handle volume keys
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyDown(keyCode, event)
        }

        if (isImeReroutedEvent()) {
            return super.onKeyDown(keyCode, event)
        }

        if (gameSurfaceView.keyboardActive &&
            (
                isControllerSource(event.source) ||
                    dpadKeyCodeToJoyButton(keyCode) >= 0 ||
                    gamepadButtonIndex(keyCode) >= 0
            )
        ) {
            return true
        }

        // Music admin panel: dismiss on BACK / ESC / B / Y before any other
        // gamepad handling. The panel itself only handles touch so without
        // this hook it traps input when opened by the admin tray
        if (musicPanel != null && isMusicPanelDismissKey(keyCode)) {
            dismissMusicPanel()
            return true
        }

        val controllerSettingsTrayShortcuts =
            shouldUseControllerSettingsTrayShortcuts(
                gamepadOnlyMode = gamepadOnlyMode,
                touchOverlayActive = touchOverlay.isActive,
                automapActive = touchOverlay.automapActive,
                adminTrayOpen = touchOverlay.isAdminTrayOpen(),
                adminTrayPausedGame = adminTrayPausedGame,
            )

        if (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT ||
            keyCode == KeyEvent.KEYCODE_BUTTON_START ||
            keyCode == KeyEvent.KEYCODE_BACK
        ) {
            logSelectRouting(
                "down kc=$keyCode src=${event.source} shortcuts=$controllerSettingsTrayShortcuts " +
                    "tray=${touchOverlay.isAdminTrayOpen()} overlay=${touchOverlay.isActive} " +
                    "automap=${touchOverlay.automapActive} inGame=${nativeIsInGame()} focus=${gameSurfaceView.hasFocus()}",
            )
        }

        // When the touch settings tray is reachable from controller input,
        // Select opens it and Start opens the engine's game menu.
        if (controllerSettingsTrayShortcuts) {
            if (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT) {
                logSelectRouting("down select -> openAdminTray")
                touchOverlay.openAdminTray(fromGamepad = true)
                return true
            }
            if (keyCode == KeyEvent.KEYCODE_BUTTON_START) {
                logSelectRouting("down start -> openGameMenu")
                if (touchOverlay.isAdminTrayOpen()) {
                    touchOverlay.closeAdminTray()
                }
                openGameMenuSafely()
                return true
            }
        }

        if (keyCode == KeyEvent.KEYCODE_BACK && isControllerSource(event.source)) {
            logSelectRouting("down back-controller -> fallthrough nativeKeyEvent")
        }

        // Route D-pad/A/B to the admin tray while it is open.
        if (touchOverlay.isAdminTrayOpen()) {
            if (touchOverlay.handleAdminTrayGamepadKey(keyCode, 0)) return true
        }

        // Gamepad face / shoulder buttons -> mixer or meta action
        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
            val inGame = nativeIsInGame()
            if (!shouldDispatchGamepadButtonDown(
                    inGame,
                    event.repeatCount,
                    gamepadButtonEdgeTracker.shouldDispatchDown(keyCode, event.repeatCount),
                )
            ) {
                return true
            }
            val metaId = buttonMetaBindings[joyBtn]
            if (metaId != null) {
                dispatchMetaAction(metaId, true)
            } else {
                val tag = "ctrl:btn$joyBtn"
                val kcIndices = mixerButtonMap[joyBtn]
                if (kcIndices != null) {
                    for (kc in kcIndices) inputMixer.setButton(kc, tag, true)
                }
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
        if (!isImeReroutedEvent() &&
            (
                isControllerSource(event.source) ||
                    dpadKeyCodeToJoyButton(keyCode) >= 0 ||
                    gamepadButtonIndex(keyCode) >= 0
            )
        ) {
            logGamepadInput(
                "onKeyUp kc=$keyCode src=${event.source} admin=${touchOverlay.isAdminTrayOpen()} " +
                    "focus=${gameSurfaceView.hasFocus()}",
            )
        }
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyUp(keyCode, event)
        }

        if (isImeReroutedEvent()) {
            return super.onKeyUp(keyCode, event)
        }

        if (gameSurfaceView.keyboardActive &&
            (
                isControllerSource(event.source) ||
                    dpadKeyCodeToJoyButton(keyCode) >= 0 ||
                    gamepadButtonIndex(keyCode) >= 0
            )
        ) {
            return true
        }

        // Swallow the key-up for music-panel dismiss keys so the event does
        // not bubble to the game after the panel closed in onKeyDown
        if (isMusicPanelDismissKey(keyCode)) {
            return true
        }

        val controllerSettingsTrayShortcuts =
            shouldUseControllerSettingsTrayShortcuts(
                gamepadOnlyMode = gamepadOnlyMode,
                touchOverlayActive = touchOverlay.isActive,
                automapActive = touchOverlay.automapActive,
                adminTrayOpen = touchOverlay.isAdminTrayOpen(),
                adminTrayPausedGame = adminTrayPausedGame,
            )

        if (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT ||
            keyCode == KeyEvent.KEYCODE_BUTTON_START ||
            keyCode == KeyEvent.KEYCODE_BACK
        ) {
            logSelectRouting(
                "up kc=$keyCode src=${event.source} shortcuts=$controllerSettingsTrayShortcuts " +
                    "tray=${touchOverlay.isAdminTrayOpen()} overlay=${touchOverlay.isActive} inGame=${nativeIsInGame()}",
            )
        }

        // Consume controller events while the admin tray is open.
        if (touchOverlay.isAdminTrayOpen()) {
            if (touchOverlay.handleAdminTrayGamepadKey(keyCode, 1)) return true
        }

        // Consume Start/Select up events when they are routed as tray shortcuts.
        if (controllerSettingsTrayShortcuts &&
            (
                keyCode == KeyEvent.KEYCODE_BUTTON_START ||
                    keyCode == KeyEvent.KEYCODE_BUTTON_SELECT
            )
        ) {
            logSelectRouting("up shortcut kc=$keyCode consumed")
            return true
        }

        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
            val inGame = nativeIsInGame()
            if (!shouldDispatchGamepadButtonUp(inGame, gamepadButtonEdgeTracker.shouldDispatchUp(keyCode))) {
                return true
            }
            val metaId = buttonMetaBindings[joyBtn]
            if (metaId != null) {
                dispatchMetaAction(metaId, false)
            } else {
                val tag = "ctrl:btn$joyBtn"
                val kcIndices = mixerButtonMap[joyBtn]
                if (kcIndices != null) {
                    for (kc in kcIndices) inputMixer.setButton(kc, tag, false)
                }
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
    private val gamepadButtonEdgeTracker = GamepadButtonEdgeTracker()

    private var hatXState = 0 // -1, 0, +1
    private var hatYState = 0

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK &&
            event.action == MotionEvent.ACTION_MOVE
        ) {
            if (gameSurfaceView.keyboardActive) {
                val lx = event.getAxisValue(MotionEvent.AXIS_X)
                val ly = event.getAxisValue(MotionEvent.AXIS_Y)
                val hx = event.getAxisValue(MotionEvent.AXIS_HAT_X)
                val hy = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
                val stickX = motionAxisDirection(lx)
                val stickY = motionAxisDirection(ly)
                val hatX = motionAxisDirection(hx)
                val hatY = motionAxisDirection(hy)
                hatXState = if (hatX != 0) hatX else stickX
                hatYState = if (hatY != 0) hatY else stickY
                return super.onGenericMotionEvent(event)
            }
            inputMixer.setAxis(0, "ctrl", event.getAxisValue(MotionEvent.AXIS_X))
            inputMixer.setAxis(1, "ctrl", event.getAxisValue(MotionEvent.AXIS_Y))
            inputMixer.setAxis(2, "ctrl", event.getAxisValue(MotionEvent.AXIS_Z))
            inputMixer.setAxis(3, "ctrl", event.getAxisValue(MotionEvent.AXIS_RZ))
            val lt = event.getAxisValue(MotionEvent.AXIS_LTRIGGER)
            val rt = event.getAxisValue(MotionEvent.AXIS_RTRIGGER)
            inputMixer.setAxis(4, "ctrl", lt)
            inputMixer.setAxis(5, "ctrl", rt)
            rawAxisValues[0] = event.getAxisValue(MotionEvent.AXIS_X)
            rawAxisValues[1] = event.getAxisValue(MotionEvent.AXIS_Y)
            rawAxisValues[2] = event.getAxisValue(MotionEvent.AXIS_Z)
            rawAxisValues[3] = event.getAxisValue(MotionEvent.AXIS_RZ)
            rawAxisValues[4] = lt
            rawAxisValues[5] = rt
            // Compute half-axis combiner virtual axes
            for ((virt, posSource, negSource) in halfAxisCombiners) {
                val pos = if (posSource in 0..5) rawAxisValues[posSource] else 0f
                val neg = if (negSource in 0..5) rawAxisValues[negSource] else 0f
                inputMixer.setAxis(virt, "ctrl:half", (pos - neg).coerceIn(-1f, 1f))
            }

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
            if (newHatX != hatXState || newHatY != hatYState) {
                logGamepadInput("hat hx=$hx hy=$hy old=($hatXState,$hatYState) new=($newHatX,$newHatY)")
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

    private fun keyboardReferenceHeightPx(): Int {
        val decorView = window.decorView
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val metricsHeight = windowManager.currentWindowMetrics.bounds.height()
            if (metricsHeight > 0) return metricsHeight
        }
        val rootHeight = decorView.rootView.height
        return if (rootHeight > 0) rootHeight else decorView.height
    }

    private fun imeIgnoringVisibilityHeightPx(decorView: View): Int =
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                decorView.rootWindowInsets
                    ?.getInsetsIgnoringVisibility(
                        android.view.WindowInsets.Type
                            .ime(),
                    )?.bottom ?: 0
            } else {
                ViewCompat
                    .getRootWindowInsets(decorView)
                    ?.getInsetsIgnoringVisibility(WindowInsetsCompat.Type.ime())
                    ?.bottom ?: 0
            }
        } catch (_: IllegalArgumentException) {
            0
        }

    private fun tvKeyboardFallbackHeightPx(screenHeight: Int): Int =
        if (gamepadOnlyMode && gameSurfaceView.keyboardActive) {
            (screenHeight * 45) / 100
        } else {
            0
        }

    private fun sampleKeyboardHeightPx(imeBottomHint: Int? = null): Int {
        val decorView = window.decorView
        val screenHeight = keyboardReferenceHeightPx()
        val imeBottom =
            imeBottomHint ?: if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                decorView.rootWindowInsets
                    ?.getInsets(
                        android.view.WindowInsets.Type
                            .ime(),
                    )?.bottom ?: 0
            } else {
                ViewCompat
                    .getRootWindowInsets(decorView)
                    ?.getInsets(WindowInsetsCompat.Type.ime())
                    ?.bottom ?: 0
            }
        val imeStableBottom = imeIgnoringVisibilityHeightPx(decorView)
        val visibleFrame = Rect()
        decorView.getWindowVisibleDisplayFrame(visibleFrame)
        val systemBottom =
            ViewCompat
                .getRootWindowInsets(decorView)
                ?.getInsetsIgnoringVisibility(WindowInsetsCompat.Type.systemBars())
                ?.bottom ?: 0
        val bottomOcclusion = (screenHeight - visibleFrame.bottom).coerceAtLeast(0)
        val fallbackBottom = (bottomOcclusion - systemBottom).coerceAtLeast(0)
        val tvFallbackBottom =
            if (imeBottom == 0 && imeStableBottom == 0 && fallbackBottom == 0) {
                tvKeyboardFallbackHeightPx(screenHeight)
            } else {
                0
            }
        return maxOf(imeBottom, imeStableBottom, fallbackBottom, tvFallbackBottom)
    }

    /** Poll for IME height via rootWindowInsets.  With adjustNothing
     *  the insets callbacks don't fire, so we poll after requesting
     *  the keyboard and stop once we detect a non-zero IME height.
     *  Some TV keyboards also leave Type.ime() at zero, so fall back
     *  to the visible window frame bottom when needed. */
    private fun pollKeyboardHeight(attemptsLeft: Int) {
        if (attemptsLeft <= 0) return
        val imeHeight = sampleKeyboardHeightPx()
        if (imeHeight > 0) {
            nativeSetKeyboardHeight(imeHeight, keyboardReferenceHeightPx())
            return
        }
        val r = Runnable { pollKeyboardHeight(attemptsLeft - 1) }
        keyboardPollRunnable = r
        window.decorView.postDelayed(r, 100)
    }

    @Suppress("unused") // Called from native code
    fun showKeyboard(inputType: Int) {
        runOnUiThread {
            gameSurfaceView.currentInputType =
                when (inputType) {
                    2 -> InputType.TYPE_CLASS_NUMBER
                    else -> InputType.TYPE_CLASS_TEXT
                }
            keyboardInputView.currentInputType = gameSurfaceView.currentInputType
            hatXState = 0
            hatYState = 0
            gameSurfaceView.keyboardActive = true
            keyboardInputView.keyboardActive = true
            keyboardInputView.requestFocus()
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.restartInput(keyboardInputView)
            imm.showSoftInput(keyboardInputView, InputMethodManager.SHOW_IMPLICIT)
            // Use WindowInsetsController API (works with setDecorFitsSystemWindows(false))
            WindowInsetsControllerCompat(window, keyboardInputView)
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
            hatXState = 0
            hatYState = 0
            gameSurfaceView.keyboardActive = false
            keyboardInputView.keyboardActive = false
            WindowInsetsControllerCompat(window, keyboardInputView)
                .hide(WindowInsetsCompat.Type.ime())
            keyboardInputView.clearFocus()
            gameSurfaceView.requestFocus()
            nativeSetKeyboardHeight(0, keyboardReferenceHeightPx())
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
                // Format: "musicType|trackIndex|totalTracks|trackName"
                val parts = info.split("|", limit = 4)
                val name =
                    if (parts.size >= 4 &&
                        parts[3].isNotEmpty()
                    ) {
                        // Safety: strip path and extension if C side returned a raw path
                        val raw = parts[3]
                        if ('/' in raw || '\\' in raw) {
                            raw
                                .substringAfterLast('/')
                                .substringAfterLast('\\')
                                .substringBeforeLast('.')
                        } else {
                            raw
                        }
                    } else {
                        "Track ${parts.getOrElse(1) { "?" }}"
                    }
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

    /** True for keys that should dismiss the music admin panel. */
    private fun isMusicPanelDismissKey(keyCode: Int): Boolean =
        keyCode == KeyEvent.KEYCODE_BACK ||
            keyCode == KeyEvent.KEYCODE_ESCAPE ||
            keyCode == KeyEvent.KEYCODE_BUTTON_B ||
            keyCode == KeyEvent.KEYCODE_BUTTON_Y

    private fun dismissMusicPanel() {
        musicPanel?.let { mp ->
            (gameSurfaceView.parent as? FrameLayout)?.removeView(mp)
        }
        musicPanel = null
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
                                    tv.alpha = 0f
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

    @Suppress("unused")
    fun showLoadingProgress(
        phase: String,
        item: String,
        percent: Int,
    ) {
        runOnUiThread {
            loadingProgressOverlay?.showProgress(phase, item, percent)
        }
    }

    @Suppress("unused")
    fun hideLoadingProgress() {
        runOnUiThread {
            loadingProgressOverlay?.hideProgress()
        }
    }

    // ── Level name overlay (called from JNI) ────────────────
    @Suppress("unused")
    fun showLevelName(name: String) {
        showOverlayLine(name)
    }

    // ── Host migration notification (called from JNI on game thread) ──
    // android port: when this client becomes the new host after the original
    // host disconnects, send a cross-process broadcast so SetupActivity's
    // LobbyService can start LAN broadcasting for the migrated game.
    @Suppress("unused")
    fun onHostMigration() {
        Log.i("DXX-MP", "Host migration: notifying SetupActivity to resume LAN broadcast")
        val intent = android.content.Intent("com.dxxredux.HOST_MIGRATION")
        intent.setPackage(packageName)
        sendBroadcast(intent)
    }

    @Suppress("unused")
    fun persistSkipIntroMovieFromNative() {
        persistSkipIntroMoviePreference()
    }

    private fun persistSkipIntroMoviePreference() {
        Log.i("DXX-Setup", "Skip every launch tapped")
        getSharedPreferences("dxx_prefs", MODE_PRIVATE)
            .edit()
            .putBoolean(PREF_SKIP_INTRO_MOVIE, true)
            .commit()
        sendBroadcast(
            Intent("com.dxxredux.SETUP_COMMAND")
                .setPackage(packageName)
                .putExtra("command", "write_bool_pref")
                .putExtra("key", PREF_SKIP_INTRO_MOVIE)
                .putExtra("value", true),
        )
    }

    // ── Debug log bridge (called from JNI on game thread) ──
    @Suppress("unused")
    fun debugLogFromNative(
        category: Int,
        message: String,
    ) {
        DebugLog.log(category, message)
        if (category == DebugLogCategory.NETWORK) {
            com.dxxredux.app.multiplayer.MatchmakingStateHolder
                .appendLog(message)
        }
    }

    // ── Hidden keyboard proxy with InputConnection for soft keyboard ──
    private inner class KeyboardInputView(
        context: Context,
    ) : androidx.appcompat.widget.AppCompatEditText(context) {
        var currentInputType = InputType.TYPE_CLASS_TEXT
        var keyboardActive = false

        override fun onCheckIsTextEditor(): Boolean = keyboardActive

        override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
            outAttrs.inputType = buildKeyboardEditorInputType(currentInputType)
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE or
                EditorInfo.IME_FLAG_NO_EXTRACT_UI
            return GameInputConnection(this)
        }
    }

    // ── GameSurfaceView render target ──
    private inner class GameSurfaceView(
        context: Context,
    ) : SurfaceView(context) {
        var currentInputType = InputType.TYPE_CLASS_TEXT
        var keyboardActive = false

        override fun onCheckIsTextEditor(): Boolean = keyboardActive

        override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
            // Disable word prediction / autocorrect so each keystroke arrives
            // immediately via commitText instead of being buffered in composition.
            outAttrs.inputType = buildKeyboardEditorInputType(currentInputType)
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
