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
import android.view.RoundedCorner
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
import com.dxxredux.app.multiplayer.RuntimeGameStateBridge
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.io.File
import java.util.Locale
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import kotlin.math.roundToInt

internal const val EXTRA_TRANSIENT_LAUNCH_TOKEN = "transient_launch_token"

internal fun shouldShowTouchOverlay(
    inGame: Boolean,
    overlayEnabled: Boolean,
    playerDead: Boolean,
    endlevel: Boolean,
    automap: Boolean,
    controllerMenuOpen: Boolean,
    settingsTrayVisible: Boolean,
): Boolean {
    val gameplayOverlayVisible = overlayEnabled && !playerDead && !endlevel && inGame
    val controllerOwnedOverlayVisible = controllerMenuOpen || settingsTrayVisible
    val gameplayOverlay = gameplayOverlayVisible || controllerOwnedOverlayVisible
    return gameplayOverlay || automap
}

internal fun defaultTouchOverlayEnabled(
    hasTouchscreen: Boolean,
    hasController: Boolean,
): Boolean = !hasTouchscreen || !hasController

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

internal data class GyroRuntimeState(
    val configured: Boolean,
    val activeInGame: Boolean,
) {
    val effectiveEnabled: Boolean
        get() = configured && activeInGame
}

internal fun gyroRuntimeStateFromConfig(config: GyroConfig): GyroRuntimeState =
    GyroRuntimeState(configured = config.enabled, activeInGame = config.enabled)

internal fun toggledGyroRuntimeState(state: GyroRuntimeState): GyroRuntimeState =
    if (!state.configured) {
        state.copy(activeInGame = false)
    } else {
        state.copy(activeInGame = !state.activeInGame)
    }

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

internal fun imeCommittedCodePointFromKeyEvent(
    keyCode: Int,
    unicodeChar: Int,
): Int? =
    when (keyCode) {
        KeyEvent.KEYCODE_DEL,
        KeyEvent.KEYCODE_ENTER,
        KeyEvent.KEYCODE_NUMPAD_ENTER,
        -> null

        else -> unicodeChar.takeIf { it > 31 }
    }

internal fun imeNativeSpecialKeyCode(keyCode: Int): Int? =
    when (keyCode) {
        KeyEvent.KEYCODE_DEL -> KeyEvent.KEYCODE_DEL

        KeyEvent.KEYCODE_ENTER,
        KeyEvent.KEYCODE_NUMPAD_ENTER,
        -> KeyEvent.KEYCODE_ENTER

        else -> null
    }

internal fun shouldConsumeKeyboardBack(
    keyboardActive: Boolean,
    keyboardImeVisible: Boolean,
    gamepadOnlyMode: Boolean,
): Boolean = keyboardActive && (keyboardImeVisible || gamepadOnlyMode)

internal fun shouldDispatchGamepadButtonDown(
    isInGame: Boolean,
    repeatCount: Int,
    edgeDispatchAllowed: Boolean,
): Boolean = if (isInGame) edgeDispatchAllowed else repeatCount == 0

internal fun shouldDispatchGamepadButtonUp(
    isInGame: Boolean,
    edgeDispatchAllowed: Boolean,
): Boolean = if (isInGame) edgeDispatchAllowed else true

internal fun shouldRouteControllerBToNativeBack(
    keyCode: Int,
    isControllerEvent: Boolean,
    nativeMenuFront: Boolean,
    controllerMenuOpen: Boolean,
    adminTrayOpen: Boolean,
): Boolean =
    isControllerEvent &&
        keyCode == KeyEvent.KEYCODE_BUTTON_B &&
        nativeMenuFront &&
        !controllerMenuOpen &&
        !adminTrayOpen

