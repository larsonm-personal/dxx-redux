package com.dxxredux.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import android.content.res.Configuration
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.InputMode
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalInputModeManager
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import androidx.lifecycle.lifecycleScope
import com.dxxredux.app.multiplayer.CoopDesyncLog
import com.dxxredux.app.multiplayer.GameLaunchInfo
import com.dxxredux.app.multiplayer.MatchmakingService
import com.dxxredux.app.multiplayer.MatchmakingStateHolder
import com.dxxredux.app.multiplayer.MultiplayerResumePrefs
import com.dxxredux.app.multiplayer.NetworkConstants
import com.dxxredux.app.multiplayer.PlayGamesAuth
import com.dxxredux.app.multiplayer.readCoopRestoreSlot
import com.dxxredux.app.multiplayer.userLabel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.json.JSONObject
import java.io.File
import java.util.Locale
import java.util.concurrent.atomic.AtomicBoolean

internal enum class LauncherPreparationPhase(
    val wireName: String,
) {
    PREPARING("preparing"),
    PAUSING_METADATA("pausing_metadata"),
    STARTING_GAME("starting_game"),
}

internal class SetupIntrospectionSingleFlight {
    private val running = AtomicBoolean(false)

    fun tryEnter(): Boolean = running.compareAndSet(false, true)

    fun exit() = running.set(false)
}

internal data class LauncherPreparationState(
    val game: String,
    val launchKind: String,
    val phase: LauncherPreparationPhase,
    val startedAtMs: Long,
)

internal const val MULTIPLAYER_LAUNCH_REQUEST_TIMEOUT_MS = 15_000L

internal fun launcherPreparationLabel(state: LauncherPreparationState): String =
    when (state.phase) {
        LauncherPreparationPhase.PREPARING -> {
            when (state.launchKind) {
                "resume" -> "Preparing saved game"
                "multiplayer" -> "Preparing multiplayer game"
                else -> "Preparing game"
            }
        }

        LauncherPreparationPhase.PAUSING_METADATA -> {
            "Pausing background analysis"
        }

        LauncherPreparationPhase.STARTING_GAME -> {
            "Starting ${if (state.game == "d1") "Descent 1" else "Descent 2"}"
        }
    }

internal fun launcherPreparationShowsDialog(state: LauncherPreparationState): Boolean =
    when (state.phase) {
        LauncherPreparationPhase.PREPARING,
        LauncherPreparationPhase.PAUSING_METADATA,
        LauncherPreparationPhase.STARTING_GAME,
        -> true
    }

internal fun formatAboutBuildLine(
    buildType: String,
    commitCount: String,
    shortHash: String,
    nativeDebugBuild: Boolean,
): String {
    val buildLine =
        if (buildType == "dev") {
            "Dev Build"
        } else {
            "Build $commitCount ($shortHash) $buildType"
        }
    return buildLine + if (nativeDebugBuild) " debug" else ""
}

/**
 * Pre-game setup screen built with Jetpack Compose.
 *
 * Shows the readiness status of required and optional game data files
 * for both Descent 2 and Descent 1, instructions for installing
 * missing files, and a button to launch (or return to) the game.
 *
 * This is the launcher activity.
 */
class SetupActivity : ComponentActivity() {
    private val routeMetadataScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private lateinit var routeMetadataCoordinator: RouteMetadataPrecomputeCoordinator
    private var routeMetadataLaunchJob: Job? = null
    private val routeMetadataFocusHandler: (LevelMetadataTarget?) -> Unit = { target ->
        routeMetadataCoordinator.setMetadataViewerFocus(target)
    }

    /** Incremented in onResume so Compose re-checks file status. */
    private val refreshTrigger = mutableIntStateOf(0)
    private val focusResumeTrigger = mutableIntStateOf(0)
    private val launcherControllerNavigationActive = mutableStateOf(false)
    private val launchFailureMessage = mutableStateOf<String?>(null)
    private val launchPreparation = mutableStateOf<LauncherPreparationState?>(null)
    private val pendingPickedImportUris = mutableStateOf<List<Uri>>(emptyList())
    private val resumeOfferRefreshHandler = Handler(Looper.getMainLooper())
    private val resumeOfferRefreshRunnable =
        Runnable {
            RouteMetadataDiagnostics.log(
                "Launcher post-resume refresh trigger=${refreshTrigger.intValue + 1}",
            )
            refreshTrigger.intValue++
        }

    internal fun launchPreparationSnapshot(): LauncherPreparationState? = launchPreparation.value

    private fun beginLaunchPreparation(
        game: String,
        launchKind: String,
    ): Boolean {
        if (launchPreparation.value != null) {
            Log.w("DXX-Setup", "Ignoring duplicate game launch during launcher preparation")
            return false
        }
        launchPreparation.value =
            LauncherPreparationState(
                game = game,
                launchKind = launchKind,
                phase = LauncherPreparationPhase.PREPARING,
                startedAtMs = SystemClock.elapsedRealtime(),
            )
        RouteMetadataDiagnostics.log("Launcher game launch requested game=$game kind=$launchKind")
        return true
    }

    private fun updateLaunchPreparation(phase: LauncherPreparationPhase) {
        launchPreparation.value = launchPreparation.value?.copy(phase = phase)
    }

    private fun finishLaunchPreparation(reason: String) {
        val state = launchPreparation.value ?: return
        RouteMetadataDiagnostics.log(
            "Launcher game launch finished game=${state.game} kind=${state.launchKind} " +
                "reason=$reason elapsed_ms=${SystemClock.elapsedRealtime() - state.startedAtMs}",
        )
        launchPreparation.value = null
    }

    // -- Setup-screen introspection --------------------------------------
    //   adb shell am broadcast -a com.dxxredux.SETUP_INTROSPECT
    //   adb shell run-as com.dxxredux.app cat files/setup_introspect.json
    private val setupIntrospectionSingleFlight = SetupIntrospectionSingleFlight()

