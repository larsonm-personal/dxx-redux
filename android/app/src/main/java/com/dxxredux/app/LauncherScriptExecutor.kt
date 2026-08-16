package com.dxxredux.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.system.Os
import android.util.Log
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileWriter
import java.util.Locale
import java.util.Random

internal fun stripLauncherJson5(text: String): String {
    val withoutComments = StringBuilder(text.length)
    var inString = false
    var i = 0
    while (i < text.length) {
        val c = text[i]
        if (inString) {
            withoutComments.append(c)
            if (c == '\\' && i + 1 < text.length) {
                withoutComments.append(text[++i])
            } else if (c == '"') {
                inString = false
            }
        } else if (c == '"') {
            inString = true
            withoutComments.append(c)
        } else if (c == '/' && i + 1 < text.length && text[i + 1] == '/') {
            while (i < text.length && text[i] != '\n') i++
            if (i < text.length) withoutComments.append('\n')
        } else {
            withoutComments.append(c)
        }
        i++
    }

    val cleaned = StringBuilder(withoutComments.length)
    inString = false
    i = 0
    while (i < withoutComments.length) {
        val c = withoutComments[i]
        if (inString) {
            cleaned.append(c)
            if (c == '\\' && i + 1 < withoutComments.length) {
                cleaned.append(withoutComments[++i])
            } else if (c == '"') {
                inString = false
            }
        } else if (c == '"') {
            inString = true
            cleaned.append(c)
        } else if (c == ',') {
            var next = i + 1
            while (next < withoutComments.length && withoutComments[next].isWhitespace()) next++
            if (next >= withoutComments.length || withoutComments[next] !in "]}") cleaned.append(c)
        } else {
            cleaned.append(c)
        }
        i++
    }
    return cleaned.toString()
}

/**
 * Executes JSON5 automation scripts in the launcher (SetupActivity) process.
 *
 * Steps like log, wait_ms, wait_for, assert, setup_command, reset_state,
 * write_config, install_staged_demo, and enter_launcher are handled here. When an enter_game
 * step is reached, the executor launches the game with intent extras
 * telling the C engine where to resume, then suspends. SetupActivity
 * calls [resume] when the game exits and automation_result.json contains
 * LAUNCHER_CONTINUE.
 */