class MainActivity :
    Activity(),
    SurfaceHolder.Callback {
    private val startupScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var playlistPreparation: Deferred<Unit>? = null
    private var routeMetadataJob: Job? = null

    companion object {
        private const val ADMIN_TRAY_CLOSE_GRACE_MS = 400L

        // Must match android_screen_advance_kind in android_screen_advance.h.
        private const val SCREEN_ADVANCE_NONE = 0
        private const val SCREEN_ADVANCE_DEATH = 1
        private const val SCREEN_ADVANCE_ENDLEVEL = 2
        private const val SCREEN_ADVANCE_LEVELCOMPLETE = 5

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

    external fun nativeSetSurfaceSize(
        width: Int,
        height: Int,
    )

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

    external fun nativeQueueMinimizeAutosave()

    external fun nativeQuit()

    external fun nativeGetGameState(): String

    external fun nativeRequestIntrospect()

    external fun nativeUpdateDormancyUiPollCounters(
        central: Long,
        independent: Long,
    )

    external fun nativeRequestMultiplayerDormancyTimeout()

    external fun nativeInitializeDormancyDiagnostics()

    external fun nativeSetIntrospectPath(path: String)

    external fun nativeLoadAutomationScript(
        path: String,
        startStep: Int,
        runId: String,
    )

    external fun nativeSetAutomationPath(path: String)

    fun automateRadialSelection(
        menuId: String,
        text: String,
    ): Boolean {
        if (android.os.Looper.myLooper() == android.os.Looper.getMainLooper()) {
            return touchOverlay.automateRadialSelection(menuId, text)
        }
        val done = CountDownLatch(1)
        var selected = false
        runOnUiThread {
            selected = touchOverlay.automateRadialSelection(menuId, text)
            done.countDown()
        }
        return done.await(2, TimeUnit.SECONDS) && selected
    }

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
    ): Int

    external fun nativeApplyLauncherGraphicsOption(
        name: String,
        value: Int,
    ): Boolean

    external fun nativeSetRoundedCornerTextInsets(
        surfaceWidth: Int,
        surfaceHeight: Int,
        topLeftPx: Int,
        bottomLeftPx: Int,
        topRightPx: Int,
        bottomRightPx: Int,
    )

    external fun nativeGetGammaLevel(): Int

    external fun nativeSetCoopIndicatorOptions(
        showNearestPlayerLine: Boolean,
        showGuidebotLine: Boolean,
    )

    external fun nativeSetHeadlightOffByDefaultQol(enabled: Boolean)

    external fun nativeSetDebugLogEnabled(
        category: Int,
        on: Boolean,
    )

    external fun nativeSetAutomaticSlowdownCapture(enabled: Boolean)

    external fun nativeJoystickAxis(
        axis: Int,
        value: Float,
        touchActive: Boolean,
    )

    external fun nativeJoystickAxes(
        axes: IntArray,
        values: FloatArray,
        touchActive: BooleanArray,
    )

    external fun nativeJoystickButton(
        button: Int,
        pressed: Int,
    )

    external fun nativeIsInGame(): Boolean

    external fun nativeSetJoystickEnabled(enabled: Boolean)

    external fun nativeIsAutomapActive(): Boolean

    external fun nativeGetScreenAdvanceState(): Long

    external fun nativeRequestScreenAdvance(generation: Long): Boolean

    external fun nativeIsIntroActive(): Boolean

    external fun nativeSetSkipIntroMovie(enabled: Boolean)

    external fun nativeSetDemoRecordPerFrameState(enabled: Boolean)

    external fun nativeIsDemoRecordingActive(): Boolean

    external fun nativeIsSaveLoadMenuActive(): Boolean

    external fun nativeIsHostSelectingPlayers(): Boolean

    external fun nativeGetCoopLevelRestartState(): Int

    external fun nativeStartSelectedPlayers()

    /** Returns the callsign of a player requesting to join, or "" if none. */
    external fun nativeGetJoinRequest(): String

    /** Accept the pending join request (equivalent to pressing F6). */
    external fun nativeAcceptJoinRequest()

    external fun nativeAutomapCenter()

    external fun nativeAutomapSetMarker(idx: Int)

    external fun nativeAutomapSelectMarker(idx: Int)

    external fun nativeAutomapNameMarker()

    external fun nativeSecretAreaRevealActive(): Boolean

    external fun nativeToggleSecretAreaReveal()

    external fun nativeMatcenMode(): Int

    external fun nativeCycleMatcenMode()

    external fun nativeObjectiveOverlayMode(): Int

    external fun nativeCycleObjectiveOverlay()

    external fun nativeMapCheatsAccessible(): Boolean

    external fun nativeReactorCountdownActive(): Boolean

    external fun nativeReactorCountdownPaused(): Boolean

    external fun nativeReactorPauseAllowed(): Boolean

    external fun nativeToggleReactorCountdownPause()

    external fun nativeGetAutomapMarkerState(): IntArray

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

    external fun nativeGetDifficulty(): Int

    external fun nativeCanShowDifficultyChange(): Boolean

    external fun nativeCanChangeDifficulty(): Boolean

    external fun nativeSetDifficulty(difficulty: Int): Boolean

    external fun nativeOpenSinglePlayerPauseIfSafe(): Boolean

    external fun nativeOpenOverlayPauseIfSafe(): Boolean

    external fun nativeCloseOverlayPauseIfOwned(): Boolean

    external fun nativeClosePauseIfFront(): Boolean

    external fun nativeOpenSaveMenuIfSafe(): Boolean

    external fun nativeOpenLoadMenuIfSafe(): Boolean

    external fun nativeOpenGameMenuIfSafe(): Boolean

    // ── Music track control (android_music_control.c) ────────────────
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
        duplicateEnergyShields: Boolean,
        fullDeathSpew: Boolean,
        playerSpewNoExpire: Boolean,
        clientsCanRequestRewind: Boolean,
        hostObserver: Boolean,
    )

    external fun nativeGetCurrentTrackInfo(): String

    external fun nativeGetMusicType(): Int

    external fun nativeGetTrackList(): String

    external fun nativeGetMusicOverlayState(): String

    external fun nativeIsMusicSourceChangePending(): Boolean

    external fun nativeSetMusicSource(source: String): Boolean

    external fun nativeSetMusicOneTrackPerLevel(enabled: Boolean): Boolean

    external fun nativeSetMusicVolume(volume: Int): Int

    external fun nativeSetMusicPaused(paused: Boolean): Boolean

    external fun nativeNotifyRouteMetadataFinished(
        requestGeneration: Int,
        success: Boolean,
    )

    external fun nativeNotifyRouteMetadataProgress(
        requestGeneration: Int,
        estimatedPermille: Int,
        state: Int,
    )

    fun gameVariantForMusicOverlay(): String = gameVariantId

    @Suppress("unused") // Called from native code on the game thread
    fun onRouteMetadataNeeded(
        game: String,
        mission: String,
        levelNum: Int,
        levelFile: String,
        routeReadiness: String,
        normalLevelFiles: Array<String>,
        secretLevelFiles: Array<String>,
        secretEntryLevels: IntArray,
        requestGeneration: Int,
    ) {
        routeMetadataJob?.cancel()
        routeMetadataJob =
            startupScope.launch(Dispatchers.IO) {
                RouteMetadataBackground.computeMission(
                    this@MainActivity,
                    game,
                    mission,
                    levelNum,
                    levelFile,
                    routeReadiness,
                    normalLevelFiles.toList(),
                    secretLevelFiles.toList(),
                    secretEntryLevels.toList(),
                    onCurrentReady = { success ->
                        nativeNotifyRouteMetadataFinished(requestGeneration, success)
                    },
                    onCurrentProgress = { estimatedPermille, state ->
                        nativeNotifyRouteMetadataProgress(
                            requestGeneration,
                            estimatedPermille,
                            state.wireValue,
                        )
                    },
                )
            }
    }

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
            SafDescriptorStager.detachSeekable(pfd, cacheDir, uri.lastPathSegment ?: "SAF source")
        } catch (e: Exception) {
            Log.e("MainActivity", "openSafFile failed for $contentUri", e)
            -1
        }
    }

    private var gameStarted = false
    private var pendingInputDemoReplayPath: String? = null
    private var pendingResumeSavePath: String? = null
    private var pendingResumeCallsign: String? = null
    private var pendingPilotCallsign: String? = null
    private var pendingTransientLaunchToken: String? = null
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
    private var overlayPollProfileWindowStartMs = 0L
    private var overlayPollProfileCount = 0
    private var overlayPollProfileTotalUs = 0L
    private var overlayPollProfileMaxUs = 0L
    private var overlayPollProfileSlowCount = 0
    private var overlayPollProfileErrorCount = 0
    private var musicPanel: MusicControlPanel? = null
    private var netStatsOverlay: com.dxxredux.app.multiplayer.MultiplayerStatsOverlay? = null
    private var netEventsOverlay: com.dxxredux.app.multiplayer.NetworkEventsOverlay? = null
    private var videoInfoOverlay: VideoInfoOverlay? = null
    private var loadingProgressOverlay: LoadingProgressOverlayView? = null
    private var warpButtonOverlay: WarpButtonOverlay? = null
    private var netEventsManualToggle = false
    private var adminTrayPausedGame = false
    private var adminTrayCloseGraceUntilMs = 0L
    private var isMultiplayerGame = false
    private var mainViewFovLockedToBase = false
    private var keyboardImeVisible = false
    private var imeNavigationDispatchDepth = 0
    private var lastTrackNum = -1 // for detecting track changes in polling
    private var remainingMusicStateRefreshes = 0
    private val musicStateRefreshRunnable =
        object : Runnable {
            override fun run() {
                val pending =
                    try {
                        nativeIsMusicSourceChangePending()
                    } catch (_: Exception) {
                        false
                    }
                if (pending && --remainingMusicStateRefreshes > 0) {
                    window.decorView.postDelayed(this, 120L)
                    return
                }
                if (pending) Log.w("DXX-MusicCtrl", "Music command still pending after refresh window")
                musicPanel?.refreshState()
                updateTrackLabel()
            }
        }
    private var gyroManager: GyroInputManager? = null
    private var gyroRuntimeState = gyroRuntimeStateFromConfig(GyroConfig())
    private var activeTouchLayout = TouchLayoutRepository.defaultLayout()
    private var isActivityResumed = false
    private var gameVariantId = "d2" // "d1" or "d2", set in onCreate
    private var lastAppliedGraphicsSettingsGeneration = -1L
    private var touchDiagLogCount = 0

    // True when no touchscreen is available (Android TV / gamepad-only)
    private var gamepadOnlyMode = false

    // Controller meta-action bindings: SDL button index -> meta action ID
    private var buttonMetaBindings = mapOf<Int, Int>()

    private var controllerBoundActions = emptySet<Int>()

    // D-pad meta-action bindings: DPAD keycode → meta action ID
    private var dpadMetaBindings = mapOf<Int, Int>()

    // Half-axis combiners: (virtualAxis, posSourceAxis, negSourceAxis)
    // Loaded from controller_config.json; used in onGenericMotionEvent()
    private var halfAxisCombiners = emptyList<Triple<Int, Int, Int>>()
    private var controllerAxisExponents = defaultControllerAxisExponents()
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

    private fun applyTvPerfTestPrefs(prefs: android.content.SharedPreferences) {
        prefs
            .edit()
            .putBoolean(PREF_SHOW_VIDEO_INFO_DEBUG_OPTIONS, false)
            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.GRAPHICS), false)
            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.TEXTURE), false)
            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.PROFILING), false)
            .commit()
    }

    private fun resetOverlayPollProfileWindow() {
        overlayPollProfileWindowStartMs = 0L
        overlayPollProfileCount = 0
        overlayPollProfileTotalUs = 0L
        overlayPollProfileMaxUs = 0L
        overlayPollProfileSlowCount = 0
        overlayPollProfileErrorCount = 0
    }

    private fun recordOverlayPollProfile(
        durationUs: Long,
        inGame: Boolean,
        automap: Boolean,
        overlayVisible: Boolean,
        netEventsVisible: Boolean,
        videoInfoVisible: Boolean,
        hadError: Boolean,
    ) {
        val nowMs = android.os.SystemClock.uptimeMillis()
        if (overlayPollProfileWindowStartMs == 0L) {
            overlayPollProfileWindowStartMs = nowMs
        }
        overlayPollProfileCount += 1
        overlayPollProfileTotalUs += durationUs
        if (durationUs > overlayPollProfileMaxUs) {
            overlayPollProfileMaxUs = durationUs
        }
        if (durationUs >= 4_000L) {
            overlayPollProfileSlowCount += 1
        }
        if (hadError) {
            overlayPollProfileErrorCount += 1
        }
        if (nowMs - overlayPollProfileWindowStartMs < 1_000L) {
            return
        }
        if (DebugLog.isCategoryEnabled(this, DebugLogCategory.PROFILING)) {
            val avgUs =
                if (overlayPollProfileCount > 0) {
                    overlayPollProfileTotalUs / overlayPollProfileCount
                } else {
                    0L
                }
            DebugLog.log(
                DebugLogCategory.PROFILING,
                "prof_v=1 type=ui_poll window_ms=${nowMs - overlayPollProfileWindowStartMs} polls=$overlayPollProfileCount avg_us=$avgUs max_us=$overlayPollProfileMaxUs slow_polls=$overlayPollProfileSlowCount errors=$overlayPollProfileErrorCount in_game=$inGame automap=$automap overlay=$overlayVisible net_events=$netEventsVisible video_info=$videoInfoVisible",
            )
        }
        resetOverlayPollProfileWindow()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        refreshTransientLaunchState(intent)
        if (shouldRedirectConsumedTransientLaunch()) {
            redirectConsumedTransientLaunchToSetup()
            return
        }
        clearTransientLaunchExtrasFromIntent(intent)

        val lacksTouchscreen = !hasTouchscreen()
        if (BuildConfig.DEBUG && lacksTouchscreen) {
            // android port work: seed a clean no-UI perf test profile for TV builds
            applyTvPerfTestPrefs(getSharedPreferences("dxx_prefs", MODE_PRIVATE))
        }

        CrashLog.install(this)
        // Append to the main-process log file if a path was passed, otherwise create new
        val netlogPath = intent.getStringExtra("netlog_path")
        if (netlogPath != null) {
            DebugLog.initAppend(this, netlogPath)
        } else {
            DebugLog.init(this)
        }
        // Load the correct game library based on the launcher's selection
        val game = intent.getStringExtra("game") ?: "d2"
        gameVariantId = game
        writeGameActivityState(this, gameVariantId)
        gamepadOnlyMode = lacksTouchscreen
        val libName = if (game == "d1") "dxx-redux-d1" else "dxx-redux-d2"
        System.loadLibrary(libName)
        Log.i("MainActivity", "Loaded native library: $libName")
        CrashLog.installNativeHandler(this)

        // Sync C-side per-category enable flags with Kotlin prefs
        syncDebugLogPrefs()

        // Rewrite audio playlist in the game process so SAF fds are valid.
        // SetupActivity runs in the default process; this activity runs in
        // :game.  PFDs opened there have fd numbers that don't exist here.
        playlistPreparation =
            startupScope.async(Dispatchers.IO) {
                AudioSourceManager(filesDir).writePlaylist(contentResolver)
            }
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
        try {
            mainViewFovLockedToBase =
                mpMode == "join" && intent.getBooleanExtra("mp_restrict_noncoop_fov_to_base", false)
            nativeSetGraphicsOption("main_view_fov_locked", if (mainViewFovLockedToBase) 1 else 0)
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
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
            val duplicateEnergyShields = intent.getBooleanExtra("mp_duplicate_energy_shields", false)
            val fullDeathSpew = intent.getBooleanExtra("mp_full_death_spew", true)
            val playerSpewNoExpire = intent.getBooleanExtra("mp_player_spew_no_expire", true)
            val clientsCanRequestRewind = intent.getBooleanExtra("mp_clients_can_request_rewind", false)
            val hostObserver = intent.getBooleanExtra("mp_host_observer", false)
            nativeSetAutoHost(
                myPort,
                mission,
                mode,
                maxPlayers,
                levelNum,
                difficulty,
                coopQol,
                duplicateEnergyShields,
                fullDeathSpew,
                playerSpewNoExpire,
                clientsCanRequestRewind,
                hostObserver,
            )
        }

        // Start foreground service during multiplayer to prevent process kill
        if (mpMode != null) {
            com.dxxredux.app.multiplayer.MultiplayerForegroundService
                .start(this)
            RuntimeGameStateBridge.connect(
                context = this,
                host = mpMode == "host",
                stateProvider = { nativeGetNetgameState() },
                onBackgroundTimeout = { nativeRequestMultiplayerDormancyTimeout() },
            )
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
        applyHeadlightDefaultPrefs(prefs)
        applyDemoRecordingPref()

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
        gyroRuntimeState = gyroRuntimeStateFromConfig(activeTouchLayout.gyro)
        touchOverlay.setLayout(activeTouchLayout)

        // Input mixer: combines button/axis inputs from touch, controller, gyro
        inputMixer =
            InputMixer(
                buttonCallback = { btn, pressed ->
                    nativeJoystickButton(TouchBindings.MIXER_BTN_BASE + btn, pressed)
                },
                axisCallback = { axis, value, touchActive -> nativeJoystickAxis(axis, value, touchActive) },
                axisBatchCallback = { axes, values, touchActive ->
                    nativeJoystickAxes(axes, values, touchActive)
                },
            )
        touchOverlay.inputMixer = inputMixer
        touchOverlay.axisCallback = { axis, value ->
            inputMixer.setAxis(axis, "touch", value)
        }

        applyGyroConfig(activeTouchLayout.gyro)
        touchOverlay.metaActionCallback = { actionId, pressed ->
            dispatchMetaAction(actionId, pressed)
        }
        touchOverlay.exactWeaponSelectCallback = { isPrimary, weaponIndex ->
            NativeMetaActions.nativeSelectWeaponExact(if (isPrimary) 0 else 1, weaponIndex)
        }
        touchOverlay.keyCallback = { action, keyCode, unicode ->
            nativeKeyEvent(action, keyCode, unicode)
        }
        touchOverlay.gameVariant = game
        touchOverlay.isMultiplayerGameProvider = { isMultiplayerGame }
        touchOverlay.hasPendingMultiplayerLaunchProvider = {
            com.dxxredux.app.multiplayer.MatchmakingStateHolder.state.value.gameLaunchInfo != null
        }
        touchOverlay.controllerBoundActionBindingsProvider = { controllerBoundActions }
        touchOverlay.workingControllerInUseProvider = { hasWorkingControllerDevice() }
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
        touchOverlay.adminTrayBrightnessProvider = {
            try {
                nativeGetGammaLevel()
            } catch (_: Exception) {
                0
            }
        }
        touchOverlay.adminTrayBrightnessSetter = { value ->
            try {
                nativeSetGraphicsOption("gamma_level", value)
            } catch (_: Exception) {
                // JNI not ready yet
            }
        }
        touchOverlay.adminTrayFovProvider = {
            val stored = readConfigValueForGame(filesDir, gameVariantId, "MainViewFov")?.toIntOrNull() ?: 0
            when (stored) {
                100, 110, 120 -> stored
                else -> 0
            }
        }
        touchOverlay.adminTrayFovSetter = { value ->
            val fov =
                when (value) {
                    100, 110, 120 -> value
                    else -> 0
                }
            try {
                nativeSetGraphicsOption("main_view_fov", fov)
            } catch (_: Exception) {
                updateAllConfigFiles(filesDir, listOf("MainViewFov" to fov.toString()))
            }
        }
        touchOverlay.adminTrayCanShowDifficultyProvider = {
            try {
                nativeCanShowDifficultyChange()
            } catch (_: Exception) {
                false
            }
        }
        touchOverlay.adminTrayDifficultyProvider = {
            try {
                nativeGetDifficulty()
            } catch (_: Exception) {
                0
            }
        }
        touchOverlay.adminTrayDifficultySetter = { difficulty ->
            try {
                nativeSetDifficulty(difficulty)
            } catch (_: Exception) {
                false
            }
        }
        touchOverlay.adminTrayToggleStateProvider = { action ->
            when (action) {
                TouchOverlayView.ADMIN_NET_STATS -> {
                    netStatsOverlay?.visibility == View.VISIBLE
                }

                TouchOverlayView.ADMIN_NET_EVENTS -> {
                    netEventsManualToggle
                }

                TouchOverlayView.ADMIN_AUTOMAP_SECRET_REVEAL -> {
                    try {
                        nativeSecretAreaRevealActive()
                    } catch (_: Exception) {
                        false
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_MATCEN_MODE -> {
                    try {
                        nativeMatcenMode() != MATCEN_MODE_DEFAULT
                    } catch (_: Exception) {
                        false
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_REACTOR -> {
                    try {
                        nativeReactorCountdownPaused()
                    } catch (_: Exception) {
                        false
                    }
                }

                TouchOverlayView.ADMIN_VIDEO_INFO -> {
                    videoInfoOverlay?.visibility == View.VISIBLE
                }

                else -> {
                    false
                }
            }
        }
        touchOverlay.adminTrayObjectiveModeProvider = {
            try {
                nativeObjectiveOverlayMode()
            } catch (_: Exception) {
                OBJECTIVE_MODE_OFF
            }
        }
        touchOverlay.adminTrayMatcenModeProvider = {
            try {
                nativeMatcenMode()
            } catch (_: Exception) {
                MATCEN_MODE_DEFAULT
            }
        }
        touchOverlay.mapCheatsAccessibleProvider = {
            try {
                nativeMapCheatsAccessible()
            } catch (_: Exception) {
                true
            }
        }
        touchOverlay.reactorCountdownPausedProvider = {
            try {
                nativeReactorCountdownPaused()
            } catch (_: Exception) {
                false
            }
        }
        touchOverlay.reactorPauseAllowedProvider = {
            try {
                nativeReactorPauseAllowed()
            } catch (_: Exception) {
                false
            }
        }
        touchOverlay.adminTrayEnabledStateProvider = { action ->
            when (action) {
                TouchOverlayView.ADMIN_NET_STATS -> {
                    isNetStatsControlEnabled()
                }

                TouchOverlayView.ADMIN_NET_EVENTS -> {
                    isNetEventsControlEnabled()
                }

                TouchOverlayView.ADMIN_ABDICATE_GUIDEBOT -> {
                    try {
                        nativeIsEscortOwner() && nativeIsBuddyReleased()
                    } catch (_: Exception) {
                        false
                    }
                }

                TouchOverlayView.ADMIN_FOV -> {
                    !mainViewFovLockedToBase
                }

                TouchOverlayView.ADMIN_DIFFICULTY -> {
                    try {
                        nativeCanChangeDifficulty()
                    } catch (_: Exception) {
                        false
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_REACTOR -> {
                    try {
                        nativeReactorCountdownActive()
                    } catch (_: Exception) {
                        false
                    }
                }

                else -> {
                    true
                }
            }
        }
        touchOverlay.adminTrayCanShowCoopLevelRestartProvider = {
            try {
                nativeGetCoopLevelRestartState() == 2
            } catch (_: Exception) {
                false
            }
        }
        touchOverlay.adminTrayCallback = { action ->
            when (action) {
                TouchOverlayView.ADMIN_INCREASE_VIEW -> {
                    nativeCycleCockpit(1)
                }

                TouchOverlayView.ADMIN_CYCLE_LEFT_VIEW -> {
                    dispatchMetaAction(TouchBindings.META_CYCLE_LEFT_VIEW, true)
                }

                TouchOverlayView.ADMIN_CYCLE_RIGHT_VIEW -> {
                    dispatchMetaAction(TouchBindings.META_CYCLE_RIGHT_VIEW, true)
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
                    openGameMenuFromControllerSettings()
                }

                TouchOverlayView.ADMIN_RESTART_LEVEL -> {
                    android.app.AlertDialog
                        .Builder(this)
                        .setTitle("Restart Level")
                        .setMessage("Restart from the beginning of this level? Current level progress will be lost.")
                        .setNegativeButton("Cancel", null)
                        .setPositiveButton("Restart") { _, _ ->
                            NativeMetaActions.nativeMetaAction(TouchBindings.META_COOP_RESTART_LEVEL, 1)
                        }.show()
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
                    if (videoInfoOverlay?.visibility == View.VISIBLE) {
                        videoInfoOverlay?.hide()
                    } else {
                        dismissMusicPanel()
                        videoInfoOverlay?.show()
                    }
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

                TouchOverlayView.ADMIN_ABDICATE_GUIDEBOT -> {
                    try {
                        dispatchMetaAction(TouchBindings.META_GUIDE_RELEASE_CONTROL, true)
                    } catch (_: Exception) {
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_RECENTER -> {
                    try {
                        nativeAutomapCenter()
                    } catch (_: Exception) {
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_NAME_MARKER -> {
                    try {
                        nativeAutomapNameMarker()
                    } catch (_: Exception) {
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_SECRET_REVEAL -> {
                    try {
                        nativeToggleSecretAreaReveal()
                    } catch (_: Exception) {
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_MATCEN_MODE -> {
                    try {
                        nativeCycleMatcenMode()
                    } catch (_: Exception) {
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_OBJECTIVES -> {
                    try {
                        nativeCycleObjectiveOverlay()
                    } catch (_: Exception) {
                    }
                }

                TouchOverlayView.ADMIN_AUTOMAP_REACTOR -> {
                    try {
                        nativeToggleReactorCountdownPause()
                    } catch (_: Exception) {
                    }
                }

                else -> {
                    automapSetMarkerAdminActionIndex(action)?.let { idx ->
                        try {
                            nativeAutomapSetMarker(idx)
                        } catch (_: Exception) {
                        }
                    } ?: automapMarkerAdminActionIndex(action)?.let { idx ->
                        try {
                            nativeAutomapSelectMarker(idx)
                        } catch (_: Exception) {
                        }
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
            touchOverlay.remainingAdminActionsProvider = {
                buildList {
                    try {
                        val st = nativeGetCoopWarpStatus()
                        if (st.isNotEmpty() && st[0] != 0) {
                            val name = nativeGetCoopWarpTargetName()
                            add(
                                RemainingTouchAction(
                                    label = if (name.isNotEmpty()) "Warp: $name" else "Warp",
                                    adminAction = TouchOverlayView.ADMIN_WARP,
                                ),
                            )
                        }
                    } catch (_: Exception) {
                    }
                    try {
                        val cs = nativeGetJoinRequest()
                        if (cs.isNotEmpty()) {
                            add(
                                RemainingTouchAction(
                                    label = "Accept: $cs",
                                    adminAction = TouchOverlayView.ADMIN_ACCEPT_JOIN,
                                ),
                            )
                        }
                    } catch (_: Exception) {
                    }
                }
            }
        }
        touchOverlay.automapActionsProvider = { markerMenuMode ->
            val includeMarkers = game != "d1"
            val markerSlots =
                try {
                    nativeGetAutomapMarkerState()
                } catch (_: Throwable) {
                    IntArray(0)
                }
            automapTouchActions(includeMarkers, markerMenuMode, markerSlots)
        }
        touchOverlay.secretAreaRevealProvider = {
            try {
                nativeSecretAreaRevealActive()
            } catch (_: Throwable) {
                false
            }
        }
        touchOverlay.weaponStateProvider = {
            try {
                WeaponState.fromArray(nativeGetWeaponState())
            } catch (_: Throwable) {
                null
            }
        }
        touchOverlay.mapButtonCallback = { toggleAutomap() }
        touchOverlay.prevTrackCallback = {
            if (nativePrevTrack() != 0) scheduleMusicStateRefresh()
        }
        touchOverlay.nextTrackCallback = {
            if (nativeNextTrack() != 0) scheduleMusicStateRefresh()
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
                screenAdvanceCallback = { generation ->
                    try {
                        nativeRequestScreenAdvance(generation)
                    } catch (_: Exception) {
                    }
                }
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
                startCallback = {
                    try {
                        nativeStartSelectedPlayers()
                    } catch (_: Exception) {
                    }
                }
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
        frame.addView(
            acceptJoinButton,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )
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
                    RuntimeGameStateBridge.getProxyStats()
                }
                connectionInfoProvider = {
                    RuntimeGameStateBridge.getConnectionInfo()
                }
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
        frame.addView(
            warpOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
            ),
        )

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
        frame.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ ->
            updateRoundedCornerTextInsets()
        }
        updateRoundedCornerTextInsets()

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
                    val imeBottomHint = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom
                    val visibleImeHeight = sampleVisibleKeyboardHeightPx(imeBottomHint)
                    val imeHeight = sampleKeyboardHeightPx(imeBottomHint)
                    keyboardImeVisible = visibleImeHeight > 0
                    if (visibleImeHeight > 0) {
                        nativeSetKeyboardHeight(imeHeight, keyboardReferenceHeightPx())
                    } else {
                        nativeSetKeyboardHeight(0, keyboardReferenceHeightPx())
                    }
                    return insets
                }

                override fun onEnd(animation: WindowInsetsAnimationCompat) {
                    val visibleImeHeight = sampleVisibleKeyboardHeightPx()
                    val imeHeight = if (visibleImeHeight > 0) sampleKeyboardHeightPx() else 0
                    keyboardImeVisible = visibleImeHeight > 0
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

    @androidx.annotation.Keep
    fun reportNativeFatalError(message: String) {
        try {
            NativeFatalErrorStore.publish(filesDir, message)
        } catch (e: Exception) {
            Log.e("MainActivity", "Could not publish native fatal error", e)
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

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        refreshTransientLaunchState(intent)
        if (gameStarted && hasPendingTransientLaunchRequest()) {
            Log.w("MainActivity", "Transient launch request delivered to a running game; leaving it pending")
            clearTransientLaunchExtrasFromIntent(intent)
            return
        }
        if (shouldRedirectConsumedTransientLaunch()) {
            redirectConsumedTransientLaunchToSetup()
            return
        }
        clearTransientLaunchExtrasFromIntent(intent)
    }

    // ── SurfaceHolder.Callback ──────────────────────────────
    override fun surfaceCreated(holder: SurfaceHolder) {
        nativeSetSurfaceSize(holder.surfaceFrame.width(), holder.surfaceFrame.height())
        nativeSetSurface(holder.surface)
        updateRoundedCornerTextInsets()

        // Start the engine only once, after the surface is ready
        if (!gameStarted) {
            gameStarted = true
            nativeInitializeDormancyDiagnostics()
            Log.i("DXX-Automate", "Game surface created, gameStarted=true")
            consumeTransientLaunchToken()

            // If launched with automation intent extras (from launcher script
            // executor), load the script automatically once the engine starts
            if (BuildConfig.DEBUG) {
                val autoScript = intent.getStringExtra("automation_script")
                val autoStep = intent.getIntExtra("automation_start_step", 0)
                val autoRunId = intent.getStringExtra("automation_run_id").orEmpty()
                if (!autoScript.isNullOrEmpty()) {
                    val resolved =
                        if (autoScript.startsWith("/")) {
                            autoScript
                        } else {
                            filesDir.absolutePath + "/" + autoScript
                        }
                    Log.i(
                        "DXX-Automate",
                        "Intent automation: script=$resolved start_step=$autoStep run_id=$autoRunId",
                    )
                    nativeLoadAutomationScript(resolved, autoStep, autoRunId)
                }
            }

            // AF/MSAA now persist in descent.cfg and are loaded by
            // ReadConfigFile() -> ogl_aniso_level / ogl_msaa_samples

            startupScope.launch {
                try {
                    playlistPreparation?.await()
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    Log.e("MainActivity", "Audio playlist preparation failed", e)
                }
                Thread {
                    startGame()
                }.start()

                warpButtonOverlay?.startPolling()
            }
        }
    }

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int,
    ) {
        nativeSetSurfaceSize(width, height)
        nativeSetSurface(holder.surface)
        updateRoundedCornerTextInsets()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSetSurfaceSize(0, 0)
        nativeSetSurface(null)
    }

    // ── Lifecycle ────────────────────────────────────────────
    private var backgroundPauseApplied = false

    // UI polling has not started until the first onResume call
    private var uiWorkSuspended = true

    private fun requestMinimizeAutosave() {
        if (gameStarted) {
            nativeQueueMinimizeAutosave()
        }
    }

    private fun publishDormancyUiPollCounters() {
        if (!gameStarted) return
        val counters = DormancyDiagnostics.snapshot()
        nativeUpdateDormancyUiPollCounters(counters[0], counters[1])
    }

    fun onNativeMultiplayerDormancyDisconnected() {
        runOnUiThread {
            DebugLog.log(DebugLogCategory.DORMANCY, "multiplayer background engine disconnect completed")
            RuntimeGameStateBridge.disconnect()
            com.dxxredux.app.multiplayer.MultiplayerForegroundService
                .stop(this)
        }
    }

    private fun suspendUiWork() {
        if (uiWorkSuspended) return
        uiWorkSuspended = true
        overlayPoller.removeCallbacksAndMessages(null)
        warpButtonOverlay?.suspendPolling()
        videoInfoOverlay?.suspendPolling()
        netStatsOverlay?.suspendPolling()
        netEventsOverlay?.suspendPolling()
        keyboardPollRunnable?.let { window.decorView.removeCallbacks(it) }
        keyboardPollRunnable = null
    }

    private fun resumeUiWork() {
        if (!uiWorkSuspended) return
        uiWorkSuspended = false
        warpButtonOverlay?.resumePolling()
        videoInfoOverlay?.resumePolling()
        netStatsOverlay?.resumePolling()
        netEventsOverlay?.resumePolling()
        startOverlayPolling()
    }

    private fun applyBackgroundPause() {
        if (gameStarted && !backgroundPauseApplied) {
            publishDormancyUiPollCounters()
            nativeOnPause()
            backgroundPauseApplied = true
        }
    }

    override fun onPause() {
        super.onPause()
        requestMinimizeAutosave()
    }

    override fun onUserLeaveHint() {
        super.onUserLeaveHint()
        requestMinimizeAutosave()
    }

    override fun onStop() {
        super.onStop()
        isActivityResumed = false
        gyroManager?.pause()
        suspendUiWork()
        gamepadButtonEdgeTracker.clear()
        if (::inputMixer.isInitialized) inputMixer.releaseAll()
        resetTouchOverlayForSuspend()
        // Inject Escape so the engine opens its pause / game menu.
        // This pauses a single-player game while the app is in the background.
        applyBackgroundPause()
        RuntimeGameStateBridge.noteActivityVisibility(background = true)
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        if (level == TRIM_MEMORY_UI_HIDDEN) {
            suspendUiWork()
            applyBackgroundPause()
            RuntimeGameStateBridge.noteActivityVisibility(background = true)
        }
    }

    override fun onResume() {
        super.onResume()
        writeGameActivityState(this, gameVariantId)
        backgroundPauseApplied = false
        isActivityResumed = true
        gyroManager?.resume()
        // Resume music that was paused when backgrounded
        if (gameStarted) {
            RuntimeGameStateBridge.noteActivityVisibility(background = false)
            publishDormancyUiPollCounters()
            nativeOnResume()
        }
        // Re-read preference (user may have toggled in SetupActivity)
        val prefs = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
        loadMetaBindings()
        // Default to enabled when touch controls are needed or when the no-touch menu layout is active.
        val hasController = hasWorkingControllerDevice()
        overlayEnabled =
            prefs.getBoolean(
                "touch_overlay_enabled",
                defaultTouchOverlayEnabled(hasTouchscreen = !gamepadOnlyMode, hasController = hasController),
            )
        syncDebugLogPrefs()
        applySkipIntroPref(prefs)
        applyCoopIndicatorPrefs(prefs)
        applyHeadlightDefaultPrefs(prefs)
        applyDemoRecordingPref()
        applyGraphicsDebugPrefs(prefs)
        applyGraphicsSettingsPrefs(prefs)
        updateRoundedCornerTextInsets()
        // Restore only work that was active before this Activity stopped
        resumeUiWork()
    }

    private fun syncDebugLogPrefs() {
        for (cat in 0 until DebugLogCategory.COUNT) {
            val enabled = DebugLog.isCategoryEnabled(this, cat)
            DebugLog.setCategoryEnabled(this, cat, enabled)
            nativeSetDebugLogEnabled(cat, enabled)
        }
        val prefs = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
        nativeSetAutomaticSlowdownCapture(prefs.getBoolean(PREF_AUTOMATIC_SLOWDOWN_CAPTURE, false))
    }

    private fun applyGraphicsDebugPrefs(prefs: android.content.SharedPreferences) {
        videoInfoOverlay?.applyLauncherPrefs(prefs)
        try {
            if (BuildConfig.DEBUG && gamepadOnlyMode) {
                nativeSetDebugFlag("merged_wall_mode", 0)
                videoInfoOverlay?.show()
            }
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    private fun applyGraphicsSettingsPrefs(prefs: android.content.SharedPreferences) {
        val generation = prefs.getLong(PREF_GRAPHICS_SETTINGS_GENERATION, 0L)
        if (generation == lastAppliedGraphicsSettingsGeneration) return
        if (!gameStarted) return

        try {
            val options = readGraphicsConfigSnapshot(filesDir, gameVariantId).toMutableList()
            if (prefs.contains(PREF_GRAPHICS_ALPHA_EFFECTS)) {
                options.add(
                    "alpha_effects" to if (prefs.getBoolean(PREF_GRAPHICS_ALPHA_EFFECTS, false)) 1 else 0,
                )
            }
            if (prefs.contains(PREF_GRAPHICS_DYNLIGHT_COLOR)) {
                options.add(
                    "dynlight_color" to if (prefs.getBoolean(PREF_GRAPHICS_DYNLIGHT_COLOR, false)) 1 else 0,
                )
            }
            if (applyGraphicsOptionSnapshot(options, ::nativeApplyLauncherGraphicsOption)) {
                lastAppliedGraphicsSettingsGeneration = generation
            }
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    @Suppress("NewApi")
    private fun roundedCornerLeftInsetPx(corner: RoundedCorner?): Int {
        if (corner == null || corner.radius <= 0) return 0
        return maxOf(corner.radius, corner.center.x)
    }

    @Suppress("NewApi")
    private fun roundedCornerRightInsetPx(
        width: Int,
        corner: RoundedCorner?,
    ): Int {
        if (corner == null || corner.radius <= 0) return 0
        return maxOf(corner.radius, width - corner.center.x)
    }

    private fun updateRoundedCornerTextInsets() {
        val decorView = window.decorView
        val width =
            when {
                ::gameSurfaceView.isInitialized && gameSurfaceView.width > 0 -> gameSurfaceView.width
                decorView.width > 0 -> decorView.width
                else -> 0
            }
        val height =
            when {
                ::gameSurfaceView.isInitialized && gameSurfaceView.height > 0 -> gameSurfaceView.height
                decorView.height > 0 -> decorView.height
                else -> 0
            }
        if (width <= 0 || height <= 0) {
            decorView.post { updateRoundedCornerTextInsets() }
            return
        }

        val fallback = (width * 0.05f).roundToInt().coerceAtLeast(1)
        var topLeft = 0
        var bottomLeft = 0
        var topRight = 0
        var bottomRight = 0
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val insets = decorView.rootWindowInsets
            topLeft = roundedCornerLeftInsetPx(insets?.getRoundedCorner(RoundedCorner.POSITION_TOP_LEFT))
            bottomLeft = roundedCornerLeftInsetPx(insets?.getRoundedCorner(RoundedCorner.POSITION_BOTTOM_LEFT))
            topRight = roundedCornerRightInsetPx(width, insets?.getRoundedCorner(RoundedCorner.POSITION_TOP_RIGHT))
            bottomRight =
                roundedCornerRightInsetPx(width, insets?.getRoundedCorner(RoundedCorner.POSITION_BOTTOM_RIGHT))
        }
        if (topLeft <= 0) topLeft = fallback
        if (bottomLeft <= 0) bottomLeft = fallback
        if (topRight <= 0) topRight = fallback
        if (bottomRight <= 0) bottomRight = fallback
        try {
            nativeSetRoundedCornerTextInsets(width, height, topLeft, bottomLeft, topRight, bottomRight)
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    private fun applySkipIntroPref(prefs: android.content.SharedPreferences) {
        val skipIntro = isMultiplayerGame || prefs.getBoolean(PREF_SKIP_INTRO_MOVIE, false)
        try {
            nativeSetSkipIntroMovie(skipIntro)
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

    private fun applyHeadlightDefaultPrefs(prefs: android.content.SharedPreferences) {
        try {
            nativeSetHeadlightOffByDefaultQol(
                prefs.getBoolean(PREF_HEADLIGHT_OFF_BY_DEFAULT, true),
            )
        } catch (_: Exception) {
            // JNI may not be ready yet when the activity is first coming up
        }
    }

    private fun applyDemoRecordingPref() {
        try {
            val prefs = getSharedPreferences("launcher_prefs", MODE_PRIVATE)
            nativeSetDemoRecordPerFrameState(prefs.getBoolean(PREF_DEMO_RECORD_PER_FRAME_STATE, false))
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
                    nativeOpenOverlayPauseIfSafe()
                } catch (_: Exception) {
                    false
                }
            return
        }

        if (!adminTrayPausedGame) return
        try {
            nativeCloseOverlayPauseIfOwned()
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

    private fun openGameMenuFromControllerSettings() {
        val pauseWindowOwnedByTray = adminTrayPausedGame
        if (pauseWindowOwnedByTray) adminTrayPausedGame = false
        closeControllerSettingsStack()
        if (!openGameMenuSafely() && pauseWindowOwnedByTray) {
            try {
                nativeCloseOverlayPauseIfOwned()
            } catch (_: Exception) {
            }
        }
    }

    private fun openGameMenuSafely(): Boolean {
        if (adminTrayPausedGame) {
            try {
                nativeCloseOverlayPauseIfOwned()
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
        if (opened) return true

        nativeKeyEvent(0, KeyEvent.KEYCODE_ESCAPE, 0)
        nativeKeyEvent(1, KeyEvent.KEYCODE_ESCAPE, 0)
        return false
    }

    private fun startOverlayPolling() {
        overlayPoller.removeCallbacksAndMessages(null)
        val pollRunnable =
            object : Runnable {
                override fun run() {
                    DormancyDiagnostics.recordCentralOverlayPoll()
                    publishDormancyUiPollCounters()
                    val pollStartNs = android.os.SystemClock.elapsedRealtimeNanos()
                    var profileInGame = false
                    var profileAutomap = false
                    var profileHadError = false
                    if (gameStarted) {
                        try {
                            val inGame = nativeIsInGame()
                            profileInGame = inGame
                            val automap =
                                try {
                                    nativeIsAutomapActive()
                                } catch (_: Exception) {
                                    false
                                }
                            profileAutomap = automap
                            val screenAdvanceState =
                                try {
                                    nativeGetScreenAdvanceState()
                                } catch (_: Exception) {
                                    0L
                                }
                            // Keep these kind values synchronized with android_screen_advance.h.
                            val screenAdvanceKind = (screenAdvanceState and 0xffL).toInt()
                            val screenAdvanceGeneration = screenAdvanceState ushr 32
                            val introActive =
                                try {
                                    nativeIsIntroActive()
                                } catch (_: Exception) {
                                    false
                                }
                            val playerDead = screenAdvanceKind == SCREEN_ADVANCE_DEATH
                            val endlevel = screenAdvanceKind == SCREEN_ADVANCE_ENDLEVEL
                            val saveloadMenu =
                                try {
                                    nativeIsSaveLoadMenuActive()
                                } catch (_: Exception) {
                                    false
                                }
                            val demoRecording =
                                try {
                                    nativeIsDemoRecordingActive()
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
                            val controllerMenuOpen = touchOverlay.isControllerMenuOpen()
                            // During death or endlevel, show skip/continue button instead of controls
                            val showCutsceneButton = screenAdvanceKind != SCREEN_ADVANCE_NONE
                            // Keep the overlay visible while the settings tray owns, or is still
                            // unwinding, its pause state so standalone overlays do not flicker off.
                            val shouldShow =
                                shouldShowTouchOverlay(
                                    inGame = inGame,
                                    overlayEnabled = overlayEnabled,
                                    playerDead = playerDead,
                                    endlevel = endlevel,
                                    automap = automap,
                                    controllerMenuOpen = controllerMenuOpen,
                                    settingsTrayVisible = settingsTrayVisible,
                                )
                            val wasActive = touchOverlay.isActive
                            touchOverlay.isActive = shouldShow
                            touchOverlay.automapActive = automap
                            touchOverlay.updateDemoRecordingState(demoRecording)
                            if (!overlayEnabled && controllerMenuOpen && shouldShow && !wasActive) {
                                logSelectRouting(
                                    "forcing touch overlay visible for controller menu " +
                                        "menuOpen=$controllerMenuOpen trayVisible=$settingsTrayVisible",
                                )
                            }
                            // Show/hide the native transient-screen action, intro preference, or BACK.
                            val levelComplete = screenAdvanceKind == SCREEN_ADVANCE_LEVELCOMPLETE
                            if (saveloadMenu) {
                                skipButton.screenAdvanceGeneration = null
                                skipButton.bigLabel = false
                                skipButton.label = "BACK"
                                skipButton.visibility = View.VISIBLE
                            } else if (levelComplete) {
                                skipButton.screenAdvanceGeneration = screenAdvanceGeneration
                                skipButton.bigLabel = false
                                skipButton.label = "NEXT"
                                skipButton.visibility = View.VISIBLE
                            } else if (introActive) {
                                skipButton.screenAdvanceGeneration = null
                                skipButton.bigLabel = true
                                skipButton.label = "Skip every launch"
                                skipButton.visibility = View.VISIBLE
                            } else if (showCutsceneButton && !shouldShow) {
                                skipButton.screenAdvanceGeneration = screenAdvanceGeneration
                                skipButton.bigLabel = false
                                skipButton.label = if (playerDead) "CONTINUE" else "SKIP"
                                skipButton.visibility = View.VISIBLE
                            } else {
                                skipButton.screenAdvanceGeneration = null
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
                            acceptJoinButton.callsign = joinCallsign
                            acceptJoinButton.visibility = if (joinCallsign.isNotEmpty()) View.VISIBLE else View.GONE
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
                            profileHadError = true
                            touchOverlay.isActive = false
                            touchOverlay.automapActive = false
                            touchOverlay.updateDemoRecordingState(false)
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
                        touchOverlay.updateDemoRecordingState(false)
                        skipButton.visibility = View.GONE
                        startGameButton.visibility = View.GONE
                        acceptJoinButton.visibility = View.GONE
                        netStatsOverlay?.hide()
                        netEventsOverlay?.hide()
                        videoInfoOverlay?.hide()
                        warpButtonOverlay?.stopPolling()
                        netEventsManualToggle = false
                    }
                    recordOverlayPollProfile(
                        durationUs = (android.os.SystemClock.elapsedRealtimeNanos() - pollStartNs) / 1_000L,
                        inGame = profileInGame,
                        automap = profileAutomap,
                        overlayVisible = touchOverlay.isActive,
                        netEventsVisible = netEventsOverlay?.visibility == View.VISIBLE,
                        videoInfoVisible = videoInfoOverlay?.visibility == View.VISIBLE,
                        hadError = profileHadError,
                    )
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
                publishDormancyUiPollCounters()
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
                val runId = intent.getStringExtra("run_id").orEmpty()
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
                Log.i("DXX-Automate", "Loading automation script: $resolvedPath run_id=$runId")
                nativeLoadAutomationScript(resolvedPath, 0, runId)
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

            DynamicReceiverPolicy.registerDebugExternal(
                this,
                introspectReceiver,
                IntentFilter("com.dxxredux.INTROSPECT"),
            )
            DynamicReceiverPolicy.registerDebugExternal(
                this,
                automateReceiver,
                IntentFilter("com.dxxredux.AUTOMATE"),
            )
            DynamicReceiverPolicy.registerDebugExternal(
                this,
                gameCommandReceiver,
                IntentFilter("com.dxxredux.GAME_COMMAND"),
            )
        }
    }

    override fun onDestroy() {
        window.decorView.removeCallbacks(musicStateRefreshRunnable)
        routeMetadataJob?.cancel()
        startupScope.cancel()
        clearGameActivityState(this)
        AudioSourceManager.closeActivePfds()
        RuntimeGameStateBridge.disconnect()
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

        if (touchDiagLogCount < 160 && action != 1) {
            touchDiagLogCount++
            val loc = IntArray(2)
            view.getLocationOnScreen(loc)
            val surfaceFrame = gameSurfaceView.holder.surfaceFrame
            val decor = window.decorView
            DebugLog.log(
                DebugLogCategory.GAME,
                "[touch-java] action=${touchDiagActionName(action)} " +
                    "view=${view.width}x${view.height}@${loc[0]},${loc[1]} " +
                    "surfaceFrame=${surfaceFrame.left},${surfaceFrame.top}," +
                    "${surfaceFrame.width()}x${surfaceFrame.height()} " +
                    "decor=${decor.width}x${decor.height} " +
                    "x=${event.x.roundToInt()} y=${event.y.roundToInt()} " +
                    "raw=${event.rawX.roundToInt()},${event.rawY.roundToInt()} " +
                    "norm=${String.format(Locale.US, "%.4f", normX)}," +
                    "${String.format(Locale.US, "%.4f", normY)} " +
                    "pointer=${event.getPointerId(event.actionIndex)}",
            )
        }

        nativeTouchEvent(action, normX, normY)
        return true
    }

    private fun touchDiagActionName(action: Int): String =
        when (action) {
            0 -> "down"
            1 -> "move"
            2 -> "up"
            else -> action.toString()
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
        resetTouchOverlayForSuspend()
        val intent = Intent(this, SetupActivity::class.java)
        intent.putExtra("gameRunning", true)
        startActivity(intent)
    }

    private fun refreshTransientLaunchState(sourceIntent: Intent) {
        val launchGame = sourceIntent.getStringExtra("game") ?: gameVariantId
        pendingInputDemoReplayPath =
            sourceIntent.getStringExtra("input_demo_replay")?.takeIf { it.isNotBlank() }
        pendingResumeSavePath =
            sourceIntent.getStringExtra("resume_save_path")?.takeIf { it.isNotBlank() }
        pendingResumeCallsign =
            sourceIntent.getStringExtra("resume_callsign")?.takeIf { it.isNotBlank() }
        pendingPilotCallsign =
            sourceIntent.getStringExtra("pilot_callsign")?.takeIf { it.isNotBlank() }
        pendingTransientLaunchToken =
            sourceIntent.getStringExtra(EXTRA_TRANSIENT_LAUNCH_TOKEN)?.takeIf { it.isNotBlank() }
        if (pendingInputDemoReplayPath == null && pendingResumeSavePath == null) {
            readPendingResumeLaunch(this, launchGame)?.let { pending ->
                pendingResumeSavePath = pending.savePath
                pendingResumeCallsign = pending.callsign
                pendingTransientLaunchToken = pending.token
                Log.i(
                    "MainActivity",
                    "Using pending resume launch file: game=${pending.game} path=${pending.savePath} " +
                        "callsign=${pending.callsign ?: ""} token=${pending.token}",
                )
            }
        } else if (pendingResumeSavePath != null) {
            Log.i(
                "MainActivity",
                "Using resume launch extras: game=$launchGame path=$pendingResumeSavePath " +
                    "callsign=${pendingResumeCallsign ?: ""} token=${pendingTransientLaunchToken ?: ""}",
            )
        }
    }

    private fun hasPendingTransientLaunchRequest(): Boolean =
        pendingInputDemoReplayPath != null || pendingResumeSavePath != null

    private fun shouldRedirectConsumedTransientLaunch(): Boolean {
        if (!hasPendingTransientLaunchRequest()) return false
        val token = pendingTransientLaunchToken ?: return false
        val lastConsumed =
            getSharedPreferences("dxx_prefs", MODE_PRIVATE)
                .getString(PREF_LAST_CONSUMED_TRANSIENT_LAUNCH_TOKEN, null)
        return token == lastConsumed
    }

    private fun consumeTransientLaunchToken() {
        if (pendingInputDemoReplayPath == null && pendingResumeSavePath == null) return
        val token = pendingTransientLaunchToken ?: return
        getSharedPreferences("dxx_prefs", MODE_PRIVATE)
            .edit()
            .putString(PREF_LAST_CONSUMED_TRANSIENT_LAUNCH_TOKEN, token)
            .apply()
    }

    private fun clearTransientLaunchExtrasFromIntent(sourceIntent: Intent) {
        sourceIntent.removeExtra("input_demo_replay")
        sourceIntent.removeExtra("resume_save_path")
        sourceIntent.removeExtra("resume_callsign")
        sourceIntent.removeExtra("pilot_callsign")
        sourceIntent.removeExtra(EXTRA_TRANSIENT_LAUNCH_TOKEN)
        setIntent(sourceIntent)
    }

    private fun redirectConsumedTransientLaunchToSetup() {
        clearPendingResumeLaunch(this, pendingTransientLaunchToken)
        startActivity(
            Intent(this, SetupActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP),
        )
        finish()
    }

    @Suppress("unused")
    fun consumeInputDemoReplayPath(): String? = pendingInputDemoReplayPath.also { pendingInputDemoReplayPath = null }

    @Suppress("unused")
    fun consumeResumeSavePath(): String? =
        pendingResumeSavePath.also { savePath ->
            if (savePath != null) clearPendingResumeLaunch(this, pendingTransientLaunchToken)
            pendingResumeSavePath = null
        }

    @Suppress("unused")
    fun consumeResumeCallsign(): String? = pendingResumeCallsign.also { pendingResumeCallsign = null }

    @Suppress("unused")
    fun consumePilotCallsign(): String? = pendingPilotCallsign.also { pendingPilotCallsign = null }

    private fun resetTouchOverlayForSuspend() {
        if (!::touchOverlay.isInitialized) return
        if (touchOverlay.isActive) {
            try {
                nativeSetJoystickEnabled(false)
            } catch (_: Exception) {
            }
        }
        touchOverlay.isActive = false
        touchOverlay.automapActive = false
    }

    /** Toggle the automap by injecting a TAB key press/release. */
    private fun toggleAutomap() {
        // Immediately flip the overlay to avoid polling lag
        touchOverlay.automapActive = !touchOverlay.automapActive
        nativeKeyEvent(0, KeyEvent.KEYCODE_TAB, '\t'.code)
        nativeKeyEvent(1, KeyEvent.KEYCODE_TAB, 0)
    }

    /** Load controller meta-action bindings from controller_config.json. */
    private fun loadMetaBindings() {
        buttonMetaBindings = emptyMap()
        controllerBoundActions = emptySet()
        dpadMetaBindings = emptyMap()
        halfAxisCombiners = emptyList()
        controllerAxisExponents = defaultControllerAxisExponents()
        mixerButtonMap = emptyMap()

        val file = File(filesDir, "controller_config.json")
        if (!file.exists()) return
        try {
            val json = JSONObject(file.readText())
            if (json.has("bindings")) {
                val bindingsObj = json.getJSONObject("bindings")
                val bindings = mutableMapOf<String, String>()
                for (key in bindingsObj.keys()) bindings[key] = bindingsObj.getString(key)
                controllerBoundActions = controllerConfigBoundActionBindings(bindings)
            }
            json.optJSONObject("axis_exponents")?.let { exponentsObj ->
                val loaded = mutableMapOf<String, Float>()
                for (key in exponentsObj.keys()) loaded[key] = exponentsObj.getDouble(key).toFloat()
                controllerAxisExponents = clampedControllerAxisExponents(loaded)
            }
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
        val effectiveConfig = config.copy(enabled = gyroRuntimeState.effectiveEnabled)
        manager.setConfig(effectiveConfig)
        touchOverlay.updateGyroState(
            configured = gyroRuntimeState.configured,
            active = gyroRuntimeState.activeInGame,
        )
        if (isActivityResumed) {
            if (effectiveConfig.enabled) {
                manager.resume()
            } else {
                manager.pause()
            }
        }
    }

    private fun toggleGyroInGame() {
        val nextState = toggledGyroRuntimeState(gyroRuntimeState)
        if (nextState == gyroRuntimeState) return
        gyroRuntimeState = nextState
        applyGyroConfig(activeTouchLayout.gyro)
    }

    private fun dispatchMetaAction(
        actionId: Int,
        pressed: Boolean,
    ) {
        if (actionId == TouchBindings.META_GYRO_TOGGLE) {
            if (pressed) toggleGyroInGame()
            return
        }
        if (actionId == TouchBindings.META_MENU_CYCLE) {
            logSelectRouting(
                "meta menu pressed=$pressed active=${touchOverlay.isActive} " +
                    "menuOpen=${touchOverlay.isControllerMenuOpen()} tray=${touchOverlay.isAdminTrayOpen()} " +
                    "overlayEnabled=$overlayEnabled music=${musicPanel != null} " +
                    "video=${videoInfoOverlay?.visibility == View.VISIBLE}",
            )
            if (
                shouldCloseControllerSettingsStackForMenu(
                    pressed = pressed,
                    settingsRootVisible = touchOverlay.isAdminTrayOpen(),
                    musicPanelVisible = musicPanel != null,
                    videoInfoVisible = videoInfoOverlay?.visibility == View.VISIBLE,
                )
            ) {
                closeControllerSettingsStack()
            } else if (pressed) {
                touchOverlay.cycleControllerMenu()
            }
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
        if (handleControllerSettingsChildKey(keyCode, action)) {
            return
        }
        if (touchOverlay.handleControllerMenuKey(keyCode, action)) {
            return
        }
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

    private fun handleOverlayBypassMetaKey(
        keyCode: Int,
        action: Int,
    ): Boolean {
        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn < 0) return false
        val metaId = buttonMetaBindings[joyBtn] ?: return false
        if (metaId != TouchBindings.META_MENU_CYCLE) return false
        dispatchMetaAction(metaId, action == 0)
        return true
    }

    private fun isControllerSource(source: Int): Boolean =
        source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
            source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD

    private fun hasWorkingControllerDevice(): Boolean =
        InputDevice.getDeviceIds().any { id ->
            val device = InputDevice.getDevice(id) ?: return@any false
            val source = device.sources
            source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
        }

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

    private fun controllerAxisValue(
        axisKey: String,
        value: Float,
    ): Float = applyControllerAxisExponent(value, controllerAxisExponents[axisKey] ?: DEFAULT_CONTROLLER_AXIS_EXPONENT)

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
                        if (shouldConsumeKeyboardBack(
                                gameSurfaceView.keyboardActive,
                                isKeyboardImeVisibleNow(),
                                gamepadOnlyMode,
                            )
                        ) {
                            hideKeyboard()
                            return true
                        }
                        deactivateKeyboardProxy()
                        return super.dispatchKeyEvent(event)
                    } else if (gameSurfaceView.keyboardActive) {
                        return true
                    }
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

        if (handleOverlayBypassMetaKey(keyCode, 0)) return true

        if (handleControllerSettingsChildKey(keyCode, 0)) return true

        if (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT ||
            keyCode == KeyEvent.KEYCODE_BUTTON_START ||
            keyCode == KeyEvent.KEYCODE_BACK
        ) {
            logSelectRouting(
                "down kc=$keyCode src=${event.source} menuOpen=${touchOverlay.isControllerMenuOpen()} " +
                    "tray=${touchOverlay.isAdminTrayOpen()} overlay=${touchOverlay.isActive} " +
                    "automap=${touchOverlay.automapActive} inGame=${nativeIsInGame()} focus=${gameSurfaceView.hasFocus()}",
            )
        }

        if (keyCode == KeyEvent.KEYCODE_BACK && isControllerSource(event.source)) {
            logSelectRouting("down back-controller -> fallthrough nativeKeyEvent")
        }

        val inGame = nativeIsInGame()
        if (
            shouldRouteControllerBToNativeBack(
                keyCode = keyCode,
                isControllerEvent = isControllerSource(event.source),
                nativeMenuFront = gameStarted && !inGame,
                controllerMenuOpen = touchOverlay.isControllerMenuOpen(),
                adminTrayOpen = touchOverlay.isAdminTrayOpen(),
            )
        ) {
            nativeKeyEvent(0, KeyEvent.KEYCODE_BACK, 0)
            return true
        }

        if (touchOverlay.handleControllerMenuKey(keyCode, 0)) return true

        // Gamepad face / shoulder buttons -> mixer or meta action
        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
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

        if (handleOverlayBypassMetaKey(keyCode, 1)) return true

        if (handleControllerSettingsChildKey(keyCode, 1)) return true

        if (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT ||
            keyCode == KeyEvent.KEYCODE_BUTTON_START ||
            keyCode == KeyEvent.KEYCODE_BACK
        ) {
            logSelectRouting(
                "up kc=$keyCode src=${event.source} menuOpen=${touchOverlay.isControllerMenuOpen()} " +
                    "tray=${touchOverlay.isAdminTrayOpen()} overlay=${touchOverlay.isActive} inGame=${nativeIsInGame()}",
            )
        }

        val inGame = nativeIsInGame()
        if (
            shouldRouteControllerBToNativeBack(
                keyCode = keyCode,
                isControllerEvent = isControllerSource(event.source),
                nativeMenuFront = gameStarted && !inGame,
                controllerMenuOpen = touchOverlay.isControllerMenuOpen(),
                adminTrayOpen = touchOverlay.isAdminTrayOpen(),
            )
        ) {
            nativeKeyEvent(1, KeyEvent.KEYCODE_BACK, 0)
            return true
        }

        if (touchOverlay.handleControllerMenuKey(keyCode, 1)) return true

        val joyBtn = gamepadButtonIndex(keyCode)
        if (joyBtn >= 0) {
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
            val lx = controllerAxisValue("LS_X", event.getAxisValue(MotionEvent.AXIS_X))
            val ly = controllerAxisValue("LS_Y", event.getAxisValue(MotionEvent.AXIS_Y))
            val rx = controllerAxisValue("RS_X", event.getAxisValue(MotionEvent.AXIS_Z))
            val ry = controllerAxisValue("RS_Y", event.getAxisValue(MotionEvent.AXIS_RZ))
            val lt = controllerAxisValue("LT", event.getAxisValue(MotionEvent.AXIS_LTRIGGER))
            val rt = controllerAxisValue("RT", event.getAxisValue(MotionEvent.AXIS_RTRIGGER))
            rawAxisValues[0] = lx
            rawAxisValues[1] = ly
            rawAxisValues[2] = rx
            rawAxisValues[3] = ry
            rawAxisValues[4] = lt
            rawAxisValues[5] = rt
            val controllerAxes =
                mutableMapOf(
                    0 to lx,
                    1 to ly,
                    2 to rx,
                    3 to ry,
                    4 to lt,
                    5 to rt,
                )
            // Compute half-axis combiner virtual axes
            for ((virt, posSource, negSource) in halfAxisCombiners) {
                val pos = if (posSource in 0..5) rawAxisValues[posSource] else 0f
                val neg = if (negSource in 0..5) rawAxisValues[negSource] else 0f
                controllerAxes[virt] = (pos - neg).coerceIn(-1f, 1f)
            }
            inputMixer.setAxes("ctrl", controllerAxes)

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

    private fun sampleVisibleKeyboardHeightPx(imeBottomHint: Int? = null): Int {
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
        val visibleFrame = Rect()
        decorView.getWindowVisibleDisplayFrame(visibleFrame)
        val systemBottom =
            ViewCompat
                .getRootWindowInsets(decorView)
                ?.getInsetsIgnoringVisibility(WindowInsetsCompat.Type.systemBars())
                ?.bottom ?: 0
        val bottomOcclusion = (screenHeight - visibleFrame.bottom).coerceAtLeast(0)
        val fallbackBottom = (bottomOcclusion - systemBottom).coerceAtLeast(0)
        return maxOf(imeBottom, fallbackBottom)
    }

    private fun sampleKeyboardHeightPx(imeBottomHint: Int? = null): Int {
        val decorView = window.decorView
        val screenHeight = keyboardReferenceHeightPx()
        val visibleBottom = sampleVisibleKeyboardHeightPx(imeBottomHint)
        val imeStableBottom = imeIgnoringVisibilityHeightPx(decorView)
        val tvFallbackBottom =
            if (visibleBottom == 0 && imeStableBottom == 0) {
                tvKeyboardFallbackHeightPx(screenHeight)
            } else {
                0
            }
        return maxOf(visibleBottom, imeStableBottom, tvFallbackBottom)
    }

    /** Poll for IME height via rootWindowInsets.  With adjustNothing
     *  the insets callbacks don't fire, so we poll after requesting
     *  the keyboard and stop once we detect a non-zero IME height.
     *  Some TV keyboards also leave Type.ime() at zero, so fall back
     *  to the visible window frame bottom when needed. */
    private fun pollKeyboardHeight(attemptsLeft: Int) {
        if (attemptsLeft <= 0) return
        val visibleImeHeight = sampleVisibleKeyboardHeightPx()
        keyboardImeVisible = visibleImeHeight > 0 || (gamepadOnlyMode && gameSurfaceView.keyboardActive)
        if (!keyboardImeVisible) {
            val r = Runnable { pollKeyboardHeight(attemptsLeft - 1) }
            keyboardPollRunnable = r
            window.decorView.postDelayed(r, 100)
            return
        }
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
    fun showKeyboard(
        inputType: Int,
        initialText: String?,
    ) {
        runOnUiThread {
            gameSurfaceView.currentInputType =
                when (inputType) {
                    2 -> InputType.TYPE_CLASS_NUMBER
                    else -> InputType.TYPE_CLASS_TEXT
                }
            keyboardInputView.currentInputType = gameSurfaceView.currentInputType
            val text = initialText.orEmpty()
            keyboardInputView.setText(text)
            keyboardInputView.setSelection(text.length)
            hatXState = 0
            hatYState = 0
            keyboardImeVisible = gamepadOnlyMode
            gameSurfaceView.keyboardActive = true
            keyboardInputView.keyboardActive = true
            keyboardInputView.requestFocus()
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.restartInput(keyboardInputView)
            imm.showSoftInput(keyboardInputView, 0)
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
            deactivateKeyboardProxy()
            WindowInsetsControllerCompat(window, keyboardInputView)
                .hide(WindowInsetsCompat.Type.ime())
            nativeSetKeyboardHeight(0, keyboardReferenceHeightPx())
        }
    }

    private fun isKeyboardImeVisibleNow(): Boolean {
        keyboardImeVisible = sampleVisibleKeyboardHeightPx() > 0 || (gamepadOnlyMode && gameSurfaceView.keyboardActive)
        return keyboardImeVisible
    }

    private fun deactivateKeyboardProxy() {
        keyboardImeVisible = false
        gameSurfaceView.keyboardActive = false
        keyboardInputView.keyboardActive = false
        keyboardInputView.clearFocus()
        gameSurfaceView.requestFocus()
    }

    // ── Music overlay helpers ─────────────────────────────────
    private fun scheduleMusicStateRefresh() {
        remainingMusicStateRefreshes = 40
        window.decorView.removeCallbacks(musicStateRefreshRunnable)
        window.decorView.postDelayed(musicStateRefreshRunnable, 120L)
    }

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
        videoInfoOverlay?.hide()
        if (musicPanel != null) return // already showing
        syncAdminTrayPause(true)
        val panel =
            MusicControlPanel(this, {
                musicPanel?.let { mp ->
                    (gameSurfaceView.parent as? FrameLayout)?.removeView(mp)
                }
                musicPanel = null
                syncAdminTrayPause(touchOverlay.isAdminTrayOpen())
            }, {
                scheduleMusicStateRefresh()
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

    private fun handleControllerSettingsChildKey(
        keyCode: Int,
        action: Int,
    ): Boolean {
        musicPanel?.let { panel ->
            if (panel.handleControllerKey(keyCode, action)) {
                return true
            }
        }

        videoInfoOverlay?.let { overlay ->
            if (overlay.handleControllerKey(keyCode, action)) {
                return true
            }
        }

        return false
    }

    private fun closeControllerSettingsStack() {
        dismissMusicPanel()
        videoInfoOverlay?.hide()
        touchOverlay.closeControllerMenu()
    }

    private fun dismissMusicPanel() {
        musicPanel?.let { mp ->
            (gameSurfaceView.parent as? FrameLayout)?.removeView(mp)
        }
        musicPanel = null
        syncAdminTrayPause(touchOverlay.isAdminTrayOpen())
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

    @Suppress("unused")
    fun debugLogForcedFromNative(
        category: Int,
        message: String,
    ) {
        DebugLog.logForced(this, category, message)
    }

    @Suppress("unused")
    fun debugLogBatchFromNative(
        category: Int,
        payload: String,
    ) {
        DebugLog.logBatch(category, payload)
    }

    @Suppress("unused")
    fun debugLogBatchForcedFromNative(
        category: Int,
        payload: String,
    ) {
        DebugLog.logBatchForcedAsync(this, category, payload)
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
            val selection = text?.length ?: 0
            outAttrs.initialSelStart = selection
            outAttrs.initialSelEnd = selection
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
            outAttrs.initialSelStart = 0
            outAttrs.initialSelEnd = 0
            return GameInputConnection(this)
        }
    }

    /**
     * Routes soft-keyboard text input into the engine via JNI.
     * commitText → nativeTextInput (one SDL key pair per character)
     * sendKeyEvent(printable keys) → nativeTextInput / nativeKeyEvent
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

        override fun sendKeyEvent(event: KeyEvent): Boolean {
            val unicodeChar = event.unicodeChar
            val specialKeyCode = imeNativeSpecialKeyCode(event.keyCode)
            val committedCodePoint = imeCommittedCodePointFromKeyEvent(event.keyCode, unicodeChar)

            if (specialKeyCode != null) {
                nativeKeyEvent(
                    if (event.action == KeyEvent.ACTION_DOWN) 0 else 1,
                    specialKeyCode,
                    if (event.action == KeyEvent.ACTION_DOWN && specialKeyCode == KeyEvent.KEYCODE_ENTER) {
                        '\r'.code
                    } else {
                        0
                    },
                )
                return true
            }

            if (committedCodePoint != null) {
                if (event.action == KeyEvent.ACTION_DOWN) {
                    nativeTextInput(committedCodePoint)
                }
                return true
            }

            return super.sendKeyEvent(event)
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