    private val introspectReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                val lightweight = intent?.getBooleanExtra("lightweight", false) == true
                if (lightweight) {
                    val pendingResult = goAsync()
                    this@SetupActivity.lifecycleScope.launch(Dispatchers.IO) {
                        try {
                            this@SetupActivity.writeReadyIntrospectJson()
                        } finally {
                            pendingResult.finish()
                        }
                    }
                    return
                }
                if (!setupIntrospectionSingleFlight.tryEnter()) {
                    Log.i("DXX-Setup", "Introspection already running; coalescing request")
                    return
                }
                val pendingResult = goAsync()
                val buttons = this@SetupActivity.collectAccessibleButtons()
                this@SetupActivity.lifecycleScope.launch(Dispatchers.IO) {
                    try {
                        this@SetupActivity.writeIntrospectJson(buttons)
                    } finally {
                        setupIntrospectionSingleFlight.exit()
                        pendingResult.finish()
                    }
                }
            }
        }

    // -- Setup-screen command API ----------------------------------------
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch --es game d1
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command create_set --es name "my set"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command switch_set --es name "my set"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command clear_set --es name "default"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_gog --es path /sdcard/setup_descent2.exe
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_sow --es path /sdcard/descent2.sow
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_cd --es cue_path /sdcard/disc.cue --es bin_path /sdcard/disc.bin
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_cd --es cue_path /sdcard/disc.cue --esa bin_paths /sdcard/track01.bin,/sdcard/track02.img
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_picked_uris --esa uris content://provider/disc.cue,content://provider/disc.bin
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_iso --es iso_path /sdcard/disc.iso
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_files --es path /sdcard/DESCENT2.HOG
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_default_config
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_autoselect --es game d2 --es primary "8,9,7,6,5,4,3,2,1,0,255" --es secondary "9,8,4,3,1,5,0,255,7,6,2"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_engine_prefs --ei cockpit_mode 2 --ez auto_leveling false --ez original_homing true
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_music_prefs --es source cd --ez prefer_mission_soundtrack false
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command clear_crash_reports
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_midi_play --ei source 0 --ei track 2
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_midi_stop
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_cd_play --ei source 0 --ei track 2
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_cd_stop
    private var gameRunningFlag = false

    private fun returnableGameActivityState(): GameActivityState? = readReturnableGameActivityState(this)

    private fun hasReturnableGameActivity(): Boolean = returnableGameActivityState() != null

    private fun runningGameProcessPid(): Int? = readRunningGameProcessPid(this)

    internal fun automationHasReturnableGameActivity(): Boolean = hasReturnableGameActivity()

    internal fun automationRunningGameProcessPid(): Int? = runningGameProcessPid()

    private fun returnToGame(): Boolean {
        val state = returnableGameActivityState() ?: return false
        val intent = createGameLaunchIntent(state.game, inputDemoReplayPath = null)
        intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT or Intent.FLAG_ACTIVITY_SINGLE_TOP)
        startActivity(intent)
        return true
    }

    /** Guard against double-launch of multiplayer game (auto-launch from
     *  LobbyScreen LaunchedEffect + explicit launch_game broadcast). Two
     *  rapid startActivity calls create two MainActivity instances in the
     *  :game process, causing a FORTIFY pthread_mutex crash. */
    private var mpGameLaunching = false
    private val multiplayerLaunchRequestTimeout =
        Runnable {
            val state = launchPreparation.value
            if (state?.launchKind == "multiplayer" && state.phase == LauncherPreparationPhase.PREPARING) {
                finishLaunchPreparation("multiplayer_request_timeout")
                showLaunchPreflightFailure("Multiplayer game start timed out")
            }
        }

    /** True if LAN discovery was active before game launch; used to auto-resume on return */
    private var wasLanDiscoveringBeforeLaunch = false

    // -- Launcher script automation (debug builds only) ------------------
    //   adb shell am broadcast -a com.dxxredux.SETUP_AUTOMATE \
    //     --es script files/test.jsonc
    // Scripts can alternate between launcher and game phases via
    // enter_launcher / enter_game steps in a single JSONC array.
    private var launcherExecutor: LauncherScriptExecutor? = null
    private val automationStatePreferences
        get() = getSharedPreferences("automation_state", Context.MODE_PRIVATE)

    private fun rememberActiveAutomationRunId(runId: String) {
        automationStatePreferences.edit().putString("active_run_id", runId).commit()
    }

    private fun activeAutomationRunId(): String? =
        if (automationStatePreferences.contains("active_run_id")) {
            automationStatePreferences.getString("active_run_id", null)
        } else {
            null
        }

    internal fun clearActiveAutomationRunId(runId: String) {
        if (activeAutomationRunId() == runId) {
            automationStatePreferences.edit().remove("active_run_id").commit()
        }
    }

    /** Accessible button discovered by walking the Compose accessibility tree. */
    data class ButtonInfo(
        val text: String,
        val enabled: Boolean,
        val focused: Boolean,
        val centerX: Float,
        val centerY: Float,
        val width: Float,
        val height: Float,
    )

    private val automateSetupReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                if (!BuildConfig.DEBUG) return
                val scriptPath = intent?.getStringExtra("script") ?: return
                val runId = intent.getStringExtra("run_id").orEmpty()
                rememberActiveAutomationRunId(runId)
                val resolved =
                    if (scriptPath.startsWith("/")) {
                        scriptPath
                    } else {
                        filesDir.absolutePath + "/" + scriptPath
                    }
                Log.i("DXX-Setup", "SETUP_AUTOMATE: loading $resolved run_id=$runId")
                val executor =
                    LauncherScriptExecutor(this@SetupActivity, runId) { game, path, startStep ->
                        launchGameForAutomation(game, path, startStep, automationRunId = runId)
                    }
                launcherExecutor = executor
                kotlinx.coroutines.MainScope().launch {
                    try {
                        executor.execute(resolved, 0)
                    } catch (e: Exception) {
                        clearActiveAutomationRunId(runId)
                        Log.e("DXX-Setup", "Launcher automation failed: ${e.message}", e)
                    }
                }
            }
        }

    private fun launchGameForAutomation(
        game: String,
        scriptPath: String,
        startStep: Int,
        resumeCandidate: ResumeSaveBridge.ResumeSaveCandidate? = null,
        automationRunId: String = "",
    ) {
        runningGameProcessPid()?.let { pid ->
            Log.w(
                "DXX-Setup",
                "Automation launch waiting for existing game process pid=$pid",
            )
            kotlinx.coroutines.MainScope().launch {
                waitForAutomationGameExit()
                if (runningGameProcessPid() != null) {
                    Log.e("DXX-Setup", "Automation launch blocked by existing game process")
                    Toast
                        .makeText(
                            this@SetupActivity,
                            "Could not stop the previous game",
                            Toast.LENGTH_SHORT,
                        ).show()
                    return@launch
                }
                launchGameForAutomation(game, scriptPath, startStep, resumeCandidate, automationRunId)
            }
            return
        }
        val launchGame = resumeCandidate?.game ?: game
        prepareGameLaunchFiles(launchGame)?.let { message ->
            showLaunchPreflightFailure(message)
            return
        }
        val resolvedResumeSavePath =
            resumeCandidate?.let { candidate ->
                resolveResumeSaveLaunchPath(filesDir, candidate)
            }
        val resolvedResumeCallsign =
            resumeCandidate?.let { candidate ->
                resolveResumeSaveLaunchCallsign(candidate)
            }
        if (resumeCandidate != null && resolvedResumeSavePath.isNullOrBlank()) {
            Log.w(
                "DXX-Setup",
                "Automation resume candidate has no launch path: path=${resumeCandidate.path} " +
                    "relative=${resumeCandidate.relativePath} callsign=${resumeCandidate.callsign}",
            )
            logResumeCandidateLaunch(
                "setup-automation-resume-candidate-invalid",
                resumeCandidate,
                null,
                resolvedResumeCallsign,
            )
            Toast.makeText(this, "Could not read the save launch details", Toast.LENGTH_SHORT).show()
            return
        }
        if (resumeCandidate != null) {
            logResumeCandidateLaunch(
                "setup-automation-resume-candidate-selected",
                resumeCandidate,
                resolvedResumeSavePath,
                resolvedResumeCallsign,
            )
        }
        val intent =
            createGameLaunchIntent(
                game = launchGame,
                inputDemoReplayPath = null,
                resumeSavePath = resolvedResumeSavePath,
                resumeCallsign = resolvedResumeCallsign,
            )
        intent.putExtra("automation_script", scriptPath)
        intent.putExtra("automation_start_step", startStep)
        intent.putExtra("automation_run_id", automationRunId)
        startGameAfterRouteMetadataHandoff(intent)
    }

    private fun startGameAfterRouteMetadataHandoff(intent: Intent) {
        if (routeMetadataLaunchJob?.isActive == true) {
            Log.w("DXX-RouteMetadata", "Ignoring duplicate game launch during route metadata handoff")
            return
        }
        val game = intent.getStringExtra("game") ?: "d2"
        if (launchPreparation.value == null && !beginLaunchPreparation(game, "game")) return
        updateLaunchPreparation(LauncherPreparationPhase.PAUSING_METADATA)
        routeMetadataLaunchJob =
            routeMetadataScope.launch {
                val startedAt = SystemClock.elapsedRealtime()
                RouteMetadataDiagnostics.log("Route metadata launcher-to-game handoff started")
                val metadataStopped = routeMetadataCoordinator.stopForGameLaunch()
                RouteMetadataDiagnostics.log(
                    "Route metadata launcher-to-game handoff finished " +
                        "elapsed_ms=${SystemClock.elapsedRealtime() - startedAt} stopped=$metadataStopped",
                )
                withContext(Dispatchers.Main.immediate) {
                    if (!isFinishing && !isDestroyed) {
                        updateLaunchPreparation(LauncherPreparationPhase.STARTING_GAME)
                        try {
                            startActivity(intent)
                        } catch (e: Exception) {
                            Log.e("DXX-Setup", "Could not start $game", e)
                            finishLaunchPreparation("activity_start_failed")
                            showLaunchPreflightFailure("Could not start ${gameDisplayName(game)}")
                        }
                    } else {
                        finishLaunchPreparation("launcher_unavailable")
                    }
                }
            }
    }

    private fun gameDisplayName(game: String): String = if (game == "d1") "Descent 1" else "Descent 2"

    private fun hasLaunchDataForGame(game: String): Boolean {
        val fsm = FileSetManager(filesDir)
        val activeSet = fsm.getActive()
        val setDir = fsm.getSetDir(activeSet)
        val manifest = AssetManifest(setDir)
        val safManifest = fsm.safManifestForSet(activeSet)
        return launchDataReadyForGame(game, setDir, manifest, safManifest)
    }

    private fun launchInputDemoReplay(demo: StagedInputDemo) {
        if (!demo.file.isFile) {
            Toast.makeText(this, "Recorded demo file is missing", Toast.LENGTH_SHORT).show()
            return
        }
        if (gameRunningFlag || hasReturnableGameActivity()) {
            Toast.makeText(this, "Close the running game before starting a recorded demo", Toast.LENGTH_SHORT).show()
            return
        }
        if (!hasLaunchDataForGame(demo.game)) {
            Toast.makeText(this, "${gameDisplayName(demo.game)} data is not ready", Toast.LENGTH_SHORT).show()
            return
        }

        prepareGameLaunchFiles(demo.game)?.let { message ->
            showLaunchPreflightFailure(message)
            return
        }

        val intent = createGameLaunchIntent(demo.game, demo.file.absolutePath)
        startGameAfterRouteMetadataHandoff(intent)
    }

    private fun createGameLaunchIntent(
        game: String,
        inputDemoReplayPath: String? = getIntent().getStringExtra("input_demo_replay"),
        resumeSavePath: String? = null,
        resumeCallsign: String? = null,
    ): Intent {
        val intent = Intent(this, MainActivity::class.java)
        val cleanInputDemoReplayPath = inputDemoReplayPath?.takeIf { it.isNotBlank() }
        val cleanResumeSavePath = resumeSavePath?.takeIf { it.isNotBlank() }
        val cleanResumeCallsign = resumeCallsign?.takeIf { it.isNotBlank() }
        val hasTransientLaunchRequest =
            cleanInputDemoReplayPath != null || cleanResumeSavePath != null
        val transientLaunchToken =
            if (hasTransientLaunchRequest) SystemClock.elapsedRealtimeNanos().toString() else null
        intent.putExtra("game", game)
        DebugLog.currentFilePath()?.let { intent.putExtra("netlog_path", it) }
        cleanInputDemoReplayPath?.let {
            intent.putExtra("input_demo_replay", it)
        }
        cleanResumeSavePath?.let {
            intent.putExtra("resume_save_path", it)
        }
        cleanResumeCallsign?.let {
            intent.putExtra("resume_callsign", it)
        }
        if (cleanResumeSavePath != null && transientLaunchToken != null) {
            writePendingResumeLaunch(this, game, cleanResumeSavePath, cleanResumeCallsign, transientLaunchToken)
            Log.i(
                "DXX-Setup",
                "resume-launch-request game=$game path=$cleanResumeSavePath " +
                    "callsign=${cleanResumeCallsign ?: ""} token=$transientLaunchToken",
            )
        }
        if (transientLaunchToken != null) {
            intent.putExtra(EXTRA_TRANSIENT_LAUNCH_TOKEN, transientLaunchToken)
        }
        return intent
    }

    private fun prepareGameLaunchFiles(game: String): String? {
        val startedAt = SystemClock.elapsedRealtime()
        var stepStartedAt = startedAt

        fun recordStep(step: String) {
            val now = SystemClock.elapsedRealtime()
            RouteMetadataDiagnostics.log(
                "Launcher preflight game=$game step=$step elapsed_ms=${now - stepStartedAt}",
            )
            stepStartedAt = now
        }

        fun complete(message: String?): String? {
            RouteMetadataDiagnostics.log(
                "Launcher preflight game=$game status=${if (message == null) "complete" else "blocked"} " +
                    "elapsed_ms=${SystemClock.elapsedRealtime() - startedAt}",
            )
            return message
        }

        val fileSetManager = FileSetManager(filesDir)
        val activeSet = fileSetManager.getActive()
        val activeSetDir = fileSetManager.getSetDir(activeSet)
        val safManifest = fileSetManager.safManifestForSet(activeSet)
        val modManager = ModManager(filesDir, this, activeSetDir)
        val contentManager = FileSetContentManager(activeSetDir)
        val contentResult =
            try {
                contentManager.reconcile()
            } catch (e: Exception) {
                Log.e("DXX-Setup", "File-set content reconciliation failed for $game", e)
                LauncherDebugLog.log("content-reconcile-failed game=$game message=${e.message ?: ""}")
                return complete("Could not prepare file-set content: ${e.message ?: "unknown error"}")
            }
        recordStep("content_reconcile")
        if (contentResult.conflicts.isNotEmpty()) {
            val details = contentResult.conflicts.take(3).joinToString("\n")
            LauncherDebugLog.log(
                "content-reconcile-block game=$game conflicts=${contentResult.conflicts.joinToString(" | ")}",
            )
            return complete("File-set content needs attention before launch:\n$details")
        }
        val includeD1MissionZipsForD2 =
            game != "d2" ||
                d1InD2Readiness(filesDir, activeSetDir, AssetManifest(activeSetDir), safManifest).ready
        recordStep("readiness")
        val compatibility = modManager.checkEnabledModCompatibility(game, activeSetDir, includeD1MissionZipsForD2)
        recordStep("mod_compatibility")
        if (!compatibility.ok) {
            Log.e("DXX-Setup", "Mod compatibility check failed for $game: ${compatibility.toLogMessage()}")
            LauncherDebugLog.log("mod-compatibility-block game=$game ${compatibility.toLogMessage()}")
            return complete(compatibility.toUserMessage())
        }
        if (game == "d2" && !includeD1MissionZipsForD2 && modManager.hasEnabledD1MissionZipForD2()) {
            LauncherDebugLog.log("d1-in-d2-hidden-missions reason=missing-d1-base-assets")
        }
        try {
            fileSetManager.writeActiveSetPath()
            recordStep("active_set")
            val audioSourceManager = AudioSourceManager(filesDir, activeSetDir)
            val pilotMusic = NativePilotPreferences.readMusicPrefsForAll(game, filesDir.absolutePath)
            recordStep("pilot_music")
            val requestedMusicMode =
                pilotMusic.source.takeIf { pilotMusic.hasPilotFile && it in setOf("cd", "files", "midi") }
                    ?: getSharedPreferences("dxx_prefs", MODE_PRIVATE).getString("music_mode", "midi")
            val cdPlaylistReady = audioSourceManager.writePlaylist(contentResolver)
            recordStep("cd_playlist")
            if (requestedMusicMode == "cd" && !cdPlaylistReady) {
                return complete("CD Audio is selected, but no enabled readable CD source is available")
            }
            val contentPaths = contentManager.buildLaunchPaths(game, includeD1MissionZipsForD2)
            recordStep("content_projection")
            modManager.writeEnabledModPaths(game, includeD1MissionZipsForD2, contentPaths)
            recordStep("mod_paths")
            writeInitialGameConfig()
            migrateLegacyHalfRenderResolution()
            recordStep("game_config")
            if (!writeMusicConfigForLaunch(game, includeD1MissionZipsForD2)) {
                return complete("Audio Files is selected, but no enabled readable custom track is available")
            }
            recordStep("music_config")
        } catch (e: InsufficientStorageException) {
            Log.e("DXX-Setup", "Launch storage preflight failed for $game", e)
            LauncherDebugLog.log("launch-storage-block game=$game message=${e.message ?: ""}")
            ImportStorageGuard.recordFailure(filesDir, "Launch preparation failed", e)
            return complete(ImportStorageGuard.messageForFailure(e))
        } catch (e: ActiveModPathCapacityException) {
            Log.e("DXX-Setup", "Launch mod-path preflight failed for $game", e)
            LauncherDebugLog.log("mod-path-capacity-block game=$game message=${e.message ?: ""}")
            return complete(e.message ?: "Too many enabled mod paths")
        }
        return complete(null)
    }

    private fun showLaunchPreflightFailure(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
    }

    private fun logResumeCandidateLaunch(
        event: String,
        candidate: ResumeSaveBridge.ResumeSaveCandidate,
        resolvedPath: String?,
        resolvedCallsign: String?,
    ) {
        LauncherDebugLog.log(
            "$event ${resumeCandidateLogSummary(candidate)} " +
                "resolved_path=${resolvedPath ?: ""} resolved_callsign=${resolvedCallsign ?: ""}",
        )
    }

    private fun requestSetupRefresh() {
        if (::routeMetadataCoordinator.isInitialized) routeMetadataCoordinator.wake()
        runOnUiThread { refreshTrigger.intValue++ }
    }

    private fun schedulePostResumeRefresh() {
        resumeOfferRefreshHandler.removeCallbacks(resumeOfferRefreshRunnable)
        resumeOfferRefreshHandler.postDelayed(resumeOfferRefreshRunnable, PostResumeRefreshPolicy.DELAY_MS)
    }

    internal fun prepareForLevelPreviewLaunch() {
        resumeOfferRefreshHandler.removeCallbacks(resumeOfferRefreshRunnable)
        LevelPreviewReturnRefreshGate.markLaunch()
    }

    private val commandReceiver =
        object : BroadcastReceiver() {
            private fun runIo(block: () -> Unit) {
                val pendingResult = goAsync()
                this@SetupActivity.lifecycleScope.launch(Dispatchers.IO) {
                    try {
                        block()
                    } finally {
                        pendingResult.finish()
                    }
                }
            }

            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                val cmd = intent?.getStringExtra("command") ?: return
                when (cmd) {
                    "launch" -> {
                        if (gameRunningFlag || hasReturnableGameActivity()) {
                            if (!returnToGame()) {
                                gameRunningFlag = false
                                requestSetupRefresh()
                            }
                        } else {
                            val game = intent.getStringExtra("game") ?: "d2"
                            if (!hasLaunchDataForGame(game)) {
                                Log.e("DXX-Setup", "Cannot launch $game: ${gameDisplayName(game)} data is not ready")
                                return
                            }
                            prepareGameLaunchFiles(game)?.let { message ->
                                showLaunchPreflightFailure(message)
                                return
                            }
                            val launchIntent = createGameLaunchIntent(game)
                            startGameAfterRouteMetadataHandoff(launchIntent)
                        }
                    }

                    "patch_pilots" -> {
                        runIo {
                            val n = this@SetupActivity.patchPilotsFromConfig()
                            Log.i("DXX-Setup", "patch_pilots: patched $n file(s)")
                        }
                    }

                    "reset_controls" -> {
                        val game = intent.getStringExtra("game")
                        runIo {
                            var d1Reset = 0
                            var d2Reset = 0
                            if (game == null || game == "d2") {
                                d2Reset = NativePilotPatcher.nativeResetToDefaults(filesDir.absolutePath, "d2")
                            }
                            if (game == null || game == "d1") {
                                d1Reset = NativePilotPatcher.nativeResetToDefaults(filesDir.absolutePath, "d1")
                            }
                            val n = d1Reset + d2Reset
                            writeControllerOperationResult("controller_reset_result.json", d1Reset, d2Reset)
                            Log.i("DXX-Setup", "reset_controls: reset $n file(s) to engine defaults")
                        }
                    }

                    "controller_introspect" -> {
                        val game = intent.getStringExtra("game")
                        runIo {
                            this@SetupActivity.writeControllerIntrospectJson(game)
                            Log.i("DXX-Setup", "controller_introspect: written (game=${game ?: "d2"})")
                        }
                    }

                    "write_default_config" -> {
                        runIo {
                            File(filesDir, "controller_config.json").delete()
                            writeDefaultControllerConfig()
                        }
                    }

                    "write_controller_patch_fixture" -> {
                        val game = intent.getStringExtra("game") ?: "d2"
                        runIo {
                            writeControllerPatchFixture(game)
                            Log.i("DXX-Setup", "write_controller_patch_fixture: written (game=$game)")
                        }
                    }

                    "write_engine_prefs" -> {
                        val cockpitMode = intent.getIntExtra("cockpit_mode", 0)
                        val autoLeveling = intent.getBooleanExtra("auto_leveling", true)
                        val showRobotHostageCounts = intent.getBooleanExtra("show_robot_hostage_counts", false)
                        val showBossHealthBar = intent.getBooleanExtra("show_boss_health_bar", true)
                        val mapCheatsAccessible = intent.getBooleanExtra("map_cheats_accessible", true)
                        val headlightActiveDefault = intent.getBooleanExtra("headlight_active_default", false)
                        val originalHoming = intent.getBooleanExtra("original_homing", false)
                        val hasOriginalHoming = intent.hasExtra("original_homing")
                        runIo {
                            val count =
                                NativePilotPreferences.writeEngineAndHomingPrefsToAll(
                                    filesDir.absolutePath,
                                    cockpitMode,
                                    autoLeveling,
                                    showRobotHostageCounts,
                                    showBossHealthBar,
                                    mapCheatsAccessible,
                                    headlightActiveDefault,
                                    originalHoming.takeIf { hasOriginalHoming },
                                )
                            if (count < 0) {
                                Log.w("DXX-Setup", "write_engine_prefs: write failed; original files restored")
                                return@runIo
                            }
                            Log.i(
                                "DXX-Setup",
                                "write_engine_prefs: patched $count file(s) " +
                                    "(cockpit_mode=$cockpitMode auto_leveling=$autoLeveling " +
                                    "show_counts=$showRobotHostageCounts " +
                                    "show_boss_health=$showBossHealthBar " +
                                    "map_cheats=$mapCheatsAccessible " +
                                    "headlight_default=$headlightActiveDefault " +
                                    "original_homing=$originalHoming)",
                            )
                        }
                    }

                    "write_music_prefs" -> {
                        val source = intent.getStringExtra("source") ?: "cd"
                        val preferMissionSoundtrack =
                            intent.getBooleanExtra("prefer_mission_soundtrack", source == "mission")
                        val playOrder = intent.getIntExtra("play_order", 0)
                        val volume = intent.getIntExtra("volume", 8)
                        runIo {
                            val n =
                                NativePilotPreferences.writeMusicPrefsToAll(
                                    filesDir.absolutePath,
                                    source,
                                    preferMissionSoundtrack,
                                    playOrder,
                                    volume,
                                )
                            getSharedPreferences("dxx_prefs", MODE_PRIVATE)
                                .edit()
                                .putString(
                                    "music_mode",
                                    source.takeIf { it in listOf("mission", "files", "midi", "cd") } ?: "cd",
                                ).commit()
                            Log.i(
                                "DXX-Setup",
                                "write_music_prefs: patched $n file(s) " +
                                    "(source=$source prefer_mission=$preferMissionSoundtrack " +
                                    "play_order=$playOrder volume=$volume)",
                            )
                        }
                    }

                    "write_bool_pref" -> {
                        val key = intent.getStringExtra("key") ?: return
                        val value = intent.getBooleanExtra("value", false)
                        val editor = getSharedPreferences("dxx_prefs", MODE_PRIVATE).edit().putBoolean(key, value)
                        if (key == PREF_SHOW_RESUME_OFFER) {
                            editor.putBoolean(PREF_SAVE_EXPLORER_PANEL_EXPANDED, value)
                        }
                        editor.commit()
                        Log.i("DXX-Setup", "write_bool_pref: $key=$value")
                        requestSetupRefresh()
                    }

                    "clear_save_files" -> {
                        runIo {
                            val deleted = this@SetupActivity.clearSaveFilesForAutomation()
                            Log.i("DXX-Setup", "clear_save_files: deleted $deleted file(s)")
                            requestSetupRefresh()
                        }
                    }

                    "clear_route_metadata_cache" -> {
                        val pendingResult = goAsync()
                        this@SetupActivity.lifecycleScope.launch {
                            try {
                                val result = routeMetadataCoordinator.clearCache()
                                Log.i(
                                    "DXX-Setup",
                                    "clear_route_metadata_cache: deleted ${result.removedFiles} file(s) " +
                                        "and ${result.removedDirectories} cache root(s)",
                                )
                            } finally {
                                pendingResult.finish()
                            }
                        }
                    }

                    "clear_level_metadata_result_cache" -> {
                        runIo {
                            val cache = File(filesDir, "level_metadata_results")
                            val deleted = cache.exists() && cache.deleteRecursively()
                            Log.i("DXX-Setup", "clear_level_metadata_result_cache: deleted=$deleted")
                        }
                    }

                    "pause_route_metadata_precompute" -> {
                        val pendingResult = goAsync()
                        this@SetupActivity.lifecycleScope.launch {
                            try {
                                routeMetadataCoordinator.stopAndAwait()
                                Log.i("DXX-Setup", "pause_route_metadata_precompute: stopped")
                            } finally {
                                pendingResult.finish()
                            }
                        }
                    }

                    "clear_crash_reports" -> {
                        runIo {
                            val deleted = CrashLog.listCrashFiles(this@SetupActivity).size
                            CrashLog.deleteAllCrashFiles(this@SetupActivity)
                            Log.i("DXX-Setup", "clear_crash_reports: deleted $deleted file(s)")
                            requestSetupRefresh()
                        }
                    }

                    "clear_pilot_files" -> {
                        runIo {
                            val deleted = this@SetupActivity.clearPilotFilesForAutomation()
                            Log.i("DXX-Setup", "clear_pilot_files: deleted $deleted file(s)")
                            requestSetupRefresh()
                        }
                    }

                    "write_probe_debug_prefs" -> {
                        val enabled = intent.getBooleanExtra("enabled", true)
                        val prefs = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
                        prefs
                            .edit()
                            .putBoolean(PREF_SHOW_VIDEO_INFO_DEBUG_OPTIONS, enabled)
                            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.NETWORK), enabled)
                            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.GRAPHICS), enabled)
                            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.TEXTURE), enabled)
                            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.GAME), enabled)
                            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.LAUNCHER), enabled)
                            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.PROFILING), false)
                            .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.COOP_DESYNC), enabled)
                            .commit()
                        Log.i("DXX-Setup", "write_probe_debug_prefs: enabled=$enabled")
                        requestSetupRefresh()
                    }

                    "create_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            try {
                                val dir = fsm.createSet(name)
                                Log.i("DXX-Setup", "create_set '$name': ${dir.absolutePath}")
                                requestSetupRefresh()
                            } catch (e: IllegalArgumentException) {
                                Log.i("DXX-Setup", "create_set '$name': already exists")
                            }
                        }
                    }

                    "switch_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            fsm.setActive(name)
                            val activeSet = fsm.getActive()
                            if (activeSet != name) {
                                Log.w("DXX-Setup", "switch_set '$name': rejected; active set remains '$activeSet'")
                                return@runIo
                            }
                            fsm.writeActiveSetPath()
                            val content = FileSetContentManager(fsm.getSetDir(activeSet)).reconcile()
                            Log.i(
                                "DXX-Setup",
                                "switch_set '$name': ok adopted=${content.adoptedIds.size} " +
                                    "conflicts=${content.conflicts.size}",
                            )
                            requestSetupRefresh()
                        }
                    }

                    "clear_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            fsm.clearSet(name, this@SetupActivity)
                            Log.i("DXX-Setup", "clear_set '$name': ok")
                            requestSetupRefresh()
                        }
                    }

                    "delete_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            fsm.deleteSet(name, this@SetupActivity)
                            fsm.writeActiveSetPath()
                            Log.i("DXX-Setup", "delete_set '$name': ok")
                            requestSetupRefresh()
                        }
                    }

                    "set_content_enabled" -> {
                        val id = intent.getStringExtra("id") ?: return
                        val enabled = intent.getBooleanExtra("enabled", true)
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            val manager = FileSetContentManager(fsm.getSetDir(fsm.getActive()))
                            manager.setEnabled(id, enabled)
                            Log.i("DXX-Setup", "set_content_enabled '$id': $enabled")
                            requestSetupRefresh()
                        }
                    }

                    "delete_content" -> {
                        val id = intent.getStringExtra("id") ?: return
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val deleted = FileSetContentManager(setDir).deleteEntry(id)
                            AudioSourceManager(filesDir, setDir).pruneMissingSources(setDir)
                            Log.i("DXX-Setup", "delete_content '$id': $deleted")
                            requestSetupRefresh()
                        }
                    }

                    "delete_mod" -> {
                        val filename = intent.getStringExtra("filename") ?: return
                        runIo {
                            ModManager.forActiveSet(filesDir, this@SetupActivity).deleteMod(filename)
                            Log.i("DXX-Setup", "delete_mod '$filename': ok")
                            requestSetupRefresh()
                        }
                    }

                    "import_gog" -> {
                        val path = intent.getStringExtra("path") ?: return
                        val audio = intent.getBooleanExtra("include_audio", true)
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val count = GogImportBridge.extractFiles(path, setDir.absolutePath, null, audio)
                            val srcManager = AudioSourceManager(filesDir, setDir)
                            if (audio && count > 0 &&
                                registerGogAudioSource(
                                    srcManager,
                                    filesDir,
                                    setDir,
                                    this@SetupActivity,
                                )
                            ) {
                                enableRedbookInConfig(filesDir, this@SetupActivity)
                            }
                            Log.i("DXX-Setup", "import_gog '$path' -> $count file(s) to ${setDir.name} (audio=$audio)")
                            if (count > 0) routeMetadataCoordinator.notifyContentImported()
                            requestSetupRefresh()
                        }
                    }

                    "import_sow" -> {
                        val path = intent.getStringExtra("path") ?: return
                        runIo {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val count = DiscImportBridge.extractSowFiles(path, setDir.absolutePath, null)
                            Log.i("DXX-Setup", "import_sow '$path' -> $count file(s) to ${setDir.name}")
                            if (count > 0) routeMetadataCoordinator.notifyContentImported()
                            requestSetupRefresh()
                        }
                    }

                    "import_cd" -> {
                        val cuePath = intent.getStringExtra("cue_path") ?: return
                        val binPaths =
                            intent
                                .getStringArrayExtra("bin_paths")
                                ?.filter { it.isNotBlank() }
                                ?.takeIf { it.isNotEmpty() }
                                ?: listOfNotNull(intent.getStringExtra("bin_path")?.takeIf { it.isNotBlank() })
                        if (binPaths.isEmpty()) return
                        val audio = intent.getBooleanExtra("include_audio", true)
                        SetupImportTracker.begin("cd")
                        runIo {
                            try {
                                val fsm = FileSetManager(filesDir)
                                val setDir = fsm.getSetDir(fsm.getActive())
                                val count =
                                    importDiscImageFromPath(
                                        filesDir = filesDir,
                                        setDir = setDir,
                                        context = this@SetupActivity,
                                        cuePath = cuePath,
                                        binPaths = binPaths,
                                        includeAudio = audio,
                                    )
                                SetupImportTracker.complete("cd", count)
                                if (count > 0) routeMetadataCoordinator.notifyContentImported()
                                Log.i(
                                    "DXX-Setup",
                                    "import_cd cue='$cuePath' images=${binPaths.size} -> $count file(s) to ${setDir.name} (audio=$audio)",
                                )
                            } catch (e: Exception) {
                                SetupImportTracker.fail("cd", e.javaClass.simpleName)
                                Log.e("DXX-Setup", "import_cd failed for '$cuePath'", e)
                            } finally {
                                requestSetupRefresh()
                            }
                        }
                    }

                    "import_picked_uris" -> {
                        if (!BuildConfig.DEBUG) return
                        val uris =
                            intent
                                .getStringArrayExtra("uris")
                                ?.filter { it.isNotBlank() }
                                ?.map(Uri::parse)
                                .orEmpty()
                        if (uris.isEmpty()) return
                        runOnUiThread { pendingPickedImportUris.value = uris }
                    }

                    "import_iso" -> {
                        val path = intent.getStringExtra("iso_path") ?: return
                        SetupImportTracker.begin("iso")
                        runIo {
                            try {
                                val fsm = FileSetManager(filesDir)
                                val setDir = fsm.getSetDir(fsm.getActive())
                                val count = importIsoImageFromPath(setDir, path)
                                SetupImportTracker.complete("iso", count)
                                if (count > 0) routeMetadataCoordinator.notifyContentImported()
                                Log.i(
                                    "DXX-Setup",
                                    "import_iso iso='$path' -> $count file(s) to ${setDir.name}",
                                )
                            } catch (e: Exception) {
                                SetupImportTracker.fail("iso", e.javaClass.simpleName)
                                Log.e("DXX-Setup", "import_iso failed for '$path'", e)
                            } finally {
                                requestSetupRefresh()
                            }
                        }
                    }

                    "import_files" -> {
                        val path = intent.getStringExtra("path") ?: return
                        val fsm = FileSetManager(filesDir)
                        val setDir = fsm.getSetDir(fsm.getActive())
                        val src = File(path)
                        if (src.isFile) {
                            val destDir =
                                if (portableGameFilenameIdentity(src.name).endsWith(".dem")) {
                                    File(setDir, "demos").also { it.mkdirs() }
                                } else {
                                    setDir
                                }
                            ImportStorageGuard.requireFreeSpace(destDir, src.length(), "import ${src.name}")
                            LauncherFileCopy.copyFileToFile(src, File(destDir, src.name))
                            Log.i("DXX-Setup", "import_files: copied ${src.name} to ${destDir.name}")
                            routeMetadataCoordinator.notifyContentImported()
                            requestSetupRefresh()
                        } else {
                            Log.w("DXX-Setup", "import_files: not a file: $path")
                        }
                    }

                    "write_autoselect" -> {
                        val game = intent.getStringExtra("game") ?: "d2"
                        val primStr = intent.getStringExtra("primary") ?: return
                        val secStr = intent.getStringExtra("secondary") ?: return
                        val prim = primStr.split(",").map { it.trim().toInt() }.toIntArray()
                        val sec = secStr.split(",").map { it.trim().toInt() }.toIntArray()
                        val count =
                            NativeAutoselectPatcher.writeAutoselect(
                                game,
                                filesDir.absolutePath,
                                prim,
                                sec,
                            )
                        if (count >= 0) {
                            Log.i("DXX-Setup", "write_autoselect ($game): patched $count file(s)")
                        } else {
                            Log.w("DXX-Setup", "write_autoselect ($game): invalid weapon order")
                        }
                    }

                    "music_midi_play" -> {
                        val srcIdx = intent.getIntExtra("source", 0)
                        val trkIdx = intent.getIntExtra("track", 0)
                        val generation = MidiPreviewBridge.reserveStart()
                        runIo midiPlay@{
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val result = MidiEnumerationBridge.enumerateTracks(setDir.absolutePath)
                            val src = result.sources.getOrNull(srcIdx)
                            if (src == null) {
                                Log.w(
                                    "DXX-Setup",
                                    "music_midi_play: source $srcIdx not found (${result.sources.size} available)",
                                )
                                return@midiPlay
                            }
                            val track = src.tracks.getOrNull(trkIdx)
                            if (track == null) {
                                Log.w(
                                    "DXX-Setup",
                                    "music_midi_play: track $trkIdx not found in ${src.label} (${src.tracks.size} available)",
                                )
                                return@midiPlay
                            }
                            MidiPreviewBridge.init(this@SetupActivity)
                            val data = MidiPreviewBridge.readHogEntry(src.hog, track.filename)
                            if (data == null) {
                                Log.w("DXX-Setup", "music_midi_play: failed to read ${track.filename} from ${src.hog}")
                                return@midiPlay
                            }
                            val isHmp = portableGameFilenameIdentity(track.filename).endsWith(".hmp")
                            val sr = MidiPreviewBridge.getNativeSampleRate(this@SetupActivity)
                            if (MidiPreviewBridge.startReserved(generation, data, isHmp, sr)) {
                                Log.i("DXX-Setup", "music_midi_play: playing ${track.filename} from ${src.label}")
                            } else {
                                Log.e(
                                    "DXX-Setup",
                                    "music_midi_play: failed to start ${track.filename} from ${src.label}",
                                )
                            }
                        }
                    }

                    "music_midi_stop" -> {
                        MidiPreviewBridge.stop()
                        Log.i("DXX-Setup", "music_midi_stop: stopped")
                    }

                    "music_midi_seek" -> {
                        val fraction = intent.getFloatExtra("fraction", 0f)
                        val accepted = MidiPreviewBridge.seek(fraction)
                        Log.i("DXX-Setup", "music_midi_seek: fraction=$fraction accepted=$accepted")
                    }

                    "music_midi_pause" -> {
                        MidiPreviewBridge.pause()
                        Log.i("DXX-Setup", "music_midi_pause: paused")
                    }

                    "music_midi_resume" -> {
                        MidiPreviewBridge.resume()
                        Log.i("DXX-Setup", "music_midi_resume: resumed")
                    }

                    "music_cd_play" -> {
                        val srcIdx = intent.getIntExtra("source", 0)
                        val trkIdx = intent.getIntExtra("track", 0)
                        val generation = CdPreviewBridge.reserveStart()
                        runIo cdPlay@{
                            val srcManager = AudioSourceManager.forActiveSet(filesDir)
                            val sources = srcManager.getEnabledSources()
                            val src = sources.getOrNull(srcIdx)
                            if (src == null) {
                                Log.w(
                                    "DXX-Setup",
                                    "music_cd_play: source $srcIdx not found (${sources.size} available)",
                                )
                                return@cdPlay
                            }
                            val cuePath = resolveCdAudioSourceFile(filesDir, src.cuePath).absolutePath
                            val localBinPaths = resolveCdPreviewLocalBinPaths(filesDir, src)
                            val safBinUris = src.binContentUriList().filterNot(::isLocalCdContentPath)
                            val sr = CdPreviewBridge.getNativeSampleRate(this@SetupActivity)
                            val ok =
                                if (localBinPaths != null) {
                                    if (localBinPaths.size == 1) {
                                        CdPreviewBridge.startReserved(
                                            generation,
                                            localBinPaths.first(),
                                            cuePath,
                                            trkIdx,
                                            sr,
                                        )
                                    } else {
                                        CdPreviewBridge.startMultiReserved(
                                            generation,
                                            localBinPaths,
                                            cuePath,
                                            trkIdx,
                                            sr,
                                        )
                                    }
                                } else if (safBinUris.isNotEmpty()) {
                                    val openedPfds = mutableListOf<android.os.ParcelFileDescriptor>()
                                    try {
                                        safBinUris.forEach { uriStr ->
                                            val pfd = contentResolver.openFileDescriptor(Uri.parse(uriStr), "r")
                                            if (pfd == null) {
                                                throw java.io.IOException("Could not open BIN URI: $uriStr")
                                            }
                                            openedPfds.add(pfd)
                                        }
                                        if (openedPfds.size == 1) {
                                            CdPreviewBridge.startFdReserved(
                                                generation,
                                                openedPfds.first().fd,
                                                cuePath,
                                                trkIdx,
                                                sr,
                                            )
                                        } else {
                                            CdPreviewBridge.startMultiFdReserved(
                                                generation,
                                                openedPfds.map { it.fd }.toIntArray(),
                                                cuePath,
                                                trkIdx,
                                                sr,
                                            )
                                        }
                                    } finally {
                                        openedPfds.forEach { pfd ->
                                            try {
                                                pfd.close()
                                            } catch (_: Exception) {
                                            }
                                        }
                                    }
                                } else {
                                    false
                                }
                            Log.i("DXX-Setup", "music_cd_play: source=${src.discLabel} track=$trkIdx ok=$ok")
                        }
                    }

                    "music_cd_stop" -> {
                        CdPreviewBridge.stop()
                        Log.i("DXX-Setup", "music_cd_stop: stopped")
                    }

                    "add_audio_source" -> {
                        // Test automation: register a disc-image audio source.
                        // bin_path/bin_paths: absolute filesystem path(s) to the image file(s)
                        // cue_name: filename of the CUE file (in filesDir)
                        // label: human-readable disc label
                        val binPaths =
                            intent
                                .getStringArrayExtra("bin_paths")
                                ?.filter { it.isNotBlank() }
                                ?.takeIf { it.isNotEmpty() }
                                ?: listOfNotNull(intent.getStringExtra("bin_path")?.takeIf { it.isNotBlank() })
                        if (binPaths.isEmpty()) return
                        val cueName = intent.getStringExtra("cue_name") ?: return
                        val label = intent.getStringExtra("label") ?: "Test Disc"
                        val id = intent.getStringExtra("id") ?: "test-${System.currentTimeMillis()}"
                        val orderedBinPaths =
                            if (binPaths.size > 1) {
                                val cueFile = resolveCdAudioSourceFile(filesDir, cueName)
                                if (cueFile.isFile) {
                                    val orderedImages = orderCueEntries(cueFile, binPaths) { it }
                                    if (orderedImages.missingNames.isEmpty()) {
                                        if (orderedImages.extraNames.isNotEmpty()) {
                                            val ignoredImagesSummary =
                                                summarizeDiscImageNames(orderedImages.extraNames)
                                            Log.i(
                                                "DXX-Setup",
                                                "add_audio_source: ignoring extra image files for $cueName: $ignoredImagesSummary",
                                            )
                                        }
                                        orderedImages.orderedEntries
                                    } else {
                                        Log.w(
                                            "DXX-Setup",
                                            "add_audio_source: ${buildMissingDiscImageSelectionMessage(
                                                orderedImages.missingNames,
                                            )}",
                                        )
                                        binPaths
                                    }
                                } else {
                                    binPaths
                                }
                            } else {
                                binPaths
                            }
                        val parsedTracks =
                            DiscImportBridge.parseCue(
                                resolveCdAudioSourceFile(filesDir, cueName).absolutePath,
                                orderedBinPaths.map { File(it).length() }.toLongArray(),
                            ) ?: return
                        val srcManager = AudioSourceManager.forActiveSet(filesDir)
                        srcManager.addSource(
                            AudioSourceManager.AudioSource(
                                id = id,
                                cuePath = cueName,
                                binPaths = orderedBinPaths.map { portableGameFilenameIdentity(File(it).name) },
                                discLabel = label,
                                discId = id,
                                trackCount = parsedTracks.size,
                                audioTrackCount = parsedTracks.count { it.isAudio },
                                legacyDiscId = 0,
                                binContentUri = orderedBinPaths.first(),
                                binContentUris = orderedBinPaths,
                            ),
                        )
                        enableRedbookInConfig(filesDir, this@SetupActivity)
                        Log.i("DXX-Setup", "add_audio_source: id=$id images=${orderedBinPaths.size} cue=$cueName")
                    }

                    "clear_audio_sources" -> {
                        runIo {
                            val srcManager = AudioSourceManager.forActiveSet(filesDir)
                            val retainedSafUris =
                                CustomAudioSetManager
                                    .forActiveSet(filesDir)
                                    .getSets()
                                    .flatMap { it.referencedUris.values }
                            srcManager.clearAll(this@SetupActivity, retainedSafUris)
                            Log.i("DXX-Setup", "clear_audio_sources: cleared all")
                        }
                    }

                    else -> {
                        Log.w("DXX-Setup", "Unknown command: $cmd")
                    }
                }
            }
        }

    // -- Multiplayer command API (test automation) ------------------------
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command connect
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command disconnect
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command create_lobby --es game d2 --es mission "counterstrike!" --es mode anarchy
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command join_first_lobby
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command chat --es text "hello"
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command set_ready --es ready true
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command start_game
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command introspect
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command set_callsign --es callsign "Player1"
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command stun_override --es addrs "10.0.2.2:13478,10.0.2.2:13479"
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command stun_override_clear
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command tap_button --es text "Multiplayer"
    //   adb shell am broadcast -a com.dxxredux.MP_COMMAND --es command dismiss_keyboard
    private var mpCallsign: String = "Player"
    private var mpJoinHostAddrOverride: String? = null
    private var mpJoinHostPortOverride: Int? = null
    private val mpCommandReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                val cmd = intent?.getStringExtra("command") ?: return
                Log.i("DXX-MP", "MP_COMMAND: $cmd")
                when (cmd) {
                    "set_callsign" -> {
                        mpCallsign = intent.getStringExtra("callsign") ?: "Player"
                        MatchmakingStateHolder.update { it.copy(callsign = mpCallsign) }
                        com.dxxredux.app.multiplayer.CallsignPrefs
                            .save(this@SetupActivity, mpCallsign)
                        Log.i("DXX-MP", "Callsign set to: $mpCallsign")
                    }

                    "connect" -> {
                        val url =
                            intent.getStringExtra("url")
                                ?: NetworkConstants.DEFAULT_SERVER_URL
                        MatchmakingService.connect(url, mpCallsign)
                    }

                    "disconnect" -> {
                        MatchmakingService.disconnect()
                    }

                    "create_lobby" -> {
                        val game = intent.getStringExtra("game") ?: "d2"
                        val mission = intent.getStringExtra("mission") ?: "counterstrike!"
                        val mode = intent.getStringExtra("mode") ?: "anarchy"
                        val maxPlayers = intent.getIntExtra("max_players", 4)
                        val coopQol = intent.getBooleanExtra("coop_qol", true)
                        val duplicateEnergyShields =
                            intent.getBooleanExtra("duplicate_energy_shields", false)
                        val fullDeathSpew = intent.getBooleanExtra("full_death_spew", true)
                        val playerSpewNoExpire = intent.getBooleanExtra("player_spew_no_expire", true)
                        val clientsCanRequestRewind = intent.getBooleanExtra("clients_can_request_rewind", false)
                        val restrictNonCoopFovToBase =
                            intent.getBooleanExtra("restrict_noncoop_fov_to_base", false)
                        val gameInfo =
                            JsonObject(
                                mapOf(
                                    "mission" to JsonPrimitive(mission),
                                    "mode" to JsonPrimitive(mode),
                                    "coop_qol" to JsonPrimitive(coopQol),
                                    "duplicate_energy_shields" to JsonPrimitive(duplicateEnergyShields),
                                    "full_death_spew" to JsonPrimitive(fullDeathSpew),
                                    "player_spew_no_expire" to JsonPrimitive(playerSpewNoExpire),
                                    "clients_can_request_rewind" to JsonPrimitive(clientsCanRequestRewind),
                                    "restrict_noncoop_fov_to_base" to JsonPrimitive(restrictNonCoopFovToBase),
                                ),
                            )
                        MatchmakingService.createLobby(game, maxPlayers, gameInfo)
                    }

                    "join_first_lobby" -> {
                        val lobbies = MatchmakingStateHolder.state.value.lobbies
                        if (lobbies.isNotEmpty()) {
                            val lobby = lobbies.first()
                            MatchmakingService.joinLobby(lobby.lobbyId)
                            Log.i("DXX-MP", "Joining lobby: ${lobby.lobbyId} (${lobby.hostCallsign})")
                        } else {
                            Log.w("DXX-MP", "No lobbies available to join")
                        }
                    }

                    "refresh_lobbies" -> {
                        MatchmakingService.requestLobbyList()
                    }

                    "chat" -> {
                        val text = intent.getStringExtra("text") ?: return
                        MatchmakingService.sendLobbyChat(text)
                    }

                    "set_ready" -> {
                        val ready = intent.getStringExtra("ready") != "false"
                        MatchmakingService.setReady(ready)
                    }

                    "start_game" -> {
                        MatchmakingService.startGame()
                    }

                    "launch_game" -> {
                        // Trigger the actual game launch from pending gameLaunchInfo
                        val info = MatchmakingStateHolder.state.value.gameLaunchInfo
                        if (info != null) {
                            Log.i("DXX-MP", "Launching game: ${info.game} ${info.mission} slot=${info.yourSlot}")
                            launchMultiplayerGame(info)
                        } else {
                            Log.w("DXX-MP", "No game launch info pending")
                        }
                    }

                    "introspect" -> {
                        this@SetupActivity.writeMpIntrospectJson()
                    }

                    "set_join_target" -> {
                        mpJoinHostAddrOverride = intent.getStringExtra("host_addr")
                        val port = intent.getIntExtra("host_port", -1)
                        mpJoinHostPortOverride = if (port > 0) port else null
                        Log.i("DXX-MP", "Join target override: $mpJoinHostAddrOverride:$mpJoinHostPortOverride")
                    }

                    "stun_override" -> {
                        val addrs =
                            intent
                                .getStringExtra("addrs")
                                ?.split(",")
                                ?.map { it.trim() }
                                ?.filter { it.isNotEmpty() }
                                ?: emptyList()
                        MatchmakingService.setStunOverride(addrs)
                        Log.i("DXX-MP", "STUN override set: $addrs")
                    }

                    "stun_override_clear" -> {
                        MatchmakingService.setStunOverride(null)
                        Log.i("DXX-MP", "STUN override cleared")
                    }

                    "lan_launch" -> {
                        val game = intent.getStringExtra("game") ?: "d2"
                        val mpMode = intent.getStringExtra("mp_mode") ?: "host"
                        val mission = intent.getStringExtra("mission") ?: ""
                        val mode = intent.getStringExtra("mode") ?: "coop"
                        val maxPlayers = intent.getIntExtra("max_players", 4)
                        val levelNum = intent.getIntExtra("level_num", 1)
                        val difficulty = intent.getIntExtra("difficulty", 1)
                        val coopQol = intent.getBooleanExtra("coop_qol", true)
                        val duplicateEnergyShields =
                            intent.getBooleanExtra("duplicate_energy_shields", false)
                        val fullDeathSpew = intent.getBooleanExtra("full_death_spew", true)
                        val playerSpewNoExpire = intent.getBooleanExtra("player_spew_no_expire", true)
                        val clientsCanRequestRewind = intent.getBooleanExtra("clients_can_request_rewind", false)
                        val hostObserver = intent.getBooleanExtra("host_observer", false)
                        val restrictNonCoopFovToBase =
                            intent.getBooleanExtra("restrict_noncoop_fov_to_base", false)
                        val hostAddr = intent.getStringExtra("host_addr")
                        val hostPort = intent.getIntExtra("host_port", NetworkConstants.ENGINE_PORT)
                        intent.getStringExtra("callsign")?.let {
                            mpCallsign = it
                            MatchmakingStateHolder.update { state -> state.copy(callsign = it) }
                        }
                        val isHost = mpMode == "host"
                        val info =
                            GameLaunchInfo(
                                game = game,
                                mission = mission,
                                mode = mode,
                                difficulty = difficulty,
                                levelNum = levelNum,
                                maxPlayers = maxPlayers,
                                yourSlot = if (isHost) 0 else 1,
                                isHost = isHost,
                                peers = emptyList(),
                                lanHostAddr = if (!isHost) hostAddr else null,
                                lanHostPort = hostPort,
                                isLan = true,
                                coopQol = coopQol,
                                duplicateEnergyShields = duplicateEnergyShields,
                                fullDeathSpew = fullDeathSpew,
                                playerSpewNoExpire = playerSpewNoExpire,
                                clientsCanRequestRewind = clientsCanRequestRewind,
                                restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                                hostObserver = hostObserver,
                            )
                        Log.i(
                            "DXX-MP",
                            "lan_launch: $mpMode $game/$mission lvl=$levelNum diff=$difficulty host=$hostAddr:$hostPort",
                        )
                        launchMultiplayerGame(info)
                    }

                    "lan_host_lobby" -> {
                        val callsign = intent.getStringExtra("callsign") ?: "TestHost"
                        val game = intent.getStringExtra("game") ?: "d2"
                        val mission = intent.getStringExtra("mission") ?: "Counterstrike!"
                        val mode = intent.getStringExtra("mode") ?: "coop"
                        val maxPlayers = intent.getIntExtra("max_players", 4)
                        mpCallsign = callsign
                        MatchmakingStateHolder.update { it.copy(callsign = callsign) }
                        com.dxxredux.app.lobby.LobbyService
                            .startDiscovery(this@SetupActivity, callsign)
                        com.dxxredux.app.lobby.LobbyService
                            .hostLobby(callsign, game, mission, mode, maxPlayers)
                        Log.i("DXX-MP", "lan_host_lobby: hosting as $callsign ($game/$mission/$mode)")
                    }

                    "lan_start_game" -> {
                        val difficulty = intent.getIntExtra("difficulty", 1)
                        val levelNum = intent.getIntExtra("level_num", 1)
                        com.dxxredux.app.lobby.LobbyService
                            .startGame(difficulty, levelNum)
                        Log.i("DXX-MP", "lan_start_game: level=$levelNum difficulty=$difficulty")
                    }

                    "lan_stop_lobby" -> {
                        com.dxxredux.app.lobby.LobbyService
                            .stopDiscovery()
                        Log.i("DXX-MP", "lan_stop_lobby: stopped")
                    }

                    "lan_discover" -> {
                        val callsign = intent.getStringExtra("callsign") ?: "TestJoin"
                        mpCallsign = callsign
                        com.dxxredux.app.lobby.LobbyService
                            .startDiscovery(this@SetupActivity, callsign)
                        Log.i("DXX-MP", "lan_discover: started discovery as $callsign")
                    }

                    "lan_discover_status" -> {
                        val lobbies = com.dxxredux.app.lobby.LobbyService.discoveredLobbies.value
                        val hosting = com.dxxredux.app.lobby.LobbyService.isHosting.value
                        val diag = com.dxxredux.app.lobby.LobbyService.diagnostics.value
                        val tx =
                            com.dxxredux.app.lobby.LobbyService.packetsSent
                                .get()
                        val rx =
                            com.dxxredux.app.lobby.LobbyService.packetsReceived
                                .get()
                        Log.i(
                            "DXX-MP",
                            "lan_discover_status: lobbies=${lobbies.size} hosting=$hosting tx=$tx rx=$rx diag=$diag",
                        )
                        for (l in lobbies) {
                            Log.i(
                                "DXX-MP",
                                "  lobby: ${l.announce.callsign} ${l.announce.game}/${l.announce.mission} from ${l.announce.hostAddress}",
                            )
                        }
                    }

                    "lan_join_first_lobby" -> {
                        val lobby =
                            com.dxxredux.app.lobby.LobbyService.discoveredLobbies.value
                                .firstOrNull()
                        if (lobby == null) {
                            Log.w("DXX-MP", "lan_join_first_lobby: no lobby discovered")
                        } else {
                            com.dxxredux.app.lobby.LobbyService.joinLobby(
                                lobby.announce.lobbyId,
                                lobby.announce.hostAddress,
                                mpCallsign,
                            )
                            Log.i("DXX-MP", "lan_join_first_lobby: joining ${lobby.announce.lobbyId}")
                        }
                    }

                    "lan_set_ready" -> {
                        val joined = com.dxxredux.app.lobby.LobbyService.joinedLobby.value
                        val ready = intent.getBooleanExtra("ready", true)
                        if (joined == null) {
                            Log.w("DXX-MP", "lan_set_ready: not joined")
                        } else {
                            com.dxxredux.app.lobby.LobbyService.setReady(
                                joined.lobbyId,
                                joined.hostAddr,
                                mpCallsign,
                                ready,
                            )
                            Log.i("DXX-MP", "lan_set_ready: ready=$ready")
                        }
                    }

                    "lan_send_chat" -> {
                        val text = intent.getStringExtra("text") ?: "lobby stability check"
                        com.dxxredux.app.lobby.LobbyService
                            .sendChat(mpCallsign, text)
                        Log.i("DXX-MP", "lan_send_chat: text=$text")
                    }

                    "lan_lobby_status" -> {
                        val lobbyService = com.dxxredux.app.lobby.LobbyService
                        val players = lobbyService.hostedLobbyPlayers.value
                        val playerStatus = players.joinToString(",") { "${it.callsign}:${it.ready}" }
                        Log.i(
                            "DXX-MP",
                            "lan_lobby_status: hosting=${lobbyService.isHosting.value} " +
                                "joined=${lobbyService.joinedLobby.value != null} players=${players.size} " +
                                "all_ready=${players.isNotEmpty() && players.all { it.ready }} " +
                                "chat_messages=${lobbyService.chatMessages.value.size} [$playerStatus]",
                        )
                    }

                    "lan_notify_backgrounded" -> {
                        com.dxxredux.app.lobby.LobbyService
                            .notifyAppBackgrounded()
                        Log.i("DXX-MP", "lan_notify_backgrounded: done")
                    }

                    "lan_notify_resumed" -> {
                        com.dxxredux.app.lobby.LobbyService
                            .notifyAppResumed(this@SetupActivity, mpCallsign)
                        Log.i("DXX-MP", "lan_notify_resumed: done")
                    }

                    "dismiss_keyboard" -> {
                        this@SetupActivity.dismissKeyboard()
                        Log.i("DXX-MP", "dismiss_keyboard: done")
                    }

                    "tap_button" -> {
                        val text =
                            intent.getStringExtra("text") ?: run {
                                Log.w("DXX-MP", "tap_button: missing 'text'")
                                return
                            }
                        // Dismiss soft keyboard first -- it can cover buttons
                        this@SetupActivity.dismissKeyboard()
                        // Launch coroutine so we can scroll if needed
                        kotlinx.coroutines.MainScope().launch {
                            // Brief delay after keyboard dismiss for layout to settle
                            kotlinx.coroutines.delay(200)
                            var scrollAttempts = 0
                            var resetToTop = false
                            val deadline = System.currentTimeMillis() + 5000
                            while (true) {
                                if (this@SetupActivity.performAccessibilityClick(text)) {
                                    Log.i("DXX-MP", "tap_button: tapped \"$text\"")
                                    return@launch
                                }
                                if (System.currentTimeMillis() > deadline) break
                                if (!resetToTop) {
                                    this@SetupActivity.scrollToTop()
                                    resetToTop = true
                                    kotlinx.coroutines.delay(300)
                                } else if (scrollAttempts < 5) {
                                    this@SetupActivity.scrollDown()
                                    scrollAttempts++
                                    kotlinx.coroutines.delay(400)
                                } else {
                                    kotlinx.coroutines.delay(500)
                                }
                            }
                            val available =
                                this@SetupActivity
                                    .collectAccessibleButtons()
                                    .joinToString(", ") { "\"${it.text}\"" }
                            Log.w("DXX-MP", "tap_button: \"$text\" not found (available: $available)")
                        }
                    }

                    else -> {
                        Log.w("DXX-MP", "Unknown MP command: $cmd")
                    }
                }
            }
        }

    // -- Host migration receiver ----------------------------------------
    // When the game process detects host departure in coop and elects this
    // client as new master, it writes host_migration.json via PhysFS and
    // sends a HOST_MIGRATION broadcast.  Read that file and start LAN
    // broadcasting so new joiners can discover the migrated game.
    private val hostMigrationReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                val context = ctx ?: return
                // This player is now the host. Replace the old client-mode
                // proxy with a host-mode proxy that accepts incoming
                // connections from the network while keeping the engine
                // on loopback
                val proxyPort =
                    com.dxxredux.app.multiplayer.NetworkConstants.HOST_PROXY_PORT
                com.dxxredux.app.multiplayer.MatchmakingService
                    .createProxy(listenPort = proxyPort)
                // PhysFS write dir is filesDir/d2x-redux/ or d1x-redux/
                val d2File = java.io.File(context.filesDir, "d2x-redux/host_migration.json")
                val d1File = java.io.File(context.filesDir, "d1x-redux/host_migration.json")
                val file =
                    when {
                        d2File.exists() && d1File.exists() -> {
                            if (d2File.lastModified() >= d1File.lastModified()) d2File else d1File
                        }

                        d2File.exists() -> {
                            d2File
                        }

                        d1File.exists() -> {
                            d1File
                        }

                        else -> {
                            Log.w("DXX-MP", "host_migration.json not found in d1 or d2 dirs")
                            return
                        }
                    }
                try {
                    val json =
                        kotlinx.serialization.json.Json
                            .parseToJsonElement(file.readText())
                            .jsonObject
                    val callsign = json["callsign"]?.jsonPrimitive?.content ?: "Player"
                    val game = json["game"]?.jsonPrimitive?.content ?: "d2"
                    val mission = json["mission"]?.jsonPrimitive?.content ?: ""
                    val mode = json["mode"]?.jsonPrimitive?.content ?: "coop"
                    val difficulty = json["difficulty"]?.jsonPrimitive?.int ?: 1
                    val levelNum = json["level_num"]?.jsonPrimitive?.int ?: 1
                    val maxPlayers = json["max_players"]?.jsonPrimitive?.int ?: 4
                    val coopQol = json["coop_qol"]?.jsonPrimitive?.content?.toBooleanStrictOrNull() ?: true
                    val duplicateEnergyShields =
                        json["duplicate_energy_shields"]?.jsonPrimitive?.content?.toBooleanStrictOrNull() ?: false
                    val fullDeathSpew = json["full_death_spew"]?.jsonPrimitive?.content?.toBooleanStrictOrNull() ?: true
                    val playerSpewNoExpire =
                        json["player_spew_no_expire"]?.jsonPrimitive?.content?.toBooleanStrictOrNull() ?: true
                    val restrictNonCoopFovToBase =
                        json["restrict_noncoop_fov_to_base"]?.jsonPrimitive?.content?.toBooleanStrictOrNull() ?: false
                    Log.i(
                        "DXX-MP",
                        "Host migration: proxy on :$proxyPort, LAN broadcast as $callsign ($game/$mission lvl=$levelNum)",
                    )
                    com.dxxredux.app.lobby.LobbyService
                        .startDiscovery(context, callsign)
                    com.dxxredux.app.lobby.LobbyService
                        .hostLobby(callsign, game, mission, mode, maxPlayers)
                    com.dxxredux.app.lobby.LobbyService
                        .startGame(
                            difficulty,
                            levelNum,
                            coopQol = coopQol,
                            duplicateEnergyShields = duplicateEnergyShields,
                            fullDeathSpew = fullDeathSpew,
                            playerSpewNoExpire = playerSpewNoExpire,
                            clientsCanRequestRewind = false,
                            restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                            hostPort = proxyPort,
                        )
                    // Clean up the migration file
                    file.delete()
                } catch (e: Exception) {
                    Log.e("DXX-MP", "Failed to process host_migration.json", e)
                }
            }
        }

    /** Active download progress visible to introspection. */
    internal val downloadStates = mutableMapOf<String, Int>()

    // -- Controller live-state -------------------------------------------

    /** Axis values observable by Compose (LX, LY, RX, RY, LT, RT). */
    internal val controllerAxes = FloatArray(6)

    /** D-Pad HAT axis values (hatX, hatY). */
    internal val dpadAxes = FloatArray(2)

    /** Last synthesized HAT-axis directions so launcher focus sees edge changes only. */
    private var hatXState = 0
    private var hatYState = 0

    /** Last debounced left-stick directions for launcher focus navigation. */
    private var leftStickXState = 0
    private var leftStickYState = 0

    /** Last debounced right-stick directions for controller-picker dialog navigation. */
    private var rightStickXState = 0
    private var rightStickYState = 0

    /** Last synthesized effective navigation directions after combining HAT and stick input. */
    private var navXState = 0
    private var navYState = 0

    /** Compose-observable axis update counter (increment triggers recompose). */
    internal val axisGeneration = mutableIntStateOf(0)

    /** Currently pressed gamepad buttons (name strings). */
    internal val pressedButtons = mutableStateListOf<String>()

    /** Set to true when the controller config page is shown (needs all button events). */
    internal var controllerConfigActive = false

    /** True while a controller-config picker dialog is open and should receive D-pad/A input. */
    internal var controllerConfigDialogOpen = false

    /** Active controller-config dialog view for dialog-local key routing. */
    internal var controllerConfigDialogView: View? = null

    private fun hatAxisDirection(value: Float): Int =
        when {
            value < -0.5f -> -1
            value > 0.5f -> 1
            else -> 0
        }

    private fun stickAxisDirection(
        value: Float,
        oldDirection: Int,
    ): Int =
        when {
            oldDirection == -1 && value < -0.25f -> -1
            oldDirection == 1 && value > 0.25f -> 1
            value < -0.6f -> -1
            value > 0.6f -> 1
            else -> 0
        }

    private fun synthesizeDpadKeyEvent(
        targetView: View?,
        keyCode: Int,
        action: Int,
    ) {
        val eventTime = SystemClock.uptimeMillis()
        val event = KeyEvent(eventTime, eventTime, action, keyCode, 0)
        if (targetView?.dispatchKeyEvent(event) != true) super.dispatchKeyEvent(event)
    }

    private fun synthesizeDpadTransition(
        targetView: View?,
        oldDirection: Int,
        newDirection: Int,
        negativeKeyCode: Int,
        positiveKeyCode: Int,
    ) {
        if (oldDirection == newDirection) return
        if (oldDirection == -1) synthesizeDpadKeyEvent(targetView, negativeKeyCode, KeyEvent.ACTION_UP)
        if (oldDirection == 1) synthesizeDpadKeyEvent(targetView, positiveKeyCode, KeyEvent.ACTION_UP)
        if (newDirection == -1) synthesizeDpadKeyEvent(targetView, negativeKeyCode, KeyEvent.ACTION_DOWN)
        if (newDirection == 1) synthesizeDpadKeyEvent(targetView, positiveKeyCode, KeyEvent.ACTION_DOWN)
    }

    internal fun handleControllerMotion(
        targetView: View? = null,
        event: MotionEvent,
    ): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK ||
            event.action != MotionEvent.ACTION_MOVE
        ) {
            return false
        }

        if (controllerConfigDialogOpen && targetView != null) {
            controllerConfigDialogView = targetView.rootView
        }

        controllerAxes[0] = event.getAxisValue(MotionEvent.AXIS_X)
        controllerAxes[1] = event.getAxisValue(MotionEvent.AXIS_Y)
        controllerAxes[2] = event.getAxisValue(MotionEvent.AXIS_Z)
        controllerAxes[3] = event.getAxisValue(MotionEvent.AXIS_RZ)
        controllerAxes[4] = event.getAxisValue(MotionEvent.AXIS_LTRIGGER)
        controllerAxes[5] = event.getAxisValue(MotionEvent.AXIS_RTRIGGER)
        dpadAxes[0] = event.getAxisValue(MotionEvent.AXIS_HAT_X)
        dpadAxes[1] = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
        axisGeneration.intValue++

        if (controllerConfigActive) {
            LauncherDebugLog.log(
                "[ctrl-picker] motion axisGen=${axisGeneration.intValue} " +
                    "lx=${"%.3f".format(controllerAxes[0])} ly=${"%.3f".format(controllerAxes[1])} " +
                    "rx=${"%.3f".format(controllerAxes[2])} ry=${"%.3f".format(controllerAxes[3])} " +
                    "lt=${"%.3f".format(controllerAxes[4])} rt=${"%.3f".format(controllerAxes[5])} " +
                    "hx=${"%.3f".format(dpadAxes[0])} hy=${"%.3f".format(dpadAxes[1])}",
            )
        }

        if (!controllerConfigActive || controllerConfigDialogOpen) {
            val newHatX = hatAxisDirection(dpadAxes[0])
            val newHatY = hatAxisDirection(dpadAxes[1])
            val newLeftStickX = stickAxisDirection(controllerAxes[0], leftStickXState)
            val newLeftStickY = stickAxisDirection(controllerAxes[1], leftStickYState)
            val newRightStickX =
                if (controllerConfigDialogOpen) {
                    stickAxisDirection(controllerAxes[2], rightStickXState)
                } else {
                    0
                }
            val newRightStickY =
                if (controllerConfigDialogOpen) {
                    stickAxisDirection(controllerAxes[3], rightStickYState)
                } else {
                    0
                }
            val newNavX =
                when {
                    newHatX != 0 -> newHatX
                    newLeftStickX != 0 -> newLeftStickX
                    else -> newRightStickX
                }
            val newNavY =
                when {
                    newHatY != 0 -> newHatY
                    newLeftStickY != 0 -> newLeftStickY
                    else -> newRightStickY
                }
            if (newNavX != 0 || newNavY != 0) launcherControllerNavigationActive.value = true
            val navTarget = if (controllerConfigDialogOpen) controllerConfigDialogView else null
            synthesizeDpadTransition(
                navTarget,
                navXState,
                newNavX,
                KeyEvent.KEYCODE_DPAD_LEFT,
                KeyEvent.KEYCODE_DPAD_RIGHT,
            )
            synthesizeDpadTransition(
                navTarget,
                navYState,
                newNavY,
                KeyEvent.KEYCODE_DPAD_UP,
                KeyEvent.KEYCODE_DPAD_DOWN,
            )
            hatXState = newHatX
            hatYState = newHatY
            leftStickXState = newLeftStickX
            leftStickYState = newLeftStickY
            rightStickXState = newRightStickX
            rightStickYState = newRightStickY
            navXState = newNavX
            navYState = newNavY
        }

        return true
    }

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            launcherControllerNavigationActive.value = false
        }
        return super.dispatchTouchEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (handleControllerMotion(event = event)) return true
        return super.dispatchGenericMotionEvent(event)
    }

    private fun gamepadButtonName(keyCode: Int): String? =
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> "A"
            KeyEvent.KEYCODE_BUTTON_B -> "B"
            KeyEvent.KEYCODE_BUTTON_X -> "X"
            KeyEvent.KEYCODE_BUTTON_Y -> "Y"
            KeyEvent.KEYCODE_BUTTON_L1 -> "L1"
            KeyEvent.KEYCODE_BUTTON_R1 -> "R1"
            KeyEvent.KEYCODE_BUTTON_L2 -> "L2"
            KeyEvent.KEYCODE_BUTTON_R2 -> "R2"
            KeyEvent.KEYCODE_BUTTON_SELECT -> "Select"
            KeyEvent.KEYCODE_BUTTON_START -> "Start"
            KeyEvent.KEYCODE_BUTTON_THUMBL -> "L3"
            KeyEvent.KEYCODE_BUTTON_THUMBR -> "R3"
            KeyEvent.KEYCODE_DPAD_UP -> "D-Up"
            KeyEvent.KEYCODE_DPAD_DOWN -> "D-Down"
            KeyEvent.KEYCODE_DPAD_LEFT -> "D-Left"
            KeyEvent.KEYCODE_DPAD_RIGHT -> "D-Right"
            else -> null
        }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val name = gamepadButtonName(event.keyCode)
        if (name != null) {
            if (event.action == KeyEvent.ACTION_DOWN) launcherControllerNavigationActive.value = true
            if (event.action == KeyEvent.ACTION_DOWN) {
                if (name !in pressedButtons) pressedButtons.add(name)
            } else if (event.action == KeyEvent.ACTION_UP) {
                pressedButtons.remove(name)
            }
            // Controller config page normally consumes controller buttons and
            // drives its own polling-based navigation. While a picker dialog
            // is open, let D-pad and A flow through so the dialog can use
            // Compose focus navigation directly.
            if (controllerConfigActive) {
                val allowPickerNavigation =
                    controllerConfigDialogOpen &&
                        event.keyCode in
                        intArrayOf(
                            KeyEvent.KEYCODE_DPAD_UP,
                            KeyEvent.KEYCODE_DPAD_DOWN,
                            KeyEvent.KEYCODE_DPAD_LEFT,
                            KeyEvent.KEYCODE_DPAD_RIGHT,
                            KeyEvent.KEYCODE_BUTTON_A,
                        )
                if (!allowPickerNavigation) return true
                val dialogView = controllerConfigDialogView
                if (event.keyCode == KeyEvent.KEYCODE_BUTTON_A) {
                    val center = KeyEvent(event.action, KeyEvent.KEYCODE_DPAD_CENTER)
                    return if (dialogView !=
                        null
                    ) {
                        dialogView.dispatchKeyEvent(center)
                    } else {
                        super.dispatchKeyEvent(center)
                    }
                }
                return if (dialogView != null) dialogView.dispatchKeyEvent(event) else super.dispatchKeyEvent(event)
            }
            // Map B button to system Back for page navigation
            Log.d("DXX-Focus", "Gamepad key: $name action=${event.action}")
            if (event.keyCode == KeyEvent.KEYCODE_BUTTON_B) {
                if (event.action == KeyEvent.ACTION_UP) onBackPressedDispatcher.onBackPressed()
                return true
            }
            // Map A button to DPAD_CENTER so it activates focused elements
            if (event.keyCode == KeyEvent.KEYCODE_BUTTON_A) {
                val center = KeyEvent(event.action, KeyEvent.KEYCODE_DPAD_CENTER)
                return super.dispatchKeyEvent(center)
            }
            val navigable =
                event.keyCode in
                    intArrayOf(
                        KeyEvent.KEYCODE_DPAD_UP,
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        KeyEvent.KEYCODE_DPAD_LEFT,
                        KeyEvent.KEYCODE_DPAD_RIGHT,
                        KeyEvent.KEYCODE_DPAD_CENTER,
                    )
            if (!navigable) return true
        }
        return super.dispatchKeyEvent(event)
    }

    private fun beginMultiplayerLaunchRequest(game: String) {
        if (!beginLaunchPreparation(game, "multiplayer")) return
        RouteMetadataDiagnostics.log("Multiplayer launch action committed game=$game")
        resumeOfferRefreshHandler.postDelayed(
            multiplayerLaunchRequestTimeout,
            MULTIPLAYER_LAUNCH_REQUEST_TIMEOUT_MS,
        )
    }

    private fun launchMultiplayerGame(info: GameLaunchInfo) {
        if (mpGameLaunching) {
            Log.w("DXX-MP", "Game already launching, ignoring duplicate")
            return
        }
        resumeOfferRefreshHandler.removeCallbacks(multiplayerLaunchRequestTimeout)
        if (launchPreparation.value == null && !beginLaunchPreparation(info.game, "multiplayer")) return
        mpGameLaunching = true
        RouteMetadataDiagnostics.log(
            "Multiplayer launch event received game=${info.game} mode=${info.mode} " +
                "elapsed_ms=${SystemClock.elapsedRealtime() - (launchPreparation.value?.startedAtMs ?: 0L)}",
        )
        lifecycleScope.launch {
            val preflightMessage =
                try {
                    withContext(Dispatchers.IO) { prepareGameLaunchFiles(info.game) }
                } catch (e: Exception) {
                    Log.e("DXX-Setup", "Multiplayer launch preflight failed for ${info.game}", e)
                    "Could not prepare ${gameDisplayName(info.game)}"
                }
            if (preflightMessage != null) {
                mpGameLaunching = false
                finishLaunchPreparation("multiplayer_preflight_failed")
                showLaunchPreflightFailure(preflightMessage)
                return@launch
            }
            val requirement = info.missionRequirement
            if (requirement != null) {
                val missionStatus =
                    withContext(Dispatchers.IO) {
                        com.dxxredux.app.multiplayer.MissionCompatibilityResolver.resolve(
                            this@SetupActivity,
                            requirement,
                            info.mode,
                        )
                    }
                if (missionStatus.status != com.dxxredux.app.multiplayer.MissionCompatibilityStatus.MATCH) {
                    mpGameLaunching = false
                    finishLaunchPreparation("multiplayer_mission_mismatch")
                    showLaunchPreflightFailure(missionStatus.status.userLabel(missionStatus))
                    return@launch
                }
            }
            continueMultiplayerGameLaunch(info)
        }
    }

    private fun continueMultiplayerGameLaunch(info: GameLaunchInfo) {
        // For LAN hosts, keep the announce broadcast alive so the game
        // remains discoverable; for everyone else, shut down fully
        wasLanDiscoveringBeforeLaunch = com.dxxredux.app.lobby.LobbyService.isDiscovering.value
        if (info.isLan && info.isHost) {
            // stopInGameBroadcast will be called when the game exits
        } else {
            com.dxxredux.app.lobby.LobbyService
                .stopDiscovery()
        }
        val launchCallsign =
            MatchmakingStateHolder.state.value.callsign
                .takeIf { it.isNotBlank() } ?: mpCallsign
        mpCallsign = launchCallsign
        val mpIntent = createGameLaunchIntent(info.game)
        mpIntent.putExtra("mp_callsign", launchCallsign)
        if (launchCallsign.isNotBlank()) {
            mpIntent.putExtra("pilot_callsign", launchCallsign)
        }
        if (info.isHost) {
            mpIntent.putExtra("mp_mode", "host")
            mpIntent.putExtra("mp_my_port", NetworkConstants.ENGINE_PORT)
            mpIntent.putExtra("mp_mission", info.mission)
            mpIntent.putExtra("mp_game_mode", NetworkConstants.gameModeToInt(info.mode))
            mpIntent.putExtra("mp_max_players", info.maxPlayers)
            mpIntent.putExtra("mp_level_num", info.levelNum)
            mpIntent.putExtra("mp_difficulty", info.difficulty)
            mpIntent.putExtra("mp_coop_qol", info.coopQol)
            mpIntent.putExtra("mp_duplicate_energy_shields", info.duplicateEnergyShields)
            mpIntent.putExtra("mp_full_death_spew", info.fullDeathSpew)
            mpIntent.putExtra("mp_player_spew_no_expire", info.playerSpewNoExpire)
            mpIntent.putExtra("mp_clients_can_request_rewind", info.clientsCanRequestRewind)
            mpIntent.putExtra("mp_host_observer", info.hostObserver)
        } else {
            mpIntent.putExtra("mp_mode", "join")
            if (info.lanHostAddr != null) {
                // LAN joiner: route through proxy for packet stats
                MatchmakingService.createProxy(
                    peerAddr = info.lanHostAddr,
                    peerPort = info.lanHostPort,
                )
                mpIntent.putExtra("mp_host_addr", "127.0.0.1")
                mpIntent.putExtra("mp_host_port", NetworkConstants.PROXY_PORT_BASE)
            } else {
                // Online: use existing proxy from matchmaking
                val hostAddr = mpJoinHostAddrOverride ?: "127.0.0.1"
                val hostPort = mpJoinHostPortOverride ?: NetworkConstants.PROXY_PORT_BASE
                Log.i(
                    "DXX-MP",
                    "launchMP join: override=$mpJoinHostAddrOverride:$mpJoinHostPortOverride resolved=$hostAddr:$hostPort",
                )
                mpIntent.putExtra("mp_host_addr", hostAddr)
                mpIntent.putExtra("mp_host_port", hostPort)
            }
            mpIntent.putExtra("mp_my_port", NetworkConstants.ENGINE_PORT)
        }
        mpIntent.putExtra("mp_restrict_noncoop_fov_to_base", info.restrictNonCoopFovToBase && info.mode != "coop")
        if (info.isLan) mpIntent.putExtra("mp_is_lan", true)
        MultiplayerResumePrefs.saveLaunch(this, info, launchCallsign, MatchmakingStateHolder.state.value)
        val coopRestoreSlot =
            if (info.isHost && info.mode == "coop") {
                readCoopRestoreSlot(filesDir, info.game) ?: -1
            } else {
                -1
            }
        val mpRole = if (info.isHost) "host" else "join"
        val mpTransport = if (info.isLan) "lan" else "matchmaking"
        CoopDesyncLog.log(
            "mp launch: role=$mpRole transport=$mpTransport " +
                "game=${info.game} mission=${info.mission} mode=${info.mode} " +
                "level=${info.levelNum} diff=${info.difficulty} " +
                "max=${info.maxPlayers} callsign=$launchCallsign restore_slot=$coopRestoreSlot",
        )
        // Clear gameLaunchInfo after consumption to prevent stale re-launches
        MatchmakingStateHolder.update { it.copy(gameLaunchInfo = null) }
        startGameAfterRouteMetadataHandoff(mpIntent)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        CrashLog.install(this)
        // The launcher uses dxx-redux-d2 JNI helpers (MIDI/CD preview,
        // enumeration, import helpers). Load it here so native breadcrumb
        // storage is ready before any launcher-side native work starts.
        System.loadLibrary("dxx-redux-d2")
        Log.i("SetupActivity", "Loaded native library: dxx-redux-d2")
        CrashLog.installNativeHandler(this)
        DebugLog.init(this)
        KnownVersions.init(this)
        com.dxxredux.app.multiplayer.NetLog
            .init(this)

        // Initialize Google Play Games sign-in (no-op if not configured)
        PlayGamesAuth.initialize(this)
        MatchmakingService.setActivity(this)

        // Load persisted callsign (or generate random on first run)
        mpCallsign =
            com.dxxredux.app.multiplayer.CallsignPrefs
                .load(this)
        MatchmakingStateHolder.update { it.copy(callsign = mpCallsign) }

        // Edge-to-edge: draw behind system bars, Compose handles insets
        WindowCompat.setDecorFitsSystemWindows(window, false)

        // These actions also have package-scoped senders in the game and launcher processes
        DynamicReceiverPolicy.registerAppInternalOrDebugExternal(
            this,
            introspectReceiver,
            IntentFilter("com.dxxredux.SETUP_INTROSPECT"),
        )
        DynamicReceiverPolicy.registerAppInternalOrDebugExternal(
            this,
            commandReceiver,
            IntentFilter("com.dxxredux.SETUP_COMMAND"),
        )

        // Test automation remains externally reachable only in debuggable builds
        DynamicReceiverPolicy.registerDebugExternal(
            this,
            mpCommandReceiver,
            IntentFilter("com.dxxredux.MP_COMMAND"),
        )
        DynamicReceiverPolicy.registerDebugExternal(
            this,
            automateSetupReceiver,
            IntentFilter("com.dxxredux.SETUP_AUTOMATE"),
        )

        // Host migration is sent only by this application's game process
        DynamicReceiverPolicy.registerAppInternal(
            this,
            hostMigrationReceiver,
            IntentFilter("com.dxxredux.HOST_MIGRATION"),
        )

        gameRunningFlag = hasReturnableGameActivity()
        routeMetadataCoordinator = RouteMetadataPrecomputeCoordinator(this, routeMetadataScope)
        RouteMetadataPrecomputeFocusBroker.attach(routeMetadataFocusHandler)
        val filesDir = filesDir

        setContent {
            var launchPreflightMessage by launchFailureMessage
            LauncherTheme {
                launchPreflightMessage?.let { message ->
                    AlertDialog(
                        onDismissRequest = { launchPreflightMessage = null },
                        title = { Text("Launch Blocked") },
                        text = {
                            SelectionContainer {
                                Text(message, fontSize = 12.sp)
                            }
                        },
                        confirmButton = {
                            TextButton(onClick = { launchPreflightMessage = null }) {
                                Text("OK")
                            }
                        },
                    )
                }
                launchPreparation.value?.takeIf(::launcherPreparationShowsDialog)?.let { preparation ->
                    LauncherPreparationDialog(preparation)
                }
            }
            SetupScreen(
                filesDir = filesDir,
                gameRunning = gameRunningFlag,
                refreshTrigger = refreshTrigger.intValue,
                focusResumeTrigger = focusResumeTrigger.intValue,
                controllerNavigationActive = launcherControllerNavigationActive.value,
                controllerAxes = controllerAxes,
                dpadAxes = dpadAxes,
                axisGeneration = axisGeneration.intValue,
                pressedButtons = pressedButtons,
                pickedImportUris = pendingPickedImportUris.value,
                onPickedImportConsumed = { pendingPickedImportUris.value = emptyList() },
                onLaunchGame = onLaunch@{ game, resumeCandidate ->
                    val pending = launcherExecutor?.consumePendingLaunch()
                    if (pending != null) {
                        launchGameForAutomation(
                            game,
                            pending.scriptPath,
                            pending.nextStep,
                            resumeCandidate,
                            pending.runId,
                        )
                    } else if (resumeCandidate == null && (gameRunningFlag || hasReturnableGameActivity())) {
                        if (!returnToGame()) {
                            gameRunningFlag = false
                            refreshTrigger.intValue++
                        }
                    } else if (resumeCandidate != null && hasReturnableGameActivity()) {
                        Toast
                            .makeText(
                                this,
                                "Return to the running game before loading a save",
                                Toast.LENGTH_SHORT,
                            ).show()
                    } else {
                        val launchGame = resumeCandidate?.game ?: game
                        val resolvedResumeSavePath =
                            resumeCandidate?.let { candidate ->
                                resolveResumeSaveLaunchPath(filesDir, candidate)
                            }
                        val resolvedResumeCallsign =
                            resumeCandidate?.let { candidate ->
                                resolveResumeSaveLaunchCallsign(candidate)
                            }
                        if (resumeCandidate != null && resolvedResumeSavePath.isNullOrBlank()) {
                            Log.w(
                                "DXX-Setup",
                                "Resume candidate has no launch path: path=${resumeCandidate.path} " +
                                    "relative=${resumeCandidate.relativePath} callsign=${resumeCandidate.callsign}",
                            )
                            logResumeCandidateLaunch(
                                "setup-resume-candidate-invalid",
                                resumeCandidate,
                                null,
                                resolvedResumeCallsign,
                            )
                            Toast.makeText(this, "Could not read the save launch details", Toast.LENGTH_SHORT).show()
                        } else {
                            val launchKind = if (resumeCandidate != null) "resume" else "game"
                            if (!beginLaunchPreparation(launchGame, launchKind)) {
                                return@onLaunch
                            }
                            lifecycleScope.launch {
                                val preflightMessage =
                                    try {
                                        withContext(Dispatchers.IO) { prepareGameLaunchFiles(launchGame) }
                                    } catch (e: Exception) {
                                        Log.e("DXX-Setup", "Launch preflight failed for $launchGame", e)
                                        "Could not prepare ${gameDisplayName(launchGame)}"
                                    }
                                if (preflightMessage != null) {
                                    launchPreflightMessage = preflightMessage
                                    finishLaunchPreparation("preflight_failed")
                                    return@launch
                                }
                                if (resumeCandidate != null) {
                                    logResumeCandidateLaunch(
                                        "setup-resume-candidate-selected",
                                        resumeCandidate,
                                        resolvedResumeSavePath,
                                        resolvedResumeCallsign,
                                    )
                                }
                                val intent =
                                    createGameLaunchIntent(
                                        game = launchGame,
                                        inputDemoReplayPath = null,
                                        resumeSavePath = resolvedResumeSavePath,
                                        resumeCallsign = resolvedResumeCallsign,
                                    )
                                startGameAfterRouteMetadataHandoff(intent)
                                // Don't finish() -- stay in back stack so quitting
                                // the game returns here instead of the launcher.
                            }
                        }
                    }
                },
                onPlayInputDemo = { demo ->
                    launchInputDemoReplay(demo)
                },
                onMultiplayerLaunch = { info ->
                    launchMultiplayerGame(info)
                },
                onMultiplayerLaunchRequested = ::beginMultiplayerLaunchRequest,
                onContentImported = { routeMetadataCoordinator.notifyContentImported() },
                onClearRouteMetadataCache = {
                    val result = routeMetadataCoordinator.clearCache()
                    result.removedFiles
                },
                onSetRouteMetadataComputeFaster = routeMetadataCoordinator::setComputeFaster,
                onRefresh = {
                    routeMetadataCoordinator.wake()
                    refreshTrigger.intValue++
                },
                onDownloadStateChanged = { name, progress ->
                    if (progress == -2) {
                        downloadStates.remove(name)
                    } else {
                        downloadStates[name] = progress
                    }
                },
            )
        }
    }

    /**
     * Write initial descent.cfg with Android-appropriate defaults if the file
     * doesn't exist yet (first launch).  Once the user changes settings in-game,
     * the engine overwrites this file and their preferences stick.
     *
     * Settings that live in binary .plr files (like ControlType) can't be
     * handled here -- those are set in config.c's android_apply_initial_defaults().
     */
    internal fun notifyRouteMetadataContentImportedForAutomation() {
        routeMetadataCoordinator.notifyContentImported()
    }

    private fun writeInitialGameConfig() {
        writeDefaultControllerConfig()
        // Check all config paths -- don't overwrite if any exist (user has a config)
        val cfgPaths = mutableListOf(File(filesDir, "descent.cfg"))
        for (sub in listOf("d1x-redux", "d2x-redux")) {
            val f = File(File(filesDir, sub), "descent.cfg")
            if (f.exists()) cfgPaths.add(f)
        }
        if (cfgPaths.any { it.exists() }) return

        // Determine the device's real screen dimensions (including system bars)
        val (screenW, screenH) =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val bounds = windowManager.currentWindowMetrics.bounds
                Pair(bounds.width(), bounds.height())
            } else {
                @Suppress("DEPRECATION")
                val size = android.graphics.Point()
                @Suppress("DEPRECATION")
                windowManager.defaultDisplay.getRealSize(size)
                Pair(size.x, size.y)
            }

        // Ensure wider dimension is treated as width (game is landscape)
        val w = maxOf(screenW, screenH)
        val h = minOf(screenW, screenH)

        // Reduce to simplest fraction via GCD
        fun gcd(
            a: Int,
            b: Int,
        ): Int = if (b == 0) a else gcd(b, a % b)
        val g = gcd(w, h)
        val aspectY = w / g // width component  (game naming: Y = wider)
        val aspectX = h / g // height component (game naming: X = narrower)

        Log.i("DXX-Setup", "First launch: writing descent.cfg with aspect ${aspectY}x$aspectX (from ${w}x$h)")

        // Default render resolution: full screen (rounded to even). The old
        // half-screen default made small in-game menu text unreadable on high
        // DPI phones after Android compositor scaling.
        val resW = (w + 1) and 0x7FFFFFFE
        val resH = (h + 1) and 0x7FFFFFFE

        // Write to all config paths (root + game subdirs) so the game finds
        // the resolution in whichever PHYSFS search path it checks first
        updateAllConfigFiles(
            filesDir,
            listOf(
                "AspectX" to "$aspectX",
                "AspectY" to "$aspectY",
                "ResolutionX" to "$resW",
                "ResolutionY" to "$resH",
                "CornerTextInset" to "1",
            ),
        )

        // Store matching preference so the picker shows the right selection
        getSharedPreferences("dxx_prefs", MODE_PRIVATE)
            .edit()
            .putString("render_resolution", "${resW}x$resH")
            .apply()
        Log.i("DXX-Setup", "First launch: default resolution ${resW}x$resH")
    }

    private fun migrateLegacyHalfRenderResolution() {
        val prefs = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
        if (prefs.getLong(PREF_GRAPHICS_SETTINGS_GENERATION, 0L) != 0L) return

        val options = computeResolutionOptions(this)
        val full = options.getOrNull(0)?.first ?: return
        val half = options.getOrNull(1)?.first ?: return
        if (full == half) return

        val stored = prefs.getString("render_resolution", null)
        val cfg =
            readConfigValue(filesDir, "ResolutionX")
                ?.let { w -> readConfigValue(filesDir, "ResolutionY")?.let { h -> "${w}x$h" } }
        if (cfg != half || (stored != null && stored != half)) return

        updateDescentCfgResolution(filesDir, full)
        prefs
            .edit()
            .putString("render_resolution", full)
            .putBoolean("render_resolution_default_full_migrated", true)
            .apply()
        Log.i("DXX-Setup", "Migrated legacy default render resolution from $half to $full")
    }

    /**
     * Write controller_config.json from bundled defaults if it doesn't exist
     * or if its version is older than CONTROLLER_CONFIG_VERSION.
     */
    private fun writeDefaultControllerConfig() {
        val file = File(filesDir, "controller_config.json")
        if (file.exists()) {
            try {
                val json = org.json.JSONObject(file.readText())
                if (isNativeControllerConfigValid(json)) return
            } catch (_: Exception) {
                // corrupt, regenerate
            }
        }
        val bindings = loadDefaultBindings(applicationContext)
        saveConfig(applicationContext, bindings, emptySet())
        Log.i("DXX-Setup", "Wrote default controller config (version $CONTROLLER_CONFIG_VERSION)")
    }

    override fun onResume() {
        super.onResume()
        try {
            NativeFatalErrorStore.consume(filesDir)?.let { launchFailureMessage.value = it }
        } catch (e: Exception) {
            Log.e("DXX-Setup", "Could not consume native fatal error", e)
        }
        val returningFromLevelPreview = LevelPreviewReturnRefreshGate.consumeReturn()
        mpGameLaunching = false
        gameRunningFlag = hasReturnableGameActivity()
        if (gameRunningFlag) {
            routeMetadataCoordinator.stop()
        } else {
            routeMetadataCoordinator.resumeAfterGame()
        }
        // If a LAN host was broadcasting in-game, stop now
        com.dxxredux.app.lobby.LobbyService
            .stopInGameBroadcast()
        // Auto-resume LAN discovery if it was active before game launch
        if (wasLanDiscoveringBeforeLaunch &&
            !com.dxxredux.app.lobby.LobbyService.isDiscovering.value
        ) {
            com.dxxredux.app.lobby.LobbyService
                .startDiscovery(this, mpCallsign)
        }
        wasLanDiscoveringBeforeLaunch = false
        focusResumeTrigger.intValue++
        if (returningFromLevelPreview) {
            resumeOfferRefreshHandler.removeCallbacks(resumeOfferRefreshRunnable)
            Log.i("DXX-Setup", "Preserving launcher metadata state after read-only level preview")
        } else {
            schedulePostResumeRefresh()
        }
        // If the host returns from a game, signal the server to reset the lobby
        val mpState =
            com.dxxredux.app.multiplayer.MatchmakingStateHolder
                .state
                .value
        if (mpState.currentLobby?.isHost == true && !hasReturnableGameActivity()) {
            com.dxxredux.app.multiplayer.MatchmakingService
                .endGame()
        }
        // Check if game exited with LAUNCHER_CONTINUE for automation
        val resultFile = File(filesDir, "automation_result.json")
        if (resultFile.exists()) {
            try {
                val json = org.json.JSONObject(resultFile.readText())
                if (json.optString("result") == "LAUNCHER_CONTINUE") {
                    val nextStep = json.getInt("next_step")
                    val resultRunId = json.optString("run_id", "")
                    val expectedRunId = activeAutomationRunId()
                    if (expectedRunId == null || expectedRunId != resultRunId) {
                        Log.w(
                            "DXX-Setup",
                            "Discarding LAUNCHER_CONTINUE for run_id=$resultRunId; expected=$expectedRunId",
                        )
                        resultFile.delete()
                        return
                    }
                    val resultScriptPath =
                        json.optString("script_path", "").takeIf { it.isNotEmpty() }?.let { path ->
                            if (path.startsWith("/")) path else filesDir.absolutePath + "/" + path
                        }
                    val existingExecutor = launcherExecutor
                    if (existingExecutor != null && existingExecutor.runId != resultRunId) {
                        Log.w(
                            "DXX-Setup",
                            "Ignoring LAUNCHER_CONTINUE for run_id=$resultRunId while run_id=${existingExecutor.runId} is active",
                        )
                        return
                    }
                    val executor =
                        existingExecutor
                            ?: LauncherScriptExecutor(this, resultRunId) { game, path, startStep ->
                                launchGameForAutomation(game, path, startStep, automationRunId = resultRunId)
                            }.also { launcherExecutor = it }

                    Log.i(
                        "DXX-Setup",
                        if (existingExecutor != null) {
                            "LAUNCHER_CONTINUE: resuming at step $nextStep"
                        } else {
                            "LAUNCHER_CONTINUE: recreating executor at step $nextStep"
                        },
                    )
                    resultFile.delete()
                    kotlinx.coroutines.MainScope().launch {
                        waitForAutomationGameExit()
                        if (existingExecutor != null) {
                            executor.resume(nextStep)
                        } else if (resultScriptPath != null) {
                            executor.execute(resultScriptPath, nextStep)
                        } else {
                            Log.e("DXX-Setup", "LAUNCHER_CONTINUE missing script_path for recovery")
                        }
                    }
                } else if (json.optString("result") in setOf("PASS", "FAIL")) {
                    clearActiveAutomationRunId(json.optString("run_id", ""))
                }
            } catch (e: Exception) {
                Log.e("DXX-Setup", "Error reading automation result", e)
            }
        }
    }

    private suspend fun waitForAutomationGameExit() {
        val deadline = SystemClock.elapsedRealtime() + 5000L
        while (
            (hasReturnableGameActivity() || runningGameProcessPid() != null) &&
            SystemClock.elapsedRealtime() < deadline
        ) {
            delay(100L)
        }
        val staleState = returnableGameActivityState()
        val stalePid = staleState?.pid ?: runningGameProcessPid()
        if (stalePid != null) {
            Log.w(
                "DXX-Setup",
                "LAUNCHER_CONTINUE: killing stale game process pid=$stalePid " +
                    "game=${staleState?.game ?: ""}",
            )
            android.os.Process.killProcess(stalePid)
            val killDeadline = SystemClock.elapsedRealtime() + 2000L
            while (
                (hasReturnableGameActivity() || runningGameProcessPid() != null) &&
                SystemClock.elapsedRealtime() < killDeadline
            ) {
                delay(100L)
            }
        }
        gameRunningFlag = hasReturnableGameActivity() || runningGameProcessPid() != null
        if (gameRunningFlag) {
            Log.w("DXX-Setup", "LAUNCHER_CONTINUE: game process still returnable after wait")
        } else {
            routeMetadataCoordinator.start()
            schedulePostResumeRefresh()
        }
    }

    override fun onPause() {
        if (launchPreparation.value?.phase == LauncherPreparationPhase.STARTING_GAME) {
            finishLaunchPreparation("activity_started")
        }
        routeMetadataCoordinator.stop("launcher paused")
        super.onPause()
    }

    override fun onDestroy() {
        RouteMetadataPrecomputeFocusBroker.detach(routeMetadataFocusHandler)
        routeMetadataCoordinator.stop("launcher destroyed")
        routeMetadataScope.cancel()
        resumeOfferRefreshHandler.removeCallbacks(resumeOfferRefreshRunnable)
        resumeOfferRefreshHandler.removeCallbacks(multiplayerLaunchRequestTimeout)
        try {
            unregisterReceiver(introspectReceiver)
        } catch (_: Exception) {
        }
        try {
            unregisterReceiver(commandReceiver)
        } catch (_: Exception) {
        }
        try {
            unregisterReceiver(hostMigrationReceiver)
        } catch (_: Exception) {
        }
        if (BuildConfig.DEBUG) {
            try {
                unregisterReceiver(mpCommandReceiver)
            } catch (_: Exception) {
            }
            try {
                unregisterReceiver(automateSetupReceiver)
            } catch (_: Exception) {
            }
        }
        super.onDestroy()
    }
}