class LauncherScriptExecutor(
    private val activity: SetupActivity,
    internal val runId: String,
    private val launchGame: (game: String, scriptPath: String, startStep: Int) -> Unit,
) {
    private val context: Context get() = activity

    companion object {
        private const val TAG = "DXX-LauncherScript"
        private const val LEVEL_PREVIEW_SMOKE_SELECTION_FILE = "level_preview_smoke_selection.json"
        private const val ROBOT_PREVIEW_SMOKE_SELECTION_FILE = "robot_preview_smoke_selection.json"
    }

    /** Pending game launch set by tap_button with launches_game=true.
     *  Consumed by SetupActivity.onLaunchGame to route through automation. */
    data class PendingGameLaunch(
        val scriptPath: String,
        val nextStep: Int,
        val runId: String,
    )

    var pendingGameLaunch: PendingGameLaunch? = null
        private set

    fun consumePendingLaunch(): PendingGameLaunch? {
        val p = pendingGameLaunch
        pendingGameLaunch = null
        return p
    }

    var scriptPath: String = ""
        private set
    var running: Boolean = false
        private set

    private var steps = JSONArray()
    private var currentStep = 0
    private var totalSteps = 0
    private var startTimeMs = 0L

    /** Execute a script from the given path, starting at [startStep]. */
    suspend fun execute(
        path: String,
        startStep: Int = 0,
    ) {
        scriptPath = path
        running = true
        startTimeMs = System.currentTimeMillis()

        val text = withContext(Dispatchers.IO) { File(path).readText() }
        val cleaned = stripLauncherJson5(text)
        steps = JSONArray(cleaned)

        // Build flat step list skipping _info blocks
        val flat = JSONArray()
        for (i in 0 until steps.length()) {
            val obj = steps.optJSONObject(i) ?: continue
            if (obj.has("_info")) continue
            flat.put(obj)
        }
        steps = flat
        totalSteps = steps.length()
        currentStep = startStep

        Log.i(TAG, "Script loaded: $path ($totalSteps steps, starting at $startStep)")
        removeStaleResult()
        runSteps()
    }

    /** Resume execution after the game exited with LAUNCHER_CONTINUE. */
    suspend fun resume(nextStep: Int) {
        running = true
        currentStep = nextStep
        Log.i(TAG, "Resuming from step $nextStep")
        removeStaleResult()
        runSteps()
    }

    private suspend fun runSteps() {
        try {
            runStepsInner()
        } catch (e: kotlinx.coroutines.CancellationException) {
            throw e // Don't swallow cancellation
        } catch (e: Exception) {
            Log.e(TAG, "Uncaught exception in runSteps", e)
            fail("uncaught exception: ${e.javaClass.simpleName}: ${e.message}")
        }
    }

    private suspend fun runStepsInner() {
        while (currentStep < totalSteps) {
            val step = steps.getJSONObject(currentStep)
            val action = step.optString("action", "")
            Log.i(TAG, "Step ${currentStep + 1}/$totalSteps: $action")

            when (action) {
                "enter_launcher" -> {
                    // No-op: already in launcher mode
                    currentStep++
                }

                "enter_game" -> {
                    val game = step.optString("game", "d2")
                    val nextStep = currentStep + 1
                    Log.i(TAG, "Launching game ($game), C engine starts at step $nextStep")
                    running = false
                    launchGame(game, scriptPath, nextStep)
                    return // Suspend -- SetupActivity.onResume will call resume()
                }

                "log" -> {
                    val msg = step.optString("message", "")
                    Log.i(TAG, "SCRIPT: $msg")
                    currentStep++
                }

                "wait_ms" -> {
                    val ms = step.optLong("ms", step.optLong("post_delay_ms", 300))
                    Log.i(TAG, "Waiting ${ms}ms")
                    delay(ms)
                    currentStep++
                }

                "wait_for" -> {
                    val field = step.optString("field", "")
                    val value = step.optString("value", "")
                    val timeoutMs = step.optLong("timeout_ms", 0)
                    val deadline = if (timeoutMs > 0) System.currentTimeMillis() + timeoutMs else 0L
                    Log.i(TAG, "Waiting for $field = $value (timeout=${timeoutMs}ms)")

                    while (true) {
                        if (checkSetupCondition(field, value)) {
                            Log.i(TAG, "Condition met: $field = $value")
                            break
                        }
                        if (deadline > 0 && System.currentTimeMillis() > deadline) {
                            val reason = "TIMEOUT waiting for $field = $value (after ${timeoutMs}ms)"
                            fail(reason)
                            return
                        }
                        delay(1000)
                    }
                    currentStep++
                }

                "assert" -> {
                    val expect = step.optJSONObject("expect")
                    if (expect == null) {
                        fail("assert step missing 'expect' object")
                        return
                    }
                    val failMsg = runSetupAssertions(expect)
                    if (failMsg != null) {
                        fail("assert: $failMsg")
                        return
                    }
                    currentStep++
                }

                "setup_command" -> {
                    val cmd = step.optString("command", "")
                    val args = step.optJSONObject("args")
                    val postDelay = step.optLong("post_delay_ms", 0)
                    Log.i(TAG, "Sending setup command: $cmd")
                    sendSetupCommand(cmd, args)
                    if (postDelay > 0) delay(postDelay)
                    currentStep++
                }

                "reset_state" -> {
                    Log.i(TAG, "Resetting game state")
                    resetGameState()
                    currentStep++
                }

                "write_config" -> {
                    val fileName = step.optString("file", "")
                    val content = step.optString("content", "")
                    if (fileName.isEmpty()) {
                        fail("write_config: missing 'file' field")
                        return
                    }
                    Log.i(TAG, "Writing config: $fileName")
                    withContext(Dispatchers.IO) {
                        val outFile = File(context.filesDir, fileName)
                        outFile.parentFile?.mkdirs()
                        FileWriter(outFile).use { it.write(content) }
                    }
                    currentStep++
                }

                "install_staged_demo" -> {
                    val requestedName = step.optString("name", "")
                    val game = step.optString("game", "")
                    val target =
                        withContext(Dispatchers.IO) {
                            InputDemoManager
                                .listStagedDemos(context.filesDir)
                                .firstOrNull { game.isEmpty() || it.game == game }
                        }
                    if (target == null) {
                        fail("install_staged_demo: no staged demo${if (game.isEmpty()) "" else " for $game"}")
                        return
                    }
                    val installName =
                        if (requestedName.isNotEmpty()) {
                            requestedName
                        } else {
                            InputDemoManager.sanitizeInstallName(target.file.nameWithoutExtension)
                        }
                    val dest =
                        withContext(Dispatchers.IO) {
                            val fileSetManager = FileSetManager(context.filesDir)
                            val activeSetName = fileSetManager.getActive()
                            val activeSetDir = fileSetManager.getSetDir(activeSetName)
                            val demosDir = File(activeSetDir, "demos").also { it.mkdirs() }

                            ImportStorageGuard.requireFreeSpace(
                                demosDir,
                                InputDemoManager.stagedFileBytes(target),
                                "install ${target.file.name}",
                            )
                            InputDemoManager.installToSet(target, activeSetDir, installName)
                        }
                    Log.i(TAG, "INSTALL_STAGED_DEMO: ${target.file.name} -> ${dest.absolutePath}")
                    currentStep++
                }

                "assert_controller_match" -> {
                    val failMsg = withContext(Dispatchers.IO) { compareControllerIntrospections() }
                    if (failMsg != null) {
                        fail("assert_controller_match: $failMsg")
                        return
                    }
                    Log.i(TAG, "ASSERT_PASS: controller configs match")
                    currentStep++
                }

                "clear_mods" -> {
                    val removed = withContext(Dispatchers.IO) { ModManager(context.filesDir).clearAllMods() }
                    Log.i(TAG, "ASSERT_PASS: clear_mods removed $removed files")
                    currentStep++
                }

                "delete_mod" -> {
                    val filename = step.optString("file", step.optString("mod_file", ""))
                    if (filename.isBlank() || File(filename).name != filename) {
                        fail("delete_mod: invalid or missing filename")
                        return
                    }
                    withContext(Dispatchers.IO) {
                        ModManager(context.filesDir).deleteMod(filename)
                    }
                    Log.i(TAG, "ASSERT_PASS: deleted mod $filename and its owned extraction cache")
                    currentStep++
                }

                "import_mission_zip" -> {
                    val label = step.optString("label", step.optString("file", step.optString("path", "")))
                    val importResult =
                        withContext(Dispatchers.IO) {
                            importMissionZipForAutomation(step)
                        }
                    if (importResult == null) {
                        fail("import_mission_zip: import failed for $label")
                        return
                    }
                    writeMissionZipImportAutomationResult(label, importResult.first, importResult.second)
                    Log.i(TAG, "ASSERT_PASS: imported mission ZIP ${importResult.first.filename}")
                    currentStep++
                }

                "analyze_level_metadata" -> {
                    val label = step.optString("label", step.optString("file", step.optString("mod_file", "")))
                    val minLevels = step.optInt("min_levels", 1)
                    val expectedStatus = step.optString("status", "ok")
                    val target =
                        withContext(Dispatchers.IO) {
                            buildLevelMetadataTargetForAutomation(step)
                        }
                    if (target == null) {
                        fail("analyze_level_metadata: could not build target for $label")
                        return
                    }
                    if (step.optBoolean("expect_result_cache_hit")) {
                        val expectedLevelCount = target.normalLevelFiles.size + target.secretLevelFiles.size
                        val identity = withContext(Dispatchers.IO) { LevelMetadataResultCache.identify(target) }
                        val cached =
                            identity?.let {
                                withContext(Dispatchers.IO) {
                                    LevelMetadataResultCache.read(
                                        File(context.filesDir, "level_metadata_results"),
                                        it,
                                        target,
                                        expectedLevelCount,
                                    )
                                }
                            }
                        if (cached == null) {
                            fail("analyze_level_metadata: expected a persisted result cache hit for $label")
                            return
                        }
                        Log.i(TAG, "ASSERT_PASS: persisted level metadata result cache hit for $label")
                    }
                    Log.i(TAG, "Analyzing level metadata: ${target.displayName}")
                    val result = LevelMetadataAnalyzer.analyze(context, target)
                    writeLevelMetadataAutomationResult(label, result)
                    if (result.status != expectedStatus) {
                        fail(
                            "analyze_level_metadata: ${target.displayName} status=${result.status} " +
                                "expected=$expectedStatus problems=${result.problems.joinToString("; ")}",
                        )
                        return
                    }
                    if (result.levels.size < minLevels) {
                        fail(
                            "analyze_level_metadata: ${target.displayName} levels=${result.levels.size} " +
                                "expected at least $minLevels",
                        )
                        return
                    }
                    Log.i(
                        TAG,
                        "ASSERT_PASS: level metadata ${target.displayName} status=${result.status} levels=${result.levels.size}",
                    )
                    currentStep++
                }

                "analyze_level_metadata_all" -> {
                    val label = step.optString("label", step.optString("file", step.optString("mod_file", "")))
                    val minTotalLevels = step.optInt("min_total_levels", 1)
                    val expectedStatus = step.optString("status", "ok")
                    val targets =
                        withContext(Dispatchers.IO) {
                            buildLevelMetadataTargetsForAutomation(step)
                        }
                    if (targets.isEmpty()) {
                        fail("analyze_level_metadata_all: could not build any targets for $label")
                        return
                    }
                    val results = mutableListOf<LevelMetadataResult>()
                    for (target in targets) {
                        Log.i(TAG, "Analyzing level metadata: ${target.displayName}")
                        results += LevelMetadataAnalyzer.analyze(context, target)
                    }
                    writeLevelMetadataAutomationResults(label, targets, results)
                    val bad = results.firstOrNull { it.status != expectedStatus }
                    if (bad != null) {
                        fail(
                            "analyze_level_metadata_all: ${bad.source} status=${bad.status} " +
                                "expected=$expectedStatus problems=${bad.problems.joinToString("; ")}",
                        )
                        return
                    }
                    val totalLevels = results.sumOf { it.levels.size }
                    if (totalLevels < minTotalLevels) {
                        fail(
                            "analyze_level_metadata_all: total levels=$totalLevels expected at least $minTotalLevels",
                        )
                        return
                    }
                    Log.i(
                        TAG,
                        "ASSERT_PASS: level metadata targets=${results.size} total_levels=$totalLevels",
                    )
                    currentStep++
                }

                "launch_random_level_preview" -> {
                    val seed = step.optLong("seed", 0L)
                    val label = step.optString("label", step.optString("mod_file", "preview"))
                    val requestedGame = step.optString("game", "")
                    val targets =
                        withContext(Dispatchers.IO) {
                            buildLevelMetadataTargetsForAutomation(step)
                        }.filter { requestedGame.isBlank() || it.game == requestedGame }
                    if (targets.isEmpty()) {
                        fail("launch_random_level_preview: no metadata targets for $label")
                        return
                    }
                    val candidates = mutableListOf<Pair<LevelMetadataTarget, LevelMetadataLevelRow>>()
                    for (target in targets) {
                        val result = LevelMetadataAnalyzer.analyze(context, target)
                        if (result.status == "ok") {
                            result.levels
                                .filter { it.status == "ok" && it.levelFile.isNotBlank() }
                                .forEach { row -> candidates += target to row }
                        }
                    }
                    if (candidates.isEmpty()) {
                        fail("launch_random_level_preview: no previewable levels for $label")
                        return
                    }
                    val selected = candidates[Random(seed).nextInt(candidates.size)]
                    val launchRequest =
                        withContext(Dispatchers.IO) {
                            LevelPreviewRequestStore.create(context.cacheDir, selected.first, selected.second)
                        }
                    writeLevelPreviewSmokeSelection(
                        seed = seed,
                        label = label,
                        candidateCount = candidates.size,
                        target = selected.first,
                        row = selected.second,
                        request = launchRequest,
                    )
                    Log.i(
                        TAG,
                        "Launching seeded level preview: seed=$seed source=${selected.first.displayName} " +
                            "level=${selected.second.levelNum} file=${selected.second.levelFile}",
                    )
                    currentStep++
                    activity.prepareForLevelPreviewLaunch()
                    activity.startActivity(LevelPreviewActivity.createIntent(context, launchRequest))
                }

                "launch_random_robot_preview" -> {
                    val seed = step.optLong("seed", 0L)
                    val label = step.optString("label", step.optString("mod_file", "robot preview"))
                    val requestedGame = step.optString("game", "")
                    val candidates =
                        mutableListOf<
                            Triple<
                                LevelMetadataTarget,
                                LevelMetadataLevelRow,
                                LevelMetadataReplacementItem,
                            >,
                        >()
                    withContext(Dispatchers.IO) { buildLevelMetadataTargetsForAutomation(step) }
                        .filter { requestedGame.isBlank() || it.game == requestedGame }
                        .forEach { target ->
                            val result = LevelMetadataAnalyzer.analyze(context, target)
                            if (result.status == "ok") {
                                result.levels.forEach { row ->
                                    row.replacementGroups
                                        .flatMap { it.items }
                                        .filter { it.kind == "robot" && it.number >= 0 }
                                        .forEach { item -> candidates += Triple(target, row, item) }
                                }
                            }
                        }
                    if (candidates.isEmpty()) {
                        fail("launch_random_robot_preview: no changed robots for $label")
                        return
                    }
                    val selected = candidates[Random(seed).nextInt(candidates.size)]
                    val robotNames = RobotNameCatalog.load(context, selected.first.game)
                    val robotLabel =
                        RobotNameCatalog.displayName(
                            robotNames,
                            selected.third.number,
                            selected.third.label,
                        )
                    val launchRequest =
                        withContext(Dispatchers.IO) {
                            RobotPreviewRequestStore.create(
                                context.cacheDir,
                                selected.first,
                                selected.second,
                                selected.third,
                                robotLabel,
                            )
                        }
                    writeRobotPreviewSmokeSelection(seed, label, candidates.size, selected, launchRequest, robotLabel)
                    Log.i(
                        TAG,
                        "Launching seeded robot preview: seed=$seed source=${selected.first.displayName} " +
                            "level=${selected.second.levelNum} robot=${selected.third.number}",
                    )
                    currentStep++
                    activity.prepareForLevelPreviewLaunch()
                    activity.startActivity(RobotPreviewActivity.createIntent(context, launchRequest))
                }

                "launch_base_robot_preview" -> {
                    val game = step.optString("game", GameFileFormats.GAME_D2)
                    val robots = RobotNameCatalog.load(context, game)
                    val robotNumber = step.optInt("robot_number", 0)
                    if (robotNumber !in robots.indices) {
                        fail("launch_base_robot_preview: robot $robotNumber is out of range for $game")
                        return
                    }
                    val robotLabel = RobotNameCatalog.displayName(robots, robotNumber, "Robot $robotNumber")
                    val fileSetManager = FileSetManager(context.filesDir)
                    val dataDir = fileSetManager.getSetDir(fileSetManager.getActive())
                    val launchRequest =
                        withContext(Dispatchers.IO) {
                            RobotPreviewRequestStore.createBase(
                                context.cacheDir,
                                game,
                                dataDir,
                                robotNumber,
                                robotLabel,
                            )
                        }
                    writeBaseRobotPreviewSmokeSelection(game, robotNumber, robotLabel, launchRequest)
                    Log.i(TAG, "Launching base robot preview: game=$game robot=$robotNumber")
                    currentStep++
                    activity.prepareForLevelPreviewLaunch()
                    activity.startActivity(RobotPreviewActivity.createIntent(context, launchRequest))
                }

                "tap_button" -> {
                    val text = step.optString("text", "")
                    val exact = step.optBoolean("exact", false)
                    val launchesGame = step.optBoolean("launches_game", false)
                    val postDelay = step.optLong("post_delay_ms", 300)
                    val timeoutMs = step.optLong("timeout_ms", if (launchesGame) 30000 else 10000)
                    if (text.isEmpty()) {
                        fail("tap_button: missing 'text' field")
                        return
                    }
                    // Poll for the button to appear AND be enabled.
                    // If not found, scroll down -- the button may be off-screen.
                    // If found but disabled, keep polling (Compose may be recomposing
                    // after file detection or import).
                    val deadline = System.currentTimeMillis() + timeoutMs
                    var button: SetupActivity.ButtonInfo? = null
                    var scrollAttempts = 0
                    var resetToTop = false
                    while (true) {
                        button = activity.findButtonByText(text, exact)
                        if (button != null && button.enabled) break
                        if (System.currentTimeMillis() > deadline) {
                            if (button != null && !button.enabled) {
                                fail("tap_button: button \"${button.text}\" is disabled")
                            } else {
                                val available =
                                    activity
                                        .collectAccessibleButtons()
                                        .joinToString(", ") { "\"${it.text}\"" }
                                fail("tap_button: no button matching \"$text\" (available: $available)")
                            }
                            return
                        }
                        if (button == null && !resetToTop) {
                            activity.scrollToTop()
                            resetToTop = true
                            delay(300)
                        } else if (button == null && scrollAttempts < 20) {
                            activity.scrollDown()
                            scrollAttempts++
                            delay(400)
                        } else {
                            delay(500)
                        }
                    }
                    Log.i(TAG, "TAP: \"${button.text}\" at (${button.centerX}, ${button.centerY})")
                    if (launchesGame) {
                        pendingGameLaunch = PendingGameLaunch(scriptPath, currentStep + 1, runId)
                    }
                    if (!activity.performAccessibilityClick(text)) {
                        // Fallback to touch injection
                        activity.injectTapAt(button.centerX, button.centerY)
                    }
                    delay(postDelay)
                    if (launchesGame) {
                        running = false
                        return // Suspend -- game launches via onClick -> onLaunchGame
                    }
                    currentStep++
                }

                "assert_button" -> {
                    val text = step.optString("text", "")
                    val exact = step.optBoolean("exact", false)
                    val timeoutMs = step.optLong("timeout_ms", 10000)
                    if (text.isEmpty()) {
                        fail("assert_button: missing 'text' field")
                        return
                    }
                    val expectEnabled = if (step.has("enabled")) step.optBoolean("enabled", true) else null
                    val deadline = System.currentTimeMillis() + timeoutMs
                    var button: SetupActivity.ButtonInfo? = null
                    var scrollAttempts = 0
                    var resetToTop = false
                    while (true) {
                        button = activity.findButtonByText(text, exact)
                        if (button != null && (expectEnabled == null || button.enabled == expectEnabled)) {
                            break
                        }
                        if (System.currentTimeMillis() > deadline) {
                            if (button != null && expectEnabled != null && button.enabled != expectEnabled) {
                                fail(
                                    "assert_button: \"${button.text}\" enabled=${button.enabled} (expected $expectEnabled)",
                                )
                            } else {
                                val available =
                                    activity
                                        .collectAccessibleButtons()
                                        .joinToString(", ") { "\"${it.text}\"" }
                                fail("assert_button: no button matching \"$text\" (available: $available)")
                            }
                            return
                        }
                        if (button == null && !resetToTop) {
                            activity.scrollToTop()
                            resetToTop = true
                            delay(300)
                        } else if (button == null && scrollAttempts < 20) {
                            activity.scrollDown()
                            scrollAttempts++
                            delay(400)
                        } else {
                            delay(500)
                        }
                    }
                    Log.i(TAG, "ASSERT_PASS: button \"${button.text}\" enabled=${button.enabled}")
                    currentStep++
                }

                else -> {
                    Log.w(TAG, "Skipping unknown/game-only action: $action")
                    currentStep++
                }
            }
        }

        // All steps completed
        Log.i(TAG, "SCRIPT_RESULT: PASS ($totalSteps steps)")
        writeResult("PASS", null)
        running = false
    }

    private suspend fun sendSetupCommand(
        cmd: String,
        args: JSONObject?,
    ) {
        val intent = Intent("com.dxxredux.SETUP_COMMAND")
        intent.setPackage(context.packageName)
        intent.putExtra("command", cmd)
        if (args != null) {
            for (key in args.keys()) {
                val v = args.get(key)
                when (v) {
                    is Boolean -> intent.putExtra(key, v)
                    is Int -> intent.putExtra(key, v)
                    is Long -> intent.putExtra(key, v.toInt())
                    is Double -> intent.putExtra(key, v.toFloat())
                    is String -> intent.putExtra(key, v)
                    else -> intent.putExtra(key, v.toString())
                }
            }
        }
        sendOrderedBroadcastAndWait(intent)
    }

    private suspend fun sendOrderedBroadcastAndWait(intent: Intent) {
        val completed = CompletableDeferred<Unit>()
        val resultReceiver =
            object : BroadcastReceiver() {
                override fun onReceive(
                    receiverContext: Context?,
                    receiverIntent: Intent?,
                ) {
                    completed.complete(Unit)
                }
            }
        context.sendOrderedBroadcast(intent, null, resultReceiver, null, 0, null, null)
        completed.await()
    }

    private fun resetGameState() {
        MidiPreviewBridge.stop()
        CdPreviewBridge.stop()
        val dir = context.filesDir
        val saveRegex = Regex("""\.(?:sg|mg)[0-9]$""", RegexOption.IGNORE_CASE)
        val patterns =
            listOf(
                "*.plr",
                "*.plx",
                "descent.cfg",
                "controller_config.json",
                "controller_introspect.json",
                "controller_patch_result.json",
                "controller_reset_result.json",
            )
        for (pattern in patterns) {
            if (pattern.startsWith("*")) {
                val ext = pattern.removePrefix("*")
                dir.listFiles()?.filter { it.name.endsWith(ext) }?.forEach { it.delete() }
            } else {
                File(dir, pattern).delete()
            }
        }
        // Also delete config and player files from game subdirs (the PHYSFS
        // write dir is d2x-redux/ or d1x-redux/, so the game writes plr/plx/cfg there).
        // Player files may be in a Players/ subdir, so walk recursively.
        for (sub in arrayOf("d1x-redux", "d2x-redux")) {
            val subDir = File(dir, sub)
            File(subDir, "descent.cfg").delete()
            subDir
                .walkTopDown()
                .filter {
                    it.isFile &&
                        (
                            it.name.endsWith(".plr") ||
                                it.name.endsWith(".plx") ||
                                saveRegex.containsMatchIn(it.name)
                        )
                }.forEach { it.delete() }
        }
        val stagedDemosDeleted = InputDemoManager.deleteAllStagedDemos(dir)
        context
            .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
            .edit()
            .clear()
            // A reset has no guaranteed CD or custom-audio registry. Keep the
            // next ordinary launch self-contained; source-specific scripts
            // explicitly select CD or files after registering their inputs.
            .putString("music_mode", "midi")
            .commit()
        context
            .getSharedPreferences("launcher_prefs", Context.MODE_PRIVATE)
            .edit()
            .clear()
            .commit()
        clearPendingResumeLaunchState(context)
        Log.i(
            TAG,
            "Game state reset (stopped music previews, deleted plr/plx/sg/mg/cfg/pending resume files " +
                "and preferences, " +
                "staged_input_demos=$stagedDemosDeleted)",
        )
    }

    /** Check a field in setup_introspect.json. Triggers a fresh introspect first. */
    private suspend fun checkSetupCondition(
        field: String,
        value: String,
    ): Boolean {
        // Trigger fresh introspection
        val intent = Intent("com.dxxredux.SETUP_INTROSPECT")
        intent.setPackage(context.packageName)
        sendOrderedBroadcastAndWait(intent)

        return withContext(Dispatchers.IO) {
            val f = File(context.filesDir, "setup_introspect.json")
            if (!f.exists()) return@withContext false
            try {
                val json = JSONObject(f.readText())
                val actual = resolveJsonPath(json, field) ?: return@withContext false
                actual.equals(value, ignoreCase = true)
            } catch (e: Exception) {
                Log.w(TAG, "Error reading setup_introspect: ${e.message}")
                false
            }
        }
    }

    /** Run assertions against setup_introspect.json. Returns null on success, error on failure. */
    private suspend fun runSetupAssertions(expect: JSONObject): String? {
        val intent = Intent("com.dxxredux.SETUP_INTROSPECT")
        intent.setPackage(context.packageName)
        sendOrderedBroadcastAndWait(intent)

        return withContext(Dispatchers.IO) {
            val f = File(context.filesDir, "setup_introspect.json")
            if (!f.exists()) return@withContext "setup_introspect.json not found"
            try {
                val json = JSONObject(f.readText())
                for (key in expect.keys()) {
                    val expected = expect.get(key)
                    val actual =
                        resolveJsonPath(json, key)
                            ?: return@withContext "key \"$key\" not found"

                    if (expected is JSONObject) {
                        // Comparison operator: {"gt": 1000}
                        val op = expected.keys().next()
                        val opVal = expected.getDouble(op)
                        val actualNum =
                            actual.toDoubleOrNull()
                                ?: return@withContext "\"$key\" is not numeric (got \"$actual\")"
                        val pass =
                            when (op) {
                                "gt" -> actualNum > opVal
                                "lt" -> actualNum < opVal
                                "gte" -> actualNum >= opVal
                                "lte" -> actualNum <= opVal
                                "ne" -> actualNum != opVal
                                "eq" -> actualNum == opVal
                                else -> return@withContext "\"$key\": unknown op \"$op\""
                            }
                        if (!pass) return@withContext "\"$key\" $op $opVal (got $actual)"
                    } else {
                        val expectedStr = expected.toString()
                        if (!actual.equals(expectedStr, ignoreCase = true)) {
                            return@withContext "\"$key\" == \"$expectedStr\" (got \"$actual\")"
                        }
                    }
                    Log.i(TAG, "ASSERT_PASS: $key = $actual")
                }
                null // all passed
            } catch (e: Exception) {
                "parse error: ${e.message}"
            }
        }
    }

    /**
     * Compare joystick_controls between controller_introspect.json (launcher)
     * and introspect.json (game). Returns null on match, error message on mismatch.
     * This also requires successful reset and patch operations for the selected
     * game's real pilot file before comparing persisted joystick and keyboard data.
     */
    private fun compareControllerIntrospections(): String? {
        val launcherFile = File(context.filesDir, "controller_introspect.json")
        val gameFile = File(context.filesDir, "introspect.json")
        if (!launcherFile.exists()) return "controller_introspect.json not found"
        if (!gameFile.exists()) return "introspect.json not found"

        val launcherJson = JSONObject(launcherFile.readText())
        val gameJson = JSONObject(gameFile.readText())
        val game = launcherJson.optString("game", "")
        if (game !in arrayOf("d1", "d2")) return "launcher JSON has invalid game '$game'"
        for (
        (operation, filename) in
        arrayOf(
            "reset" to "controller_reset_result.json",
            "patch" to "controller_patch_result.json",
        )
        ) {
            val resultFile = File(context.filesDir, filename)
            if (!resultFile.exists()) return "$operation result not found"
            val count = JSONObject(resultFile.readText()).optInt(game, 0)
            if (count < 1) return "$operation did not process a real $game pilot file"
        }
        val ljc =
            launcherJson.optJSONObject("joystick_controls")
                ?: return "launcher JSON missing joystick_controls"
        val gjc =
            gameJson.optJSONObject("joystick_controls")
                ?: return "game JSON missing joystick_controls"

        // control_type must match exactly
        val lct = ljc.opt("control_type")?.toString() ?: ""
        val gct = gjc.opt("control_type")?.toString() ?: ""
        if (lct != gct) return "control_type: launcher=$lct game=$gct"

        for (field in arrayOf("bound_count", "bound_controls", "total_count")) {
            val lv = ljc.opt(field)?.toString() ?: ""
            val gv = gjc.opt(field)?.toString() ?: ""
            if (lv != gv) return "$field: launcher=$lv game=$gv"
        }

        val lItems = ljc.optJSONArray("items") ?: return "launcher missing items array"
        val gItems = gjc.optJSONArray("items") ?: return "game missing items array"
        if (lItems.length() == 0) return "no items to compare"
        if (lItems.length() != gItems.length()) {
            return "item count: launcher=${lItems.length()} game=${gItems.length()}"
        }
        for (i in 0 until lItems.length()) {
            val li = lItems.getJSONObject(i)
            val gi = gItems.getJSONObject(i)
            for (f in arrayOf("name", "type", "value")) {
                val lv = li.opt(f)?.toString() ?: ""
                val gv = gi.opt(f)?.toString() ?: ""
                val match = if (f == "name") lv.equals(gv, ignoreCase = true) else lv == gv
                if (!match) return "item[$i] ${gi.optString("name", "?")}.$f: launcher=$lv game=$gv"
            }
        }

        val lKeyboard = launcherJson.optJSONArray("keyboard_settings") ?: return "launcher missing keyboard settings"
        val gKeyboard = gameJson.optJSONArray("keyboard_settings") ?: return "game missing keyboard settings"
        if (lKeyboard.length() != gKeyboard.length()) {
            return "keyboard count: launcher=${lKeyboard.length()} game=${gKeyboard.length()}"
        }
        for (i in 0 until lKeyboard.length()) {
            val lv = lKeyboard.getInt(i)
            val gv = gKeyboard.getInt(i)
            if (lv != gv) return "keyboard[$i]: launcher=$lv game=$gv"
        }
        return null
    }

    private fun buildLevelMetadataTargetForAutomation(step: JSONObject): LevelMetadataTarget? {
        val fileName = step.optString("file", "")
        val modFileName = step.optString("mod_file", "")
        val game = step.optString("game", GameFileFormats.GAME_D2)
        val fileSetManager = FileSetManager(context.filesDir)
        val setDir = fileSetManager.getSetDir(fileSetManager.getActive())

        if (fileName.isNotBlank()) {
            val file = findFile(setDir, fileName)?.let { File(setDir, it) } ?: File(setDir, fileName)
            val metadata = GameFileMetadata.summarizeLocalFile(file)
            return LevelMetadataTargets.directFile(file, setDir, metadata)
        }

        if (modFileName.isNotBlank()) {
            val modFile = File(File(context.filesDir, "mods"), modFileName)
            MissionZip.inspect(modFile)?.let { scan ->
                LevelMetadataTargets.missionZip(modFile.absolutePath, setDir, scan)?.let { return it }
            }
            return LevelMetadataTargets.genericZip(modFile.absolutePath, setDir, modFile.name, game)
        }

        return null
    }

    private fun buildLevelMetadataTargetsForAutomation(step: JSONObject): List<LevelMetadataTarget> {
        val fileName = step.optString("file", "")
        val modFileName = step.optString("mod_file", "")
        val game = step.optString("game", GameFileFormats.GAME_D2)
        val fileSetManager = FileSetManager(context.filesDir)
        val setDir = fileSetManager.getSetDir(fileSetManager.getActive())

        if (fileName.isNotBlank()) {
            val file = findFile(setDir, fileName)?.let { File(setDir, it) } ?: File(setDir, fileName)
            val metadata = GameFileMetadata.summarizeLocalFile(file)
            return listOfNotNull(LevelMetadataTargets.directFile(file, setDir, metadata))
        }

        if (modFileName.isNotBlank()) {
            val modFile = File(File(context.filesDir, "mods"), modFileName)
            MissionZip.inspect(modFile)?.let { scan ->
                return LevelMetadataTargets.missionZipTargets(modFile.absolutePath, setDir, scan)
            }
            return listOfNotNull(LevelMetadataTargets.genericZip(modFile.absolutePath, setDir, modFile.name, game))
        }

        return emptyList()
    }

    private fun importMissionZipForAutomation(step: JSONObject): Pair<ModManager.ModInfo, MissionZip.ScanResult?>? {
        val source = resolveAutomationFile(step)
        if (!source.isFile) {
            Log.e(TAG, "import_mission_zip: source missing: ${source.absolutePath}")
            return null
        }
        val displayName = step.optString("display_name", source.name).ifBlank { source.name }
        val modManager = ModManager(context.filesDir, context)
        val before = modManager.listMods().map { it.filename }.toSet()
        val shouldMoveSource = isInAutomationCache(source)
        val mod =
            if (shouldMoveSource) {
                modManager.importMissionZipFileMovingSource(source, displayName)
            } else {
                modManager.importMissionZipFile(source, displayName)
            } ?: return null
        modManager.reload()
        val imported = modManager.listMods().firstOrNull { it.filename == mod.filename } ?: return null
        if (imported.filename in before) {
            Log.w(TAG, "import_mission_zip: ${imported.filename} replaced an existing manifest entry")
        }
        val scan = MissionZip.inspect(File(File(context.filesDir, "mods"), imported.filename))
        return imported to scan
    }

    private fun isInAutomationCache(source: File): Boolean {
        val cacheDir = File(context.filesDir, "mission_zip_batch_cache").canonicalFile
        val canonicalSource = source.canonicalFile
        return canonicalSource.parentFile == cacheDir
    }

    private fun resolveAutomationFile(step: JSONObject): File {
        val path = step.optString("path", "")
        if (path.isNotBlank()) {
            val candidate = File(path)
            return if (candidate.isAbsolute) candidate else File(context.filesDir, path)
        }
        return File(File(context.filesDir, "mission_zip_batch_cache"), step.optString("file", ""))
    }

    private fun writeLevelMetadataAutomationResult(
        label: String,
        result: LevelMetadataResult,
    ) {
        val safeLabel = safeAutomationLabel(label.ifBlank { result.source.ifBlank { "metadata" } })
        val file = File(context.filesDir, "level_metadata_automation_$safeLabel.json")
        AtomicFilePublication.writeUtf8(file, levelMetadataResultJson(result).toString(2) + "\n")
    }

    private fun writeLevelMetadataAutomationResults(
        label: String,
        targets: List<LevelMetadataTarget>,
        results: List<LevelMetadataResult>,
    ) {
        val file = File(context.filesDir, "level_metadata_automation_${safeAutomationLabel(label)}.json")
        val array = JSONArray()
        results.forEachIndexed { index, result ->
            val missionDisplayName = targets.getOrNull(index)?.missionDisplayName.orEmpty()
            array.put(
                levelMetadataResultJson(result)
                    .apply {
                        if (missionDisplayName.isNotBlank()) put("mission_name", missionDisplayName)
                    }.put("target_index", index),
            )
        }
        AtomicFilePublication.writeUtf8(file, array.toString(2) + "\n")
    }

    private fun levelMetadataResultJson(result: LevelMetadataResult): JSONObject {
        val levels = JSONArray()
        result.levels.forEach { row ->
            val rowJson =
                JSONObject()
                    .put("level_num", row.levelNum)
                    .put("secret", row.secret)
                    .put("level_name", row.levelName)
                    .put("level_file", row.levelFile)
                    .put("robots", row.robots)
                    .put("hostages", row.hostages)
                    .put("secrets", row.secrets)
                    .put("matcens", row.matcens)
                    .put("energy_centers", row.energyCenters)
                    .put("mine_volume", row.mineVolume)
                    .put("mine_volume_normalized", row.mineVolumeNormalized)
                    .put("mine_volume_text", row.mineVolumeText)
                    .put("travel_distance", row.travelDistance)
                    .put("travel_time_seconds", row.travelTimeSeconds)
                    .put("travel_time_text", row.travelTimeText)
                    .put("guidebot_count", row.guidebotCount)
                    .put("guidebot_placed", row.guidebotPlaced)
                    .put("guidebot_accessible", row.guidebotAccessible)
                    .put("route_status", row.routeStatus)
                    .put("route_steps", levelMetadataRouteStepsJson(row.routeSteps))
                    .put("replacements", levelMetadataReplacementsJson(row.replacements))
                    .put("replacement_groups", levelMetadataReplacementGroupsJson(row.replacementGroups))
            if (row.guidebotPlacementNote.isNotBlank()) {
                rowJson.put("guidebot_placement_note", row.guidebotPlacementNote)
            }
            if (row.guidebotNote.isNotBlank()) rowJson.put("guidebot_note", row.guidebotNote)
            if (row.routeProblem.isNotBlank()) rowJson.put("route_problem", row.routeProblem)
            if (row.routeNote.isNotBlank()) rowJson.put("route_note", row.routeNote)
            if (row.problems.isNotEmpty()) rowJson.put("problems", JSONArray(row.problems))
            val notes =
                (row.notes + listOf(row.routeNote, row.guidebotPlacementNote, row.guidebotNote))
                    .filter { it.isNotBlank() }
                    .distinct()
            if (notes.isNotEmpty()) rowJson.put("notes", JSONArray(notes))
            if (row.status != "ok") rowJson.put("status", row.status)
            levels.put(rowJson)
        }
        val json =
            JSONObject()
                .put("status", result.status)
                .put("source", result.source)
                .put("game", result.game)
                .put("mission_name", result.missionName)
                .put("mission_filename", result.missionFilename)
                .apply {
                    if (result.coopStarts.isNotBlank()) put("coop_starts", result.coopStarts)
                    if (result.musicTracks.isNotEmpty()) {
                        put(
                            "music_tracks",
                            JSONArray().apply {
                                result.musicTracks.forEach { track ->
                                    put(
                                        JSONObject()
                                            .put("slot_index", track.slotIndex)
                                            .put("slot_kind", track.slotKind)
                                            .put("filename", track.filename)
                                            .put("format", track.format)
                                            .put("metadata_source_filename", track.metadata.metadata_source_filename)
                                            .put("inherited_from_midi", track.metadata.inherited_from_midi)
                                            .put("parse_status", track.metadata.parse_status)
                                            .put("smf_format", track.metadata.smf_format)
                                            .put("track_count", track.metadata.track_count)
                                            .put("time_division", track.metadata.time_division)
                                            .put("title", track.metadata.title)
                                            .put("composer", track.metadata.composer)
                                            .put("display_name", track.metadata.display_name)
                                            .put("metadata_truncated", track.metadata.metadata_truncated)
                                            .put(
                                                "text_events",
                                                JSONArray().apply {
                                                    track.metadata.text_events.forEach { event ->
                                                        put(
                                                            JSONObject()
                                                                .put("track_index", event.track_index)
                                                                .put("type", event.type)
                                                                .put("text", event.text),
                                                        )
                                                    }
                                                },
                                            ),
                                    )
                                }
                            },
                        )
                    }
                }.put("level_count", result.levels.size)
                .put("levels", levels)
        if (result.problems.isNotEmpty()) json.put("problems", JSONArray(result.problems))
        if (result.diagnostics.isNotEmpty()) json.put("diagnostics", JSONArray(result.diagnostics))
        return json
    }

    private fun levelMetadataRouteStepsJson(steps: List<LevelMetadataRouteStep>): JSONArray {
        val array = JSONArray()
        steps.forEach { step ->
            val item =
                JSONObject()
                    .put("index", step.index)
                    .put("kind", step.kind)
            if (step.activationKind.isNotBlank()) item.put("activation_kind", step.activationKind)
            if (step.label.isNotBlank()) item.put("label", step.label)
            if (step.seg >= 0) item.put("seg", step.seg)
            if (step.side >= 0) item.put("side", step.side)
            if (step.wall >= 0) item.put("wall", step.wall)
            step.labelPosition?.let { pos ->
                item.put(
                    "label_pos",
                    JSONObject()
                        .put("x", pos.x)
                        .put("y", pos.y)
                        .put("z", pos.z),
                )
            }
            if (step.distance > 0.0) item.put("distance", step.distance)
            if (step.key.isNotBlank()) item.put("key", step.key)
            if (!step.calculated) item.put("calculated", false)
            if (step.canBeBypassed) item.put("can_be_bypassed", true)
            if (step.keyCarrierObjnum >= 0) item.put("key_carrier_objnum", step.keyCarrierObjnum)
            if (step.trigger >= 0) item.put("trigger", step.trigger)
            if (step.triggerTypeId >= 0) item.put("trigger_type_id", step.triggerTypeId)
            if (step.triggerType.isNotBlank()) item.put("trigger_type", step.triggerType)
            if (step.opens.isNotEmpty()) {
                val opens = JSONArray()
                step.opens.forEach { open ->
                    val openJson = JSONObject()
                    if (open.seg >= 0) openJson.put("seg", open.seg)
                    if (open.side >= 0) openJson.put("side", open.side)
                    if (open.wall >= 0) openJson.put("wall", open.wall)
                    opens.put(openJson)
                }
                item.put("opens", opens)
            }
            array.put(item)
        }
        return array
    }

    private fun levelMetadataReplacementsJson(replacements: List<LevelMetadataReplacement>): JSONArray {
        val array = JSONArray()
        replacements.forEach { replacement ->
            array.put(
                JSONObject()
                    .put("kind", replacement.kind)
                    .put("label", replacement.label)
                    .put("base_game", replacement.baseGame)
                    .put("mod", replacement.mod),
            )
        }
        return array
    }

    private fun levelMetadataReplacementGroupsJson(groups: List<LevelMetadataReplacementGroup>): JSONArray {
        val array = JSONArray()
        groups.forEach { group ->
            val items = JSONArray()
            group.items.forEach { item ->
                val fields = JSONArray()
                item.fields.forEach { field ->
                    fields.put(
                        JSONObject()
                            .put("kind", field.kind)
                            .put("label", field.label)
                            .put("base_game", field.baseGame)
                            .put("mod", field.mod)
                            .put("base_game_text", field.baseGameText)
                            .put("mod_text", field.modText)
                            .put("format", field.format),
                    )
                }
                items.put(
                    JSONObject()
                        .put("kind", item.kind)
                        .put("number", item.number)
                        .put("label", item.label)
                        .put("summary", item.summary)
                        .put("fields", fields),
                )
            }
            array.put(
                JSONObject()
                    .put("kind", group.kind)
                    .put("label", group.label)
                    .put("summary", group.summary)
                    .put("items", items),
            )
        }
        return array
    }

    private fun writeMissionZipImportAutomationResult(
        label: String,
        mod: ModManager.ModInfo,
        scan: MissionZip.ScanResult?,
    ) {
        val file = File(context.filesDir, "mission_zip_import_${safeAutomationLabel(label)}.json")
        file.writeText(
            JSONObject()
                .put("status", "ok")
                .put("mod", modJson(mod))
                .put("mission_sets", missionSetArrayJson(scan))
                .toString(2) + "\n",
            Charsets.UTF_8,
        )
    }

    private fun writeLevelPreviewSmokeSelection(
        seed: Long,
        label: String,
        candidateCount: Int,
        target: LevelMetadataTarget,
        row: LevelMetadataLevelRow,
        request: LevelPreviewLaunchRequest,
    ) {
        val output = File(context.filesDir, LEVEL_PREVIEW_SMOKE_SELECTION_FILE)
        val temporary = File(context.filesDir, "$LEVEL_PREVIEW_SMOKE_SELECTION_FILE.tmp")
        val json =
            JSONObject()
                .put("schema", "dxx-level-preview-smoke-selection-v1")
                .put("seed", seed)
                .put("label", label)
                .put("candidate_level_count", candidateCount)
                .put("source", target.displayName)
                .put("game", target.game)
                .put("mission_name", target.missionName.orEmpty())
                .put("mission_filename", target.missionFilename.orEmpty())
                .put("level_num", row.levelNum)
                .put("secret", row.secret)
                .put("level_name", row.levelName)
                .put("level_file", row.levelFile)
                .put(
                    "request_id",
                    request.requestFile.parentFile
                        ?.name
                        .orEmpty(),
                )
        temporary.writeText(json.toString(2) + "\n", Charsets.UTF_8)
        Os.rename(temporary.absolutePath, output.absolutePath)
    }

    private fun writeRobotPreviewSmokeSelection(
        seed: Long,
        label: String,
        candidateCount: Int,
        selected: Triple<LevelMetadataTarget, LevelMetadataLevelRow, LevelMetadataReplacementItem>,
        request: RobotPreviewLaunchRequest,
        robotLabel: String,
    ) {
        val output = File(context.filesDir, ROBOT_PREVIEW_SMOKE_SELECTION_FILE)
        val temporary = File(context.filesDir, "$ROBOT_PREVIEW_SMOKE_SELECTION_FILE.tmp")
        val json =
            JSONObject()
                .put("schema", "dxx-robot-preview-smoke-selection-v1")
                .put("seed", seed)
                .put("label", label)
                .put("candidate_robot_count", candidateCount)
                .put("source", selected.first.displayName)
                .put("game", selected.first.game)
                .put("level_num", selected.second.levelNum)
                .put("level_name", selected.second.levelName)
                .put("level_file", selected.second.levelFile)
                .put("robot_number", selected.third.number)
                .put("robot_label", robotLabel)
                .put(
                    "request_id",
                    request.requestFile.parentFile
                        ?.name
                        .orEmpty(),
                )
        temporary.writeText(json.toString(2) + "\n", Charsets.UTF_8)
        Os.rename(temporary.absolutePath, output.absolutePath)
    }

    private fun writeBaseRobotPreviewSmokeSelection(
        game: String,
        robotNumber: Int,
        robotLabel: String,
        request: RobotPreviewLaunchRequest,
    ) {
        val output = File(context.filesDir, ROBOT_PREVIEW_SMOKE_SELECTION_FILE)
        val temporary = File(context.filesDir, "$ROBOT_PREVIEW_SMOKE_SELECTION_FILE.tmp")
        val json =
            JSONObject()
                .put("schema", "dxx-robot-preview-smoke-selection-v1")
                .put("source", "base_game")
                .put("game", game)
                .put("level_num", 0)
                .put("level_file", "")
                .put("robot_number", robotNumber)
                .put("robot_label", robotLabel)
                .put(
                    "request_id",
                    request.requestFile.parentFile
                        ?.name
                        .orEmpty(),
                )
        temporary.writeText(json.toString(2) + "\n", Charsets.UTF_8)
        Os.rename(temporary.absolutePath, output.absolutePath)
    }

    private fun modJson(mod: ModManager.ModInfo): JSONObject =
        JSONObject()
            .put("filename", mod.filename)
            .put("display_name", mod.displayName)
            .put("enabled", mod.enabled)
            .put("size_bytes", mod.sizeBytes)
            .put("game", mod.game)
            .put("order", mod.order)
            .put("kind", mod.kind)
            .put("category", mod.category ?: JSONObject.NULL)
            .put("mission_title", mod.missionTitle ?: JSONObject.NULL)
            .put("import_mode", mod.importMode ?: JSONObject.NULL)

    private fun missionSetArrayJson(scan: MissionZip.ScanResult?): JSONArray {
        val array = JSONArray()
        if (scan == null) return array
        val sets = scan.missionSets.ifEmpty { listOf(MissionZip.MissionSet(scan.mission, scan.constituents)) }
        sets.forEach { set ->
            array.put(
                JSONObject()
                    .put("display_name", set.mission.displayName)
                    .put("mission_path", set.mission.path)
                    .put("game", set.mission.game)
                    .put("normal_level_count", set.mission.levelNames.size)
                    .put("secret_level_count", set.mission.secretLevelNames.size)
                    .put(
                        "hog_files",
                        JSONArray(set.constituents.filter { it.name.endsWith(".hog", true) }.map { it.name }),
                    ).put(
                        "descriptor_files",
                        JSONArray(
                            set.constituents.filter { GameFileFormats.isMissionDescriptor(it.name) }.map { it.name },
                        ),
                    ),
            )
        }
        return array
    }

    private fun safeAutomationLabel(label: String): String =
        label
            .ifBlank { "metadata" }
            .lowercase(Locale.US)
            .replace(Regex("[^a-z0-9._-]+"), "_")
            .trim('_')
            .ifBlank { "metadata" }

    /** Navigate dot-path keys like "d2.ready" or "audio_sources[0].disc_id". */
    private fun resolveJsonPath(
        root: JSONObject,
        path: String,
    ): String? {
        var cur: Any = root
        for (seg in path.split(".")) {
            val bracket = seg.indexOf('[')
            val key = if (bracket >= 0) seg.substring(0, bracket) else seg
            val idx = if (bracket >= 0) seg.substring(bracket + 1, seg.indexOf(']')).toIntOrNull() else null

            if (key.isNotEmpty()) {
                cur = (cur as? JSONObject)?.opt(key) ?: return null
            }
            if (idx != null) {
                cur = (cur as? JSONArray)?.opt(idx) ?: return null
            }
        }
        return when (cur) {
            is Boolean -> {
                cur.toString()
            }

            is Number -> {
                if (cur.toDouble() == cur.toLong().toDouble()) {
                    cur.toLong().toString()
                } else {
                    cur.toString()
                }
            }

            is String -> {
                cur
            }

            JSONObject.NULL -> {
                "null"
            }

            else -> {
                cur.toString()
            }
        }
    }

    private fun fail(reason: String) {
        Log.e(TAG, "SCRIPT_RESULT: FAIL at step ${currentStep + 1}/$totalSteps -- $reason")
        writeResult("FAIL", reason)
        running = false
    }

    private fun writeResult(
        result: String,
        reason: String?,
    ) {
        val elapsed = System.currentTimeMillis() - startTimeMs
        val f = File(context.filesDir, "automation_result.json")
        val json = JSONObject()
        json.put("result", result)
        json.put("run_id", runId)
        val stepsCompleted = if (result == "PASS") currentStep else currentStep + 1
        json.put("steps_completed", stepsCompleted.coerceIn(0, totalSteps))
        json.put("total_steps", totalSteps)
        json.put("elapsed_ms", elapsed)
        if (reason != null) json.put("reason", reason)
        val temp = File(context.filesDir, "automation_result.json.tmp")
        temp.writeText(json.toString() + "\n")
        Os.rename(temp.absolutePath, f.absolutePath)
        activity.clearActiveAutomationRunId(runId)
    }

    private fun removeStaleResult() {
        File(context.filesDir, "automation_result.json").delete()
        File(context.filesDir, "automation_result.json.tmp").delete()
    }
}