// -- Composables -------------------------------------------------------------

@Composable
private fun LauncherTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = darkColorScheme(), content = content)
}

@Composable
private fun LauncherPreparationDialog(preparation: LauncherPreparationState) {
    AlertDialog(
        onDismissRequest = {},
        title = { Text("Launching game") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text(launcherPreparationLabel(preparation), fontSize = 12.sp)
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            }
        },
        confirmButton = {},
    )
}

@Composable
private fun SetupScreen(
    filesDir: File,
    gameRunning: Boolean,
    refreshTrigger: Int,
    focusResumeTrigger: Int,
    controllerNavigationActive: Boolean,
    controllerAxes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    pickedImportUris: List<Uri>,
    onPickedImportConsumed: () -> Unit,
    onLaunchGame: (String, ResumeSaveBridge.ResumeSaveCandidate?) -> Unit,
    onPlayInputDemo: (StagedInputDemo) -> Unit,
    onMultiplayerLaunch: (com.dxxredux.app.multiplayer.GameLaunchInfo) -> Unit,
    onMultiplayerLaunchRequested: (String) -> Unit,
    onContentImported: () -> Unit,
    onClearRouteMetadataCache: suspend () -> Int,
    onSetRouteMetadataComputeFaster: (Boolean) -> Unit,
    onRefresh: () -> Unit,
    onDownloadStateChanged: (String, Int) -> Unit = { _, _ -> },
) {
    val fileSetManager =
        remember {
            FileSetManager(filesDir).also {
                it.migrateDefaultSetIfNeeded()
                it.sweepRootGameFiles()
                it.migratePilotFiles()
            }
        }
    val ctxLocal = LocalContext.current
    var cleanedTmpFiles by remember { mutableStateOf<List<String>>(emptyList()) }
    LaunchedEffect(Unit) {
        cleanedTmpFiles = cleanupTmpDirWithReport(filesDir)
        val mgr = ImportLocationManager(filesDir)
        mgr.handleStaleInProgressMarkers(ctxLocal)
        if (mgr.isOverrideUnreachable()) {
            Toast
                .makeText(
                    ctxLocal,
                    "Imported-files override volume not present; using default app storage",
                    Toast.LENGTH_LONG,
                ).show()
        }
    }
    var activeSetName by remember { mutableStateOf(fileSetManager.getActive()) }
    val setDir = remember(activeSetName) { fileSetManager.getSetDir(activeSetName) }
    val manifest = remember(activeSetName) { AssetManifest(setDir) }
    val safManifest = remember(activeSetName) { fileSetManager.safManifestForSet(activeSetName) }
    val d2FileList = remember(refreshTrigger, activeSetName) { detectD2FileList(setDir, safManifest) }
    val d2Statuses = remember(refreshTrigger, activeSetName) { checkFiles(setDir, d2FileList, manifest, safManifest) }
    val d1Statuses = remember(refreshTrigger, activeSetName) { checkFiles(setDir, D1_FILES, manifest, safManifest) }

    // -- Hashing progress state ------------------------------
    var hashingFile by remember { mutableStateOf<String?>(null) }
    var hashingFileIndex by remember { mutableIntStateOf(0) }
    var hashingTotalFiles by remember { mutableIntStateOf(0) }
    var hashingProgress by remember { mutableFloatStateOf(0f) }
    var resultImporting by remember { mutableStateOf(false) }
    val isHashing = hashingFile != null

    val d2RequiredOk =
        remember(refreshTrigger, activeSetName) {
            launchDataReadyForGame("d2", setDir, manifest, safManifest)
        }
    val d1RequiredOk =
        remember(refreshTrigger, activeSetName) {
            launchDataReadyForGame("d1", setDir, manifest, safManifest)
        }
    val canLaunch = d2RequiredOk || d1RequiredOk

    val context = androidx.compose.ui.platform.LocalContext.current
    val gamePrefs = remember { context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE) }
    val resumeOfferPrefEnabled = gamePrefs.getBoolean(PREF_SHOW_RESUME_OFFER, true)
    val resumeSaveOptions by produceState<ResumeSaveBridge.ResumeSaveOptions?>(
        initialValue = null,
        refreshTrigger,
        focusResumeTrigger,
        gameRunning,
        resumeOfferPrefEnabled,
    ) {
        value = ResumeSaveBridge.findOptions(filesDir)
        if (value == null && !gameRunning) {
            repeat(20) {
                delay(500L)
                value = ResumeSaveBridge.findOptions(filesDir)
                if (value != null) {
                    return@produceState
                }
            }
        }
    }

    fun resumeCandidateReady(candidate: ResumeSaveBridge.ResumeSaveCandidate?): Boolean =
        candidate != null && ((candidate.game == "d1" && d1RequiredOk) || (candidate.game != "d1" && d2RequiredOk))
    val availableResumeOptions =
        resumeSaveOptions?.let { options ->
            ResumeSaveBridge.ResumeSaveOptions(
                latestOverall = options.latestOverall?.takeIf { resumeCandidateReady(it) },
                highestProgress = options.highestProgress?.takeIf { resumeCandidateReady(it) },
                lastExit = options.lastExit?.takeIf { resumeCandidateReady(it) },
                lastAbort = options.lastAbort?.takeIf { resumeCandidateReady(it) },
                lastMinimize = options.lastMinimize?.takeIf { resumeCandidateReady(it) },
            )
        }
    val resumeCandidate =
        availableResumeOptions?.latestOverall
    val resumeOfferKey = resumeCandidate?.let { "${it.path}|${it.saveTimeUnixSeconds}" }
    var saveExplorerPanelExpanded by remember(refreshTrigger) {
        mutableStateOf(gamePrefs.getBoolean(PREF_SAVE_EXPLORER_PANEL_EXPANDED, resumeOfferPrefEnabled))
    }
    val showResumeArea =
        !gameRunning &&
            !isHashing &&
            resumeCandidate != null &&
            resumeOfferKey != null
    val showResumePanel = showResumeArea && saveExplorerPanelExpanded

    fun setSaveExplorerPanelExpanded(expanded: Boolean) {
        saveExplorerPanelExpanded = expanded
        val editor = gamePrefs.edit().putBoolean(PREF_SAVE_EXPLORER_PANEL_EXPANDED, expanded)
        if (expanded) {
            editor.putBoolean(PREF_SHOW_RESUME_OFFER, true)
        }
        editor.apply()
    }

    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }

    // -- Startup and refresh audit: prune stale entries, then hash new/changed files --
    var prunedSourceNames by remember { mutableStateOf<List<String>>(emptyList()) }
    var prunedDataFiles by remember { mutableStateOf<List<String>>(emptyList()) }
    LaunchedEffect(activeSetName, refreshTrigger) {
        val persistedActiveSet = fileSetManager.getActive()
        if (persistedActiveSet != activeSetName) {
            activeSetName = persistedActiveSet
            return@LaunchedEffect
        }
        if (LauncherDebugLog.isEnabled(context)) {
            launcherDumpFileTable(
                reason = "startup-before-prune",
                filesDir = filesDir,
                activeSetName = activeSetName,
                setDir = setDir,
                manifest = manifest,
                safManifest = safManifest,
            )
        }

        // 1. Reconcile all non-base content before consumers inspect paths.
        val contentResult = withContext(Dispatchers.IO) { FileSetContentManager(setDir).reconcile() }
        contentResult.conflicts.forEach { conflict ->
            LauncherDebugLog.log("content-reconcile-conflict active_set=$activeSetName detail=$conflict")
        }

        // 2. Prune audio sources
        val srcManager = AudioSourceManager(filesDir, setDir)
        val prunedSrc = srcManager.pruneMissingSources(setDir)
        if (prunedSrc.isNotEmpty()) {
            prunedSourceNames = prunedSrc
        }

        // 3. Prune stale manifest entries (before hashing, so hashing
        //    doesn't race against pruning on the same manifest file)
        val prunedAssets = manifest.pruneStaleEntries()
        val prunedSaf = safManifest.pruneStaleEntries(context)
        // Only report files that are truly gone from disk
        val allPruned =
            (prunedAssets + prunedSaf).filter { name ->
                findFile(setDir, name) == null
            }
        if (prunedAssets.isNotEmpty() || prunedSaf.isNotEmpty()) {
            val prunedAssetsText = if (prunedAssets.isEmpty()) "-" else prunedAssets.joinToString(",")
            val prunedSafText = if (prunedSaf.isEmpty()) "-" else prunedSaf.joinToString(",")
            val popupFilesText = if (allPruned.isEmpty()) "-" else allPruned.joinToString(",")
            LauncherDebugLog.log(
                "launcher-prune-summary active_set=$activeSetName pruned_assets=$prunedAssetsText pruned_saf=$prunedSafText popup_files=$popupFilesText",
            )
        }
        if (allPruned.isNotEmpty()) {
            LauncherDebugLog.log(
                "launcher-prune-popup active_set=$activeSetName files=${allPruned.joinToString(",")}",
            )
            if (LauncherDebugLog.isEnabled(context)) {
                launcherDumpFileTable(
                    reason = "popup-after-prune",
                    filesDir = filesDir,
                    activeSetName = activeSetName,
                    setDir = setDir,
                    manifest = manifest,
                    safManifest = safManifest,
                )
            }
            prunedDataFiles = allPruned
        }

        // 4. Hash files on disk that are missing manifest entries (or size changed)
        val allGameNames = ALL_GAME_FILENAMES
        val staleFiles = manifest.findStaleFiles(allGameNames)
        if (staleFiles.isNotEmpty()) {
            hashingTotalFiles = staleFiles.size
            for ((i, file) in staleFiles.withIndex()) {
                hashingFileIndex = i + 1
                hashingFile = file.name
                hashingProgress = 0f
                val sha256 =
                    withContext(Dispatchers.IO) {
                        AssetManifest.computeSha256(file) { bytesRead, totalBytes ->
                            if (totalBytes > 0) {
                                mainHandler.post {
                                    hashingProgress = bytesRead.toFloat() / totalBytes
                                }
                            }
                        }
                    }
                if (sha256 != null) {
                    withContext(Dispatchers.IO) { manifest.upsert(file.name, sha256, file.length()) }
                } else {
                    Log.w("DXX-Setup", "Skipping manifest update for ${file.name}: file disappeared during hashing")
                }
            }
            hashingFile = null
        }

        if (contentResult.adoptedIds.isNotEmpty() || contentResult.removedDuplicatePaths.isNotEmpty() ||
            prunedSrc.isNotEmpty() || allPruned.isNotEmpty() || staleFiles.isNotEmpty()
        ) {
            onRefresh()
        }
    }

    // Download state: filename -> progress (0..100, -1 = error, -2 = complete)
    val downloadProgress = remember { mutableStateMapOf<String, Int>() }
    val scope = rememberCoroutineScope()

    fun launchResultImport(block: suspend () -> Unit) {
        if (resultImporting) return
        resultImporting = true
        scope.launch {
            try {
                block()
            } finally {
                resultImporting = false
            }
        }
    }

    // -- File detail popup state -----------------------------
    var detailStatus by remember { mutableStateOf<FileStatus?>(null) }
    var detailIsD2 by remember { mutableStateOf(true) }

    // -- Set management dialog state -------------------------
    var showSetDialog by remember { mutableStateOf(false) }
    var showSaveExplorer by remember { mutableStateOf(false) }

    // -- Game selection state --------------------------------
    var showDemoInstallerOffer by remember {
        mutableStateOf(gamePrefs.getBoolean(PREF_SHOW_DEMO_INSTALLER_OFFER, true))
    }
    val demoInstallerOffers =
        visibleDemoInstallerOffers(
            showDemoInstallerOffer = showDemoInstallerOffer,
            d1Ready = d1RequiredOk,
            d2Ready = d2RequiredOk,
        )
    var selectedGame by remember {
        val saved = gamePrefs.getString("selected_game", null)
        mutableStateOf(
            when {
                saved == "d1" && d1RequiredOk -> "d1"
                saved == "d2" && d2RequiredOk -> "d2"
                d1RequiredOk && !d2RequiredOk -> "d1"
                d2RequiredOk -> "d2"
                else -> "d2"
            },
        )
    }
    // Auto-correct if readiness changes (e.g. user adds/removes files)
    LaunchedEffect(d1RequiredOk, d2RequiredOk) {
        if (selectedGame == "d1" && !d1RequiredOk && d2RequiredOk) selectedGame = "d2"
        if (selectedGame == "d2" && !d2RequiredOk && d1RequiredOk) selectedGame = "d1"
    }

    var scanResults by remember { mutableStateOf<List<FoundFile>?>(null) }
    var scanning by remember { mutableStateOf(false) }
    var importStatus by remember { mutableStateOf("") }

    // -- Demo download state ---------------------------------
    var demoDownloading by remember { mutableStateOf<String?>(null) } // package name or null
    var demoDownloadProgress by remember { mutableIntStateOf(0) }
    var demoDownloadError by remember { mutableStateOf<String?>(null) }
    var demoDownloadErrorName by remember { mutableStateOf<String?>(null) }

    // -- ZIP extraction state ----------------------------
    var zipExtracted by remember { mutableStateOf<List<ExtractedFile>?>(null) }
    var zipPackageName by remember { mutableStateOf<String?>(null) }
    var zipExtracting by remember { mutableStateOf(false) }
    var zipProgressFile by remember { mutableStateOf("") }
    var zipProgressBytes by remember { mutableLongStateOf(0L) }
    var zipProgressTotal by remember { mutableLongStateOf(0L) }
    var zipHadAudioFiles by remember { mutableStateOf(false) }
    var missionArchiveImporting by remember { mutableStateOf(false) }
    var missionArchiveProgressLabel by remember { mutableStateOf("") }
    var missionArchiveProgressBytes by remember { mutableLongStateOf(0L) }
    var missionArchiveProgressTotal by remember { mutableLongStateOf(0L) }

    // -- BIN/CUE disc import state -----------------------
    var discImportCueName by remember { mutableStateOf<String?>(null) }
    var discImportCueUri by remember { mutableStateOf<Uri?>(null) }
    var discImportBins by remember { mutableStateOf<List<Pair<String, Uri>>>(emptyList()) }

    // -- ISO disc import state ---------------------------
    var isoImportName by remember { mutableStateOf<String?>(null) }
    var isoImportUri by remember { mutableStateOf<Uri?>(null) }

    // -- GOG installer import state ----------------------
    var gogImportUri by remember { mutableStateOf<Uri?>(null) }
    var gogImportName by remember { mutableStateOf<String?>(null) }

    // -- SOW archive import state ------------------------
    var sowImportUri by remember { mutableStateOf<Uri?>(null) }
    var sowImportName by remember { mutableStateOf<String?>(null) }

    // -- Audio file auto-import state --------------------
    var audioImportUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
    var audioImporting by remember { mutableStateOf(false) }
    var audioImportLabel by remember { mutableStateOf("") }
    var audioImportBytes by remember { mutableLongStateOf(0L) }
    var audioImportTotal by remember { mutableLongStateOf(0L) }
    var zipArchiveUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
    val audioCustomMgr = remember(setDir.absolutePath) { CustomAudioSetManager(filesDir, setDir) }

    // -- DXA mod import state ----------------------------
    val dxaImportUris = remember { mutableListOf<Pair<String, Uri>>() }

    // -- Config JSON import state ------------------------
    var preparedConfigImport by remember { mutableStateOf<PreparedConfigImport?>(null) }
    var configImportName by remember { mutableStateOf<String?>(null) }

    val androidTvDevice = remember(context) { context.isAndroidTv() }
    val shouldSeedLauncherFocus = shouldSeedLauncherControllerFocus(androidTvDevice, controllerNavigationActive)
    val importChooserConfig = remember(androidTvDevice) { importChooserConfigForDevice(androidTvDevice) }
    var showImportChooser by remember { mutableStateOf(false) }

    suspend fun processPickedUris(uris: List<Uri>) {
        try {
            val dxaImportUris = mutableListOf<Pair<String, Uri>>()
            val missionZipImportUris = mutableListOf<Pair<String, Uri>>()
            val zipUris = mutableListOf<Pair<String, Uri>>()
            val gameUris = mutableListOf<FoundFile>()
            val cueUris = mutableListOf<Pair<String, Uri>>()
            val binUris = mutableListOf<Pair<String, Uri>>()
            val isoUris = mutableListOf<Pair<String, Uri>>()
            var gogUri: Pair<String, Uri>? = null
            var sowUri: Pair<String, Uri>? = null
            // Track raw .gog/.inst pairs (GOG CD images picked directly)
            var gogDiscUri: Pair<String, Uri>? = null // .gog BIN file
            var instDiscUri: Pair<String, Uri>? = null // .inst CUE sheet
            val unhandledFiles = mutableListOf<String>()
            val warnings = mutableListOf<String>()
            val audioFileUris = mutableListOf<Uri>()
            var jsonConfig: Pair<String, PreparedConfigImport>? = null
            for (uri in uris) {
                val name = getDisplayName(context, uri)
                if (name != null) {
                    val ext = GameFileFormats.extensionOf(name)
                    val demoPackage = matchDemoInstallerPackage(context, name, uri)
                    when {
                        demoPackage != null -> {
                            zipUris.add(demoPackage.filename to uri)
                        }

                        ext == "zip" -> {
                            val declaredSize = ImportStorageGuard.queryUriSizeBytes(context.contentResolver, uri)
                            val probeLimit =
                                declaredSize
                                    ?.coerceAtLeast(ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES)
                                    ?.coerceAtMost(ExtractionLimits.MAX_TOTAL_BYTES)
                                    ?: ExtractionLimits.MAX_TOTAL_BYTES
                            val missionZip =
                                try {
                                    ImportStorageGuard.requireFreeSpace(
                                        context.cacheDir,
                                        declaredSize ?: 0L,
                                        "inspect archive $name",
                                    )
                                    context.contentResolver.openInputStream(uri)?.use { input ->
                                        MissionZip.isImportCandidate(input, context.cacheDir, probeLimit)
                                    }
                                } catch (e: Exception) {
                                    Log.w("DXX-Setup", "Mission ZIP probe failed for $name: ${e.message}")
                                    false
                                }
                            val unsupportedD2xxl =
                                if (missionZip == true) {
                                    false
                                } else {
                                    try {
                                        context.contentResolver.openInputStream(uri)?.use { input ->
                                            MissionZip.containsUnsupportedD2xxlHog(
                                                input,
                                                context.cacheDir,
                                                probeLimit,
                                            )
                                        } == true
                                    } catch (e: Exception) {
                                        Log.w("DXX-Setup", "D2X-XL HOG probe failed for $name: ${e.message}")
                                        false
                                    }
                                }
                            if (missionZip == true || unsupportedD2xxl) {
                                missionZipImportUris.add(name to uri)
                            } else {
                                zipUris.add(name to uri)
                            }
                        }

                        ext == "7z" || ext == "rar" -> {
                            missionZipImportUris.add(name to uri)
                        }

                        ext in setOf("sit", "hqx") -> {
                            zipUris.add(name to uri)
                        }

                        ext == "cue" -> {
                            cueUris.add(name to uri)
                        }

                        ext == "iso" -> {
                            isoUris.add(name to uri)
                        }

                        ext == "inst" -> {
                            instDiscUri = name to uri
                        }

                        ext == "gog" -> {
                            gogDiscUri = name to uri
                        }

                        ext in setOf("bin", "img") -> {
                            binUris.add(name to uri)
                        }

                        ext in setOf("exe", "pkg") -> {
                            gogUri = name to uri
                        }

                        ext == "sow" -> {
                            sowUri = name to uri
                        }

                        isDirectGameDataImportName(name) -> {
                            gameUris.add(FoundFile(name, uri))
                        }

                        GogImportBridge.isAudioFile(name) -> {
                            audioFileUris.add(uri)
                        }

                        isLauncherDxaFilename(name) -> {
                            dxaImportUris.add(name to uri)
                        }

                        ext == "json" -> {
                            if (uris.size == 1) {
                                when (val preparation = ConfigImportExport.prepareFromUri(context, uri)) {
                                    is ConfigImportPreparation.Ready -> {
                                        jsonConfig = name to preparation.config
                                    }

                                    is ConfigImportPreparation.Error -> {
                                        warnings.add("$name: ${preparation.message}")
                                    }
                                }
                            } else {
                                unhandledFiles.add(name)
                            }
                        }

                        else -> {
                            unhandledFiles.add(name)
                        }
                    }
                }
            }
            if (gogDiscUri != null && instDiscUri != null) {
                Log.i(
                    "DXX-Setup",
                    "Routing .gog+.inst pair to disc import: gog=${gogDiscUri.first}, inst=${instDiscUri.first}",
                )
                cueUris.add(instDiscUri)
                binUris.add(gogDiscUri)
            } else {
                gogDiscUri?.let { warnings.add("${it.first} requires a matching .inst file") }
                instDiscUri?.let { warnings.add("${it.first} requires a matching .gog file") }
            }
            if (binUris.isNotEmpty() && cueUris.isEmpty()) {
                for (b in binUris) warnings.add("${b.first} requires a matching CUE file")
            }
            if (cueUris.isNotEmpty() && binUris.isEmpty()) {
                for (c in cueUris) warnings.add("${c.first} requires matching disc image files (.bin/.img)")
            }
            if (isoUris.size > 1) {
                warnings.add("Only one ISO image can be imported at a time")
            }
            if (isoUris.isNotEmpty() && cueUris.isNotEmpty() && binUris.isNotEmpty()) {
                warnings.add("Select either a standalone ISO or a CUE/image set")
            }
            for (f in unhandledFiles) {
                warnings.add("$f: file type not recognized")
            }
            val directGameCollision = ambiguousLogicalImportName(gameUris.map { it.name })
            if (directGameCollision != null) {
                warnings.add("Selected files have colliding game-file output $directGameCollision")
            }
            withContext(Dispatchers.Main) {
                for (w in warnings) {
                    Toast.makeText(context, w, Toast.LENGTH_LONG).show()
                    Log.w("DXX-Setup", "Import warning: $w")
                }
                if (audioFileUris.isNotEmpty()) {
                    audioImportUris = audioFileUris
                }
                if (gameUris.isNotEmpty() && directGameCollision == null) {
                    scanResults = gameUris
                }
                if (cueUris.isNotEmpty() && binUris.isNotEmpty()) {
                    discImportCueName = cueUris.first().first
                    discImportCueUri = cueUris.first().second
                    discImportBins = binUris
                } else if (isoUris.isNotEmpty()) {
                    isoImportName = isoUris.first().first
                    isoImportUri = isoUris.first().second
                }
                gogUri?.let {
                    gogImportName = it.first
                    gogImportUri = it.second
                }
                sowUri?.let {
                    sowImportName = it.first
                    sowImportUri = it.second
                }
                jsonConfig?.let {
                    configImportName = it.first
                    preparedConfigImport = it.second
                }
                scanning = false
            }
            if (dxaImportUris.isNotEmpty()) {
                val modMgr = ModManager.forActiveSet(filesDir)
                val dxaResults = mutableListOf<String>()
                for ((name, uri) in dxaImportUris) {
                    try {
                        val mod = modMgr.importMod(uri, name, context.contentResolver)
                        if (mod != null) {
                            dxaResults.add("Imported mod: ${mod.displayName}")
                            Log.i("DXX-Setup", "DXA import ok: $name (${mod.sizeBytes} bytes)")
                        } else {
                            dxaResults.add("Failed to import $name")
                            Log.e("DXX-Setup", "DXA import returned null: $name")
                        }
                    } catch (e: Exception) {
                        dxaResults.add("Failed to import $name: ${e.message}")
                        Log.e("DXX-Setup", "DXA import exception: $name", e)
                    }
                }
                val failures = dxaResults.filter { it.startsWith("Failed") }
                withContext(Dispatchers.Main) {
                    if (failures.isNotEmpty()) {
                        for (f in failures) {
                            Toast.makeText(context, f, Toast.LENGTH_LONG).show()
                        }
                        importStatus = failures.joinToString("; ")
                    } else {
                        importStatus = dxaResults.joinToString("; ")
                    }
                    if (dxaResults.any { it.startsWith("Imported") }) onContentImported()
                    onRefresh()
                }
            }
            if (missionZipImportUris.isNotEmpty()) {
                val modMgr = ModManager.forActiveSet(filesDir, context)
                val results = mutableListOf<String>()
                withContext(Dispatchers.Main) {
                    missionArchiveImporting = true
                    missionArchiveProgressLabel = "Importing level pack"
                    missionArchiveProgressBytes = 0L
                    missionArchiveProgressTotal = 0L
                }
                for ((name, uri) in missionZipImportUris) {
                    try {
                        withContext(Dispatchers.Main) {
                            missionArchiveProgressLabel = "Importing level pack: $name"
                            missionArchiveProgressBytes = 0L
                            missionArchiveProgressTotal = 0L
                        }
                        val mod =
                            modMgr.importMissionZip(uri, name, context.contentResolver) { progress ->
                                scope.launch(Dispatchers.Main) {
                                    missionArchiveProgressLabel = progress.label
                                    missionArchiveProgressBytes = progress.bytesDone
                                    missionArchiveProgressTotal = progress.bytesTotal
                                }
                            }
                        if (mod != null) {
                            if (mod.importMode == "extracted_bundle") {
                                results.add("Extracted level pack: ${mod.displayName} (cached for faster launches)")
                            } else {
                                results.add("Imported level pack: ${mod.displayName}")
                            }
                            Log.i("DXX-Setup", "Mission ZIP import ok: $name (${mod.sizeBytes} bytes)")
                        } else {
                            results.add("Failed to import $name")
                            Log.e("DXX-Setup", "Mission ZIP import returned null: $name")
                        }
                    } catch (e: Exception) {
                        results.add("Failed to import $name: ${e.message}")
                        Log.e("DXX-Setup", "Mission ZIP import exception: $name", e)
                    }
                }
                val failures = results.filter { it.startsWith("Failed") }
                withContext(Dispatchers.Main) {
                    missionArchiveImporting = false
                    missionArchiveProgressLabel = ""
                    missionArchiveProgressBytes = 0L
                    missionArchiveProgressTotal = 0L
                    if (failures.isNotEmpty()) {
                        for (f in failures) {
                            Toast.makeText(context, f, Toast.LENGTH_LONG).show()
                        }
                        importStatus = failures.joinToString("; ")
                    } else {
                        importStatus = results.joinToString("; ")
                    }
                    if (results.any { it.startsWith("Imported") || it.startsWith("Extracted") }) {
                        onContentImported()
                    }
                    onRefresh()
                }
            }
            if (zipUris.isNotEmpty()) {
                withContext(Dispatchers.Main) {
                    zipExtracting = true
                    zipProgressFile = ""
                    zipProgressBytes = 0L
                    zipProgressTotal = 0L
                }
                val tmpDir = OwnedCacheDirectories.create(File(filesDir, "tmp"))
                val allExtracted = mutableListOf<ExtractedFile>()
                val zipExtractionBudget = ExtractionBudget()
                var anyAudio = false
                val archiveErrors = mutableListOf<String>()
                for ((arcName, arcUri) in zipUris) {
                    val archiveDir = OwnedCacheDirectories.create(tmpDir)
                    val arcLower = portableGameFilenameIdentity(arcName)
                    val result =
                        if (arcLower.endsWith(".7z")) {
                            extract7zContents(context, arcUri, archiveDir) { name, copied, total ->
                                zipProgressFile = "$arcName: $name"
                                zipProgressBytes = copied
                                zipProgressTotal = total
                            }
                        } else if (arcLower.endsWith(".rar")) {
                            extractRarContents(context, arcUri, archiveDir) { name, copied, total ->
                                zipProgressFile = "$arcName: $name"
                                zipProgressBytes = copied
                                zipProgressTotal = total
                            }
                        } else if (arcLower.endsWith(".sit") || arcLower.endsWith(".hqx")) {
                            extractStuffitContents(
                                context,
                                arcUri,
                                archiveDir,
                                archiveName = arcName,
                            ) { name, copied, total ->
                                zipProgressFile = "$arcName: $name"
                                zipProgressBytes = copied
                                zipProgressTotal = total
                            }
                        } else {
                            extractZipContents(
                                context,
                                arcUri,
                                archiveDir,
                                budget = zipExtractionBudget,
                                archiveName = arcName,
                            ) { name, copied, total ->
                                zipProgressFile = "$arcName: $name"
                                zipProgressBytes = copied
                                zipProgressTotal = total
                            }
                        }
                    allExtracted.addAll(result.files)
                    if (result.hadAudioFiles) anyAudio = true
                    if (result.error != null) archiveErrors.add("$arcName: ${result.error}")
                }
                ambiguousLogicalImportName(allExtracted.map { it.name })?.let { collision ->
                    allExtracted.clear()
                    archiveErrors.add("Selected archives have colliding game-file output $collision")
                }
                val fileHashes = allExtracted.associate { it.name to it.sha256 }
                val pkgName = KnownVersions.identifyPackage(fileHashes)
                withContext(Dispatchers.Main) {
                    zipExtracted = allExtracted
                    zipPackageName = pkgName
                    zipHadAudioFiles = anyAudio
                    zipArchiveUris = zipUris.map { it.second }
                    zipExtracting = false
                    zipProgressFile = ""
                    zipProgressBytes = 0L
                    zipProgressTotal = 0L
                    if (archiveErrors.isNotEmpty()) {
                        val msg = archiveErrors.joinToString("\n")
                        importStatus = msg
                        Toast.makeText(context, msg, Toast.LENGTH_LONG).show()
                    }
                }
            }
        } catch (e: Exception) {
            Log.e("DXX-Setup", "File picker processing failed", e)
            withContext(Dispatchers.Main) {
                scanning = false
                zipExtracting = false
                missionArchiveImporting = false
                importStatus = "File processing failed: ${e.message}"
            }
        }
    }

    fun startPickedUriImport(uris: List<Uri>) {
        if (uris.isEmpty()) return
        scanning = true
        importStatus = ""
        missionArchiveImporting = false
        scope.launch(Dispatchers.IO) {
            processPickedUris(uris)
        }
    }

    LaunchedEffect(pickedImportUris) {
        if (pickedImportUris.isNotEmpty()) {
            startPickedUriImport(pickedImportUris)
            onPickedImportConsumed()
        }
    }

    fun startDirectoryImport(treeUri: Uri) {
        scanning = true
        importStatus = ""
        missionArchiveImporting = false
        scope.launch(Dispatchers.IO) {
            try {
                // Folder import is a one-shot flow inside this activity, so the
                // temporary grant from the picker is sufficient.
                val scanResult = scanTreeForImportUris(context, treeUri)
                val largeDirectoryWarning =
                    largeDirectoryImportWarning(
                        scanResult.scannedFileCount,
                        scanResult.skippedUnknownFileCount,
                    )
                if (largeDirectoryWarning != null) {
                    withContext(Dispatchers.Main) {
                        Toast.makeText(context, largeDirectoryWarning, Toast.LENGTH_LONG).show()
                        Log.w("DXX-Setup", "Import warning: $largeDirectoryWarning")
                    }
                }
                if (scanResult.uris.isEmpty()) {
                    withContext(Dispatchers.Main) {
                        scanning = false
                        importStatus = "No importable files found in selected folder"
                        Toast.makeText(context, importStatus, Toast.LENGTH_LONG).show()
                    }
                    return@launch
                }
                processPickedUris(scanResult.uris)
            } catch (e: ImportTreeScanException) {
                Log.w("DXX-Setup", "Directory scan stopped: ${e.message}")
                withContext(Dispatchers.Main) {
                    scanning = false
                    zipExtracting = false
                    missionArchiveImporting = false
                    importStatus = "Folder scan stopped: ${e.message}"
                    Toast.makeText(context, importStatus, Toast.LENGTH_LONG).show()
                }
            } catch (e: kotlinx.coroutines.CancellationException) {
                throw e
            } catch (e: Exception) {
                Log.e("DXX-Setup", "Directory picker processing failed", e)
                withContext(Dispatchers.Main) {
                    scanning = false
                    zipExtracting = false
                    missionArchiveImporting = false
                    importStatus = "File processing failed: ${e.message}"
                }
            }
        }
    }

    val filePickerLauncher =
        rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenMultipleDocuments(),
        ) { uris: List<Uri> ->
            startPickedUriImport(uris)
        }

    val dirPickerLauncher =
        rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocumentTree(),
        ) { treeUri: Uri? ->
            if (treeUri == null) return@rememberLauncherForActivityResult
            startDirectoryImport(treeUri)
        }

    // -- Initial focus for D-pad/keyboard navigation -----
    val initialFocus = remember { FocusRequester() }
    val focusManager = LocalFocusManager.current
    val inputModeManager = LocalInputModeManager.current

    // -- Page navigation state ----------------------------
    var showControllerPage by remember { mutableStateOf(false) }
    var showTouchEditorPage by remember { mutableStateOf(false) }
    var showAdvancedPage by remember { mutableStateOf(false) }
    var showGraphicsPage by remember { mutableStateOf(false) }
    var showEnginePrefsPage by remember { mutableStateOf(false) }
    var showMultiplayerPage by remember { mutableStateOf(false) }
    var showAutoselectPage by remember { mutableStateOf(false) }
    var showMusicPage by remember { mutableStateOf(false) }

    // Re-establish focus when returning from any sub-page
    val anySubPageOpen =
        showControllerPage ||
            showTouchEditorPage ||
            showAdvancedPage ||
            showGraphicsPage ||
            showEnginePrefsPage ||
            showMultiplayerPage ||
            showAutoselectPage ||
            showMusicPage

    fun closeEnginePrefsPage() {
        showDemoInstallerOffer = gamePrefs.getBoolean(PREF_SHOW_DEMO_INSTALLER_OFFER, true)
        showEnginePrefsPage = false
    }
    LaunchedEffect(anySubPageOpen, canLaunch, showResumePanel, focusResumeTrigger, shouldSeedLauncherFocus) {
        if (!anySubPageOpen) {
            if (shouldSeedLauncherFocus) {
                inputModeManager.requestInputMode(InputMode.Keyboard)
                withFrameNanos { }
                withFrameNanos { }
                initialFocus.requestFocus()
                delay(300)
                inputModeManager.requestInputMode(InputMode.Keyboard)
                withFrameNanos { }
                initialFocus.requestFocus()
            } else {
                focusManager.clearFocus(force = true)
            }
        }
    }

    LauncherTheme {
        if (showControllerPage) {
            val activity = LocalContext.current as SetupActivity
            DisposableEffect(Unit) {
                activity.controllerConfigActive = true
                onDispose { activity.controllerConfigActive = false }
            }
            BackHandler { showControllerPage = false }
            ControllerConfigPage(
                axes = controllerAxes,
                dpadAxes = dpadAxes,
                axisGeneration = axisGeneration,
                pressedButtons = pressedButtons,
                gameVariant = selectedGame,
                controllerNavigationActive = controllerNavigationActive,
                onDialogGenericMotionEvent = { view, event -> activity.handleControllerMotion(view, event) },
                onDialogViewChanged = { activity.controllerConfigDialogView = it },
                onPickerOpenChanged = { activity.controllerConfigDialogOpen = it },
                onBack = { showControllerPage = false },
            )
            return@LauncherTheme
        }
        if (showTouchEditorPage) {
            BackHandler { showTouchEditorPage = false }
            TouchEditorPage(
                gameVariant = selectedGame,
                onBack = { showTouchEditorPage = false },
            )
            return@LauncherTheme
        }
        if (showAdvancedPage) {
            BackHandler { showAdvancedPage = false }
            AdvancedSettingsPage(
                filesDir = filesDir,
                fileSetManager = fileSetManager,
                isGameReady = { game -> if (game == "d1") d1RequiredOk else d2RequiredOk },
                refreshTrigger = refreshTrigger,
                controllerFocusActive = shouldSeedLauncherFocus,
                onPlayInputDemo = onPlayInputDemo,
                onClearRouteMetadataCache = onClearRouteMetadataCache,
                onSetRouteMetadataComputeFaster = onSetRouteMetadataComputeFaster,
                onBack = { showAdvancedPage = false },
            )
            return@LauncherTheme
        }
        if (showGraphicsPage) {
            BackHandler { showGraphicsPage = false }
            GraphicsSettingsPage(
                gameVariant = selectedGame,
                filesDir = filesDir,
                controllerFocusActive = shouldSeedLauncherFocus,
                onBack = { showGraphicsPage = false },
            )
            return@LauncherTheme
        }
        if (showEnginePrefsPage) {
            BackHandler { closeEnginePrefsPage() }
            EnginePreferencesPage(
                gameVariant = selectedGame,
                filesDir = filesDir,
                controllerFocusActive = shouldSeedLauncherFocus,
                onBack = { closeEnginePrefsPage() },
            )
            return@LauncherTheme
        }
        if (showMultiplayerPage) {
            BackHandler { showMultiplayerPage = false }
            com.dxxredux.app.multiplayer.MultiplayerScreen(
                onBack = { showMultiplayerPage = false },
                onLaunchGame = onMultiplayerLaunch,
                onLaunchRequested = onMultiplayerLaunchRequested,
            )
            return@LauncherTheme
        }
        if (showAutoselectPage) {
            BackHandler { showAutoselectPage = false }
            AutoselectEditorPage(
                gameVariant = selectedGame,
                filesDir = filesDir.absolutePath,
                onBack = { showAutoselectPage = false },
            )
            return@LauncherTheme
        }
        if (showMusicPage) {
            BackHandler { showMusicPage = false }
            MusicPickerPage(
                filesDir = filesDir,
                controllerFocusActive = shouldSeedLauncherFocus,
                onBack = { showMusicPage = false },
            )
            return@LauncherTheme
        }
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background,
        ) {
            val isLandscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE

            Column(
                modifier =
                    Modifier
                        .fillMaxSize()
                        .safeDrawingPadding()
                        .padding(if (isLandscape) 8.dp else 16.dp),
            ) {
                // -- Title + About ----------------------------
                var showAbout by remember { mutableStateOf(false) }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = "DXX-Redux Setup",
                        fontSize = 22.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.primary,
                    )
                    TextButton(onClick = { showAbout = true }) {
                        Text("About", fontSize = 12.sp)
                    }
                }
                if (!isLandscape) Spacer(modifier = Modifier.height(8.dp))

                UpdateBanner()

                if (showAbout) {
                    val context = LocalContext.current
                    AlertDialog(
                        onDismissRequest = { showAbout = false },
                        confirmButton = {
                            TextButton(onClick = { showAbout = false }) { Text("OK") }
                        },
                        dismissButton = {
                            TextButton(onClick = { openPlayStorePage(context) }) {
                                Text("Store page")
                            }
                        },
                        title = { Text("DXX-Redux") },
                        text = {
                            val arch = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown"
                            val buildLine =
                                formatAboutBuildLine(
                                    buildType = BuildInfo.BUILD_TYPE,
                                    commitCount = BuildInfo.GIT_COMMIT_COUNT,
                                    shortHash = BuildInfo.GIT_SHORT_HASH,
                                    nativeDebugBuild = BuildConfig.NATIVE_DEBUG_BUILD,
                                )
                            Text(
                                "$buildLine\n" +
                                    "Date: ${BuildInfo.BUILD_DATE}" +
                                    " ${BuildInfo.BUILD_TIME}\n" +
                                    "Arch: $arch\n" +
                                    "Renderer: ${BuildConfig.RENDERER}",
                            )
                        },
                    )
                }

                val resumeThumbnail =
                    remember(resumeOfferKey) {
                        resumeCandidate?.let { decodeResumeSaveThumbnail(it) }
                    }
                val resumePanel: (@Composable () -> Unit)? =
                    resumeCandidate?.let { candidate ->
                        {
                            AnimatedVisibility(
                                visible = showResumePanel,
                                enter =
                                    slideInVertically(initialOffsetY = { -it / 2 }) +
                                        expandVertically(expandFrom = Alignment.Top) +
                                        fadeIn(),
                                exit =
                                    slideOutVertically(targetOffsetY = { -it / 2 }) +
                                        shrinkVertically(shrinkTowards = Alignment.Top) +
                                        fadeOut(),
                            ) {
                                ResumeSavePanel(
                                    candidate = candidate,
                                    thumbnail = resumeThumbnail,
                                    onLoad = {
                                        selectedGame = candidate.game
                                        gamePrefs.edit().putString("selected_game", candidate.game).apply()
                                        onLaunchGame(candidate.game, candidate)
                                    },
                                    onOpenSaveExplorer = { showSaveExplorer = true },
                                    onHide = { setSaveExplorerPanelExpanded(false) },
                                )
                            }
                            AnimatedVisibility(
                                visible = showResumeArea && !saveExplorerPanelExpanded,
                                enter =
                                    expandVertically(expandFrom = Alignment.Top) +
                                        fadeIn(),
                                exit =
                                    shrinkVertically(shrinkTowards = Alignment.Top) +
                                        fadeOut(),
                            ) {
                                ResumeSavePanelCollapsed(
                                    onOpen = { setSaveExplorerPanelExpanded(true) },
                                )
                            }
                            if (showResumeArea) {
                                Spacer(modifier = Modifier.height(10.dp))
                            }
                        }
                    }

                if (!isLandscape) {
                    resumePanel?.invoke()
                }

                if (showSaveExplorer) {
                    SaveExplorerDialog(
                        filesDir = filesDir,
                        resumeOptions = availableResumeOptions,
                        refreshTrigger = refreshTrigger,
                        canLaunchGame = { game -> (game == "d1" && d1RequiredOk) || (game != "d1" && d2RequiredOk) },
                        onLoadCandidate = { selectedCandidate ->
                            selectedGame = selectedCandidate.game
                            gamePrefs.edit().putString("selected_game", selectedCandidate.game).apply()
                            showSaveExplorer = false
                            onLaunchGame(selectedCandidate.game, selectedCandidate)
                        },
                        onChanged = onRefresh,
                        onDismiss = { showSaveExplorer = false },
                    )
                }

                // -- File detail popup --
                detailStatus?.let { status ->
                    FileDetailDialog(
                        status = status,
                        setDir = setDir,
                        refreshTrigger = refreshTrigger,
                        onDismiss = { detailStatus = null },
                        onDelete =
                            when {
                                // SAF leave-in-place file - unlink from SAF manifest
                                status.safUri != null -> {
                                    {
                                        safManifest.remove(status.info.filename)
                                        detailStatus = null
                                        onRefresh()
                                    }
                                }

                                // File on disk with manifest entry - delete file + manifest entry
                                status.found && status.manifestEntry != null -> {
                                    {
                                        val entry = status.manifestEntry
                                        val actualName = findFile(setDir, entry.filename)
                                        if (actualName != null) {
                                            File(setDir, actualName).delete()
                                        } else {
                                            File(setDir, entry.filename).delete()
                                        }
                                        manifest.remove(entry.filename)
                                        detailStatus = null
                                        onRefresh()
                                    }
                                }

                                // External import but missing from disk - forget the manifest entry
                                status.manifestEntry?.isExternal == true -> {
                                    {
                                        manifest.remove(status.manifestEntry.filename)
                                        detailStatus = null
                                        onRefresh()
                                    }
                                }

                                else -> {
                                    null
                                }
                            },
                    )
                }

                // -- Set management dialog --
                if (showSetDialog) {
                    SetManagementDialog(
                        fileSetManager = fileSetManager,
                        activeSetName = activeSetName,
                        onSwitchSet = { newSet ->
                            fileSetManager.setActive(newSet)
                            activeSetName = newSet
                            showSetDialog = false
                            onRefresh()
                        },
                        onDismiss = { showSetDialog = false },
                    )
                }

                // -- BIN/CUE disc import dialog --
                if (discImportCueUri != null) {
                    DiscImportDialog(
                        cueName = discImportCueName ?: "unknown.cue",
                        cueUri = discImportCueUri!!,
                        binUris = discImportBins,
                        filesDir = filesDir,
                        setDir = setDir,
                        context = context,
                        onChanged = onRefresh,
                        onImported = {
                            discImportCueUri = null
                            discImportCueName = null
                            discImportBins = emptyList()
                            onContentImported()
                            onRefresh()
                        },
                        onDismiss = {
                            discImportCueUri = null
                            discImportCueName = null
                            discImportBins = emptyList()
                            onRefresh()
                        },
                    )
                }

                // -- ISO disc import dialog --
                if (isoImportUri != null) {
                    IsoImportDialog(
                        isoName = isoImportName ?: "unknown.iso",
                        isoUri = isoImportUri!!,
                        setDir = setDir,
                        context = context,
                        onChanged = onRefresh,
                        onImported = {
                            isoImportUri = null
                            isoImportName = null
                            onContentImported()
                            onRefresh()
                        },
                        onDismiss = {
                            isoImportUri = null
                            isoImportName = null
                            onRefresh()
                        },
                    )
                }

                // -- GOG installer import dialog --
                if (gogImportUri != null) {
                    GogImportDialog(
                        installerName = gogImportName ?: "installer",
                        installerUri = gogImportUri!!,
                        filesDir = filesDir,
                        setDir = setDir,
                        context = context,
                        onImported = {
                            gogImportUri = null
                            gogImportName = null
                            onContentImported()
                            onRefresh()
                        },
                        onDismiss = {
                            gogImportUri = null
                            gogImportName = null
                            onRefresh()
                        },
                    )
                }

                // -- SOW archive import dialog --
                if (sowImportUri != null) {
                    SowImportDialog(
                        sowName = sowImportName ?: "archive.sow",
                        sowUri = sowImportUri!!,
                        filesDir = filesDir,
                        setDir = setDir,
                        context = context,
                        onImported = {
                            sowImportUri = null
                            sowImportName = null
                            onContentImported()
                            onRefresh()
                        },
                        onDismiss = {
                            sowImportUri = null
                            sowImportName = null
                        },
                    )
                }

                // -- Config JSON import dialog --
                if (preparedConfigImport != null) {
                    val configImportFocus = remember { FocusRequester() }
                    LaunchedEffect(preparedConfigImport) {
                        if (preparedConfigImport != null) configImportFocus.requestFocus()
                    }
                    AlertDialog(
                        onDismissRequest = {
                            preparedConfigImport = null
                            configImportName = null
                        },
                        title = { Text("Import Game Config?") },
                        text = {
                            Text(
                                "Import settings from ${configImportName ?: "config file"}? This will overwrite your current touch layout, controller config, and/or weapon ordering",
                            )
                        },
                        confirmButton = {
                            TextButton(
                                onClick = {
                                    val prepared = preparedConfigImport!!
                                    preparedConfigImport = null
                                    configImportName = null
                                    scope.launch {
                                        val result = ConfigImportExport.importPrepared(context, prepared)
                                        Toast.makeText(context, result, Toast.LENGTH_LONG).show()
                                    }
                                },
                                modifier = Modifier.focusRequester(configImportFocus),
                            ) { Text("Import") }
                        },
                        dismissButton = {
                            TextButton(onClick = {
                                preparedConfigImport = null
                                configImportName = null
                            }) { Text("Cancel") }
                        },
                    )
                }

                // -- Audio file auto-import dialog --
                if (audioImportUris.isNotEmpty()) {
                    AddToSetDialog(
                        existingSets = audioCustomMgr.getSets(),
                        defaultName = "Set ${audioCustomMgr.getSets().size + 1}",
                        selectedUris = audioImportUris,
                        onDismiss = { audioImportUris = emptyList() },
                        onConfirm = { targetSetId, newName, copyToStorage ->
                            val uris = audioImportUris
                            audioImportUris = emptyList()
                            audioImporting = true
                            audioImportLabel = ""
                            audioImportBytes = 0L
                            audioImportTotal = 0L
                            scope.launch {
                                val imported =
                                    importAudioFiles(
                                        context,
                                        filesDir,
                                        audioCustomMgr,
                                        newName,
                                        uris,
                                        targetSetId,
                                        copyToStorage,
                                    ) { label, copied, total ->
                                        mainHandler.post {
                                            audioImportLabel = label
                                            audioImportBytes = copied
                                            audioImportTotal = total
                                        }
                                    }
                                audioImporting = false
                                audioImportLabel = ""
                                audioImportBytes = 0L
                                audioImportTotal = 0L
                                if (imported) {
                                    context
                                        .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
                                        .edit()
                                        .putString("music_mode", "files")
                                        .apply()
                                }
                            }
                        },
                    )
                }

                // -- Shared composable blocks --

                val filesPane: @Composable ColumnScope.() -> Unit = {
                    // -- Stale temp cleanup notification --------
                    if (cleanedTmpFiles.isNotEmpty()) {
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .padding(bottom = 8.dp)
                                    .background(
                                        Color(0xFFFFF3E0),
                                        shape = RoundedCornerShape(6.dp),
                                    ).padding(horizontal = 10.dp, vertical = 8.dp),
                            verticalAlignment = Alignment.Top,
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    "Cleaned up stale temporary import files:",
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = Color(0xFF6D4C00),
                                )
                                cleanedTmpFiles.take(8).forEach { name ->
                                    Text(
                                        "  - $name",
                                        fontSize = 11.sp,
                                        color = Color(0xFF6D4C00),
                                    )
                                }
                                if (cleanedTmpFiles.size > 8) {
                                    Text(
                                        "  - and ${cleanedTmpFiles.size - 8} more",
                                        fontSize = 11.sp,
                                        color = Color(0xFF6D4C00),
                                    )
                                }
                            }
                            TextButton(
                                onClick = { cleanedTmpFiles = emptyList() },
                                contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                                modifier = Modifier.height(24.dp),
                            ) {
                                Text("x", fontSize = 12.sp, color = Color(0xFF6D4C00))
                            }
                        }
                    }

                    if (audioImporting) {
                        Card(
                            modifier = Modifier.fillMaxWidth().padding(bottom = 8.dp),
                            colors =
                                CardDefaults.cardColors(
                                    containerColor = MaterialTheme.colorScheme.secondaryContainer,
                                ),
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Text(
                                    "Importing audio files...",
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = MaterialTheme.colorScheme.onSecondaryContainer,
                                )
                                if (audioImportLabel.isNotEmpty()) {
                                    Text(
                                        audioImportLabel,
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                }
                                Spacer(modifier = Modifier.height(4.dp))
                                if (audioImportTotal > 0L) {
                                    val audioPct =
                                        (audioImportBytes.toFloat() / audioImportTotal.toFloat()).coerceIn(0f, 1f)
                                    LinearProgressIndicator(
                                        progress = { audioPct },
                                        modifier = Modifier.fillMaxWidth().height(4.dp),
                                    )
                                } else {
                                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth().height(4.dp))
                                }
                            }
                        }
                    }

                    // -- Pruned audio sources notification --------
                    if (prunedSourceNames.isNotEmpty()) {
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .padding(bottom = 8.dp)
                                    .background(
                                        Color(0xFFFFF3E0),
                                        shape = RoundedCornerShape(6.dp),
                                    ).padding(horizontal = 10.dp, vertical = 8.dp),
                            verticalAlignment = Alignment.Top,
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    "Removed stale audio sources (files no longer present):",
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = Color(0xFF6D4C00),
                                )
                                prunedSourceNames.forEach { name ->
                                    Text(
                                        "  - $name",
                                        fontSize = 11.sp,
                                        color = Color(0xFF6D4C00),
                                    )
                                }
                            }
                            TextButton(
                                onClick = { prunedSourceNames = emptyList() },
                                contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                                modifier = Modifier.height(24.dp),
                            ) {
                                Text("\u2717", fontSize = 12.sp, color = Color(0xFF6D4C00))
                            }
                        }
                    }

                    // -- Pruned game data notification -----------
                    if (prunedDataFiles.isNotEmpty()) {
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .padding(bottom = 8.dp)
                                    .background(
                                        Color(0xFFFFF3E0),
                                        shape = RoundedCornerShape(6.dp),
                                    ).padding(horizontal = 10.dp, vertical = 8.dp),
                            verticalAlignment = Alignment.Top,
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    "Cleaned up stale file references:",
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = Color(0xFF6D4C00),
                                )
                                prunedDataFiles.forEach { name ->
                                    Text(
                                        "  - $name",
                                        fontSize = 11.sp,
                                        color = Color(0xFF6D4C00),
                                    )
                                }
                            }
                            TextButton(
                                onClick = { prunedDataFiles = emptyList() },
                                contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                                modifier = Modifier.height(24.dp),
                            ) {
                                Text("\u2717", fontSize = 12.sp, color = Color(0xFF6D4C00))
                            }
                        }
                    }

                    // -- Active set indicator ----------------------
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .padding(bottom = 4.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(
                            text = "Files in use: ",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Text(
                            text = activeSetName,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.Bold,
                            color = MaterialTheme.colorScheme.onSurface,
                        )
                        Spacer(modifier = Modifier.weight(1f))
                        TextButton(
                            onClick = { showSetDialog = true },
                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                            modifier = Modifier.height(28.dp),
                        ) {
                            Text("Change", fontSize = 12.sp)
                        }
                    }
                    // -- Missing-files help ----------------------
                    if (!canLaunch && !gameRunning) {
                        MissingFilesHelp()
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // -- Demo download offers ------------------
                    if (!gameRunning) {
                        for (demo in demoInstallerOffers) {
                            Card(
                                modifier = Modifier.fillMaxWidth(),
                                colors =
                                    CardDefaults.cardColors(
                                        containerColor = MaterialTheme.colorScheme.secondaryContainer,
                                    ),
                            ) {
                                Column(modifier = Modifier.padding(12.dp)) {
                                    Text(
                                        text = demo.name,
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                    Text(
                                        text = "${demo.description} (${formatSize(demo.sizeBytes)})",
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                    Spacer(modifier = Modifier.height(6.dp))
                                    if (demoDownloading == demo.name) {
                                        Text(
                                            text = "Downloading and installing... $demoDownloadProgress%",
                                            fontSize = 12.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer,
                                        )
                                        LinearProgressIndicator(
                                            progress = { demoDownloadProgress / 100f },
                                            modifier = Modifier.fillMaxWidth().height(4.dp),
                                            color = MaterialTheme.colorScheme.primary,
                                            trackColor = MaterialTheme.colorScheme.primaryContainer,
                                        )
                                    } else {
                                        if (demoDownloadErrorName == demo.name && demoDownloadError != null) {
                                            Text(
                                                text = "Error: $demoDownloadError",
                                                fontSize = 12.sp,
                                                color = MaterialTheme.colorScheme.error,
                                            )
                                            Spacer(modifier = Modifier.height(4.dp))
                                        }
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                                        ) {
                                            Button(
                                                onClick = {
                                                    demoDownloadError = null
                                                    demoDownloadErrorName = null
                                                    demoDownloading = demo.name
                                                    demoDownloadProgress = 0
                                                    scope.launch {
                                                        val tmpDir = File(filesDir, "tmp")
                                                        tmpDir.mkdirs()
                                                        val archiveFile = File(tmpDir, demo.downloadFilename)
                                                        var downloadOk = false
                                                        setupDownloadFile(
                                                            url = demo.url,
                                                            destDir = tmpDir,
                                                            filename = archiveFile.name,
                                                            onProgress = { pct -> demoDownloadProgress = pct },
                                                            onDone = { success -> downloadOk = success },
                                                        )
                                                        if (!downloadOk) {
                                                            demoDownloading = null
                                                            demoDownloadErrorName = demo.name
                                                            demoDownloadError = "Download failed"
                                                            cleanupTmpDir(filesDir)
                                                            return@launch
                                                        }
                                                        val archiveUri = android.net.Uri.fromFile(archiveFile)
                                                        val result =
                                                            extractStuffitContents(
                                                                context,
                                                                archiveUri,
                                                                tmpDir,
                                                                archiveName = demo.archiveName,
                                                            ) { _, _, _ -> }
                                                        if (result.files.isEmpty()) {
                                                            demoDownloading = null
                                                            demoDownloadErrorName = demo.name
                                                            demoDownloadError =
                                                                result.error ?: "No game files found in installer"
                                                            cleanupTmpDir(filesDir)
                                                            return@launch
                                                        }
                                                        var imported = 0
                                                        hashingTotalFiles = result.files.size
                                                        for ((i, ef) in result.files.withIndex()) {
                                                            hashingFileIndex = i + 1
                                                            hashingFile = ef.name
                                                            hashingProgress = 0f
                                                            val destFile = File(setDir, ef.name)
                                                            val ok =
                                                                withContext(Dispatchers.IO) {
                                                                    try {
                                                                        ImportStorageGuard.requireFreeSpace(
                                                                            setDir,
                                                                            ef.sizeBytes,
                                                                            "install ${ef.name}",
                                                                        )
                                                                        LauncherFileCopy.copyFileToFile(
                                                                            ef.tmpFile,
                                                                            destFile,
                                                                            ef.name,
                                                                        ) { progress ->
                                                                            mainHandler.post {
                                                                                hashingProgress = progress.fraction
                                                                            }
                                                                        }
                                                                        true
                                                                    } catch (e: Exception) {
                                                                        Log.e(
                                                                            "DXX-Setup",
                                                                            "Failed to move demo file ${ef.name}",
                                                                            e,
                                                                        )
                                                                        false
                                                                    }
                                                                }
                                                            if (ok) {
                                                                imported++
                                                                manifest.upsert(ef.name, ef.sha256, ef.sizeBytes)
                                                            }
                                                        }
                                                        hashingFile = null
                                                        cleanupTmpDir(filesDir)
                                                        selectedGame = demo.game
                                                        gamePrefs
                                                            .edit()
                                                            .putString(
                                                                "selected_game",
                                                                demo.game,
                                                            ).apply()
                                                        demoDownloading = null
                                                        importStatus = "Installed ${demo.name}: $imported files"
                                                        onRefresh()
                                                    }
                                                },
                                                enabled = demoDownloading == null,
                                                modifier = Modifier.weight(1f),
                                            ) {
                                                Text("Download and install", fontSize = 12.sp)
                                            }
                                            OutlinedButton(
                                                onClick = {
                                                    gamePrefs
                                                        .edit()
                                                        .putBoolean(PREF_SHOW_DEMO_INSTALLER_OFFER, false)
                                                        .apply()
                                                    showDemoInstallerOffer = false
                                                },
                                                enabled = demoDownloading == null,
                                                modifier = Modifier.weight(1f),
                                            ) {
                                                Text("Stop showing this", fontSize = 12.sp)
                                            }
                                        }
                                    }
                                }
                            }
                            Spacer(modifier = Modifier.height(8.dp))
                        }
                    }

                    // -- Hashing progress bar --
                    if (isHashing) {
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors =
                                CardDefaults.cardColors(
                                    containerColor = MaterialTheme.colorScheme.primaryContainer,
                                ),
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Text(
                                    text =
                                        "Hashing: $hashingFile " +
                                            "($hashingFileIndex/$hashingTotalFiles)",
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = MaterialTheme.colorScheme.onPrimaryContainer,
                                )
                                Spacer(modifier = Modifier.height(6.dp))
                                // Per-file progress
                                LinearProgressIndicator(
                                    progress = { hashingProgress },
                                    modifier = Modifier.fillMaxWidth().height(8.dp),
                                    color = MaterialTheme.colorScheme.primary,
                                    trackColor = MaterialTheme.colorScheme.primaryContainer,
                                )
                                // Overall progress across all files
                                if (hashingTotalFiles > 1) {
                                    Spacer(modifier = Modifier.height(4.dp))
                                    val overallProgress =
                                        ((hashingFileIndex - 1).toFloat() + hashingProgress) / hashingTotalFiles
                                    LinearProgressIndicator(
                                        progress = { overallProgress },
                                        modifier = Modifier.fillMaxWidth().height(4.dp),
                                        color = MaterialTheme.colorScheme.tertiary,
                                        trackColor = MaterialTheme.colorScheme.primaryContainer,
                                    )
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // -- Import files button --
                    if (showImportChooser) {
                        val importChoiceFocus = remember(androidTvDevice) { FocusRequester() }
                        LaunchedEffect(showImportChooser, androidTvDevice) {
                            if (showImportChooser) {
                                importChoiceFocus.requestFocus()
                            }
                        }
                        AlertDialog(
                            onDismissRequest = { showImportChooser = false },
                            confirmButton = {},
                            title = { Text("Import Files") },
                            text = {
                                Column(
                                    verticalArrangement = Arrangement.spacedBy(8.dp),
                                ) {
                                    Text(
                                        importChooserConfig.helpText,
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                    Button(
                                        onClick = {
                                            showImportChooser = false
                                            filePickerLauncher.launch(
                                                arrayOf("application/octet-stream", "application/zip", "*/*"),
                                            )
                                        },
                                        modifier = Modifier.fillMaxWidth().focusRequester(importChoiceFocus),
                                    ) {
                                        Text(importChooserConfig.directPickLabel)
                                    }
                                    OutlinedButton(
                                        onClick = {
                                            showImportChooser = false
                                            dirPickerLauncher.launch(null)
                                        },
                                        modifier = Modifier.fillMaxWidth(),
                                    ) {
                                        Text("Pick Folder")
                                    }
                                    TextButton(
                                        onClick = { showImportChooser = false },
                                        modifier = Modifier.fillMaxWidth(),
                                    ) {
                                        Text("Cancel")
                                    }
                                }
                            },
                        )
                    }
                    Button(
                        onClick = {
                            showImportChooser = true
                        },
                        enabled = !scanning && !isHashing && !zipExtracting && !missionArchiveImporting,
                        modifier = Modifier.fillMaxWidth().height(44.dp),
                        colors =
                            ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.secondary,
                            ),
                    ) {
                        Text(
                            text =
                                if (scanning || zipExtracting || missionArchiveImporting) {
                                    "Importing\u2026"
                                } else {
                                    "\uD83D\uDCC2 Select Game Files or Archive to Import"
                                },
                            fontSize = 11.sp,
                        )
                    }
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        text =
                            "${importChooserConfig.helpText}. Supports .hog, .ham, .pig files, " +
                                ".zip/.7z/.rar archives, .cue disc images with .bin/.img tracks, " +
                                ".sow archives, and GOG installers.",
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(modifier = Modifier.height(8.dp))

                    // -- Scan results / import card --------------
                    if (scanResults != null) {
                        val found = scanResults!!
                        val importAllFocus = remember(found) { FocusRequester() }
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors =
                                CardDefaults.cardColors(
                                    containerColor =
                                        if (found.isEmpty()) {
                                            MaterialTheme.colorScheme.errorContainer
                                        } else {
                                            MaterialTheme.colorScheme.secondaryContainer
                                        },
                                ),
                        ) {
                            Column(
                                modifier =
                                    Modifier
                                        .padding(12.dp),
                            ) {
                                if (found.isEmpty()) {
                                    Text(
                                        text = "No game files found in that folder.",
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onErrorContainer,
                                    )
                                    Text(
                                        text = "Try selecting the folder that contains .hog, .ham, and .pig files.",
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onErrorContainer,
                                    )
                                } else {
                                    LaunchedEffect(found) {
                                        importAllFocus.requestFocus()
                                    }
                                    Text(
                                        text = "Found ${found.size} game file(s): ${found.joinToString(
                                            ", ",
                                        ) { it.name }}",
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                    Spacer(modifier = Modifier.height(8.dp))
                                    Row(
                                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                                    ) {
                                        Button(
                                            onClick = {
                                                launchResultImport {
                                                    try {
                                                        var imported = 0
                                                        hashingTotalFiles = found.size
                                                        for ((i, f) in found.withIndex()) {
                                                            hashingFileIndex = i + 1
                                                            hashingFile = f.name
                                                            hashingProgress = 0f
                                                            val canonicalName = f.name.lowercase(Locale.ROOT)
                                                            val destFile =
                                                                if (canonicalName.endsWith(".dem")) {
                                                                    File(File(setDir, "demos"), canonicalName)
                                                                } else {
                                                                    File(setDir, canonicalName)
                                                                }
                                                            // Determine track: native data-dir vs external
                                                            val priorState =
                                                                withContext(Dispatchers.IO) {
                                                                    destFile.exists() to
                                                                        manifest.getEntry(canonicalName)
                                                                }
                                                            val existedBefore = priorState.first
                                                            val existingEntry = priorState.second
                                                            val ok =
                                                                withContext(Dispatchers.IO) {
                                                                    importFile(context, f, setDir) { progress ->
                                                                        mainHandler.post {
                                                                            hashingProgress = progress.fraction
                                                                        }
                                                                    }
                                                                }
                                                            if (ok) {
                                                                imported++
                                                                val sha256 =
                                                                    withContext(Dispatchers.IO) {
                                                                        AssetManifest.computeSha256(
                                                                            destFile,
                                                                        ) { bytesRead, totalBytes ->
                                                                            if (totalBytes > 0) {
                                                                                mainHandler.post {
                                                                                    hashingProgress =
                                                                                        bytesRead.toFloat() /
                                                                                        totalBytes
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                if (sha256 != null) {
                                                                    // Data-dir track: file existed on disk without a sourceUri
                                                                    val sourceUri =
                                                                        if (existedBefore &&
                                                                            (
                                                                                existingEntry == null ||
                                                                                    !existingEntry.isExternal
                                                                            )
                                                                        ) {
                                                                            null
                                                                        } else {
                                                                            f.uri.toString()
                                                                        }
                                                                    withContext(Dispatchers.IO) {
                                                                        manifest.upsert(
                                                                            destFile.name,
                                                                            sha256,
                                                                            destFile.length(),
                                                                            sourceUri,
                                                                        )
                                                                    }
                                                                } else {
                                                                    Log.w(
                                                                        "DXX-Setup",
                                                                        "Import completed but hashing failed for ${destFile.name}",
                                                                    )
                                                                }
                                                            }
                                                        }
                                                        hashingFile = null
                                                        importStatus =
                                                            "Imported $imported of ${found.size} files."
                                                        scanResults = null
                                                        if (imported > 0) onContentImported()
                                                        onRefresh()
                                                    } catch (e: Exception) {
                                                        Log.e("DXX-Setup", "Import failed", e)
                                                        hashingFile = null
                                                        importStatus = "Import failed: ${e.message}"
                                                        scanResults = null
                                                    }
                                                }
                                            },
                                            modifier = Modifier.focusRequester(importAllFocus),
                                            enabled = !resultImporting,
                                        ) {
                                            Text("Import All", fontSize = 13.sp)
                                        }
                                        OutlinedButton(
                                            onClick = { scanResults = null },
                                            enabled = !resultImporting,
                                        ) {
                                            Text("Dismiss", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    if (missionArchiveImporting) {
                        Column(modifier = Modifier.padding(bottom = 8.dp)) {
                            val extractingCache =
                                missionArchiveProgressLabel.contains("faster launches", ignoreCase = true) ||
                                    missionArchiveProgressLabel.contains("cache", ignoreCase = true)
                            Text(
                                text = missionArchiveProgressLabel.ifBlank { "Importing level pack" },
                                fontSize = 13.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = MaterialTheme.colorScheme.primary,
                            )
                            if (extractingCache) {
                                Text(
                                    text = "Unpacking into app storage so future launches do not reprocess the archive",
                                    fontSize = 12.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            if (missionArchiveProgressTotal > 0L) {
                                Text(
                                    text =
                                        formatMissionArchiveProgressAmount(
                                            missionArchiveProgressLabel,
                                            missionArchiveProgressBytes,
                                            missionArchiveProgressTotal,
                                        ),
                                    fontSize = 12.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            Spacer(modifier = Modifier.height(4.dp))
                            if (missionArchiveProgressTotal > 0L) {
                                val archivePct =
                                    (
                                        missionArchiveProgressBytes.toFloat() /
                                            missionArchiveProgressTotal.toFloat()
                                    ).coerceIn(0f, 1f)
                                LinearProgressIndicator(
                                    progress = { archivePct },
                                    modifier = Modifier.fillMaxWidth().height(4.dp),
                                    color = MaterialTheme.colorScheme.primary,
                                    trackColor = MaterialTheme.colorScheme.primaryContainer,
                                )
                            } else {
                                LinearProgressIndicator(
                                    modifier = Modifier.fillMaxWidth().height(4.dp),
                                    color = MaterialTheme.colorScheme.primary,
                                    trackColor = MaterialTheme.colorScheme.primaryContainer,
                                )
                            }
                        }
                    } else if (importStatus.isNotEmpty()) {
                        val cachedLevelPack = importStatus.contains("cached for faster launches", ignoreCase = true)
                        if (cachedLevelPack) {
                            Surface(
                                modifier =
                                    Modifier
                                        .fillMaxWidth()
                                        .padding(bottom = 8.dp),
                                shape = MaterialTheme.shapes.small,
                                color = MaterialTheme.colorScheme.secondaryContainer,
                            ) {
                                Column(modifier = Modifier.padding(10.dp)) {
                                    Text(
                                        text = importStatus,
                                        fontSize = 13.sp,
                                        fontWeight = FontWeight.SemiBold,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                    Text(
                                        text = "Future launches will use the extracted files automatically",
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                }
                            }
                        } else {
                            Text(
                                text = importStatus,
                                fontSize = 13.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = Color(0xFF4CAF50),
                                modifier = Modifier.padding(bottom = 8.dp),
                            )
                        }
                    }

                    // -- ZIP extraction progress -----------------
                    if (zipExtracting) {
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors =
                                CardDefaults.cardColors(
                                    containerColor = MaterialTheme.colorScheme.secondaryContainer,
                                ),
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Text(
                                    text = "Extracting archive\u2026",
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 14.sp,
                                    color = MaterialTheme.colorScheme.onSecondaryContainer,
                                )
                                if (zipProgressFile.isNotEmpty()) {
                                    Text(
                                        text = zipProgressFile,
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                }
                                Spacer(modifier = Modifier.height(4.dp))
                                if (zipProgressTotal > 0L) {
                                    val archivePct =
                                        (zipProgressBytes.toFloat() / zipProgressTotal.toFloat()).coerceIn(0f, 1f)
                                    Text(
                                        "${(archivePct * 100f).toInt()}%",
                                        fontSize = 11.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                    LinearProgressIndicator(
                                        progress = { archivePct },
                                        modifier = Modifier.fillMaxWidth().height(4.dp),
                                        color = MaterialTheme.colorScheme.primary,
                                        trackColor = MaterialTheme.colorScheme.primaryContainer,
                                    )
                                } else {
                                    LinearProgressIndicator(
                                        modifier = Modifier.fillMaxWidth().height(4.dp),
                                        color = MaterialTheme.colorScheme.primary,
                                        trackColor = MaterialTheme.colorScheme.primaryContainer,
                                    )
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // -- ZIP results card -----------------------
                    if (zipExtracted != null) {
                        val extracted = zipExtracted!!
                        val zipImportFocus = remember(extracted) { FocusRequester() }
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors =
                                CardDefaults.cardColors(
                                    containerColor =
                                        if (extracted.isEmpty()) {
                                            MaterialTheme.colorScheme.errorContainer
                                        } else {
                                            MaterialTheme.colorScheme.secondaryContainer
                                        },
                                ),
                        ) {
                            Column(
                                modifier =
                                    Modifier
                                        .padding(12.dp),
                            ) {
                                if (extracted.isEmpty()) {
                                    if (zipHadAudioFiles) {
                                        // Auto-trigger music import dialog for audio-only archives
                                        LaunchedEffect(Unit) {
                                            audioImportUris = zipArchiveUris
                                            zipExtracted = null
                                            zipPackageName = null
                                            zipHadAudioFiles = false
                                            zipArchiveUris = emptyList()
                                            cleanupTmpDir(filesDir)
                                        }
                                    } else {
                                        Text(
                                            text = "No game files found in archive",
                                            fontWeight = FontWeight.Bold,
                                            fontSize = 14.sp,
                                            color = MaterialTheme.colorScheme.onErrorContainer,
                                        )
                                        Spacer(modifier = Modifier.height(4.dp))
                                        OutlinedButton(
                                            onClick = {
                                                zipExtracted = null
                                                zipPackageName = null
                                                zipHadAudioFiles = false
                                                cleanupTmpDir(filesDir)
                                            },
                                        ) {
                                            Text("Dismiss", fontSize = 13.sp)
                                        }
                                    }
                                } else {
                                    LaunchedEffect(extracted) {
                                        zipImportFocus.requestFocus()
                                    }
                                    if (zipPackageName != null) {
                                        Text(
                                            text = "\u2705 Recognized: $zipPackageName",
                                            fontWeight = FontWeight.Bold,
                                            fontSize = 14.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer,
                                        )
                                    } else {
                                        Text(
                                            text = "Found ${extracted.size} game file(s)",
                                            fontWeight = FontWeight.Bold,
                                            fontSize = 14.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer,
                                        )
                                    }
                                    Spacer(modifier = Modifier.height(4.dp))
                                    for (ef in extracted) {
                                        Text(
                                            text = "\u2022 ${ef.name} (${formatBinarySize(ef.sizeBytes)})",
                                            fontSize = 12.sp,
                                            color = MaterialTheme.colorScheme.onSecondaryContainer,
                                        )
                                    }
                                    Spacer(modifier = Modifier.height(8.dp))
                                    Row(
                                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                                    ) {
                                        Button(
                                            onClick = {
                                                launchResultImport {
                                                    var imported = 0
                                                    hashingTotalFiles = extracted.size
                                                    for ((i, ef) in extracted.withIndex()) {
                                                        hashingFileIndex = i + 1
                                                        hashingFile = ef.name
                                                        hashingProgress = 0f
                                                        val destFile = File(setDir, ef.name)
                                                        val ok =
                                                            withContext(Dispatchers.IO) {
                                                                try {
                                                                    LauncherFileCopy.copyFileToFile(
                                                                        ef.tmpFile,
                                                                        destFile,
                                                                        ef.name,
                                                                    ) { progress ->
                                                                        mainHandler.post {
                                                                            hashingProgress = progress.fraction
                                                                        }
                                                                    }
                                                                    true
                                                                } catch (e: Exception) {
                                                                    Log.e(
                                                                        "DXX-Setup",
                                                                        "Failed to move extracted file ${ef.name}",
                                                                        e,
                                                                    )
                                                                    false
                                                                }
                                                            }
                                                        if (ok) {
                                                            imported++
                                                            manifest.upsert(ef.name, ef.sha256, ef.sizeBytes)
                                                        }
                                                    }
                                                    hashingFile = null
                                                    importStatus =
                                                        "Imported $imported of ${extracted.size} files from archive."
                                                    zipExtracted = null
                                                    zipPackageName = null
                                                    cleanupTmpDir(filesDir)
                                                    if (imported > 0) onContentImported()
                                                    onRefresh()
                                                }
                                            },
                                            modifier = Modifier.focusRequester(zipImportFocus),
                                            enabled = !resultImporting,
                                        ) {
                                            Text("Import to Current Set", fontSize = 13.sp)
                                        }
                                        OutlinedButton(
                                            onClick = {
                                                zipExtracted = null
                                                zipPackageName = null
                                                cleanupTmpDir(filesDir)
                                            },
                                            enabled = !resultImporting,
                                        ) {
                                            Text("Dismiss", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // -- File sections --------------------
                    var d2Expanded by remember { mutableStateOf(false) }
                    var d1Expanded by remember { mutableStateOf(false) }

                    GameSectionHeader(
                        title = "Descent 2",
                        ready = d2RequiredOk,
                        expanded = d2Expanded,
                        onToggle = { d2Expanded = !d2Expanded },
                    )

                    if (d2Expanded) {
                        SectionHeader("Required Files")
                        d2Statuses.filter { it.info.required }.forEach {
                            FileStatusRow(it) {
                                detailStatus = it
                                detailIsD2 = true
                            }
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        SectionHeader("Optional Files")
                        d2Statuses.filter { !it.info.required }.forEach {
                            FileStatusRow(it) {
                                detailStatus = it
                                detailIsD2 = true
                            }
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))

                    GameSectionHeader(
                        title = "Descent 1",
                        ready = d1RequiredOk,
                        expanded = d1Expanded,
                        onToggle = { d1Expanded = !d1Expanded },
                    )

                    if (d1Expanded) {
                        SectionHeader("Required Files")
                        d1Statuses.filter { it.info.required }.forEach {
                            FileStatusRow(it) {
                                detailStatus = it
                                detailIsD2 = false
                            }
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        SectionHeader("Optional Files")
                        d1Statuses.filter { !it.info.required }.forEach { status ->
                            if (!status.found && status.info.downloadUrl != null) {
                                DownloadableFileRow(
                                    status = status,
                                    progress = downloadProgress[status.info.filename],
                                    onInfo = {
                                        detailStatus = status
                                        detailIsD2 = false
                                    },
                                    onDownload = {
                                        scope.launch {
                                            setupDownloadFile(
                                                url = status.info.downloadUrl,
                                                destDir = setDir,
                                                filename = status.info.filename,
                                                onProgress = { pct ->
                                                    downloadProgress[status.info.filename] = pct
                                                    onDownloadStateChanged(status.info.filename, pct)
                                                },
                                                onDone = { success ->
                                                    val code = if (success) -2 else -1
                                                    downloadProgress[status.info.filename] = code
                                                    onDownloadStateChanged(status.info.filename, code)
                                                    if (success) {
                                                        onRefresh()
                                                    } else {
                                                        Toast
                                                            .makeText(
                                                                context,
                                                                "Download failed: ${status.info.filename}",
                                                                Toast.LENGTH_LONG,
                                                            ).show()
                                                    }
                                                },
                                            )
                                        }
                                    },
                                )
                            } else {
                                FileStatusRow(status) {
                                    detailStatus = status
                                    detailIsD2 = false
                                }
                            }
                        }
                    } // end if (d1Expanded)

                    Spacer(modifier = Modifier.height(16.dp))
                    MusicInfoSection(
                        filesDir = filesDir,
                        setDir = setDir,
                        refreshTrigger = refreshTrigger,
                        hasMidiSource =
                            d2RequiredOk || d1RequiredOk,
                        onEditMusic = { showMusicPage = true },
                    )

                    Spacer(modifier = Modifier.height(16.dp))
                    ModsSection(
                        filesDir = filesDir,
                        setDir = setDir,
                        refreshTrigger = refreshTrigger,
                    )
                }

                val controlsPane: @Composable ColumnScope.() -> Unit = {
                    val prefs = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
                    ControllerSection(
                        axes = controllerAxes,
                        dpadAxes = dpadAxes,
                        axisGeneration = axisGeneration,
                        pressedButtons = pressedButtons,
                        prefs = prefs,
                        selectedGame = selectedGame,
                        initialFocusRequester = if (shouldSeedLauncherFocus) initialFocus else null,
                        onDefineControls = { showControllerPage = true },
                        onEditTouchLayout = { showTouchEditorPage = true },
                        onAdvancedSettings = { showAdvancedPage = true },
                        onGraphicsSettings = { showGraphicsPage = true },
                        onEnginePreferences = { showEnginePrefsPage = true },
                        onEditAutoselect = { showAutoselectPage = true },
                    )

                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Game selection toggle ----------------
                    if (d1RequiredOk && d2RequiredOk) {
                        Text("Select Game", fontWeight = FontWeight.Bold, fontSize = 14.sp)
                        Spacer(modifier = Modifier.height(4.dp))
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            FilterChip(
                                selected = selectedGame == "d1",
                                onClick = {
                                    selectedGame = "d1"
                                    gamePrefs.edit().putString("selected_game", "d1").apply()
                                },
                                label = { Text("Descent 1") },
                                modifier = Modifier.weight(1f).tvFocusBorder(),
                            )
                            FilterChip(
                                selected = selectedGame == "d2",
                                onClick = {
                                    selectedGame = "d2"
                                    gamePrefs.edit().putString("selected_game", "d2").apply()
                                },
                                label = { Text("Descent 2") },
                                modifier = Modifier.weight(1f).tvFocusBorder(),
                            )
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    Button(
                        onClick = {
                            com.dxxredux.app.multiplayer.MatchmakingStateHolder.update {
                                it.copy(nav = com.dxxredux.app.multiplayer.MultiplayerNav.LAN)
                            }
                            showMultiplayerPage = true
                        },
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .height(40.dp),
                        enabled = canLaunch,
                        colors =
                            ButtonDefaults.buttonColors(
                                containerColor =
                                    if (!canLaunch) {
                                        MaterialTheme.colorScheme.surfaceVariant
                                    } else {
                                        MaterialTheme.colorScheme.secondaryContainer
                                    },
                                contentColor =
                                    if (!canLaunch) {
                                        MaterialTheme.colorScheme.onSurfaceVariant
                                    } else {
                                        MaterialTheme.colorScheme.onSecondaryContainer
                                    },
                            ),
                    ) {
                        Text("Multiplayer", fontSize = 14.sp)
                    }

                    Spacer(modifier = Modifier.height(8.dp))

                    Button(
                        onClick = { onLaunchGame(selectedGame, null) },
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .height(56.dp),
                        enabled = (canLaunch || gameRunning) && !isHashing,
                        colors =
                            ButtonDefaults.buttonColors(
                                containerColor =
                                    if ((!canLaunch && !gameRunning) || isHashing) {
                                        MaterialTheme.colorScheme.surfaceVariant
                                    } else {
                                        MaterialTheme.colorScheme.primary
                                    },
                            ),
                    ) {
                        Text(
                            text =
                                when {
                                    gameRunning -> "Return to Game"
                                    selectedGame == "d1" -> "Launch Descent 1"
                                    else -> "Launch Descent 2"
                                },
                            fontSize = 18.sp,
                        )
                    }
                }

                // -- Layout: landscape = side-by-side, portrait = stacked --

                if (isLandscape) {
                    Row(modifier = Modifier.weight(1f)) {
                        val leftScroll = rememberScrollState()
                        LaunchedEffect(focusResumeTrigger, showResumePanel) {
                            if (showResumePanel) {
                                leftScroll.scrollTo(0)
                            }
                        }
                        Box(modifier = Modifier.weight(1f).fillMaxHeight()) {
                            Column(
                                modifier =
                                    Modifier
                                        .fillMaxSize()
                                        .verticalScroll(leftScroll)
                                        .padding(end = 8.dp),
                            ) {
                                resumePanel?.invoke()
                                filesPane()
                            }
                            SetupScrollArrows(leftScroll)
                        }
                        val rightScroll = rememberScrollState()
                        Box(modifier = Modifier.weight(1f).fillMaxHeight()) {
                            Column(
                                modifier =
                                    Modifier
                                        .fillMaxSize()
                                        .verticalScroll(rightScroll)
                                        .padding(start = 8.dp),
                            ) {
                                controlsPane()
                            }
                            SetupScrollArrows(rightScroll)
                        }
                    }
                } else {
                    val portraitScroll = rememberScrollState()
                    LaunchedEffect(focusResumeTrigger, showResumePanel) {
                        if (showResumePanel) {
                            portraitScroll.scrollTo(0)
                        }
                    }
                    Box(modifier = Modifier.weight(1f)) {
                        Column(
                            modifier =
                                Modifier
                                    .fillMaxSize()
                                    .verticalScroll(portraitScroll),
                        ) {
                            filesPane()
                            Spacer(modifier = Modifier.height(16.dp))
                            controlsPane()
                        }
                        SetupScrollArrows(portraitScroll)
                    }
                }
            }
        }
    }
}

/** Format byte size as human-readable (KB, MB, GB). */
private fun formatSize(bytes: Long): String = formatBinarySize(bytes)

internal fun formatMissionArchiveProgressAmount(
    label: String,
    done: Long,
    total: Long,
): String =
    if (missionArchiveProgressUsesCounts(label)) {
        "$done / $total"
    } else {
        "${formatSize(done)} / ${formatSize(total)}"
    }

private fun missionArchiveProgressUsesCounts(label: String): Boolean =
    label.startsWith("Identifying music tracks:", ignoreCase = true)

@Composable
private fun ControllerSection(
    axes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    prefs: SharedPreferences,
    selectedGame: String = "d2",
    initialFocusRequester: FocusRequester? = null,
    onDefineControls: () -> Unit = {},
    onEditTouchLayout: () -> Unit = {},
    onAdvancedSettings: () -> Unit = {},
    onGraphicsSettings: () -> Unit = {},
    onEnginePreferences: () -> Unit = {},
    onEditAutoselect: () -> Unit = {},
) {
    val context = LocalContext.current

    // Poll for controller connect/disconnect every 1 second
    var pollTick by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) {
        while (true) {
            kotlinx.coroutines.delay(1000)
            pollTick++
        }
    }

    // Detect connected gamepads (re-evaluated on axis events OR poll tick)
    val gamepads =
        remember(axisGeneration, pollTick) {
            InputDevice
                .getDeviceIds()
                .toList()
                .mapNotNull { InputDevice.getDevice(it) }
                .filter { d ->
                    val src = d.sources
                    src and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                        src and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
                }
        }

    val displayedController = remember(gamepads) { selectDisplayedController(gamepads.map(::controllerDisplayDevice)) }
    val hasController = displayedController != null
    var expanded by remember { mutableStateOf(false) }
    var defineControlsReady by remember { mutableStateOf(false) }
    val inputModeManager = LocalInputModeManager.current

    LaunchedEffect(initialFocusRequester, defineControlsReady) {
        if (initialFocusRequester != null && defineControlsReady) {
            inputModeManager.requestInputMode(InputMode.Keyboard)
            withFrameNanos { }
            initialFocusRequester.requestFocus()
            kotlinx.coroutines.delay(300)
            inputModeManager.requestInputMode(InputMode.Keyboard)
            withFrameNanos { }
            initialFocusRequester.requestFocus()
        }
    }

    // -- Header --
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "Controller",
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = displayedController?.let { "\u2713 ${it.name}" } ?: "\u2717 Not detected",
            color = if (hasController) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
        )
        if (hasController) {
            Spacer(modifier = Modifier.width(8.dp))
            TextButton(
                onClick = { expanded = !expanded },
                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                modifier = Modifier.height(28.dp),
            ) {
                Text(
                    text = if (expanded) "Hide" else "Test",
                    fontSize = 12.sp,
                )
            }
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )

    // -- Touch overlay toggle --
    val hasTouchscreen = remember(context) { context.hasTouchscreen() }
    val defaultOverlay =
        defaultTouchOverlayEnabled(
            hasTouchscreen = hasTouchscreen,
            hasController = hasController,
        )
    var touchOverlay by remember {
        mutableStateOf(prefs.getBoolean("touch_overlay_enabled", defaultOverlay))
    }
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Checkbox(
            checked = touchOverlay,
            onCheckedChange = { checked ->
                touchOverlay = checked
                prefs.edit().putBoolean("touch_overlay_enabled", checked).apply()
            },
            modifier = Modifier.height(24.dp).tvFocusBorder(),
        )
        Spacer(modifier = Modifier.width(4.dp))
        Text(
            text = "Touch controls overlay",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurface,
        )
    }

    // -- In-game orientation lock --
    var orientLandscape by remember {
        mutableStateOf(prefs.getString("game_orientation", "landscape") == "landscape")
    }
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "In-game:",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurface,
            modifier = Modifier.padding(end = 8.dp),
        )
        TextButton(
            onClick = {
                orientLandscape = true
                prefs.edit().putString("game_orientation", "landscape").apply()
            },
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                "Landscape",
                fontSize = 12.sp,
                color = if (orientLandscape) MaterialTheme.colorScheme.primary else Color.Gray,
            )
        }
        TextButton(
            onClick = {
                orientLandscape = false
                prefs.edit().putString("game_orientation", "portrait").apply()
            },
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                "Portrait",
                fontSize = 12.sp,
                color = if (!orientLandscape) MaterialTheme.colorScheme.primary else Color.Gray,
            )
        }
    }

    // -- Controller Mapping button --
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = onDefineControls,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier =
                Modifier
                    .height(32.dp)
                    .padding(vertical = 2.dp)
                    .onGloballyPositioned {
                        defineControlsReady = true
                    }.then(
                        if (initialFocusRequester !=
                            null
                        ) {
                            Modifier.focusRequester(initialFocusRequester)
                        } else {
                            Modifier
                        },
                    ),
        ) {
            Text("Controller Mapping", fontSize = 12.sp)
        }
        OutlinedButton(
            onClick = onEditTouchLayout,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Touch Layout", fontSize = 12.sp)
        }
    }

    // -- Weapon Autoselect / Game Preferences / Graphics / Advanced --
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = onEditAutoselect,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Weapon Autoselect", fontSize = 12.sp)
        }
        OutlinedButton(
            onClick = onEnginePreferences,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Game Preferences", fontSize = 12.sp)
        }
    }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = onGraphicsSettings,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Graphics", fontSize = 12.sp)
        }
        OutlinedButton(
            onClick = onAdvancedSettings,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Advanced", fontSize = 12.sp)
        }
    }

    if (expanded && hasController) {
        // Read axes from the array (axisGeneration triggers recomposition)
        @Suppress("UNUSED_EXPRESSION")
        axisGeneration
        val lx = axes[0]
        val ly = axes[1]
        val rx = axes[2]
        val ry = axes[3]
        val lt = axes[4]
        val rt = axes[5]

        val axisColor = MaterialTheme.colorScheme.onSurfaceVariant
        val labelColor = MaterialTheme.colorScheme.onSurface

        Text(
            "Analog Sticks",
            fontSize = 14.sp,
            fontWeight = FontWeight.SemiBold,
            color = labelColor,
            modifier = Modifier.padding(bottom = 2.dp),
        )
        Row(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f)) {
                Text("Left Stick", fontSize = 12.sp, color = labelColor)
                Text("  X: ${"%.2f".format(lx)}", fontSize = 12.sp, color = axisColor)
                Text("  Y: ${"%.2f".format(ly)}", fontSize = 12.sp, color = axisColor)
            }
            Column(modifier = Modifier.weight(1f)) {
                Text("Right Stick", fontSize = 12.sp, color = labelColor)
                Text("  X: ${"%.2f".format(rx)}", fontSize = 12.sp, color = axisColor)
                Text("  Y: ${"%.2f".format(ry)}", fontSize = 12.sp, color = axisColor)
            }
        }

        Spacer(modifier = Modifier.height(4.dp))
        Text(
            "Triggers",
            fontSize = 14.sp,
            fontWeight = FontWeight.SemiBold,
            color = labelColor,
            modifier = Modifier.padding(bottom = 2.dp),
        )
        Row(modifier = Modifier.fillMaxWidth()) {
            Text(
                "  L: ${"%.2f".format(lt)}",
                fontSize = 12.sp,
                color = axisColor,
                modifier = Modifier.weight(1f),
            )
            Text(
                "  R: ${"%.2f".format(rt)}",
                fontSize = 12.sp,
                color = axisColor,
                modifier = Modifier.weight(1f),
            )
        }

        val hatX = dpadAxes[0]
        val hatY = dpadAxes[1]
        val dpadDir =
            buildString {
                if (hatY < -0.5f) append("Up ")
                if (hatY > 0.5f) append("Down ")
                if (hatX < -0.5f) append("Left ")
                if (hatX > 0.5f) append("Right ")
            }.trimEnd().ifEmpty { "(none)" }
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            "D-Pad",
            fontSize = 14.sp,
            fontWeight = FontWeight.SemiBold,
            color = labelColor,
            modifier = Modifier.padding(bottom = 2.dp),
        )
        Text(
            "  $dpadDir",
            fontSize = 12.sp,
            color = if (dpadDir == "(none)") axisColor else Color(0xFF4CAF50),
        )

        Spacer(modifier = Modifier.height(4.dp))
        Text(
            "Buttons",
            fontSize = 14.sp,
            fontWeight = FontWeight.SemiBold,
            color = labelColor,
            modifier = Modifier.padding(bottom = 2.dp),
        )
        Text(
            text =
                if (pressedButtons.isEmpty()) {
                    "  (none pressed)"
                } else {
                    "  " + pressedButtons.joinToString(", ")
                },
            fontSize = 12.sp,
            color = if (pressedButtons.isEmpty()) axisColor else Color(0xFF4CAF50),
        )
        Spacer(modifier = Modifier.height(8.dp))
    }
}
