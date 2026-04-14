package com.dxxredux.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Rect
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.SystemClock
import android.provider.DocumentsContract
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
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
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import com.dxxredux.app.multiplayer.GameLaunchInfo
import com.dxxredux.app.multiplayer.MatchmakingService
import com.dxxredux.app.multiplayer.MatchmakingStateHolder
import com.dxxredux.app.multiplayer.NetworkConstants
import com.dxxredux.app.multiplayer.PlayGamesAuth
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.apache.commons.compress.archivers.sevenz.SevenZFile
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.FileWriter
import java.net.HttpURLConnection
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipInputStream

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
    /** Incremented in onResume so Compose re-checks file status. */
    private val refreshTrigger = mutableIntStateOf(0)

    // ── Setup-screen introspection ──────────────────────────────────────
    //   adb shell am broadcast -a com.dxxredux.SETUP_INTROSPECT
    //   adb shell run-as com.dxxredux.app cat files/setup_introspect.json
    private val introspectReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                writeIntrospectJson()
            }
        }

    // ── Setup-screen command API ────────────────────────────────────────
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch --es game d1
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command create_set --es name "my set"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command switch_set --es name "my set"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command clear_set --es name "default"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_gog --es path /sdcard/setup_descent2.exe
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_sow --es path /sdcard/descent2.sow
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_cd --es cue_path /sdcard/disc.cue --es bin_path /sdcard/disc.bin
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_iso --es iso_path /sdcard/disc.iso
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_files --es path /sdcard/DESCENT2.HOG
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_default_config
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_autoselect --es game d2 --es primary "8,9,7,6,5,4,3,2,1,0,255" --es secondary "9,8,4,3,1,5,0,255,7,6,2"
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_engine_prefs --ei cockpit_mode 2 --ez auto_leveling false
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_midi_play --ei source 0 --ei track 2
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_midi_stop
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_cd_play --ei source 0 --ei track 2
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command music_cd_stop
    private var gameRunningFlag = false

    /** Check if the :game process is alive (game engine still running).
     *  On Android 5.1+ runningAppProcesses returns only same-uid processes,
     *  which is exactly what we need -- :game shares our uid. */
    private fun isGameProcessAlive(): Boolean {
        val am = getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
        return am.runningAppProcesses?.any { it.processName == "$packageName:game" } == true
    }

    /** Guard against double-launch of multiplayer game (auto-launch from
     *  LobbyScreen LaunchedEffect + explicit launch_game broadcast). Two
     *  rapid startActivity calls create two MainActivity instances in the
     *  :game process, causing a FORTIFY pthread_mutex crash. */
    private var mpGameLaunching = false

    /** True if LAN discovery was active before game launch; used to auto-resume on return */
    private var wasLanDiscoveringBeforeLaunch = false

    // ── Launcher script automation (debug builds only) ──────────────────
    //   adb shell am broadcast -a com.dxxredux.SETUP_AUTOMATE \
    //     --es script files/test.json5
    // Scripts can alternate between launcher and game phases via
    // enter_launcher / enter_game steps in a single JSON5 array.
    private var launcherExecutor: LauncherScriptExecutor? = null

    /** Accessible button discovered by walking the Compose accessibility tree. */
    data class ButtonInfo(
        val text: String,
        val enabled: Boolean,
        val centerX: Float,
        val centerY: Float,
        val width: Float,
        val height: Float,
    )

    /**
     * Walk the Compose accessibility node provider to discover all interactive
     * elements (buttons, chips, checkboxes) with their text, enabled state, and
     * screen-pixel bounds. Zero annotation required -- Compose auto-generates
     * accessibility nodes for every semantics-bearing composable.
     *
     * Compose assigns sequential integer IDs to semantics nodes starting from 1.
     * We scan a range and collect nodes that are clickable or checkable with
     * non-empty text and non-zero bounds.
     */
    fun collectAccessibleButtons(): List<ButtonInfo> {
        val root = window.decorView
        val composeView = findComposeView(root)
        if (composeView == null) {
            Log.w("DXX-Buttons", "No ComposeView found in view tree")
            return emptyList()
        }
        val provider = composeView.accessibilityNodeProvider
        if (provider == null) {
            Log.w("DXX-Buttons", "ComposeView has no accessibility node provider")
            return emptyList()
        }

        // Collect text-bearing nodes and clickable nodes separately,
        // then match by spatial containment (text bounds inside clickable bounds).
        data class TextNode(
            val text: String,
            val bounds: Rect,
        )

        data class ClickableNode(
            val enabled: Boolean,
            val bounds: Rect,
        )

        val textNodes = mutableListOf<TextNode>()
        val clickableNodes = mutableListOf<ClickableNode>()

        // Use the Compose semantics tree to find the ID range, then do a
        // sequential scan from 0 to max+500. This catches everything
        // including LazyColumn items and their child text nodes, while
        // being much faster than the full 0..16383 scan.
        val semanticsIds = collectSemanticsNodeIds(composeView)
        val maxScanId =
            if (semanticsIds.isNotEmpty()) {
                semanticsIds.max() + 500
            } else {
                16383
            }
        for (id in -1..maxScanId) {
            val info = provider.createAccessibilityNodeInfo(id) ?: continue
            val bounds = Rect()
            info.getBoundsInScreen(bounds)
            if (bounds.width() > 0 && bounds.height() > 0) {
                info.text?.toString()?.let { t ->
                    if (t.isNotEmpty()) textNodes.add(TextNode(t, Rect(bounds)))
                }
                if (info.isClickable || info.isCheckable) {
                    clickableNodes.add(ClickableNode(info.isEnabled, Rect(bounds)))
                }
            }
            info.recycle()
        }

        // Match: text belongs to the smallest clickable node that contains it
        val buttons =
            clickableNodes.mapNotNull { click ->
                val contained = textNodes.filter { click.bounds.contains(it.bounds) }
                if (contained.isEmpty()) return@mapNotNull null
                val label = contained.joinToString(" ") { it.text }
                ButtonInfo(
                    text = label,
                    enabled = click.enabled,
                    centerX = (click.bounds.left + click.bounds.right) / 2f,
                    centerY = (click.bounds.top + click.bounds.bottom) / 2f,
                    width = click.bounds.width().toFloat(),
                    height = click.bounds.height().toFloat(),
                )
            }
        Log.i(
            "DXX-Buttons",
            "Scan: ${textNodes.size} text, ${clickableNodes.size} clickable, ${buttons.size} matched, range=0..$maxScanId (semantics=${semanticsIds.size})",
        )
        return buttons
    }

    private fun findComposeView(view: View): View? {
        if (view.accessibilityNodeProvider != null &&
            view.javaClass.simpleName.contains("Compose")
        ) {
            return view
        }
        if (view is ViewGroup) {
            for (i in 0 until view.childCount) {
                val result = findComposeView(view.getChildAt(i))
                if (result != null) return result
            }
        }
        return null
    }

    // Collect all semantics node IDs from the Compose semantic tree via
    // reflection on Jetpack library classes (not restricted by hidden API).
    // This discovers LazyColumn items that sequential ID scanning misses,
    // because lazy items can have IDs well above the 0..16383 scan range.
    private fun collectSemanticsNodeIds(composeView: View): Set<Int> {
        return try {
            val getOwner = composeView.javaClass.getMethod("getSemanticsOwner")
            val owner = getOwner.invoke(composeView) ?: return emptySet()
            val getRoot = owner.javaClass.getMethod("getRootSemanticsNode")
            val rootNode = getRoot.invoke(owner) ?: return emptySet()
            val getId = rootNode.javaClass.getMethod("getId")
            val getChildren = rootNode.javaClass.getMethod("getChildren")
            val ids = mutableSetOf<Int>()
            val stack = ArrayDeque<Any>()
            stack.add(rootNode)
            while (stack.isNotEmpty()) {
                val node = stack.removeFirst()
                ids.add(getId.invoke(node) as Int)
                @Suppress("UNCHECKED_CAST")
                val children = getChildren.invoke(node) as? List<Any> ?: emptyList()
                stack.addAll(children)
            }
            ids
        } catch (e: Exception) {
            Log.w("DXX-Buttons", "collectSemanticsNodeIds: ${e.message}")
            emptySet()
        }
    }

    /** Find a button by case-insensitive substring match on its text. */
    fun findButtonByText(text: String): ButtonInfo? {
        val buttons = collectAccessibleButtons()
        val lower = text.lowercase()
        // Prefer exact match, fall back to substring
        return buttons.find { it.text.lowercase() == lower }
            ?: buttons.find { it.text.lowercase().contains(lower) }
    }

    /** Inject a real tap (ACTION_DOWN + delay + ACTION_UP) at screen coordinates. */
    suspend fun injectTapAt(
        screenX: Float,
        screenY: Float,
    ) {
        withContext(kotlinx.coroutines.Dispatchers.Main) {
            val decorView = window.decorView
            val loc = IntArray(2)
            decorView.getLocationOnScreen(loc)
            val localX = screenX - loc[0]
            val localY = screenY - loc[1]
            val downTime = SystemClock.uptimeMillis()
            val down =
                MotionEvent.obtain(
                    downTime,
                    downTime,
                    MotionEvent.ACTION_DOWN,
                    localX,
                    localY,
                    0,
                )
            decorView.dispatchTouchEvent(down)
            down.recycle()
            kotlinx.coroutines.delay(50)
            val upTime = SystemClock.uptimeMillis()
            val up =
                MotionEvent.obtain(
                    downTime,
                    upTime,
                    MotionEvent.ACTION_UP,
                    localX,
                    localY,
                    0,
                )
            decorView.dispatchTouchEvent(up)
            up.recycle()
        }
    }

    /**
     * Perform a click on a Compose button via its accessibility node.
     * More reliable than touch injection because it bypasses coordinate
     * mapping and uses the semantic click action directly.
     * Returns true if the click was performed.
     */
    fun performAccessibilityClick(buttonText: String): Boolean {
        val root = window.decorView
        val composeView = findComposeView(root) ?: return false
        val provider = composeView.accessibilityNodeProvider ?: return false

        data class TextNode(
            val text: String,
            val bounds: Rect,
        )

        data class ClickNode(
            val id: Int,
            val bounds: Rect,
        )

        val textNodes = mutableListOf<TextNode>()
        val clickNodes = mutableListOf<ClickNode>()

        val semanticsIds = collectSemanticsNodeIds(composeView)
        val maxScanId =
            if (semanticsIds.isNotEmpty()) {
                semanticsIds.max() + 500
            } else {
                16383
            }
        for (id in -1..maxScanId) {
            val info = provider.createAccessibilityNodeInfo(id) ?: continue
            val bounds = Rect()
            info.getBoundsInScreen(bounds)
            if (bounds.width() > 0 && bounds.height() > 0) {
                info.text?.toString()?.let { t ->
                    if (t.isNotEmpty()) textNodes.add(TextNode(t, Rect(bounds)))
                }
                if (info.isClickable) clickNodes.add(ClickNode(id, Rect(bounds)))
            }
            info.recycle()
        }

        val lower = buttonText.lowercase()
        for (click in clickNodes) {
            val contained = textNodes.filter { click.bounds.contains(it.bounds) }
            val label = contained.joinToString(" ") { it.text }
            if (label.lowercase() == lower || label.lowercase().contains(lower)) {
                return provider.performAction(
                    click.id,
                    android.view.accessibility.AccessibilityNodeInfo.ACTION_CLICK,
                    null,
                )
            }
        }
        return false
    }

    /** Hide the soft keyboard if it's showing. */
    private fun dismissKeyboard() {
        val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as? android.view.inputmethod.InputMethodManager
        val focused = currentFocus ?: window.decorView
        imm?.hideSoftInputFromWindow(focused.windowToken, 0)
        // Clear focus so the keyboard doesn't reappear on next recomposition
        focused.clearFocus()
    }

    /** Inject a scroll-down swipe gesture (finger moves upward to scroll content down). */
    suspend fun scrollDown() {
        withContext(kotlinx.coroutines.Dispatchers.Main) {
            val decorView = window.decorView
            val centerX = decorView.width / 2f
            val startY = decorView.height * 0.75f
            val endY = decorView.height * 0.25f
            val downTime = SystemClock.uptimeMillis()
            val down =
                MotionEvent.obtain(
                    downTime,
                    downTime,
                    MotionEvent.ACTION_DOWN,
                    centerX,
                    startY,
                    0,
                )
            decorView.dispatchTouchEvent(down)
            down.recycle()
            kotlinx.coroutines.delay(30)
            val mid =
                MotionEvent.obtain(
                    downTime,
                    SystemClock.uptimeMillis(),
                    MotionEvent.ACTION_MOVE,
                    centerX,
                    (startY + endY) / 2f,
                    0,
                )
            decorView.dispatchTouchEvent(mid)
            mid.recycle()
            kotlinx.coroutines.delay(30)
            val end =
                MotionEvent.obtain(
                    downTime,
                    SystemClock.uptimeMillis(),
                    MotionEvent.ACTION_MOVE,
                    centerX,
                    endY,
                    0,
                )
            decorView.dispatchTouchEvent(end)
            end.recycle()
            kotlinx.coroutines.delay(30)
            val up =
                MotionEvent.obtain(
                    downTime,
                    SystemClock.uptimeMillis(),
                    MotionEvent.ACTION_UP,
                    centerX,
                    endY,
                    0,
                )
            decorView.dispatchTouchEvent(up)
            up.recycle()
        }
    }

    private val automateSetupReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                if (!BuildConfig.DEBUG) return
                val scriptPath = intent?.getStringExtra("script") ?: return
                val resolved =
                    if (scriptPath.startsWith("/")) {
                        scriptPath
                    } else {
                        filesDir.absolutePath + "/" + scriptPath
                    }
                Log.i("DXX-Setup", "SETUP_AUTOMATE: loading $resolved")
                val executor =
                    LauncherScriptExecutor(this@SetupActivity) { game, path, startStep ->
                        launchGameForAutomation(game, path, startStep)
                    }
                launcherExecutor = executor
                kotlinx.coroutines.MainScope().launch {
                    try {
                        executor.execute(resolved, 0)
                    } catch (e: Exception) {
                        Log.e("DXX-Setup", "Launcher automation failed: ${e.message}", e)
                    }
                }
            }
        }

    private fun launchGameForAutomation(
        game: String,
        scriptPath: String,
        startStep: Int,
    ) {
        FileSetManager(filesDir).writeActiveSetPath()
        AudioSourceManager(filesDir).writePlaylist(contentResolver)
        ModManager(filesDir).writeEnabledModPaths(game)
        writeInitialGameConfig()
        writeMusicConfigForLaunch()
        val intent = Intent(this, MainActivity::class.java)
        intent.putExtra("game", game)
        intent.putExtra("automation_script", scriptPath)
        intent.putExtra("automation_start_step", startStep)
        startActivity(intent)
    }

    private fun requestSetupRefresh() {
        runOnUiThread { refreshTrigger.intValue++ }
    }

    private val commandReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(
                ctx: Context?,
                intent: Intent?,
            ) {
                val cmd = intent?.getStringExtra("command") ?: return
                when (cmd) {
                    "launch" -> {
                        if (gameRunningFlag || isGameProcessAlive()) {
                            finish()
                        } else {
                            val game = intent.getStringExtra("game") ?: "d2"
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val hogFile = if (game == "d1") "descent.hog" else "descent2.hog"
                            val hasData =
                                setDir.listFiles()?.any {
                                    it.name.equals(hogFile, ignoreCase = true)
                                } ?: false
                            if (!hasData) {
                                Log.e("DXX-Setup", "Cannot launch $game: $hogFile not found in ${setDir.absolutePath}")
                                return
                            }
                            fsm.writeActiveSetPath()
                            AudioSourceManager(filesDir).writePlaylist(contentResolver)
                            ModManager(filesDir).writeEnabledModPaths(game)
                            writeInitialGameConfig()
                            writeMusicConfigForLaunch()
                            val launchIntent = Intent(this@SetupActivity, MainActivity::class.java)
                            launchIntent.putExtra("game", game)
                            startActivity(launchIntent)
                        }
                    }
                    "patch_pilots" -> {
                        val n = patchPilotsFromConfig()
                        Log.i("DXX-Setup", "patch_pilots: patched $n file(s)")
                    }
                    "reset_controls" -> {
                        val game = intent.getStringExtra("game")
                        var n = 0
                        if (game == null || game == "d2") {
                            n += NativePilotPatcher.nativeResetToDefaults(filesDir.absolutePath, "d2")
                        }
                        if (game == null || game == "d1") {
                            n += NativePilotPatcher.nativeResetToDefaults(filesDir.absolutePath, "d1")
                        }
                        Log.i("DXX-Setup", "reset_controls: reset $n file(s) to engine defaults")
                    }
                    "controller_introspect" -> {
                        val game = intent.getStringExtra("game")
                        writeControllerIntrospectJson(game)
                        Log.i("DXX-Setup", "controller_introspect: written (game=${game ?: "d2"})")
                    }
                    "write_default_config" -> {
                        File(filesDir, "controller_config.json").delete()
                        writeDefaultControllerConfig()
                    }
                    "write_engine_prefs" -> {
                        val cockpitMode = intent.getIntExtra("cockpit_mode", 0)
                        val autoLeveling = intent.getBooleanExtra("auto_leveling", true)
                        val n =
                            NativePilotPreferences.writeEnginePrefsToAll(
                                filesDir.absolutePath,
                                cockpitMode,
                                autoLeveling,
                            )
                        Log.i(
                            "DXX-Setup",
                            "write_engine_prefs: patched $n file(s) (cockpit_mode=$cockpitMode auto_leveling=$autoLeveling)",
                        )
                    }
                    "create_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        val fsm = FileSetManager(filesDir)
                        try {
                            val dir = fsm.createSet(name)
                            Log.i("DXX-Setup", "create_set '$name': ${dir.absolutePath}")
                            requestSetupRefresh()
                        } catch (e: IllegalArgumentException) {
                            Log.i("DXX-Setup", "create_set '$name': already exists")
                        }
                    }
                    "switch_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        val fsm = FileSetManager(filesDir)
                        fsm.setActive(name)
                        fsm.writeActiveSetPath()
                        Log.i("DXX-Setup", "switch_set '$name': ok")
                        requestSetupRefresh()
                    }
                    "clear_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        val fsm = FileSetManager(filesDir)
                        val dir = fsm.getSetDir(name)
                        val count = dir.listFiles()?.count { it.isFile && it.delete() } ?: 0
                        Log.i("DXX-Setup", "clear_set '$name': deleted $count file(s)")
                        requestSetupRefresh()
                    }
                    "import_gog" -> {
                        val path = intent.getStringExtra("path") ?: return
                        val audio = intent.getBooleanExtra("include_audio", true)
                        Thread {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val count = GogImportBridge.extractFiles(path, setDir.absolutePath, null, audio)
                            val srcManager = AudioSourceManager(filesDir)
                            if (audio && count > 0 && findGogPair(setDir) != null) {
                                enableRedbookInConfig(filesDir, this@SetupActivity)
                                registerGogAudioSource(srcManager, filesDir, setDir, this@SetupActivity)
                            }
                            Log.i("DXX-Setup", "import_gog '$path' -> $count file(s) to ${setDir.name} (audio=$audio)")
                            requestSetupRefresh()
                        }.start()
                    }
                    "import_sow" -> {
                        val path = intent.getStringExtra("path") ?: return
                        Thread {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val count = DiscImportBridge.extractSowFiles(path, setDir.absolutePath, null)
                            Log.i("DXX-Setup", "import_sow '$path' -> $count file(s) to ${setDir.name}")
                            requestSetupRefresh()
                        }.start()
                    }
                    "import_cd" -> {
                        val cuePath = intent.getStringExtra("cue_path") ?: return
                        val binPath = intent.getStringExtra("bin_path") ?: return
                        val audio = intent.getBooleanExtra("include_audio", true)
                        Thread {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val count =
                                importDiscImageFromPath(
                                    filesDir = filesDir,
                                    setDir = setDir,
                                    context = this@SetupActivity,
                                    cuePath = cuePath,
                                    binPath = binPath,
                                    includeAudio = audio,
                                )
                            Log.i(
                                "DXX-Setup",
                                "import_cd cue='$cuePath' bin='$binPath' -> $count file(s) to ${setDir.name} (audio=$audio)",
                            )
                            requestSetupRefresh()
                        }.start()
                    }
                    "import_iso" -> {
                        val path = intent.getStringExtra("iso_path") ?: return
                        Thread {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val count = importIsoImageFromPath(setDir, path)
                            Log.i(
                                "DXX-Setup",
                                "import_iso iso='$path' -> $count file(s) to ${setDir.name}",
                            )
                            requestSetupRefresh()
                        }.start()
                    }
                    "import_files" -> {
                        val path = intent.getStringExtra("path") ?: return
                        val fsm = FileSetManager(filesDir)
                        val setDir = fsm.getSetDir(fsm.getActive())
                        val src = File(path)
                        if (src.isFile) {
                            val destDir =
                                if (src.name.lowercase().endsWith(".dem")) {
                                    File(setDir, "demos").also { it.mkdirs() }
                                } else {
                                    setDir
                                }
                            src.copyTo(File(destDir, src.name), overwrite = true)
                            Log.i("DXX-Setup", "import_files: copied ${src.name} to ${destDir.name}")
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
                        Log.i("DXX-Setup", "write_autoselect ($game): patched $count file(s)")
                    }
                    "music_midi_play" -> {
                        val srcIdx = intent.getIntExtra("source", 0)
                        val trkIdx = intent.getIntExtra("track", 0)
                        Thread {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val result = MidiEnumerationBridge.enumerateTracks(setDir.absolutePath)
                            val src = result.sources.getOrNull(srcIdx)
                            if (src == null) {
                                Log.w(
                                    "DXX-Setup",
                                    "music_midi_play: source $srcIdx not found (${result.sources.size} available)",
                                )
                                return@Thread
                            }
                            val track = src.tracks.getOrNull(trkIdx)
                            if (track == null) {
                                Log.w(
                                    "DXX-Setup",
                                    "music_midi_play: track $trkIdx not found in ${src.label} (${src.tracks.size} available)",
                                )
                                return@Thread
                            }
                            MidiPreviewBridge.init(this@SetupActivity)
                            val data = MidiPreviewBridge.readHogEntry(src.hog, track.filename)
                            if (data == null) {
                                Log.w("DXX-Setup", "music_midi_play: failed to read ${track.filename} from ${src.hog}")
                                return@Thread
                            }
                            val isHmp = track.filename.lowercase().endsWith(".hmp")
                            val sr = MidiPreviewBridge.getNativeSampleRate(this@SetupActivity)
                            MidiPreviewBridge.start(data, isHmp, sr)
                            Log.i("DXX-Setup", "music_midi_play: playing ${track.filename} from ${src.label}")
                        }.start()
                    }
                    "music_midi_stop" -> {
                        MidiPreviewBridge.stop()
                        Log.i("DXX-Setup", "music_midi_stop: stopped")
                    }
                    "music_cd_play" -> {
                        val srcIdx = intent.getIntExtra("source", 0)
                        val trkIdx = intent.getIntExtra("track", 0)
                        Thread {
                            val srcManager = AudioSourceManager(filesDir)
                            val sources = srcManager.getEnabledSources()
                            val src = sources.getOrNull(srcIdx)
                            if (src == null) {
                                Log.w(
                                    "DXX-Setup",
                                    "music_cd_play: source $srcIdx not found (${sources.size} available)",
                                )
                                return@Thread
                            }
                            val binPath = File(filesDir, src.binPaths.first()).absolutePath
                            val cuePath = File(filesDir, src.cuePath).absolutePath
                            val sr = CdPreviewBridge.getNativeSampleRate(this@SetupActivity)
                            val ok = CdPreviewBridge.start(binPath, cuePath, trkIdx, sr)
                            Log.i("DXX-Setup", "music_cd_play: source=${src.discLabel} track=$trkIdx ok=$ok")
                        }.start()
                    }
                    "music_cd_stop" -> {
                        CdPreviewBridge.stop()
                        Log.i("DXX-Setup", "music_cd_stop: stopped")
                    }
                    "add_audio_source" -> {
                        // Test automation: register a BIN/CUE audio source.
                        // bin_path: absolute filesystem path to the BIN file
                        // cue_name: filename of the CUE file (in filesDir)
                        // label: human-readable disc label
                        val binPath = intent.getStringExtra("bin_path") ?: return
                        val cueName = intent.getStringExtra("cue_name") ?: return
                        val label = intent.getStringExtra("label") ?: "Test Disc"
                        val id = intent.getStringExtra("id") ?: "test-${System.currentTimeMillis()}"
                        val srcManager = AudioSourceManager(filesDir)
                        srcManager.addSource(
                            AudioSourceManager.AudioSource(
                                id = id,
                                cuePath = cueName,
                                binPaths = listOf(cueName.replace(".cue", ".bin")),
                                discLabel = label,
                                discId = id,
                                trackCount = 0,
                                audioTrackCount = 0,
                                legacyDiscId = 0,
                                binContentUri = binPath,
                            ),
                        )
                        enableRedbookInConfig(filesDir, this@SetupActivity)
                        Log.i("DXX-Setup", "add_audio_source: id=$id bin=$binPath cue=$cueName")
                    }
                    "clear_audio_sources" -> {
                        val srcManager = AudioSourceManager(filesDir)
                        srcManager.clearAll()
                        File(filesDir, "audio_playlist.json").delete()
                        Log.i("DXX-Setup", "clear_audio_sources: cleared all")
                    }
                    else -> Log.w("DXX-Setup", "Unknown command: $cmd")
                }
            }
        }

    // ── Multiplayer command API (test automation) ────────────────────────
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
                        val gameInfo =
                            JsonObject(
                                mapOf(
                                    "mission" to JsonPrimitive(mission),
                                    "mode" to JsonPrimitive(mode),
                                    "coop_qol" to JsonPrimitive(coopQol),
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
                        writeMpIntrospectJson()
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
                        val hostAddr = intent.getStringExtra("host_addr")
                        val hostPort = intent.getIntExtra("host_port", NetworkConstants.ENGINE_PORT)
                        intent.getStringExtra("callsign")?.let { mpCallsign = it }
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
                        com.dxxredux.app.lobby.LobbyService
                            .startDiscovery(this@SetupActivity, callsign)
                        com.dxxredux.app.lobby.LobbyService
                            .hostLobby(callsign, game, mission, mode, maxPlayers)
                        Log.i("DXX-MP", "lan_host_lobby: hosting as $callsign ($game/$mission/$mode)")
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
                    "dismiss_keyboard" -> {
                        dismissKeyboard()
                        Log.i("DXX-MP", "dismiss_keyboard: done")
                    }
                    "tap_button" -> {
                        val text =
                            intent.getStringExtra("text") ?: run {
                                Log.w("DXX-MP", "tap_button: missing 'text'")
                                return
                            }
                        // Dismiss soft keyboard first -- it can cover buttons
                        dismissKeyboard()
                        // Launch coroutine so we can scroll if needed
                        kotlinx.coroutines.MainScope().launch {
                            // Brief delay after keyboard dismiss for layout to settle
                            kotlinx.coroutines.delay(200)
                            var scrollAttempts = 0
                            val deadline = System.currentTimeMillis() + 5000
                            while (true) {
                                if (performAccessibilityClick(text)) {
                                    Log.i("DXX-MP", "tap_button: tapped \"$text\"")
                                    return@launch
                                }
                                if (System.currentTimeMillis() > deadline) break
                                if (scrollAttempts < 5) {
                                    scrollDown()
                                    scrollAttempts++
                                    kotlinx.coroutines.delay(400)
                                } else {
                                    kotlinx.coroutines.delay(500)
                                }
                            }
                            val available =
                                collectAccessibleButtons()
                                    .joinToString(", ") { "\"${it.text}\"" }
                            Log.w("DXX-MP", "tap_button: \"$text\" not found (available: $available)")
                        }
                    }
                    else -> Log.w("DXX-MP", "Unknown MP command: $cmd")
                }
            }
        }

    // ── Host migration receiver ────────────────────────────────────────
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
                        d2File.exists() && d1File.exists() ->
                            if (d2File.lastModified() >= d1File.lastModified()) d2File else d1File
                        d2File.exists() -> d2File
                        d1File.exists() -> d1File
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
                    Log.i(
                        "DXX-MP",
                        "Host migration: proxy on :$proxyPort, LAN broadcast as $callsign ($game/$mission lvl=$levelNum)",
                    )
                    com.dxxredux.app.lobby.LobbyService
                        .startDiscovery(context, callsign)
                    com.dxxredux.app.lobby.LobbyService
                        .hostLobby(callsign, game, mission, mode, maxPlayers)
                    com.dxxredux.app.lobby.LobbyService
                        .startGame(difficulty, levelNum, coopQol = coopQol, hostPort = proxyPort)
                    // Clean up the migration file
                    file.delete()
                } catch (e: Exception) {
                    Log.e("DXX-MP", "Failed to process host_migration.json", e)
                }
            }
        }

    /** Active download progress visible to introspection. */
    internal val downloadStates = mutableMapOf<String, Int>()

    // ── Controller live-state ───────────────────────────────────────────

    /** Axis values observable by Compose (LX, LY, RX, RY, LT, RT). */
    internal val controllerAxes = FloatArray(6)

    /** D-Pad HAT axis values (hatX, hatY). */
    internal val dpadAxes = FloatArray(2)

    /** Compose-observable axis update counter (increment triggers recompose). */
    internal val axisGeneration = mutableIntStateOf(0)

    /** Currently pressed gamepad buttons (name strings). */
    internal val pressedButtons = mutableStateListOf<String>()

    /** Set to true when the controller config page is shown (needs all button events). */
    internal var controllerConfigActive = false

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK &&
            event.action == MotionEvent.ACTION_MOVE
        ) {
            controllerAxes[0] = event.getAxisValue(MotionEvent.AXIS_X)
            controllerAxes[1] = event.getAxisValue(MotionEvent.AXIS_Y)
            controllerAxes[2] = event.getAxisValue(MotionEvent.AXIS_Z)
            controllerAxes[3] = event.getAxisValue(MotionEvent.AXIS_RZ)
            controllerAxes[4] = event.getAxisValue(MotionEvent.AXIS_LTRIGGER)
            controllerAxes[5] = event.getAxisValue(MotionEvent.AXIS_RTRIGGER)
            dpadAxes[0] = event.getAxisValue(MotionEvent.AXIS_HAT_X)
            dpadAxes[1] = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
            axisGeneration.intValue++
            return true
        }
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
            if (event.action == KeyEvent.ACTION_DOWN) {
                if (name !in pressedButtons) pressedButtons.add(name)
            } else if (event.action == KeyEvent.ACTION_UP) {
                pressedButtons.remove(name)
            }
            // Controller config page needs all button events consumed for its
            // test visualization. Other pages let D-pad and A/B flow through
            // so Compose's focus system can handle navigation.
            if (controllerConfigActive) return true
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

    /**
     * Read controller_config.json and patch all .plr files with its KeySettings.
     * Patches both D1 and D2 pilots using game-specific byte arrays.
     * Returns the number of files patched.
     */
    private fun patchPilotsFromConfig(): Int {
        val cfg = File(filesDir, "controller_config.json")
        if (!cfg.exists()) return 0
        try {
            val json = org.json.JSONObject(cfg.readText())
            val kbArr = json.optJSONArray("key_settings_keyboard") ?: return 0
            val kb = ByteArray(kbArr.length()) { (kbArr.getInt(it) and 0xFF).toByte() }
            val ct = json.optInt("control_type", 1)
            var total = 0
            for (game in arrayOf("d2", "d1")) {
                val joyKey = "key_settings_joystick_$game"
                val jArr = json.optJSONArray(joyKey) ?: continue
                val joy = ByteArray(jArr.length()) { (jArr.getInt(it) and 0xFF).toByte() }
                total +=
                    NativePilotPatcher.nativePatchPilotFiles(
                        filesDir.absolutePath,
                        joy,
                        kb,
                        ct,
                        game,
                    )
            }
            return total
        } catch (e: Exception) {
            Log.e("DXX-Setup", "patchPilotsFromConfig failed", e)
            return 0
        }
    }

    // ── kc_joystick[] metadata for controller introspection ────────
    // IMPORTANT: Mirrors kc_joystick[NUM_JOYSTICK_CONTROLS] in d2/main/kconfig.c.
    // Update both locations together when the joystick control layout changes.
    private data class KcMeta(
        val name: String,
        val type: String,
    )

    // D2: 56 entries matching d2/main/kconfig.c kc_joystick[]
    private val KC_JOY_META_D2 =
        listOf(
            KcMeta("Fire primary", "joy_button"), //  0
            KcMeta("Fire secondary", "joy_button"), //  1
            KcMeta("Accelerate", "joy_button"), //  2
            KcMeta("reverse", "joy_button"), //  3
            KcMeta("Fire flare", "joy_button"), //  4
            KcMeta("Slide on", "joy_button"), //  5
            KcMeta("Slide left", "joy_button"), //  6
            KcMeta("Slide right", "joy_button"), //  7
            KcMeta("Slide up", "joy_button"), //  8
            KcMeta("Slide down", "joy_button"), //  9
            KcMeta("Bank on", "joy_button"), // 10
            KcMeta("Bank left", "joy_button"), // 11
            KcMeta("Bank right", "joy_button"), // 12
            KcMeta("Pitch U/D", "joy_axis"), // 13
            KcMeta("Pitch U/D", "invert"), // 14
            KcMeta("Turn L/R", "joy_axis"), // 15
            KcMeta("Turn L/R", "invert"), // 16
            KcMeta("Slide L/R", "joy_axis"), // 17
            KcMeta("Slide L/R", "invert"), // 18
            KcMeta("Slide U/D", "joy_axis"), // 19
            KcMeta("Slide U/D", "invert"), // 20
            KcMeta("Bank L/R", "joy_axis"), // 21
            KcMeta("Bank L/R", "invert"), // 22
            KcMeta("throttle", "joy_axis"), // 23
            KcMeta("throttle", "invert"), // 24
            KcMeta("REAR VIEW", "joy_button"), // 25
            KcMeta("Drop Bomb", "joy_button"), // 26
            KcMeta("Afterburner", "joy_button"), // 27
            KcMeta("Cycle Primary", "joy_button"), // 28
            KcMeta("Cycle Secondary", "joy_button"), // 29
            KcMeta("Headlight", "joy_button"), // 30
            KcMeta("Fire primary", "joy_button"), // 31 (secondary)
            KcMeta("Fire secondary", "joy_button"), // 32
            KcMeta("Accelerate", "joy_button"), // 33
            KcMeta("reverse", "joy_button"), // 34
            KcMeta("Fire flare", "joy_button"), // 35
            KcMeta("Slide on", "joy_button"), // 36
            KcMeta("Slide left", "joy_button"), // 37
            KcMeta("Slide right", "joy_button"), // 38
            KcMeta("Slide up", "joy_button"), // 39
            KcMeta("Slide down", "joy_button"), // 40
            KcMeta("Bank on", "joy_button"), // 41
            KcMeta("Bank left", "joy_button"), // 42
            KcMeta("Bank right", "joy_button"), // 43
            KcMeta("REAR VIEW", "joy_button"), // 44
            KcMeta("Drop Bomb", "joy_button"), // 45
            KcMeta("Afterburner", "joy_button"), // 46
            KcMeta("Cycle Primary", "joy_button"), // 47
            KcMeta("Cycle Secondary", "joy_button"), // 48
            KcMeta("Headlight", "joy_button"), // 49
            KcMeta("Automap", "joy_button"), // 50
            KcMeta("Automap", "joy_button"), // 51 (secondary)
            KcMeta("Energy->Shield", "joy_button"), // 52
            KcMeta("Energy->Shield", "joy_button"), // 53 (secondary)
            KcMeta("Toggle Bomb", "joy_button"), // 54
            KcMeta("Toggle Bomb", "joy_button"), // 55 (secondary)
        )

    // D1: 48 entries matching d1/main/kconfig.c kc_joystick[]
    // Key differences from D2: no Afterburner/Headlight/Energy->Shield/Toggle Bomb;
    // Automap at 27-28 (not 50-51); Cycle Primary/Secondary at 44-47 (not 28-29);
    // different capitalization on several names.
    private val KC_JOY_META_D1 =
        listOf(
            KcMeta("Fire primary", "joy_button"), //  0
            KcMeta("Fire secondary", "joy_button"), //  1
            KcMeta("Accelerate", "joy_button"), //  2
            KcMeta("Reverse", "joy_button"), //  3
            KcMeta("Fire flare", "joy_button"), //  4
            KcMeta("Slide on", "joy_button"), //  5
            KcMeta("Slide left", "joy_button"), //  6
            KcMeta("Slide right", "joy_button"), //  7
            KcMeta("Slide up", "joy_button"), //  8
            KcMeta("Slide down", "joy_button"), //  9
            KcMeta("Bank on", "joy_button"), // 10
            KcMeta("Bank left", "joy_button"), // 11
            KcMeta("Bank right", "joy_button"), // 12
            KcMeta("Pitch U/D", "joy_axis"), // 13
            KcMeta("Pitch U/D", "invert"), // 14
            KcMeta("Turn L/R", "joy_axis"), // 15
            KcMeta("Turn L/R", "invert"), // 16
            KcMeta("Slide L/R", "joy_axis"), // 17
            KcMeta("Slide L/R", "invert"), // 18
            KcMeta("Slide U/D", "joy_axis"), // 19
            KcMeta("Slide U/D", "invert"), // 20
            KcMeta("Bank L/R", "joy_axis"), // 21
            KcMeta("Bank L/R", "invert"), // 22
            KcMeta("Throttle", "joy_axis"), // 23
            KcMeta("Throttle", "invert"), // 24
            KcMeta("Rear view", "joy_button"), // 25
            KcMeta("Drop bomb", "joy_button"), // 26
            KcMeta("Automap", "joy_button"), // 27
            KcMeta("Automap", "joy_button"), // 28 (secondary)
            KcMeta("Fire primary", "joy_button"), // 29 (secondary)
            KcMeta("Fire secondary", "joy_button"), // 30
            KcMeta("Accelerate", "joy_button"), // 31
            KcMeta("Reverse", "joy_button"), // 32
            KcMeta("Fire flare", "joy_button"), // 33
            KcMeta("Slide on", "joy_button"), // 34
            KcMeta("Slide left", "joy_button"), // 35
            KcMeta("Slide right", "joy_button"), // 36
            KcMeta("Slide up", "joy_button"), // 37
            KcMeta("Slide down", "joy_button"), // 38
            KcMeta("Bank on", "joy_button"), // 39
            KcMeta("Bank left", "joy_button"), // 40
            KcMeta("Bank right", "joy_button"), // 41
            KcMeta("Rear view", "joy_button"), // 42 (secondary)
            KcMeta("Drop bomb", "joy_button"), // 43
            KcMeta("Cycle Primary", "joy_button"), // 44
            KcMeta("Cycle Secondary", "joy_button"), // 45
            KcMeta("Cycle Primary", "joy_button"), // 46 (secondary)
            KcMeta("Cycle Secondary", "joy_button"), // 47 (secondary)
        )

    /**
     * Write controller_introspect.json in the same format as the in-game
     * joystick_controls introspection, but using the launcher's config.
     *
     *   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command controller_introspect --es game d2
     *   adb shell run-as com.dxxredux.app cat files/controller_introspect.json
     */
    private fun writeControllerIntrospectJson(game: String? = null) {
        try {
            val cfg = File(filesDir, "controller_config.json")
            if (!cfg.exists()) {
                Log.w("DXX-Setup", "No controller_config.json to introspect")
                return
            }
            val json = JSONObject(cfg.readText())
            // Select the right metadata and byte array for the requested game
            val gameId = game ?: "d2"
            val meta = if (gameId == "d1") KC_JOY_META_D1 else KC_JOY_META_D2
            val joyArr =
                json.optJSONArray("key_settings_joystick_$gameId")
                    ?: json.optJSONArray("key_settings_joystick")
            val ct = json.optInt("control_type", 1)

            val n = meta.size
            val items = JSONArray()
            var boundCount = 0
            var boundControls = 0
            // Mirror android_apply_gamepad_defaults() fallback: if Slide U/D
            // and Bank L/R axis slots are unset (0xFF), the game fills them
            // with virtual gyro axes 7 and 6 respectively
            val gyroFallback = mapOf(19 to 7, 21 to 6)
            for (i in 0 until n) {
                var value = if (joyArr != null && i < joyArr.length()) joyArr.getInt(i) else 255
                if (value == 255 && i in gyroFallback) value = gyroFallback[i]!!
                // Apply the same normalization as kc_set_controls in the game:
                // BT_INVERT values are clamped to 0 or 1 (any value != 1 becomes 0)
                if (meta[i].type == "invert") {
                    value = if (value == 1) 1 else 0
                }
                val bound = value != 255
                if (bound) boundCount++
                if (bound && meta[i].type != "invert") boundControls++
                val item = JSONObject()
                item.put("index", i)
                item.put("name", meta[i].name)
                item.put("type", meta[i].type)
                item.put("value", value)
                item.put("bound", bound)
                items.put(item)
            }

            val root = JSONObject()
            root.put("source", "launcher")
            val jc = JSONObject()
            jc.put("control_type", ct)
            jc.put("bound_count", boundCount)
            jc.put("bound_controls", boundControls)
            jc.put("total_count", n)
            jc.put("items", items)
            root.put("joystick_controls", jc)

            // Also include the human-readable bindings for reference
            if (json.has("bindings")) root.put("bindings", json.getJSONObject("bindings"))
            if (json.has("inverts")) root.put("inverts", json.getJSONArray("inverts"))

            val outFile = File(filesDir, "controller_introspect.json")
            FileWriter(outFile).use { it.write(root.toString(2)) }
            Log.i("DXX-Setup", "Controller introspect written: ${outFile.absolutePath}")
        } catch (e: Exception) {
            Log.e("DXX-Setup", "Failed to write controller introspect JSON", e)
        }
    }

    private fun launchMultiplayerGame(info: GameLaunchInfo) {
        if (mpGameLaunching) {
            Log.w("DXX-MP", "Game already launching, ignoring duplicate")
            return
        }
        mpGameLaunching = true
        // For LAN hosts, keep the announce broadcast alive so the game
        // remains discoverable; for everyone else, shut down fully
        wasLanDiscoveringBeforeLaunch = com.dxxredux.app.lobby.LobbyService.isDiscovering.value
        if (info.isLan && info.isHost) {
            // stopInGameBroadcast will be called when the game exits
        } else {
            com.dxxredux.app.lobby.LobbyService
                .stopDiscovery()
        }
        FileSetManager(filesDir).writeActiveSetPath()
        AudioSourceManager(filesDir).writePlaylist(contentResolver)
        ModManager(filesDir).writeEnabledModPaths(info.game)
        writeInitialGameConfig()
        writeMusicConfigForLaunch()
        val mpIntent = Intent(this, MainActivity::class.java)
        mpIntent.putExtra("game", info.game)
        mpIntent.putExtra("mp_callsign", mpCallsign)
        if (info.isHost) {
            mpIntent.putExtra("mp_mode", "host")
            mpIntent.putExtra("mp_my_port", NetworkConstants.ENGINE_PORT)
            mpIntent.putExtra("mp_mission", info.mission)
            mpIntent.putExtra("mp_game_mode", NetworkConstants.gameModeToInt(info.mode))
            mpIntent.putExtra("mp_max_players", info.maxPlayers)
            mpIntent.putExtra("mp_level_num", info.levelNum)
            mpIntent.putExtra("mp_difficulty", info.difficulty)
            mpIntent.putExtra("mp_coop_qol", info.coopQol)
        } else {
            mpIntent.putExtra("mp_mode", "join")
            if (info.lanHostAddr != null) {
                // LAN joiner: route through proxy for packet stats
                com.dxxredux.app.multiplayer.MatchmakingService.createProxy(
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
        if (info.isLan) mpIntent.putExtra("mp_is_lan", true)
        // Pass current debug log path so :game process appends to the same file
        DebugLog.currentFilePath()?.let {
            mpIntent.putExtra("netlog_path", it)
        }
        // Clear gameLaunchInfo after consumption to prevent stale re-launches
        MatchmakingStateHolder.update { it.copy(gameLaunchInfo = null) }
        startActivity(mpIntent)
    }

    private fun writeMpIntrospectJson() {
        try {
            val s = MatchmakingStateHolder.state.value
            val root = JSONObject()
            root.put("status", s.status.name)
            root.put("callsign", s.callsign)
            root.put("player_id", s.playerId ?: JSONObject.NULL)
            root.put("nav", s.nav.name)
            root.put("error", s.errorMessage ?: JSONObject.NULL)

            val lobby = s.currentLobby
            if (lobby != null) {
                val lj = JSONObject()
                lj.put("lobby_id", lobby.lobbyId)
                lj.put("is_host", lobby.isHost)
                lj.put("player_count", lobby.players.size)
                val pArr = JSONArray()
                for (p in lobby.players) {
                    val pj = JSONObject()
                    pj.put("player_id", p.playerId)
                    pj.put("callsign", p.callsign)
                    pj.put("ready", p.ready)
                    pArr.put(pj)
                }
                lj.put("players", pArr)
                root.put("lobby", lj)
            }

            root.put("lobby_count", s.lobbies.size)
            val lobbiesArr = JSONArray()
            for (l in s.lobbies) {
                val lj = JSONObject()
                lj.put("lobby_id", l.lobbyId)
                lj.put("host_callsign", l.hostCallsign)
                lj.put("game", l.game)
                lj.put("mission", l.gameInfo["mission"]?.jsonPrimitive?.content ?: "")
                lj.put("mode", l.gameInfo["mode"]?.jsonPrimitive?.content ?: "")
                lj.put("player_count", l.playerCount)
                lj.put("joinable", l.joinable)
                lobbiesArr.put(lj)
            }
            root.put("lobbies", lobbiesArr)

            val chatArr = JSONArray()
            for (msg in s.chatMessages) {
                val mj = JSONObject()
                mj.put("from", msg.fromCallsign)
                mj.put("text", msg.text)
                mj.put("is_me", msg.isMe)
                chatArr.put(mj)
            }
            root.put("chat", chatArr)

            root.put("game_launch_pending", s.gameLaunchInfo != null)

            val logArr = JSONArray()
            for (line in s.statusLog.takeLast(20)) {
                logArr.put(line)
            }
            root.put("log", logArr)

            val file = File(filesDir, "mp_introspect.json")
            file.writeText(root.toString(2))
            Log.i("DXX-MP", "MP introspection written to ${file.absolutePath}")
        } catch (e: Exception) {
            Log.e("DXX-MP", "Failed to write MP introspection", e)
        }
    }

    private fun writeIntrospectJson() {
        try {
            val dir = filesDir
            val fsm = FileSetManager(dir)
            val activeSet = fsm.getActive()
            val setDir = fsm.getSetDir(activeSet)
            val manifest = AssetManifest(setDir)
            val safManifest = fsm.safManifestForSet(activeSet)
            val d2FileList = detectD2FileList(setDir, safManifest)
            val d2Statuses = checkFiles(setDir, d2FileList, manifest, safManifest)
            val d1Statuses = checkFiles(setDir, D1_FILES, manifest, safManifest)
            val d2Ready = d2Statuses.filter { it.info.required }.all { it.found }
            val d1Ready = d1Statuses.filter { it.info.required }.all { it.found }

            val root = JSONObject()
            root.put("screen", "setup")
            root.put("can_launch", d2Ready || d1Ready)
            root.put("active_set", activeSet)

            // All files on disk (legacy: root app dir)
            val allFiles = dir.listFiles()?.map { it.name }?.sorted() ?: emptyList()
            root.put("files_on_disk", JSONArray(allFiles))

            // Active set directory contents and path
            val setFiles = setDir.listFiles()?.map { it.name }?.sorted() ?: emptyList()
            root.put("set_files", JSONArray(setFiles))
            root.put("active_set_path", setDir.absolutePath)

            // D2 section
            val d2 = JSONObject()
            d2.put("ready", d2Ready)
            d2.put("files", fileStatusArray(d2Statuses))
            root.put("d2", d2)

            // D1 section
            val d1 = JSONObject()
            d1.put("ready", d1Ready)
            d1.put("files", fileStatusArray(d1Statuses))
            root.put("d1", d1)

            // Active downloads
            if (downloadStates.isNotEmpty()) {
                val dl = JSONObject()
                for ((name, progress) in downloadStates) {
                    dl.put(
                        name,
                        when (progress) {
                            -2 -> "complete"
                            -1 -> "error"
                            else -> "$progress%"
                        },
                    )
                }
                root.put("downloads", dl)
            }

            // All file sets with file counts
            val setsArr = JSONArray()
            for (setInfo in fsm.listSets()) {
                val so = JSONObject()
                so.put("name", setInfo.name)
                val sd = fsm.getSetDir(setInfo.name)
                so.put("file_count", sd.listFiles()?.count { it.isFile } ?: 0)
                so.put("active", setInfo.name == activeSet)
                setsArr.put(so)
            }
            root.put("sets", setsArr)

            // Audio sources
            val srcManager = AudioSourceManager(dir)
            val sources = srcManager.getSources()
            if (sources.isNotEmpty()) {
                val audioArr = JSONArray()
                for (src in sources) {
                    val ao = JSONObject()
                    ao.put("id", src.id)
                    ao.put("label", src.discLabel)
                    ao.put("disc_id", src.discId)
                    ao.put("cue_path", src.cuePath)
                    ao.put("track_count", src.trackCount)
                    ao.put("audio_track_count", src.audioTrackCount)
                    if (src.trackNames.isNotEmpty()) {
                        val tn = JSONObject()
                        for ((k, v) in src.trackNames) tn.put(k.toString(), v)
                        ao.put("track_names", tn)
                    }
                    audioArr.put(ao)
                }
                root.put("audio_sources", audioArr)
            }
            if (findGogPair(setDir) != null) root.put("has_legacy_gog_audio", true)

            // Music preview playback state
            val musicPreview = JSONObject()
            val midiState = MidiPreviewBridge.getState()
            val midiObj = JSONObject()
            midiObj.put(
                "state",
                when (midiState.state) {
                    MidiPreviewBridge.STATE_PLAYING -> "playing"
                    MidiPreviewBridge.STATE_PAUSED -> "paused"
                    else -> "stopped"
                },
            )
            midiObj.put("position_ms", midiState.positionMs)
            midiObj.put("duration_ms", midiState.durationMs)
            musicPreview.put("midi", midiObj)
            val cdState = CdPreviewBridge.getState()
            val cdObj = JSONObject()
            cdObj.put(
                "state",
                when (cdState.state) {
                    CdPreviewBridge.STATE_PLAYING -> "playing"
                    CdPreviewBridge.STATE_PAUSED -> "paused"
                    else -> "stopped"
                },
            )
            cdObj.put("position_ms", cdState.positionMs)
            cdObj.put("duration_ms", cdState.durationMs)
            musicPreview.put("cd", cdObj)
            // MIDI source enumeration (which HOG files have tracks)
            val midiEnum = MidiEnumerationBridge.enumerateTracks(setDir.absolutePath)
            if (midiEnum.sources.isNotEmpty()) {
                val midiSrcArr = JSONArray()
                for (ms in midiEnum.sources) {
                    val mso = JSONObject()
                    mso.put("id", ms.id)
                    mso.put("label", ms.label)
                    mso.put("track_count", ms.tracks.size)
                    midiSrcArr.put(mso)
                }
                musicPreview.put("midi_sources", midiSrcArr)
            }
            root.put("music_preview", musicPreview)

            // All interactive UI elements (buttons, chips) with screen coordinates
            val buttonsArr = JSONArray()
            for (btn in collectAccessibleButtons()) {
                val bo = JSONObject()
                bo.put("text", btn.text)
                bo.put("enabled", btn.enabled)
                bo.put("x", btn.centerX.toInt())
                bo.put("y", btn.centerY.toInt())
                bo.put("w", btn.width.toInt())
                bo.put("h", btn.height.toInt())
                buttonsArr.put(bo)
            }
            root.put("buttons", buttonsArr)

            val outFile = File(dir, "setup_introspect.json")
            FileWriter(outFile).use { it.write(root.toString()) }
            Log.i("DXX-Setup", "Introspect written: ${outFile.absolutePath}")
        } catch (e: Exception) {
            Log.e("DXX-Setup", "Failed to write introspect JSON", e)
        }
    }

    private fun fileStatusArray(statuses: List<FileStatus>): JSONArray {
        val arr = JSONArray()
        for (s in statuses) {
            val obj = JSONObject()
            obj.put("filename", s.info.filename)
            obj.put("required", s.info.required)
            obj.put("found", s.found)
            if (s.foundName != null) obj.put("found_as", s.foundName)
            if (s.info.alternatives.isNotEmpty()) {
                obj.put("alternatives", JSONArray(s.info.alternatives))
            }
            if (s.info.downloadUrl != null) {
                obj.put("download_url", s.info.downloadUrl)
            }
            obj.put("description", s.info.description)
            if (s.safUri != null) {
                obj.put("saf_linked", true)
                obj.put("saf_uri", s.safUri)
            }
            if (s.manifestEntry != null) {
                obj.put("sha256", s.manifestEntry.sha256)
                obj.put("version", s.manifestEntry.versionDisplay)
                if (s.manifestEntry.isExternal) {
                    obj.put("source_uri", s.manifestEntry.sourceUri)
                    obj.put("external", true)
                }
                if (!s.found) {
                    obj.put("missing_from_disk", true)
                }
            }
            arr.put(obj)
        }
        return arr
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        CrashLog.install(this)
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

        // Register introspection receiver
        val filter = IntentFilter("com.dxxredux.SETUP_INTROSPECT")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(introspectReceiver, filter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(introspectReceiver, filter)
        }

        // Register command receiver
        val cmdFilter = IntentFilter("com.dxxredux.SETUP_COMMAND")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(commandReceiver, cmdFilter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(commandReceiver, cmdFilter)
        }

        // Register multiplayer command receiver
        val mpFilter = IntentFilter("com.dxxredux.MP_COMMAND")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(mpCommandReceiver, mpFilter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(mpCommandReceiver, mpFilter)
        }

        // Register host migration receiver (coop host takeover)
        val hmFilter = IntentFilter("com.dxxredux.HOST_MIGRATION")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(hostMigrationReceiver, hmFilter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(hostMigrationReceiver, hmFilter)
        }

        // Register automation receiver (debug only)
        if (BuildConfig.DEBUG) {
            val autoFilter = IntentFilter("com.dxxredux.SETUP_AUTOMATE")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(automateSetupReceiver, autoFilter, RECEIVER_EXPORTED)
            } else {
                registerReceiver(automateSetupReceiver, autoFilter)
            }
        }

        val gameRunning = intent.getBooleanExtra("gameRunning", false)
        gameRunningFlag = gameRunning
        val filesDir = filesDir

        setContent {
            SetupScreen(
                filesDir = filesDir,
                gameRunning = gameRunning,
                refreshTrigger = refreshTrigger.intValue,
                controllerAxes = controllerAxes,
                dpadAxes = dpadAxes,
                axisGeneration = axisGeneration.intValue,
                pressedButtons = pressedButtons,
                onLaunchGame = { game ->
                    val pending = launcherExecutor?.consumePendingLaunch()
                    if (pending != null) {
                        launchGameForAutomation(game, pending.scriptPath, pending.nextStep)
                    } else if (gameRunning || isGameProcessAlive()) {
                        finish() // return to the already-running game
                    } else {
                        FileSetManager(filesDir).writeActiveSetPath()
                        AudioSourceManager(filesDir).writePlaylist(contentResolver)
                        ModManager(filesDir).writeEnabledModPaths(game)
                        writeInitialGameConfig()
                        writeMusicConfigForLaunch()
                        val intent = Intent(this, MainActivity::class.java)
                        intent.putExtra("game", game)
                        startActivity(intent)
                        // Don't finish() -- stay in back stack so quitting
                        // the game returns here instead of the launcher.
                    }
                },
                onMultiplayerLaunch = { info ->
                    launchMultiplayerGame(info)
                },
                onRefresh = { refreshTrigger.intValue++ },
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
     * handled here — those are set in config.c's android_apply_initial_defaults().
     */
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

        // Default render resolution: 1/2 screen (rounded to even)
        val resW = (w / 2 + 1) and 0x7FFFFFFE
        val resH = (h / 2 + 1) and 0x7FFFFFFE

        // Write to all config paths (root + game subdirs) so the game finds
        // the resolution in whichever PHYSFS search path it checks first
        updateAllConfigFiles(
            filesDir,
            listOf(
                "AspectX" to "$aspectX",
                "AspectY" to "$aspectY",
                "ResolutionX" to "$resW",
                "ResolutionY" to "$resH",
            ),
        )

        // Store matching preference so the picker shows the right selection
        getSharedPreferences("dxx_prefs", MODE_PRIVATE)
            .edit()
            .putString("render_resolution", "${resW}x$resH")
            .apply()
        Log.i("DXX-Setup", "First launch: default resolution ${resW}x$resH")
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
                if (json.optInt("version", 0) >= CONTROLLER_CONFIG_VERSION) return
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
        mpGameLaunching = false
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
        refreshTrigger.intValue++
        // If the host returns from a game, signal the server to reset the lobby
        val mpState =
            com.dxxredux.app.multiplayer.MatchmakingStateHolder
                .state
                .value
        if (mpState.currentLobby?.isHost == true && !isGameProcessAlive()) {
            com.dxxredux.app.multiplayer.MatchmakingService
                .endGame()
        }
        // Check if game exited with LAUNCHER_CONTINUE for automation
        val executor = launcherExecutor
        if (executor != null) {
            val resultFile = File(filesDir, "automation_result.json")
            if (resultFile.exists()) {
                try {
                    val json = org.json.JSONObject(resultFile.readText())
                    if (json.optString("result") == "LAUNCHER_CONTINUE") {
                        val nextStep = json.getInt("next_step")
                        Log.i("DXX-Setup", "LAUNCHER_CONTINUE: resuming at step $nextStep")
                        resultFile.delete()
                        kotlinx.coroutines.MainScope().launch {
                            executor.resume(nextStep)
                        }
                    }
                } catch (e: Exception) {
                    Log.e("DXX-Setup", "Error reading automation result", e)
                }
            }
        }
    }

    override fun onDestroy() {
        try {
            unregisterReceiver(introspectReceiver)
        } catch (_: Exception) {
        }
        try {
            unregisterReceiver(commandReceiver)
        } catch (_: Exception) {
        }
        try {
            unregisterReceiver(mpCommandReceiver)
        } catch (_: Exception) {
        }
        try {
            unregisterReceiver(hostMigrationReceiver)
        } catch (_: Exception) {
        }
        try {
            unregisterReceiver(automateSetupReceiver)
        } catch (_: Exception) {
        }
        super.onDestroy()
    }
}

// ── Data model ──────────────────────────────────────────────────────────────

private data class GameFileInfo(
    val filename: String,
    val description: String,
    val required: Boolean,
    val alternatives: List<String> = emptyList(),
    // non-null = show [Download] button
    val downloadUrl: String? = null,
)

private data class FileStatus(
    val info: GameFileInfo,
    val found: Boolean,
    val foundName: String?,
    val manifestEntry: AssetManifest.AssetEntry? = null,
    val safUri: String? = null,
    val safSizeBytes: Long = 0,
)

private fun launcherDumpDirectoryState(
    prefix: String,
    dir: File,
) {
    val entries = dir.listFiles()?.sortedBy { it.name.lowercase() } ?: emptyArray<File>().toList()
    LauncherDebugLog.log("$prefix dir=${dir.absolutePath} count=${entries.size}")
    if (entries.isEmpty()) {
        LauncherDebugLog.log("$prefix entry=<none>")
        return
    }
    for (entry in entries) {
        val kind = if (entry.isDirectory) "dir" else "file"
        val size = if (entry.isFile) entry.length() else -1L
        LauncherDebugLog.log(
            "$prefix entry kind=$kind name=${entry.name} size=$size path=${entry.absolutePath}",
        )
    }
}

private fun launcherDumpStatusList(
    prefix: String,
    statuses: List<FileStatus>,
) {
    LauncherDebugLog.log("$prefix status_count=${statuses.size}")
    for (status in statuses) {
        val alternatives =
            if (status.info.alternatives.isEmpty()) "-" else status.info.alternatives.joinToString("|")
        LauncherDebugLog.log(
            "$prefix filename=${status.info.filename} required=${status.info.required} found=${status.found} found_name=${status.foundName ?: "-"} manifest_filename=${status.manifestEntry?.filename ?: "-"} manifest_source_uri=${status.manifestEntry?.sourceUri ?: "-"} saf_uri=${status.safUri ?: "-"} saf_size=${status.safSizeBytes} alternatives=$alternatives",
        )
    }
}

private fun launcherDumpFileTable(
    reason: String,
    filesDir: File,
    activeSetName: String,
    setDir: File,
    manifest: AssetManifest,
    safManifest: SafManifest,
) {
    LauncherDebugLog.log(
        "launcher-file-dump reason=$reason active_set=$activeSetName set_dir=${setDir.absolutePath}",
    )
    launcherDumpDirectoryState("launcher-root-files", filesDir)
    launcherDumpDirectoryState("launcher-set-files", setDir)

    val assetEntries = manifest.load().sortedBy { it.filename }
    val assetsPath = File(setDir, "assets.json")
    LauncherDebugLog.log(
        "launcher-asset-manifest file=${assetsPath.absolutePath} exists=${assetsPath.exists()} count=${assetEntries.size}",
    )
    if (assetEntries.isEmpty()) {
        LauncherDebugLog.log("launcher-asset-entry <none>")
    } else {
        for (entry in assetEntries) {
            val matchedName = findFile(setDir, entry.filename)
            val path = matchedName?.let { File(setDir, it) } ?: File(setDir, entry.filename)
            LauncherDebugLog.log(
                "launcher-asset-entry filename=${entry.filename} matched_name=${matchedName ?: "-"} path=${path.absolutePath} exists=${path.exists()} size=${entry.sizeBytes} source_uri=${entry.sourceUri ?: "-"} version=${entry.versionName ?: "-"}",
            )
        }
    }

    val safEntries = safManifest.read().sortedBy { it.filename }
    val safPath = File(setDir, SafManifest.FILENAME)
    LauncherDebugLog.log(
        "launcher-saf-manifest file=${safPath.absolutePath} exists=${safPath.exists()} count=${safEntries.size}",
    )
    if (safEntries.isEmpty()) {
        LauncherDebugLog.log("launcher-saf-entry <none>")
    } else {
        for (entry in safEntries) {
            LauncherDebugLog.log(
                "launcher-saf-entry filename=${entry.filename} uri=${entry.contentUri} size=${entry.sizeBytes}",
            )
        }
    }

    val d2FileList = detectD2FileList(setDir, safManifest)
    val d2Statuses = checkFiles(setDir, d2FileList, manifest, safManifest)
    val d1Statuses = checkFiles(setDir, D1_FILES, manifest, safManifest)
    launcherDumpStatusList("launcher-d2-status", d2Statuses)
    launcherDumpStatusList("launcher-d1-status", d1Statuses)
}

// ── Helpers ─────────────────────────────────────────────────────────────────

/** Case-insensitive file lookup (Android ext4 is case-sensitive). */
private fun findFile(
    dir: File,
    name: String,
): String? {
    val files = dir.listFiles() ?: return null
    return files.firstOrNull { it.name.equals(name, ignoreCase = true) }?.name
}

private fun checkFiles(
    dir: File,
    fileList: List<GameFileInfo>,
    manifest: AssetManifest? = null,
    safManifest: SafManifest? = null,
): List<FileStatus> {
    val safEntries = safManifest?.read() ?: emptyList()
    return fileList.map { info ->
        val primaryMatch = findFile(dir, info.filename)
        val altMatch =
            if (primaryMatch == null) {
                info.alternatives.firstNotNullOfOrNull { findFile(dir, it) }
            } else {
                null
            }
        val foundName = primaryMatch ?: altMatch
        // SAF leave-in-place: if the file isn't on disk, check the SAF manifest.
        val safEntry =
            if (foundName == null) {
                safEntries.firstOrNull { it.filename.equals(info.filename, ignoreCase = true) }
                    ?: info.alternatives.firstNotNullOfOrNull { alt ->
                        safEntries.firstOrNull { it.filename.equals(alt, ignoreCase = true) }
                    }
            } else {
                null
            }
        val entry =
            if (foundName != null) {
                manifest?.getEntry(foundName)
            } else {
                manifest?.getEntry(info.filename)
            }
        FileStatus(
            info,
            found = foundName != null || safEntry != null,
            foundName = foundName ?: if (safEntry != null) info.filename else null,
            manifestEntry = entry,
            safUri = safEntry?.contentUri,
            safSizeBytes = safEntry?.sizeBytes ?: 0,
        )
    }
}

/** Look up the description for a filename from the known file lists. */
private fun descriptionForFile(filename: String): String {
    val lower = filename.lowercase()
    val allFiles = D2_FILES + D2_DEMO_FILES + D1_FILES
    return allFiles
        .firstOrNull { info ->
            info.filename.equals(lower, ignoreCase = true) ||
                info.alternatives.any { it.equals(lower, ignoreCase = true) }
        }?.description ?: "Unknown file"
}

/** Describe a file's type based on its extension. */
private fun describeExtension(filename: String): String {
    val ext = filename.substringAfterLast('.', "").lowercase()
    return EXTENSION_TYPES[ext] ?: "[.$ext] \u2014 unknown type"
}

private val EXTENSION_TYPES =
    mapOf(
        "hog" to ".hog \u2014 mission archive",
        "mn2" to ".mn2 \u2014 Descent II mission descriptor",
        "msn" to ".msn \u2014 Descent I mission descriptor",
        "ham" to ".ham \u2014 global robot/weapon data",
        "vham" to ".vham \u2014 variant HAM (D2X-XL)",
        "pig" to ".pig \u2014 texture/sound container",
        "pog" to ".pog \u2014 texture override pack",
        "pcx" to ".pcx \u2014 briefing/cutscene image",
        "s11" to ".s11 \u2014 11 kHz PCM sound",
        "s22" to ".s22 \u2014 22 kHz PCM sound",
        "hmp" to ".hmp \u2014 HMI-format MIDI music",
        "raw" to ".raw \u2014 raw PCM audio",
        "rl2" to ".rl2 \u2014 Descent II level",
        "rdl" to ".rdl \u2014 Descent I level",
        "mvl" to ".mvl \u2014 movie library archive",
        "dxa" to ".dxa \u2014 Rebirth zip addon file",
        "dtx" to ".dtx \u2014 D2X-XL texture pack",
        "gog" to ".gog \u2014 GOG CD image (Redbook audio)",
        "inst" to ".inst \u2014 GOG CD cue sheet",
        "bin" to ".bin \u2014 CD disc image (BIN/CUE)",
        "cue" to ".cue \u2014 CD cue sheet (BIN/CUE)",
        "dem" to ".dem \u2014 game demo recording",
    )

// ── File definitions ────────────────────────────────────────────────────────

private val D2_FILES =
    listOf(
        // Required – core engine files
        GameFileInfo(
            "descent2.hog",
            "Main game data",
            required = true,
            alternatives = listOf("d2demo.hog"),
        ),
        GameFileInfo(
            "descent2.ham",
            "Models & objects",
            required = true,
            alternatives = listOf("d2demo.ham"),
        ),
        GameFileInfo(
            "groupa.pig",
            "Main textures",
            required = true,
            alternatives = listOf("d2demo.pig"),
        ),
        GameFileInfo(
            "descent2.s22",
            "Sound effects (22 kHz)",
            required = true,
            alternatives = listOf("descent2.s11"),
        ),
        // Required – level texture packs
        GameFileInfo("alien1.pig", "Alien 1 level textures", required = true),
        GameFileInfo("alien2.pig", "Alien 2 level textures", required = true),
        GameFileInfo("fire.pig", "Fire level textures", required = true),
        GameFileInfo("ice.pig", "Ice level textures", required = true),
        GameFileInfo("water.pig", "Water level textures", required = true),
        // Optional – movies & extras
        GameFileInfo(
            "intro-h.mvl",
            "Intro movie",
            required = false,
            alternatives = listOf("intro-l.mvl"),
        ),
        GameFileInfo(
            "other-h.mvl",
            "Cutscene movies",
            required = false,
            alternatives = listOf("other-l.mvl"),
        ),
        GameFileInfo(
            "robots-h.mvl",
            "Robot movies",
            required = false,
            alternatives = listOf("robots-l.mvl"),
        ),
        GameFileInfo("d2x.hog", "Vertigo expansion", required = false),
        GameFileInfo("hoard.ham", "Hoard multiplayer mode", required = false),
    )

private val D2_DEMO_FILES =
    listOf(
        GameFileInfo("d2demo.hog", "Demo game data", required = true),
        GameFileInfo("d2demo.ham", "Demo models & objects", required = true),
        GameFileInfo("d2demo.pig", "Demo textures", required = true),
    )

/**
 * Detect whether the files on disk (and in SAF manifest) correspond to the
 * D2 demo or the full game, and return the appropriate file list.
 */
private fun detectD2FileList(
    dir: File,
    safManifest: SafManifest? = null,
): List<GameFileInfo> {
    val demoFiles = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig")
    val hasDemoOnDisk = demoFiles.any { findFile(dir, it) != null }
    val hasDemoInSaf =
        safManifest?.let { sm ->
            val entries = sm.read()
            demoFiles.any { demo -> entries.any { it.filename.equals(demo, ignoreCase = true) } }
        } ?: false
    return if (hasDemoOnDisk || hasDemoInSaf) D2_DEMO_FILES else D2_FILES
}

private val D1_FILES =
    listOf(
        // Required -- core D1 files
        GameFileInfo("descent.hog", "D1 game data", required = true),
        GameFileInfo("descent.pig", "D1 textures", required = true),
    )

// Mods recommended for download (shown in ModsSection when not already installed)
private data class RecommendedMod(
    val filename: String,
    val displayName: String,
    val description: String,
    val downloadUrl: String,
    val game: String,
)

private val RECOMMENDED_MODS =
    listOf(
        RecommendedMod(
            "d1xr-mac-demo-sounds.dxa",
            "D1 Mac Demo Sounds",
            "Sound replacements from the Mac demo",
            "https://dxx-redux.com/dl/d1xr-mac-demo-sounds.dxa",
            "d1",
        ),
        RecommendedMod(
            "d1xr-hires.dxa",
            "D1 High-Res Pack",
            "High-resolution textures for D1",
            "https://dxx-redux.com/dl/d1xr-hires.dxa",
            "d1",
        ),
    )

// ── Demo downloads ──────────────────────────────────────────────────────────

private data class DemoPackage(
    val name: String,
    val url: String,
    val description: String,
    val sizeBytes: Long,
    // expected extracted filenames (lowercase)
    val files: List<String>,
)

private val DEMO_DOWNLOADS =
    listOf(
        DemoPackage(
            name = "D2 Demo",
            url = "https://dxx-redux.com/dl/d2demo.zip",
            description = "Official Descent 2 Demo (3 levels)",
            sizeBytes = 5_500_000L,
            files = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig"),
        ),
    )

// ── SAF directory scanning ───────────────────────────────────────────────────

/** All filenames we care about (D2 + D2 Demo + D1), lowercase for matching. */
private val ALL_GAME_FILENAMES: Set<String> by lazy {
    (D2_FILES + D2_DEMO_FILES + D1_FILES)
        .flatMap { info ->
            listOf(info.filename) + info.alternatives
        }.map { it.lowercase() }
        .toSet()
}

/** Result of scanning a user-chosen directory tree. */
private data class FoundFile(
    // original filename (preserving case)
    val name: String,
    // content:// URI to read from
    val uri: Uri,
)

/** Result of extracting a game file from a ZIP archive. */
private data class ExtractedFile(
    // lowercase canonical filename
    val name: String,
    // temp location
    val tmpFile: File,
    // SHA-256 of extracted file
    val sha256: String,
    // file size
    val sizeBytes: Long,
)

/**
 * Recursively walk a SAF document tree and return game files found.
 * Uses DocumentsContract for efficiency (no MediaStore needed).
 */
private fun scanTreeForGameFiles(
    context: Context,
    treeUri: Uri,
): List<FoundFile> {
    val results = mutableListOf<FoundFile>()
    val docId = DocumentsContract.getTreeDocumentId(treeUri)
    val queue = ArrayDeque<String>()
    queue.add(docId)

    while (queue.isNotEmpty()) {
        val parentId = queue.removeFirst()
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentId)
        val cursor =
            context.contentResolver.query(
                childrenUri,
                arrayOf(
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE,
                ),
                null,
                null,
                null,
            ) ?: continue

        cursor.use {
            while (it.moveToNext()) {
                val childId = it.getString(0)
                val displayName = it.getString(1) ?: continue
                val mimeType = it.getString(2) ?: ""

                if (mimeType == DocumentsContract.Document.MIME_TYPE_DIR) {
                    queue.add(childId)
                } else if (displayName.lowercase() in ALL_GAME_FILENAMES ||
                    displayName.lowercase().endsWith(".dem")
                ) {
                    val fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId)
                    results.add(FoundFile(displayName, fileUri))
                }
            }
        }
    }
    return results
}

/**
 * Copy a SAF document to the app's files directory.
 * Returns true on success.
 */
private fun importFile(
    context: Context,
    source: FoundFile,
    destDir: File,
): Boolean =
    try {
        // Use lowercase canonical name so the engine finds it
        val canonicalName = source.name.lowercase()
        val actualDestDir =
            if (canonicalName.endsWith(".dem")) {
                File(destDir, "demos").also { it.mkdirs() }
            } else {
                destDir
            }
        val destFile = File(actualDestDir, canonicalName)
        context.contentResolver.openInputStream(source.uri)?.use { input ->
            FileOutputStream(destFile).use { output ->
                input.copyTo(output, bufferSize = 8192)
            }
        }
        Log.i("DXX-Setup", "Imported ${source.name} → $canonicalName (${destFile.length()} bytes)")
        true
    } catch (e: Exception) {
        Log.e("DXX-Setup", "Failed to import ${source.name}", e)
        false
    }

/** Get the display name (filename) for a content:// URI. */
private fun getDisplayName(
    context: Context,
    uri: Uri,
): String? =
    try {
        context.contentResolver
            .query(
                uri,
                arrayOf(android.provider.OpenableColumns.DISPLAY_NAME),
                null,
                null,
                null,
            )?.use { cursor ->
                if (cursor.moveToFirst()) cursor.getString(0) else null
            }
    } catch (e: Exception) {
        null
    }

/**
 * Extract game files from a ZIP archive. Streams one entry at a time to tmpDir.
 * Returns list of extracted files with SHA-256 hashes.
 */
private data class ZipExtractionResult(
    val files: List<ExtractedFile>,
    val hadAudioFiles: Boolean,
    val error: String? = null,
)

private suspend fun extractZipContents(
    context: Context,
    zipUri: Uri,
    tmpDir: File,
    onProgress: (String) -> Unit,
): ZipExtractionResult =
    kotlinx.coroutines.withContext(Dispatchers.IO) {
        tmpDir.mkdirs()
        val results = mutableListOf<ExtractedFile>()
        var foundAudio = false
        val audioExts = setOf("mp3", "ogg", "flac")
        try {
            context.contentResolver.openInputStream(zipUri)?.use { raw ->
                ZipInputStream(raw).use { zis ->
                    var entry = zis.nextEntry
                    while (entry != null) {
                        val name = entry.name.substringAfterLast('/').lowercase()
                        if (!entry.isDirectory && !foundAudio) {
                            val ext = name.substringAfterLast('.', "")
                            if (ext in audioExts) foundAudio = true
                        }
                        if (!entry.isDirectory && name in ALL_GAME_FILENAMES) {
                            kotlinx.coroutines.withContext(Dispatchers.Main) {
                                onProgress(name)
                            }
                            val tmpFile = File(tmpDir, name)
                            val digest = java.security.MessageDigest.getInstance("SHA-256")
                            var size = 0L
                            FileOutputStream(tmpFile).use { out ->
                                val buf = ByteArray(8192)
                                while (true) {
                                    val n = zis.read(buf)
                                    if (n <= 0) break
                                    out.write(buf, 0, n)
                                    digest.update(buf, 0, n)
                                    size += n
                                }
                            }
                            val sha256 = digest.digest().joinToString("") { "%02x".format(it) }
                            results.add(ExtractedFile(name, tmpFile, sha256, size))
                            Log.i("DXX-Setup", "Extracted from ZIP: $name ($size bytes, sha256=${sha256.take(16)}...)")
                        }
                        zis.closeEntry()
                        entry = zis.nextEntry
                    }
                }
            }
        } catch (e: Exception) {
            Log.e("DXX-Setup", "ZIP extraction failed", e)
            return@withContext ZipExtractionResult(results, foundAudio, "ZIP extraction failed: ${e.message}")
        }
        ZipExtractionResult(results, foundAudio)
    }

/**
 * Extract game files from a 7z archive. Copies content URI to temp file first
 * since SevenZFile requires a seekable file.
 */
private suspend fun extract7zContents(
    context: Context,
    archiveUri: Uri,
    tmpDir: File,
    onProgress: (String) -> Unit,
): ZipExtractionResult =
    kotlinx.coroutines.withContext(Dispatchers.IO) {
        tmpDir.mkdirs()
        val results = mutableListOf<ExtractedFile>()
        var foundAudio = false
        val audioExts = setOf("mp3", "ogg", "flac")
        val tmpArchive = File(tmpDir, ".tmp_7z_import")
        try {
            context.contentResolver.openInputStream(archiveUri)?.use { input ->
                FileOutputStream(tmpArchive).use { output -> input.copyTo(output) }
            }
            SevenZFile.builder().setFile(tmpArchive).get().use { szf ->
                var entry = szf.nextEntry
                while (entry != null) {
                    val name = entry.name.substringAfterLast('/').lowercase()
                    if (!entry.isDirectory && !foundAudio) {
                        val ext = name.substringAfterLast('.', "")
                        if (ext in audioExts) foundAudio = true
                    }
                    if (!entry.isDirectory && name in ALL_GAME_FILENAMES) {
                        kotlinx.coroutines.withContext(Dispatchers.Main) {
                            onProgress(name)
                        }
                        val tmpFile = File(tmpDir, name)
                        val digest = java.security.MessageDigest.getInstance("SHA-256")
                        var size = 0L
                        FileOutputStream(tmpFile).use { out ->
                            val buf = ByteArray(8192)
                            while (true) {
                                val n = szf.read(buf)
                                if (n <= 0) break
                                out.write(buf, 0, n)
                                digest.update(buf, 0, n)
                                size += n
                            }
                        }
                        val sha256 = digest.digest().joinToString("") { "%02x".format(it) }
                        results.add(ExtractedFile(name, tmpFile, sha256, size))
                        Log.i("DXX-Setup", "Extracted from 7z: $name ($size bytes, sha256=${sha256.take(16)}...)")
                    }
                    entry = szf.nextEntry
                }
            }
        } catch (e: Exception) {
            Log.e("DXX-Setup", "7z extraction failed", e)
            return@withContext ZipExtractionResult(results, foundAudio, "7z extraction failed: ${e.message}")
        } finally {
            tmpArchive.delete()
        }
        ZipExtractionResult(results, foundAudio)
    }

/** Clean up temporary extraction directory. */
private fun cleanupTmpDir(filesDir: File) {
    val tmpDir = File(filesDir, "tmp")
    if (tmpDir.exists()) tmpDir.deleteRecursively()
}

// ── Composables ─────────────────────────────────────────────────────────────

@Composable
private fun SetupScreen(
    filesDir: File,
    gameRunning: Boolean,
    refreshTrigger: Int,
    controllerAxes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    onLaunchGame: (String) -> Unit,
    onMultiplayerLaunch: (com.dxxredux.app.multiplayer.GameLaunchInfo) -> Unit,
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
    var activeSetName by remember { mutableStateOf(fileSetManager.getActive()) }
    val setDir = remember(activeSetName) { fileSetManager.getSetDir(activeSetName) }
    val manifest = remember(activeSetName) { AssetManifest(setDir) }
    val safManifest = remember(activeSetName) { fileSetManager.safManifestForSet(activeSetName) }
    val d2FileList = remember(refreshTrigger, activeSetName) { detectD2FileList(setDir, safManifest) }
    val d2Statuses = remember(refreshTrigger, activeSetName) { checkFiles(setDir, d2FileList, manifest, safManifest) }
    val d1Statuses = remember(refreshTrigger, activeSetName) { checkFiles(setDir, D1_FILES, manifest, safManifest) }

    // ── Hashing progress state ──────────────────────────────
    var hashingFile by remember { mutableStateOf<String?>(null) }
    var hashingFileIndex by remember { mutableIntStateOf(0) }
    var hashingTotalFiles by remember { mutableIntStateOf(0) }
    var hashingProgress by remember { mutableFloatStateOf(0f) }
    val isHashing = hashingFile != null

    val d2RequiredOk = d2Statuses.filter { it.info.required }.all { it.found }
    val d1RequiredOk = d1Statuses.filter { it.info.required }.all { it.found }
    val canLaunch = d2RequiredOk || d1RequiredOk

    val context = androidx.compose.ui.platform.LocalContext.current

    // ── Startup: prune stale entries, then hash new/changed files ──
    var prunedSourceNames by remember { mutableStateOf<List<String>>(emptyList()) }
    var prunedDataFiles by remember { mutableStateOf<List<String>>(emptyList()) }
    LaunchedEffect(activeSetName) {
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

        // 1. Prune audio sources
        val srcManager = AudioSourceManager(filesDir)
        val prunedSrc = srcManager.pruneMissingSources(setDir)
        if (prunedSrc.isNotEmpty()) {
            prunedSourceNames = prunedSrc
        }

        // 2. Prune stale manifest entries (before hashing, so hashing
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

        // 3. Hash files on disk that are missing manifest entries (or size changed)
        val allGameNames = ALL_GAME_FILENAMES
        val staleFiles = manifest.findStaleFiles(allGameNames)
        if (staleFiles.isNotEmpty()) {
            hashingTotalFiles = staleFiles.size
            for ((i, file) in staleFiles.withIndex()) {
                hashingFileIndex = i + 1
                hashingFile = file.name
                hashingProgress = 0f
                val sha256 =
                    AssetManifest.computeSha256(file) { bytesRead, totalBytes ->
                        if (totalBytes > 0) hashingProgress = bytesRead.toFloat() / totalBytes
                    }
                if (sha256 != null) {
                    manifest.upsert(file.name, sha256, file.length())
                } else {
                    Log.w("DXX-Setup", "Skipping manifest update for ${file.name}: file disappeared during hashing")
                }
            }
            hashingFile = null
        }

        if (prunedSrc.isNotEmpty() || allPruned.isNotEmpty() || staleFiles.isNotEmpty()) {
            onRefresh()
        }
    }

    // True when zero required files are found for either game
    val noRequiredFiles =
        d2Statuses.filter { it.info.required }.none { it.found } &&
            d1Statuses.filter { it.info.required }.none { it.found }

    // Download state: filename → progress (0..100, -1 = error, -2 = complete)
    val downloadProgress = remember { mutableStateMapOf<String, Int>() }
    val scope = rememberCoroutineScope()

    // ── File detail popup state ─────────────────────────────
    var detailStatus by remember { mutableStateOf<FileStatus?>(null) }
    var detailIsD2 by remember { mutableStateOf(true) }

    // ── Set management dialog state ─────────────────────────
    var showSetDialog by remember { mutableStateOf(false) }

    // ── Game selection state ────────────────────────────────
    val gamePrefs = remember { context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE) }
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

    // ── Demo download state ─────────────────────────────────
    var demoDownloading by remember { mutableStateOf<String?>(null) } // package name or null
    var demoDownloadProgress by remember { mutableIntStateOf(0) }
    var demoDownloadError by remember { mutableStateOf<String?>(null) }

    // ── ZIP extraction state ────────────────────────────
    var zipExtracted by remember { mutableStateOf<List<ExtractedFile>?>(null) }
    var zipPackageName by remember { mutableStateOf<String?>(null) }
    var zipExtracting by remember { mutableStateOf(false) }
    var zipProgressFile by remember { mutableStateOf("") }
    var zipHadAudioFiles by remember { mutableStateOf(false) }

    // ── BIN/CUE disc import state ───────────────────────
    var discImportCueName by remember { mutableStateOf<String?>(null) }
    var discImportCueUri by remember { mutableStateOf<Uri?>(null) }
    var discImportBins by remember { mutableStateOf<List<Pair<String, Uri>>>(emptyList()) }

    // ── ISO disc import state ───────────────────────────
    var isoImportName by remember { mutableStateOf<String?>(null) }
    var isoImportUri by remember { mutableStateOf<Uri?>(null) }

    // ── GOG installer import state ──────────────────────
    var gogImportUri by remember { mutableStateOf<Uri?>(null) }
    var gogImportName by remember { mutableStateOf<String?>(null) }

    // ── SOW archive import state ────────────────────────
    var sowImportUri by remember { mutableStateOf<Uri?>(null) }
    var sowImportName by remember { mutableStateOf<String?>(null) }

    // ── Audio file auto-import state ────────────────────
    var audioImportUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
    var audioImporting by remember { mutableStateOf(false) }
    var zipArchiveUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
    val audioCustomMgr = remember { CustomAudioSetManager(filesDir) }

    // ── DXA mod import state ────────────────────────────
    val dxaImportUris = remember { mutableListOf<Pair<String, Uri>>() }

    // ── Config JSON import state ────────────────────────
    var configImportUri by remember { mutableStateOf<Uri?>(null) }
    var configImportName by remember { mutableStateOf<String?>(null) }

    val filePickerLauncher =
        rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenMultipleDocuments(),
        ) { uris: List<Uri> ->
            if (uris.isEmpty()) return@rememberLauncherForActivityResult
            scanning = true
            importStatus = ""
            scope.launch(Dispatchers.IO) {
                try {
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
                    val audioFileUris = mutableListOf<Uri>()
                    var jsonConfigUri: Pair<String, Uri>? = null
                    for (uri in uris) {
                        val name = getDisplayName(context, uri)
                        if (name != null) {
                            val lname = name.lowercase()
                            when {
                                lname.endsWith(".zip") || lname.endsWith(".7z") -> zipUris.add(name to uri)
                                lname.endsWith(".cue") -> cueUris.add(name to uri)
                                lname.endsWith(".iso") -> isoUris.add(name to uri)
                                lname.endsWith(".inst") -> instDiscUri = name to uri
                                lname.endsWith(".gog") -> gogDiscUri = name to uri
                                lname.endsWith(".bin") -> binUris.add(name to uri)
                                lname.endsWith(".exe") || lname.endsWith(".pkg") -> gogUri = name to uri
                                lname.endsWith(".sow") -> sowUri = name to uri
                                lname.endsWith(".dem") -> gameUris.add(FoundFile(name, uri))
                                lname in ALL_GAME_FILENAMES -> gameUris.add(FoundFile(name, uri))
                                lname.endsWith(".mp3") || lname.endsWith(".ogg") || lname.endsWith(".flac") ->
                                    audioFileUris.add(uri)
                                lname.endsWith(".dxa") -> dxaImportUris.add(name to uri)
                                lname.endsWith(".json") -> {
                                    // Detect game config JSON (only when picked alone)
                                    if (uris.size == 1) {
                                        try {
                                            val text =
                                                context.contentResolver
                                                    .openInputStream(uri)
                                                    ?.bufferedReader()
                                                    ?.use { it.readText() }
                                            if (text != null) {
                                                val json = org.json.JSONObject(text)
                                                val cfgType = HumanReadableConfig.detectConfigType(json)
                                                if (cfgType != "unknown") {
                                                    jsonConfigUri = name to uri
                                                } else {
                                                    unhandledFiles.add(name)
                                                }
                                            } else {
                                                unhandledFiles.add(name)
                                            }
                                        } catch (_: Exception) {
                                            unhandledFiles.add(name)
                                        }
                                    } else {
                                        unhandledFiles.add(name)
                                    }
                                }
                                else -> unhandledFiles.add(name)
                            }
                        }
                    }
                    // Collect warnings for files the picker couldn't route
                    val warnings = mutableListOf<String>()
                    // If .gog+.inst pair found, route to disc import as CUE+BIN
                    if (gogDiscUri != null && instDiscUri != null) {
                        Log.i(
                            "DXX-Setup",
                            "Routing .gog+.inst pair to disc import: gog=${gogDiscUri.first}, inst=${instDiscUri.first}",
                        )
                        cueUris.add(instDiscUri)
                        binUris.add(gogDiscUri)
                    } else {
                        // Warn about unpaired .gog/.inst (same as .bin/.cue)
                        gogDiscUri?.let { warnings.add("${it.first} requires a matching .inst file") }
                        instDiscUri?.let { warnings.add("${it.first} requires a matching .gog file") }
                    }
                    if (binUris.isNotEmpty() && cueUris.isEmpty()) {
                        for (b in binUris) warnings.add("${b.first} requires a matching CUE file")
                    }
                    if (cueUris.isNotEmpty() && binUris.isEmpty()) {
                        for (c in cueUris) warnings.add("${c.first} requires a matching BIN file")
                    }
                    if (isoUris.size > 1) {
                        warnings.add("Only one ISO image can be imported at a time")
                    }
                    if (isoUris.isNotEmpty() && cueUris.isNotEmpty() && binUris.isNotEmpty()) {
                        warnings.add("Select either a standalone ISO or a CUE/BIN set")
                    }
                    for (f in unhandledFiles) {
                        warnings.add("$f: file type not recognized")
                    }
                    withContext(Dispatchers.Main) {
                        for (w in warnings) {
                            Toast.makeText(context, w, Toast.LENGTH_LONG).show()
                            Log.w("DXX-Setup", "Import warning: $w")
                        }
                        // Trigger audio import dialog if audio files found
                        if (audioFileUris.isNotEmpty()) {
                            audioImportUris = audioFileUris
                        }
                        if (gameUris.isNotEmpty()) {
                            scanResults = gameUris
                        }
                        // Trigger disc import dialog if CUE+BIN pair found
                        if (cueUris.isNotEmpty() && binUris.isNotEmpty()) {
                            discImportCueName = cueUris.first().first
                            discImportCueUri = cueUris.first().second
                            discImportBins = binUris
                        } else if (isoUris.isNotEmpty()) {
                            isoImportName = isoUris.first().first
                            isoImportUri = isoUris.first().second
                        }
                        // Trigger GOG import dialog if .exe/.pkg found
                        gogUri?.let {
                            gogImportName = it.first
                            gogImportUri = it.second
                        }
                        // Trigger SOW import dialog if .sow found
                        sowUri?.let {
                            sowImportName = it.first
                            sowImportUri = it.second
                        }
                        // Trigger config import dialog if single JSON config picked
                        jsonConfigUri?.let {
                            configImportName = it.first
                            configImportUri = it.second
                        }
                        scanning = false
                    }
                    // Import .dxa mod files (simple copy to mods dir)
                    if (dxaImportUris.isNotEmpty()) {
                        val modMgr = ModManager(filesDir)
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
                        dxaImportUris.clear()
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
                            onRefresh()
                        }
                    }
                    // Handle ZIP/7z files
                    if (zipUris.isNotEmpty()) {
                        withContext(Dispatchers.Main) { zipExtracting = true }
                        val tmpDir = File(filesDir, "tmp")
                        val allExtracted = mutableListOf<ExtractedFile>()
                        var anyAudio = false
                        val archiveErrors = mutableListOf<String>()
                        for ((arcName, arcUri) in zipUris) {
                            val result =
                                if (arcName.lowercase().endsWith(".7z")) {
                                    extract7zContents(context, arcUri, tmpDir) { name ->
                                        zipProgressFile = name
                                    }
                                } else {
                                    extractZipContents(context, arcUri, tmpDir) { name ->
                                        zipProgressFile = name
                                    }
                                }
                            allExtracted.addAll(result.files)
                            if (result.hadAudioFiles) anyAudio = true
                            if (result.error != null) archiveErrors.add("$arcName: ${result.error}")
                        }
                        // Identify package
                        val fileHashes = allExtracted.associate { it.name to it.sha256 }
                        val pkgName = KnownVersions.identifyPackage(fileHashes)
                        withContext(Dispatchers.Main) {
                            zipExtracted = allExtracted
                            zipPackageName = pkgName
                            zipHadAudioFiles = anyAudio
                            zipArchiveUris = zipUris.map { it.second }
                            zipExtracting = false
                            zipProgressFile = ""
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
                        importStatus = "File processing failed: ${e.message}"
                    }
                }
            }
        }

    // ── Initial focus for D-pad/keyboard navigation ─────
    val initialFocus = remember { FocusRequester() }

    // ── Page navigation state ────────────────────────────
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
    LaunchedEffect(anySubPageOpen) {
        if (!anySubPageOpen) initialFocus.requestFocus()
    }

    MaterialTheme(colorScheme = darkColorScheme()) {
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
                onBack = { showControllerPage = false },
            )
            return@MaterialTheme
        }
        if (showTouchEditorPage) {
            BackHandler { showTouchEditorPage = false }
            TouchEditorPage(
                gameVariant = selectedGame,
                onBack = { showTouchEditorPage = false },
            )
            return@MaterialTheme
        }
        if (showAdvancedPage) {
            BackHandler { showAdvancedPage = false }
            AdvancedSettingsPage(
                filesDir = filesDir,
                fileSetManager = fileSetManager,
                onBack = { showAdvancedPage = false },
            )
            return@MaterialTheme
        }
        if (showGraphicsPage) {
            BackHandler { showGraphicsPage = false }
            GraphicsSettingsPage(
                filesDir = filesDir,
                onBack = { showGraphicsPage = false },
            )
            return@MaterialTheme
        }
        if (showEnginePrefsPage) {
            BackHandler { showEnginePrefsPage = false }
            EnginePreferencesPage(
                gameVariant = selectedGame,
                filesDir = filesDir,
                onBack = { showEnginePrefsPage = false },
            )
            return@MaterialTheme
        }
        if (showMultiplayerPage) {
            BackHandler { showMultiplayerPage = false }
            com.dxxredux.app.multiplayer.MultiplayerScreen(
                onBack = { showMultiplayerPage = false },
                onLaunchGame = onMultiplayerLaunch,
            )
            return@MaterialTheme
        }
        if (showAutoselectPage) {
            BackHandler { showAutoselectPage = false }
            AutoselectEditorPage(
                gameVariant = selectedGame,
                filesDir = filesDir.absolutePath,
                onBack = { showAutoselectPage = false },
            )
            return@MaterialTheme
        }
        if (showMusicPage) {
            BackHandler { showMusicPage = false }
            MusicPickerPage(
                filesDir = filesDir,
                onBack = { showMusicPage = false },
            )
            return@MaterialTheme
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
                // ── Title + About ────────────────────────────
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
                    AlertDialog(
                        onDismissRequest = { showAbout = false },
                        confirmButton = {
                            TextButton(onClick = { showAbout = false }) { Text("OK") }
                        },
                        title = { Text("DXX-Redux") },
                        text = {
                            val arch = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown"
                            val buildLine =
                                if (BuildInfo.BUILD_TYPE == "dev") {
                                    "Dev Build"
                                } else {
                                    "Build ${BuildInfo.GIT_COMMIT_COUNT}" +
                                        " (${BuildInfo.GIT_SHORT_HASH})" +
                                        " ${BuildInfo.BUILD_TYPE}"
                                }
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

                // ── File detail popup ──
                detailStatus?.let { status ->
                    FileDetailDialog(
                        status = status,
                        onDismiss = { detailStatus = null },
                        onDelete =
                            when {
                                // SAF leave-in-place file — unlink from SAF manifest
                                status.safUri != null -> {
                                    {
                                        safManifest.remove(status.info.filename)
                                        detailStatus = null
                                        onRefresh()
                                    }
                                }
                                // File on disk with manifest entry — delete file + manifest entry
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
                                // External import but missing from disk — forget the manifest entry
                                status.manifestEntry?.isExternal == true -> {
                                    {
                                        manifest.remove(status.manifestEntry.filename)
                                        detailStatus = null
                                        onRefresh()
                                    }
                                }
                                else -> null
                            },
                    )
                }

                // ── Set management dialog ──
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

                // ── BIN/CUE disc import dialog ──
                if (discImportCueUri != null) {
                    DiscImportDialog(
                        cueName = discImportCueName ?: "unknown.cue",
                        cueUri = discImportCueUri!!,
                        binUris = discImportBins,
                        filesDir = filesDir,
                        setDir = setDir,
                        context = context,
                        onImported = {
                            discImportCueUri = null
                            discImportCueName = null
                            discImportBins = emptyList()
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

                // ── ISO disc import dialog ──
                if (isoImportUri != null) {
                    IsoImportDialog(
                        isoName = isoImportName ?: "unknown.iso",
                        isoUri = isoImportUri!!,
                        setDir = setDir,
                        context = context,
                        onImported = {
                            isoImportUri = null
                            isoImportName = null
                            onRefresh()
                        },
                        onDismiss = {
                            isoImportUri = null
                            isoImportName = null
                            onRefresh()
                        },
                    )
                }

                // ── GOG installer import dialog ──
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
                            onRefresh()
                        },
                        onDismiss = {
                            gogImportUri = null
                            gogImportName = null
                            onRefresh()
                        },
                    )
                }

                // ── SOW archive import dialog ──
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
                            onRefresh()
                        },
                        onDismiss = {
                            sowImportUri = null
                            sowImportName = null
                        },
                    )
                }

                // ── Config JSON import dialog ──
                if (configImportUri != null) {
                    AlertDialog(
                        onDismissRequest = {
                            configImportUri = null
                            configImportName = null
                        },
                        title = { Text("Import Game Config?") },
                        text = {
                            Text(
                                "Import settings from ${configImportName ?: "config file"}? This will overwrite your current touch layout, controller config, and/or weapon ordering",
                            )
                        },
                        confirmButton = {
                            TextButton(onClick = {
                                val uri = configImportUri!!
                                configImportUri = null
                                configImportName = null
                                val result = ConfigImportExport.importFromUri(context, uri)
                                Toast.makeText(context, result, Toast.LENGTH_LONG).show()
                            }) { Text("Import") }
                        },
                        dismissButton = {
                            TextButton(onClick = {
                                configImportUri = null
                                configImportName = null
                            }) { Text("Cancel") }
                        },
                    )
                }

                // ── Audio file auto-import dialog ──
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
                            scope.launch {
                                importAudioFiles(
                                    context,
                                    filesDir,
                                    audioCustomMgr,
                                    newName,
                                    uris,
                                    targetSetId,
                                    copyToStorage,
                                )
                                audioImporting = false
                                // Auto-switch music mode to "files"
                                context
                                    .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
                                    .edit()
                                    .putString("music_mode", "files")
                                    .apply()
                            }
                        },
                    )
                }

                // ── Shared composable blocks ──

                val filesPane: @Composable ColumnScope.() -> Unit = {
                    // ── Pruned audio sources notification ────────
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

                    // ── Pruned game data notification ───────────
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

                    // ── Active set indicator ──────────────────────
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

                    // ── Missing-files help ──────────────────────
                    if (!canLaunch && !gameRunning) {
                        MissingFilesHelp()
                        Spacer(modifier = Modifier.height(8.dp))

                        // ── Demo download offers ──────────────────
                        for (demo in DEMO_DOWNLOADS) {
                            Card(
                                modifier = Modifier.fillMaxWidth(),
                                colors =
                                    CardDefaults.cardColors(
                                        containerColor = MaterialTheme.colorScheme.secondaryContainer,
                                    ),
                            ) {
                                Column(modifier = Modifier.padding(12.dp)) {
                                    Text(
                                        text = "\uD83C\uDFAE ${demo.name}",
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 14.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                    Text(
                                        text = "${demo.description} (${demo.sizeBytes / 1_000_000} MB)",
                                        fontSize = 12.sp,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                                    )
                                    Spacer(modifier = Modifier.height(6.dp))
                                    if (demoDownloading == demo.name) {
                                        Text(
                                            text = "Downloading\u2026 $demoDownloadProgress%",
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
                                        if (demoDownloadError != null) {
                                            Text(
                                                text = "Error: $demoDownloadError",
                                                fontSize = 12.sp,
                                                color = MaterialTheme.colorScheme.error,
                                            )
                                            Spacer(modifier = Modifier.height(4.dp))
                                        }
                                        Button(
                                            onClick = {
                                                demoDownloadError = null
                                                demoDownloading = demo.name
                                                demoDownloadProgress = 0
                                                scope.launch {
                                                    val tmpDir = File(filesDir, "tmp")
                                                    tmpDir.mkdirs()
                                                    val zipFile =
                                                        File(tmpDir, "${demo.name.lowercase().replace(' ', '_')}.zip")
                                                    // Download ZIP
                                                    var downloadOk = false
                                                    downloadFile(
                                                        url = demo.url,
                                                        destDir = tmpDir,
                                                        filename = zipFile.name,
                                                        onProgress = { pct -> demoDownloadProgress = pct },
                                                        onDone = { success -> downloadOk = success },
                                                    )
                                                    if (!downloadOk) {
                                                        demoDownloading = null
                                                        demoDownloadError = "Download failed"
                                                        cleanupTmpDir(filesDir)
                                                        return@launch
                                                    }
                                                    // Extract ZIP contents
                                                    val zipUri = android.net.Uri.fromFile(zipFile)
                                                    val result = extractZipContents(context, zipUri, tmpDir) { _ -> }
                                                    if (result.files.isEmpty()) {
                                                        demoDownloading = null
                                                        demoDownloadError = "No game files found in ZIP"
                                                        cleanupTmpDir(filesDir)
                                                        return@launch
                                                    }
                                                    // Move files to setDir
                                                    var imported = 0
                                                    for (ef in result.files) {
                                                        val destFile = File(setDir, ef.name)
                                                        val ok =
                                                            withContext(Dispatchers.IO) {
                                                                try {
                                                                    ef.tmpFile.copyTo(destFile, overwrite = true)
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
                                                    cleanupTmpDir(filesDir)
                                                    demoDownloading = null
                                                    importStatus = "Installed ${demo.name}: $imported files."
                                                    onRefresh()
                                                }
                                            },
                                            enabled = demoDownloading == null,
                                        ) {
                                            Text("Download & Install", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                            Spacer(modifier = Modifier.height(8.dp))
                        }
                    }

                    // ── Hashing progress bar ──
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
                                    text = "Hashing: $hashingFile ($hashingFileIndex/$hashingTotalFiles)",
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

                    // ── Import files button ──
                    Button(
                        onClick = {
                            filePickerLauncher.launch(
                                arrayOf("application/octet-stream", "application/zip", "*/*"),
                            )
                        },
                        enabled = !scanning && !isHashing && !zipExtracting,
                        modifier = Modifier.fillMaxWidth().height(44.dp),
                        colors =
                            ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.secondary,
                            ),
                    ) {
                        Text(
                            text =
                                if (scanning || zipExtracting) {
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
                            "Select .hog, .ham, .pig files, a .zip/.7z archive, .cue/.bin disc images," +
                                " .sow archive, or GOG installer.",
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(modifier = Modifier.height(8.dp))

                    // ── Scan results / import card ──────────────
                    if (scanResults != null) {
                        val found = scanResults!!
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
                                                scope.launch {
                                                    try {
                                                        var imported = 0
                                                        hashingTotalFiles = found.size
                                                        for ((i, f) in found.withIndex()) {
                                                            hashingFileIndex = i + 1
                                                            hashingFile = f.name
                                                            hashingProgress = 0f
                                                            val canonicalName = f.name.lowercase()
                                                            val destFile =
                                                                if (canonicalName.endsWith(".dem")) {
                                                                    File(File(setDir, "demos"), canonicalName)
                                                                } else {
                                                                    File(setDir, canonicalName)
                                                                }
                                                            // Determine track: native data-dir vs external
                                                            val existedBefore = destFile.exists()
                                                            val existingEntry = manifest.getEntry(canonicalName)
                                                            val ok =
                                                                withContext(Dispatchers.IO) {
                                                                    importFile(context, f, setDir)
                                                                }
                                                            if (ok) {
                                                                imported++
                                                                val sha256 =
                                                                    AssetManifest.computeSha256(
                                                                        destFile,
                                                                    ) { bytesRead, totalBytes ->
                                                                        if (totalBytes >
                                                                            0
                                                                        ) {
                                                                            hashingProgress =
                                                                                bytesRead.toFloat() / totalBytes
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
                                                                    manifest.upsert(
                                                                        destFile.name,
                                                                        sha256,
                                                                        destFile.length(),
                                                                        sourceUri,
                                                                    )
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
                                                        onRefresh()
                                                    } catch (e: Exception) {
                                                        Log.e("DXX-Setup", "Import failed", e)
                                                        hashingFile = null
                                                        importStatus = "Import failed: ${e.message}"
                                                        scanResults = null
                                                    }
                                                }
                                            },
                                        ) {
                                            Text("Import All", fontSize = 13.sp)
                                        }
                                        OutlinedButton(
                                            onClick = { scanResults = null },
                                        ) {
                                            Text("Dismiss", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    if (importStatus.isNotEmpty()) {
                        Text(
                            text = importStatus,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = Color(0xFF4CAF50),
                            modifier = Modifier.padding(bottom = 8.dp),
                        )
                    }

                    // ── ZIP extraction progress ─────────────────
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
                                LinearProgressIndicator(
                                    modifier = Modifier.fillMaxWidth().height(4.dp),
                                    color = MaterialTheme.colorScheme.primary,
                                    trackColor = MaterialTheme.colorScheme.primaryContainer,
                                )
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // ── ZIP results card ───────────────────────
                    if (zipExtracted != null) {
                        val extracted = zipExtracted!!
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
                                            text = "\u2022 ${ef.name} (${ef.sizeBytes / 1024} KB)",
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
                                                scope.launch {
                                                    var imported = 0
                                                    hashingTotalFiles = extracted.size
                                                    for ((i, ef) in extracted.withIndex()) {
                                                        hashingFileIndex = i + 1
                                                        hashingFile = ef.name
                                                        hashingProgress = 1f
                                                        val destFile = File(setDir, ef.name)
                                                        val ok =
                                                            withContext(Dispatchers.IO) {
                                                                try {
                                                                    ef.tmpFile.copyTo(destFile, overwrite = true)
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
                                                        "Imported $imported of ${extracted.size} files from ZIP."
                                                    zipExtracted = null
                                                    zipPackageName = null
                                                    cleanupTmpDir(filesDir)
                                                    onRefresh()
                                                }
                                            },
                                        ) {
                                            Text("Import to Current Set", fontSize = 13.sp)
                                        }
                                        OutlinedButton(
                                            onClick = {
                                                zipExtracted = null
                                                zipPackageName = null
                                                cleanupTmpDir(filesDir)
                                            },
                                        ) {
                                            Text("Dismiss", fontSize = 13.sp)
                                        }
                                    }
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    // ── File sections ────────────────────
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
                                            downloadFile(
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
                        refreshTrigger = refreshTrigger,
                    )

                    DemosSection(
                        setDir = setDir,
                        refreshTrigger = refreshTrigger,
                        onRefresh = onRefresh,
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
                        onDefineControls = { showControllerPage = true },
                        onEditTouchLayout = { showTouchEditorPage = true },
                        onAdvancedSettings = { showAdvancedPage = true },
                        onGraphicsSettings = { showGraphicsPage = true },
                        onEnginePreferences = { showEnginePrefsPage = true },
                        onEditAutoselect = { showAutoselectPage = true },
                    )

                    Spacer(modifier = Modifier.height(16.dp))

                    // ── Game selection toggle ────────────────
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
                                modifier = Modifier.weight(1f),
                            )
                            FilterChip(
                                selected = selectedGame == "d2",
                                onClick = {
                                    selectedGame = "d2"
                                    gamePrefs.edit().putString("selected_game", "d2").apply()
                                },
                                label = { Text("Descent 2") },
                                modifier = Modifier.weight(1f),
                            )
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    Button(
                        onClick = { showMultiplayerPage = true },
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .height(40.dp)
                                .focusRequester(initialFocus),
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
                        onClick = { onLaunchGame(selectedGame) },
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

                // ── Layout: landscape = side-by-side, portrait = stacked ──

                if (isLandscape) {
                    Row(modifier = Modifier.weight(1f)) {
                        val leftScroll = rememberScrollState()
                        Box(modifier = Modifier.weight(1f).fillMaxHeight()) {
                            Column(
                                modifier =
                                    Modifier
                                        .fillMaxSize()
                                        .verticalScroll(leftScroll)
                                        .padding(end = 8.dp),
                            ) {
                                filesPane()
                            }
                            ScrollArrows(leftScroll)
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
                            ScrollArrows(rightScroll)
                        }
                    }
                } else {
                    val portraitScroll = rememberScrollState()
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
                        ScrollArrows(portraitScroll)
                    }
                }
            }
        }
    }
}

@Composable
private fun BoxScope.ScrollArrows(scrollState: ScrollState) {
    if (scrollState.canScrollBackward) {
        Surface(
            modifier = Modifier.align(Alignment.TopCenter).padding(top = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowUp,
                contentDescription = "Scroll up",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
    if (scrollState.canScrollForward) {
        Surface(
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowDown,
                contentDescription = "Scroll down",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun GameSectionHeader(
    title: String,
    ready: Boolean,
    expanded: Boolean,
    onToggle: () -> Unit,
    notReadyLabel: String = "\u2717 Missing",
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = title,
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = if (ready) "\u2713 Ready" else notReadyLabel,
            color = if (ready) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(modifier = Modifier.width(8.dp))
        TextButton(
            onClick = onToggle,
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                text = if (expanded) "Hide Files" else "Show Files",
                fontSize = 12.sp,
            )
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )
}

@Composable
private fun SectionHeader(title: String) {
    Text(
        text = title,
        fontSize = 15.sp,
        fontWeight = FontWeight.SemiBold,
        color = MaterialTheme.colorScheme.onSurface,
        modifier = Modifier.padding(bottom = 4.dp, top = 2.dp),
    )
}

@Composable
private fun ModsSection(
    filesDir: File,
    refreshTrigger: Int,
) {
    val context = LocalContext.current
    val modManager = remember { ModManager(filesDir) }
    var mods by remember { mutableStateOf(modManager.listMods()) }
    var expanded by remember { mutableStateOf(false) }
    var deleteTarget by remember { mutableStateOf<String?>(null) }
    val modDownloadProgress = remember { mutableStateMapOf<String, Int>() }
    // Cache DXA scan results per filename
    val scanCache = remember { mutableStateMapOf<String, DxaTextureScanner.ScanResult?>() }
    val scope = rememberCoroutineScope()

    fun logOversizedTextureScan(
        mod: ModManager.ModInfo,
        file: File,
        scanResult: DxaTextureScanner.ScanResult,
    ) {
        if (scanResult.oversizedEntries.isEmpty()) return
        val details =
            scanResult.oversizedEntries.joinToString(" | ") {
                "${it.name} ${it.width}x${it.height} pow2=${it.pow2Width}x${it.pow2Height}"
            }
        LauncherDebugLog.log(
            "mod-dxa-oversized file=${mod.filename} bytes=${file.length()} " +
                "textures=${scanResult.textureCount} oversized=${scanResult.oversizedCount} " +
                "max=${scanResult.maxWidth}x${scanResult.maxHeight} entries=$details",
        )
    }

    LaunchedEffect(refreshTrigger) {
        modManager.reload()
        mods = modManager.listMods()
    }

    // Scan texture DXAs once when section expands
    LaunchedEffect(expanded) {
        if (expanded) {
            for (mod in mods) {
                if (mod.filename !in scanCache &&
                    mod.filename.contains("textur", ignoreCase = true)
                ) {
                    val file = File(filesDir, "mods/${mod.filename}")
                    val scanResult =
                        withContext(kotlinx.coroutines.Dispatchers.IO) {
                            DxaTextureScanner.scan(file)
                        }
                    if (scanResult != null && scanResult.oversizedCount > 0) {
                        logOversizedTextureScan(mod, file, scanResult)
                    }
                    scanCache[mod.filename] = scanResult
                }
            }
        }
    }

    val enabledCount = mods.count { it.enabled }
    val totalCount = mods.size
    val summary = if (totalCount == 0) "none" else "$enabledCount of $totalCount enabled"

    GameSectionHeader(
        title = "Mods",
        ready = true,
        expanded = expanded,
        onToggle = { expanded = !expanded },
        notReadyLabel = summary,
    )

    if (expanded) {
        if (mods.isEmpty()) {
            Text(
                "No mods installed",
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 8.dp),
            )
        } else {
            Text(
                summary,
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
            )
            mods.forEachIndexed { index, mod ->
                ModRow(
                    mod = mod,
                    isFirst = index == 0,
                    isLast = index == mods.size - 1,
                    scanResult = scanCache[mod.filename],
                    onToggle = { enabled ->
                        modManager.setEnabled(mod.filename, enabled)
                        mods = modManager.listMods()
                    },
                    onMoveUp = {
                        modManager.moveUp(index)
                        mods = modManager.listMods()
                    },
                    onMoveDown = {
                        modManager.moveDown(index)
                        mods = modManager.listMods()
                    },
                    onDelete = { deleteTarget = mod.filename },
                )
            }
        }

        // Recommended mods (show ones not already installed)
        val installedNames = mods.map { it.filename.lowercase() }.toSet()
        val uninstalled = RECOMMENDED_MODS.filter { it.filename.lowercase() !in installedNames }
        if (uninstalled.isNotEmpty()) {
            Spacer(modifier = Modifier.height(8.dp))
            Text(
                "Recommended",
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
            )
            uninstalled.forEach { rec ->
                RecommendedModRow(
                    rec = rec,
                    progress = modDownloadProgress[rec.filename],
                    onDownload = {
                        val modsDir = File(filesDir, "mods")
                        scope.launch {
                            downloadFile(
                                url = rec.downloadUrl,
                                destDir = modsDir,
                                filename = rec.filename,
                                onProgress = { pct ->
                                    modDownloadProgress[rec.filename] = pct
                                },
                                onDone = { success ->
                                    modDownloadProgress[rec.filename] =
                                        if (success) -2 else -1
                                    if (success) {
                                        val file = File(modsDir, rec.filename)
                                        modManager.importCompleted(
                                            rec.filename,
                                            rec.displayName,
                                            file.length(),
                                            rec.game,
                                        )
                                        mods = modManager.listMods()
                                    } else {
                                        Toast
                                            .makeText(
                                                context,
                                                "Download failed: ${rec.filename}",
                                                Toast.LENGTH_LONG,
                                            ).show()
                                    }
                                },
                            )
                        }
                    },
                )
            }
        }
    }

    // Delete confirmation dialog
    deleteTarget?.let { filename ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text("Delete Mod") },
            text = { Text("Remove $filename? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    modManager.deleteMod(filename)
                    mods = modManager.listMods()
                    deleteTarget = null
                }) { Text("Delete") }
            },
            dismissButton = {
                TextButton(onClick = { deleteTarget = null }) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun DemosSection(
    setDir: File,
    refreshTrigger: Int,
    onRefresh: () -> Unit,
) {
    val demosDir = File(setDir, "demos")
    var demoFiles by remember { mutableStateOf(emptyList<File>()) }
    var expanded by remember { mutableStateOf(false) }
    var deleteAllConfirm by remember { mutableStateOf(false) }
    var deleteSingleTarget by remember { mutableStateOf<File?>(null) }

    LaunchedEffect(refreshTrigger) {
        demoFiles =
            (
                demosDir.listFiles()?.filter {
                    it.isFile && it.name.lowercase().endsWith(".dem")
                } ?: emptyList()
            ).sortedBy { it.name.lowercase() }
    }

    if (demoFiles.isEmpty()) return

    val totalSize = demoFiles.sumOf { it.length() }
    val summary = "${demoFiles.size} demos, ${formatSize(totalSize)}"

    Spacer(modifier = Modifier.height(16.dp))

    // Header row with title, summary, expand toggle, and delete-all X
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "Demos",
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = summary,
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(modifier = Modifier.width(8.dp))
        TextButton(
            onClick = { expanded = !expanded },
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                text = if (expanded) "Hide" else "Show",
                fontSize = 12.sp,
            )
        }
        TextButton(
            onClick = { deleteAllConfirm = true },
            contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
            modifier = Modifier.height(24.dp),
        ) {
            Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )

    if (expanded) {
        demoFiles.forEach { file ->
            Row(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .padding(start = 8.dp, bottom = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = file.name,
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.weight(1f),
                )
                Text(
                    text = formatSize(file.length()),
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                TextButton(
                    onClick = { deleteSingleTarget = file },
                    contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                    modifier = Modifier.height(24.dp),
                ) {
                    Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
                }
            }
        }
    }

    // Delete all confirmation
    if (deleteAllConfirm) {
        AlertDialog(
            onDismissRequest = { deleteAllConfirm = false },
            title = { Text("Delete All Demos") },
            text = { Text("Remove all ${demoFiles.size} demo files? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    demoFiles.forEach { it.delete() }
                    deleteAllConfirm = false
                    onRefresh()
                }) { Text("Delete") }
            },
            dismissButton = {
                TextButton(onClick = { deleteAllConfirm = false }) { Text("Cancel") }
            },
        )
    }

    // Delete single confirmation
    deleteSingleTarget?.let { file ->
        AlertDialog(
            onDismissRequest = { deleteSingleTarget = null },
            title = { Text("Delete Demo") },
            text = { Text("Remove ${file.name}? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    file.delete()
                    deleteSingleTarget = null
                    onRefresh()
                }) { Text("Delete") }
            },
            dismissButton = {
                TextButton(onClick = { deleteSingleTarget = null }) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun ModRow(
    mod: ModManager.ModInfo,
    isFirst: Boolean,
    isLast: Boolean,
    scanResult: DxaTextureScanner.ScanResult? = null,
    onToggle: (Boolean) -> Unit,
    onMoveUp: () -> Unit,
    onMoveDown: () -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(start = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Checkbox(
            checked = mod.enabled,
            onCheckedChange = onToggle,
            modifier = Modifier.size(20.dp),
        )
        Spacer(modifier = Modifier.width(6.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = mod.displayName,
                fontSize = 12.sp,
                color =
                    if (mod.enabled) {
                        MaterialTheme.colorScheme.onSurface
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    },
            )
            Text(
                text = "${formatSize(mod.sizeBytes)} - ${mod.game.uppercase()}",
                fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (scanResult != null && scanResult.oversizedCount > 0) {
                Text(
                    text =
                        "${scanResult.oversizedCount} of ${scanResult.textureCount} textures exceed " +
                            "${DxaTextureScanner.ENGINE_TEXTURE_CAP}px " +
                            "(max ${scanResult.maxWidth}x${scanResult.maxHeight}) -- will be skipped",
                    fontSize = 10.sp,
                    color = Color(0xFFF44336),
                )
            } else if (scanResult != null && scanResult.textureCount > 0) {
                Text(
                    text = "${scanResult.textureCount} textures, max ${scanResult.maxWidth}x${scanResult.maxHeight}",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        // Move up
        if (!isFirst) {
            IconButton(onClick = onMoveUp, modifier = Modifier.size(24.dp)) {
                Icon(Icons.Filled.KeyboardArrowUp, "Move up", modifier = Modifier.size(16.dp))
            }
        } else {
            Spacer(modifier = Modifier.size(24.dp))
        }
        // Move down
        if (!isLast) {
            IconButton(onClick = onMoveDown, modifier = Modifier.size(24.dp)) {
                Icon(Icons.Filled.KeyboardArrowDown, "Move down", modifier = Modifier.size(16.dp))
            }
        } else {
            Spacer(modifier = Modifier.size(24.dp))
        }
        // Delete
        TextButton(
            onClick = onDelete,
            contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
            modifier = Modifier.height(24.dp),
        ) {
            Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
        }
    }
}

@Composable
private fun RecommendedModRow(
    rec: RecommendedMod,
    progress: Int?, // null = not started, 0..100 = %, -1 = error, -2 = done
    onDownload: () -> Unit,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(start = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(rec.displayName, fontSize = 12.sp)
            Text(
                "${rec.description} - ${rec.game.uppercase()}",
                fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Spacer(modifier = Modifier.width(8.dp))
        when (progress) {
            null ->
                Button(
                    onClick = onDownload,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp),
                ) { Text("Download", fontSize = 11.sp) }
            in 0..100 ->
                Text(
                    "$progress%",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.width(40.dp),
                )
            -1 -> Text("Error", fontSize = 12.sp, color = Color(0xFFF44336))
            -2 ->
                Text(
                    "\u2713",
                    fontSize = 14.sp,
                    color = Color(0xFF4CAF50),
                    fontWeight = FontWeight.Bold,
                )
        }
    }
}

@Composable
private fun MusicInfoSection(
    filesDir: File,
    setDir: File,
    refreshTrigger: Int,
    hasMidiSource: Boolean = false,
    onEditMusic: () -> Unit = {},
) {
    val audioSrcManager = remember { AudioSourceManager(filesDir) }
    var audioSources by remember { mutableStateOf(audioSrcManager.getSources()) }
    val hasCdAudio = audioSources.isNotEmpty()
    var expanded by remember { mutableStateOf(false) }
    var detailStatus by remember { mutableStateOf<FileStatus?>(null) }

    // Re-read sources when refreshTrigger changes
    LaunchedEffect(refreshTrigger) { audioSources = audioSrcManager.getSources() }

    // Read current music mode from prefs for display
    val context = LocalContext.current
    val prefs = context.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val musicMode = prefs.getString("music_mode", "cd") ?: "cd"
    val modeLabel =
        when (musicMode) {
            "midi" -> "MIDI"
            "cd" -> "CD Audio"
            "files" -> "Audio Files"
            else -> "CD Audio"
        }

    // Status reflects the currently selected music mode
    val musicReady =
        when (musicMode) {
            "midi" -> hasMidiSource
            "cd" -> hasCdAudio
            "files" -> true // custom files are optional
            else -> hasCdAudio
        }

    val musicLabel =
        when {
            musicReady -> "\u2713 Ready"
            musicMode == "cd" && hasMidiSource -> "\u2717 Missing, will use MIDI"
            else -> "\u2717 Missing"
        }

    // Custom header: "Music" [mode label] [status] [Edit] [Show Files]
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "Music",
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = modeLabel,
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(modifier = Modifier.weight(1f))
        Text(
            text = musicLabel,
            color = if (musicReady) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(modifier = Modifier.width(4.dp))
        TextButton(
            onClick = onEditMusic,
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text("Edit", fontSize = 12.sp)
        }
        TextButton(
            onClick = { expanded = !expanded },
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                text = if (expanded) "Hide" else "Files",
                fontSize = 12.sp,
            )
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )
    if (expanded) {
        Text(
            text =
                "MIDI audio is supported from game files. " +
                    "Redbook audio from BIN/CUE disc images is supported.",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(start = 4.dp, end = 4.dp, bottom = 8.dp),
        )
        // Registered audio sources
        if (audioSources.isNotEmpty()) {
            Spacer(modifier = Modifier.height(8.dp))
            Text(
                "Audio Sources:",
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
                modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
            )
            audioSources.forEachIndexed { index, src ->
                Row(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .padding(start = 8.dp, bottom = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    // Enable/disable toggle
                    Checkbox(
                        checked = src.enabled,
                        onCheckedChange = { checked ->
                            audioSrcManager.setEnabled(src.id, checked)
                            audioSources = audioSrcManager.getSources()
                        },
                        modifier = Modifier.size(20.dp),
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "${src.discLabel} (${src.audioTrackCount} tracks)",
                        fontSize = 12.sp,
                        color =
                            if (src.enabled) {
                                MaterialTheme.colorScheme.onSurface
                            } else {
                                MaterialTheme.colorScheme.onSurfaceVariant
                            },
                        modifier = Modifier.weight(1f),
                    )
                    // Move up
                    if (index > 0) {
                        IconButton(
                            onClick = {
                                val ids = audioSources.map { it.id }.toMutableList()
                                ids[index] = ids[index - 1].also { ids[index - 1] = ids[index] }
                                audioSrcManager.reorder(ids)
                                audioSources = audioSrcManager.getSources()
                            },
                            modifier = Modifier.size(24.dp),
                        ) {
                            Icon(
                                Icons.Filled.KeyboardArrowUp,
                                "Move up",
                                modifier = Modifier.size(16.dp),
                            )
                        }
                    } else {
                        Spacer(modifier = Modifier.size(24.dp))
                    }
                    // Move down
                    if (index < audioSources.size - 1) {
                        IconButton(
                            onClick = {
                                val ids = audioSources.map { it.id }.toMutableList()
                                ids[index] = ids[index + 1].also { ids[index + 1] = ids[index] }
                                audioSrcManager.reorder(ids)
                                audioSources = audioSrcManager.getSources()
                            },
                            modifier = Modifier.size(24.dp),
                        ) {
                            Icon(
                                Icons.Filled.KeyboardArrowDown,
                                "Move down",
                                modifier = Modifier.size(16.dp),
                            )
                        }
                    } else {
                        Spacer(modifier = Modifier.size(24.dp))
                    }
                    // Remove
                    TextButton(
                        onClick = {
                            audioSrcManager.removeSource(src.id)
                            audioSources = audioSrcManager.getSources()
                        },
                        contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                        modifier = Modifier.height(24.dp),
                    ) {
                        Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
                    }
                }
            }
        }
    }
    detailStatus?.let { st ->
        FileDetailDialog(status = st, onDismiss = { detailStatus = null })
    }
}

/** Format epoch millis as a human-readable date/time string. */
private fun formatTimestamp(millis: Long): String {
    val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US)
    return sdf.format(Date(millis))
}

/** Format byte size as human-readable (KB, MB, GB). */
private fun formatSize(bytes: Long): String =
    when {
        bytes >= 1_073_741_824 -> "%.2f GB".format(bytes / 1_073_741_824.0)
        bytes >= 1_048_576 -> "%.1f MB".format(bytes / 1_048_576.0)
        bytes >= 1_024 -> "%.0f KB".format(bytes / 1_024.0)
        else -> "$bytes B"
    }

@Composable
private fun FileDetailDialog(
    status: FileStatus,
    onDismiss: () -> Unit,
    onDelete: (() -> Unit)? = null,
) {
    val entry = status.manifestEntry
    val name = status.foundName ?: status.info.filename
    val description = descriptionForFile(name)
    val isMissing = !status.found && entry != null
    val isExternal = entry?.isExternal == true
    var confirmingDelete by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        dismissButton =
            if (onDelete != null) {
                {
                    if (status.safUri != null) {
                        // SAF leave-in-place: single-step "Unlink"
                        TextButton(onClick = onDelete) {
                            Text("Unlink", color = MaterialTheme.colorScheme.error)
                        }
                    } else if (isExternal) {
                        // External files: single-step "Forget"
                        TextButton(onClick = onDelete) {
                            Text("Forget", color = MaterialTheme.colorScheme.error)
                        }
                    } else if (!confirmingDelete) {
                        // Data-dir files: first step
                        TextButton(onClick = { confirmingDelete = true }) {
                            Text("Delete from data folder?", color = MaterialTheme.colorScheme.error)
                        }
                    } else {
                        // Data-dir files: confirmation step
                        TextButton(onClick = onDelete) {
                            Text("Are you sure? Delete", color = MaterialTheme.colorScheme.error)
                        }
                    }
                }
            } else {
                null
            },
        title = {
            Text(name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            val scrollState = rememberScrollState()
            Box {
                Column(modifier = Modifier.verticalScroll(scrollState)) {
                    // Category / description
                    DetailRow("Category", description)
                    DetailRow("Type", describeExtension(name))

                    // Status
                    val statusText =
                        when {
                            status.found -> "Found"
                            isMissing -> "Error: not found"
                            else -> "Missing"
                        }
                    DetailRow("Status", statusText)
                    if (status.info.required) {
                        DetailRow("Required", "Yes")
                    }

                    // Location
                    if (status.safUri != null) {
                        DetailRow("Location", "leave-in-place (linked)")
                        if (status.safSizeBytes > 0) {
                            DetailRow("Size", formatSize(status.safSizeBytes))
                        }
                    } else if (isExternal && entry.sourceUri != null) {
                        DetailRow("Location", entry.sourceUri)
                    } else if (entry != null) {
                        DetailRow("Location", "(in data folder)")
                    }

                    if (entry != null) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                        // File details from manifest
                        DetailRow("File on disk", entry.filename)
                        DetailRow("Size", formatSize(entry.sizeBytes))
                        DetailRow("Imported", formatTimestamp(entry.importedAt))

                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                        // SHA-256 (full, selectable)
                        Text(
                            "SHA-256",
                            fontSize = 11.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        SelectionContainer {
                            Text(
                                entry.sha256,
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurface,
                                modifier = Modifier.padding(bottom = 4.dp),
                            )
                        }

                        // Version match
                        if (entry.versionName != null) {
                            DetailRow("Version match", entry.versionName)
                        } else {
                            DetailRow("Version match", "Unknown (#${entry.shortHash})")
                        }

                        if (isMissing) {
                            HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                            Text(
                                "This file was previously imported but is no longer on disk.",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.error,
                            )
                        }
                    } else if (!status.found) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                        if (status.info.alternatives.isNotEmpty()) {
                            DetailRow(
                                "Alternatives",
                                status.info.alternatives.joinToString(", "),
                            )
                        }
                        if (status.info.downloadUrl != null) {
                            DetailRow("Download", status.info.downloadUrl)
                        }
                    }
                }
                ScrollArrows(scrollState)
            }
        },
    )
}

@Composable
private fun DetailRow(
    label: String,
    value: String,
) {
    Row(modifier = Modifier.padding(vertical = 2.dp)) {
        Text(
            "$label: ",
            fontSize = 12.sp,
            fontWeight = FontWeight.SemiBold,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text(
            value,
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurface,
        )
    }
}

/**
 * Update ResolutionX/ResolutionY in descent.cfg.
 * If the file exists, replace existing lines; otherwise create with just those keys.
 */
internal fun updateDescentCfgResolution(
    filesDir: File,
    resolution: String,
) {
    val parts = resolution.split("x")
    val w = parts.getOrNull(0)?.toIntOrNull() ?: return
    val h = parts.getOrNull(1)?.toIntOrNull() ?: return
    updateAllConfigFiles(filesDir, listOf("ResolutionX" to "$w", "ResolutionY" to "$h"))
}

/**
 * Read a key from descent.cfg. Checks d2x-redux/ first, then d1x-redux/, then root.
 * Returns null if not found.
 */
internal fun readConfigValue(
    filesDir: File,
    key: String,
): String? {
    for (sub in listOf("d2x-redux", "d1x-redux", "")) {
        val cfgFile = if (sub.isEmpty()) File(filesDir, "descent.cfg") else File(File(filesDir, sub), "descent.cfg")
        if (!cfgFile.exists()) continue
        val regex = Regex("^$key=(.*)$", RegexOption.MULTILINE)
        val match = regex.find(cfgFile.readText()) ?: continue
        return match.groupValues[1].trim()
    }
    return null
}

/**
 * Apply key=value settings to all descent.cfg files: root (first-launch fallback),
 * d1x-redux/ and d2x-redux/ (per-game configs created after first run).
 * Each game's PHYSFS reads only its own subdir config, so we must write to all.
 */
internal fun updateAllConfigFiles(
    filesDir: File,
    settings: List<Pair<String, String>>,
) {
    val cfgPaths = mutableListOf(File(filesDir, "descent.cfg"))
    for (sub in listOf("d1x-redux", "d2x-redux")) {
        val dir = File(filesDir, sub)
        if (dir.isDirectory) cfgPaths.add(File(dir, "descent.cfg"))
    }
    for (cfgFile in cfgPaths) {
        var text = if (cfgFile.exists()) cfgFile.readText() else ""
        for ((key, value) in settings) {
            val regex = Regex("^$key=.*$", RegexOption.MULTILINE)
            text =
                if (regex.containsMatchIn(text)) {
                    regex.replace(text, "$key=$value")
                } else {
                    text.trimEnd() + "\n$key=$value\n"
                }
        }
        cfgFile.writeText(text)
    }
    Log.i(
        "DXX-Setup",
        "Updated ${cfgPaths.size} descent.cfg files: ${settings.joinToString { "${it.first}=${it.second}" }}",
    )
}

/**
 * Set MusicType=2 (REDBOOK) and OrigTrackOrder=1 in descent.cfg after GOG audio import.
 * Also sets the launcher's music_mode pref to "cd" so the Music tab reflects the change.
 * Mirrors the C engine's android_apply_initial_defaults() which only runs on first launch.
 * MUSIC_TYPE_REDBOOK = 2 (shared constant, defined in d2/main/digi.h)
 */
private fun enableRedbookInConfig(
    filesDir: File,
    context: Context,
) {
    updateAllConfigFiles(filesDir, listOf("MusicType" to "2", "OrigTrackOrder" to "1"))
    context
        .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        .edit()
        .putString("music_mode", "cd")
        .apply()
    Log.i("DXX-Setup", "Set music_mode=cd in SharedPreferences")
}

/**
 * Find a GOG .gog/.inst pair in a directory (case-insensitive).
 * Returns the actual base filename preserving disk case (e.g. "DESCENT_II") or null.
 */
private fun findGogPair(dir: File): String? {
    val files = dir.list() ?: return null
    val gogFile = files.firstOrNull { it.equals("descent_ii.gog", ignoreCase = true) } ?: return null
    val instFile = files.firstOrNull { it.equals("descent_ii.inst", ignoreCase = true) } ?: return null
    return gogFile.substringBeforeLast('.')
}

/**
 * Register a GOG .gog/.inst pair as a BIN/CUE audio source.
 * The .gog file is the BIN image and the .inst file is the CUE sheet.
 * Paths are stored relative to filesDir so the preview player can resolve them.
 */
private fun registerGogAudioSource(
    srcManager: AudioSourceManager,
    filesDir: File,
    setDir: File,
    context: Context? = null,
) {
    val base = findGogPair(setDir) ?: return
    val relDir = setDir.toRelativeString(filesDir)
    val relBase = if (relDir.isEmpty()) base else "$relDir${File.separator}$base"
    // Look up track names from known_discs.json5 if context available
    val trackNames =
        context?.let {
            try {
                FingerprintBridge.lookupTrackNames(it, "d2-gog-v1.2")
            } catch (e: Exception) {
                emptyMap()
            }
        } ?: emptyMap()
    srcManager.addSource(
        AudioSourceManager.AudioSource(
            id = "d2-gog-v1.2",
            cuePath = "$relBase.inst",
            binPaths = listOf("$relBase.gog"),
            discLabel = "Descent II (GOG)",
            discId = "d2-gog-v1.2",
            trackCount = 9,
            audioTrackCount = 8,
            legacyDiscId = 0x7d0ff809L,
            trackNames = trackNames,
        ),
    )
}

private fun extractSowArchives(setDir: File): Int {
    val sowFiles = DiscImportBridge.scanSowFiles(setDir.absolutePath) ?: return 0
    var sowExtracted = 0
    for (sow in sowFiles) {
        sowExtracted += DiscImportBridge.extractSowFiles(sow, setDir.absolutePath, null).coerceAtLeast(0)
    }
    return sowExtracted
}

private fun registerDiscAudioSourceFromPath(
    srcManager: AudioSourceManager,
    filesDir: File,
    context: Context,
    cuePath: String,
    binPath: String,
    tracks: List<DiscImportBridge.CueTrack>,
) {
    var discLabel: String? = null
    var discId: String? = null
    var legacyDiscId = 0L
    val firstAudio = tracks.firstOrNull { it.isAudio }
    val trackNames = mutableMapOf<Int, String>()

    if (firstAudio != null) {
        try {
            val identifier = DiscIdentifier(context)
            val trackOffset = firstAudio.startSector.toLong() * 2352L
            val trackBytes = firstAudio.numSectors.toLong() * 2352L
            File(binPath).inputStream().use { input ->
                input.channel.position(trackOffset)
                val sha1 = DiscIdentifier.sha1Hash(input, trackBytes)
                val match = identifier.identify(mapOf(firstAudio.trackNum to sha1))
                if (match.matched) {
                    discLabel = match.label
                    discId = match.disc?.id
                    match.disc?.legacyDiscId?.let {
                        legacyDiscId = java.lang.Long.decode(it)
                    }
                }
            }
        } catch (e: Exception) {
            Log.w("DXX-DiscImport", "Disc identification failed for $cuePath", e)
        }
    }

    try {
        if (discId != null) {
            trackNames.putAll(FingerprintBridge.lookupTrackNames(context, discId!!))
        }
        if (trackNames.isEmpty() && tracks.any { it.isAudio }) {
            trackNames.putAll(FingerprintBridge.fingerprintAndMatchDisc(context, binPath, tracks))
        }
    } catch (e: Exception) {
        Log.w("DXX-DiscImport", "Track name identification failed for $cuePath", e)
    }

    val id = discId ?: "custom-${System.currentTimeMillis()}"
    val destCue = File(filesDir, "$id.cue")
    File(cuePath).copyTo(destCue, overwrite = true)
    srcManager.addSource(
        AudioSourceManager.AudioSource(
            id = id,
            cuePath = destCue.name,
            binPaths = listOf(File(binPath).name.lowercase()),
            discLabel = discLabel ?: File(cuePath).nameWithoutExtension,
            discId = discId ?: "unknown",
            trackCount = tracks.size,
            audioTrackCount = tracks.count { it.isAudio },
            legacyDiscId = legacyDiscId,
            trackNames = trackNames,
            binContentUri = binPath,
        ),
    )
}

private fun importDiscImageFromPath(
    filesDir: File,
    setDir: File,
    context: Context,
    cuePath: String,
    binPath: String,
    includeAudio: Boolean,
): Int {
    val cueFile = File(cuePath)
    val binFile = File(binPath)

    if (!cueFile.isFile || !binFile.isFile) {
        Log.w("DXX-DiscImport", "importDiscImageFromPath: missing cue/bin ($cuePath, $binPath)")
        return -1
    }

    val tracks = DiscImportBridge.parseCue(cueFile.absolutePath, longArrayOf(binFile.length()))
    if (tracks.isNullOrEmpty()) {
        Log.w("DXX-DiscImport", "importDiscImageFromPath: parseCue failed for $cuePath")
        return -1
    }

    val dataTrack = tracks.firstOrNull { it.isData }
    if (dataTrack == null || dataTrack.fileIndex != 0) {
        Log.w(
            "DXX-DiscImport",
            "importDiscImageFromPath: unsupported data track mapping for $cuePath (fileIndex=${dataTrack?.fileIndex})",
        )
        return -1
    }

    val isoExtracted =
        DiscImportBridge.extractIsoFiles(
            binFile.absolutePath,
            dataTrack.startSector,
            dataTrack.numSectors,
            setDir.absolutePath,
            null,
        )
    val macExtracted =
        if (isoExtracted > 0) {
            0
        } else {
            DiscImportBridge.extractMacFiles(
                binFile.absolutePath,
                dataTrack.startSector,
                dataTrack.numSectors,
                setDir.absolutePath,
                null,
            )
        }

    var sowExtracted = 0
    if (isoExtracted > 0) {
        sowExtracted = extractSowArchives(setDir)
    }

    val extracted = if (isoExtracted > 0) isoExtracted else macExtracted.coerceAtLeast(0)
    if (includeAudio && extracted > 0 && tracks.any { it.isAudio }) {
        registerDiscAudioSourceFromPath(
            srcManager = AudioSourceManager(filesDir),
            filesDir = filesDir,
            context = context,
            cuePath = cueFile.absolutePath,
            binPath = binFile.absolutePath,
            tracks = tracks,
        )
        enableRedbookInConfig(filesDir, context)
    }

    Log.i(
        "DXX-DiscImport",
        "importDiscImageFromPath: cue=$cuePath iso=$isoExtracted mac=$macExtracted sow=$sowExtracted audio=$includeAudio",
    )
    return extracted + sowExtracted
}

private fun importIsoImageFromPath(
    setDir: File,
    isoPath: String,
): Int {
    val isoFile = File(isoPath)

    if (!isoFile.isFile) {
        Log.w("DXX-DiscImport", "importIsoImageFromPath: missing iso ($isoPath)")
        return -1
    }

    val isoExtracted = DiscImportBridge.extractIsoImageFiles(isoFile.absolutePath, setDir.absolutePath, null)
    val sowExtracted = if (isoExtracted > 0) extractSowArchives(setDir) else 0

    Log.i(
        "DXX-DiscImport",
        "importIsoImageFromPath: iso=$isoPath files=$isoExtracted sow=$sowExtracted",
    )
    return if (isoExtracted < 0) isoExtracted else isoExtracted + sowExtracted
}

/**
 * Write music mode config to descent.cfg based on the user's music_mode pref.
 *
 * Music mode constants (shared with d2/main/digi.h):
 *   MUSIC_TYPE_NONE=0, MUSIC_TYPE_BUILTIN=1 (MIDI),
 *   MUSIC_TYPE_REDBOOK=2 (CD), MUSIC_TYPE_CUSTOM=3 (jukebox)
 *
 * For Custom mode, also writes an M3U playlist and sets CMLevelMusicPath.
 */
private fun SetupActivity.writeMusicConfigForLaunch() {
    val prefs = getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val mode = prefs.getString("music_mode", "cd") ?: "cd"
    val musicType =
        when (mode) {
            "midi" -> "1"
            "cd" -> "2"
            "files" -> "3"
            else -> "2"
        }

    // Build settings list
    val settings = mutableListOf("MusicType" to musicType)

    if (mode == "cd") {
        settings.add("OrigTrackOrder" to "1")
    } else if (mode == "files") {
        // Generate M3U playlist from custom audio sets
        val m3uPath = CustomAudioSetManager(filesDir).writeM3U(this)
        if (m3uPath != null) {
            settings.add("CMLevelMusicPath" to m3uPath)
            settings.add("CMLevelMusicPlayOrder" to "0") // continuous
        }
    }

    updateAllConfigFiles(filesDir, settings)
}

@Composable
private fun ControllerSection(
    axes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    prefs: SharedPreferences,
    selectedGame: String = "d2",
    onDefineControls: () -> Unit = {},
    onEditTouchLayout: () -> Unit = {},
    onAdvancedSettings: () -> Unit = {},
    onGraphicsSettings: () -> Unit = {},
    onEnginePreferences: () -> Unit = {},
    onEditAutoselect: () -> Unit = {},
) {
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

    val hasController = gamepads.isNotEmpty()
    var expanded by remember { mutableStateOf(false) }

    // ── Header ──
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
            text =
                if (hasController) {
                    "\u2713 ${gamepads.first().name}"
                } else {
                    "\u2717 Not detected"
                },
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

    // ── Touch overlay toggle ──
    val defaultOverlay = !hasController
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
            modifier = Modifier.height(24.dp),
        )
        Spacer(modifier = Modifier.width(4.dp))
        Text(
            text = "Touch controls overlay",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurface,
        )
    }

    // ── In-game orientation lock ──
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

    // ── Define Controls button ──
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = onDefineControls,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Define Controls", fontSize = 12.sp)
        }
        OutlinedButton(
            onClick = onEditTouchLayout,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Touch Layout", fontSize = 12.sp)
        }
    }

    // ── Weapon Autoselect / Game Preferences / Graphics / Advanced ──
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

@Composable
private fun FileStatusRow(
    status: FileStatus,
    onClick: (() -> Unit)? = null,
) {
    val isMissing = !status.found && status.manifestEntry != null
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .let { if (onClick != null) it.clickable(onClick = onClick) else it }
                .padding(vertical = 1.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text =
                when {
                    status.found -> "\u2713"
                    isMissing -> "\u26A0"
                    else -> "\u2717"
                },
            color =
                when {
                    status.found -> Color(0xFF4CAF50)
                    isMissing -> Color(0xFFFF9800) // orange warning
                    else -> Color(0xFFF44336)
                },
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp),
        )

        val name = status.foundName ?: status.info.filename
        val altHint =
            if (!status.found && !isMissing && status.info.alternatives.isNotEmpty()) {
                " (or ${status.info.alternatives.joinToString(", ")})"
            } else {
                ""
            }
        val versionHint =
            if (status.found && status.manifestEntry != null) {
                " [${status.manifestEntry.versionDisplay}]"
            } else {
                ""
            }
        val missingHint = if (isMissing) " [Error: not found]" else ""
        Text(
            text = "$name \u2014 ${status.info.description}$altHint$versionHint$missingHint",
            color =
                when {
                    status.found -> MaterialTheme.colorScheme.onSurface
                    isMissing -> Color(0xFFFF9800)
                    else -> MaterialTheme.colorScheme.onSurfaceVariant
                },
            fontSize = 13.sp,
            maxLines = 1,
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
private fun DownloadableFileRow(
    status: FileStatus,
    progress: Int?, // null = not started, 0..100 = %, -1 = error, -2 = done
    onDownload: () -> Unit,
    onInfo: (() -> Unit)? = null,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "\u2717",
            color = Color(0xFFFFA726), // orange for optional missing
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp),
        )

        Text(
            text = "${status.info.filename} \u2014 ${status.info.description}",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 13.sp,
            maxLines = 1,
            modifier =
                Modifier
                    .weight(1f)
                    .then(if (onInfo != null) Modifier.clickable(onClick = onInfo) else Modifier),
        )

        Spacer(modifier = Modifier.width(8.dp))

        when (progress) {
            null -> {
                // Not started — show download button
                Button(
                    onClick = onDownload,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Download", fontSize = 11.sp)
                }
            }
            in 0..100 -> {
                // Downloading — show progress
                Text(
                    text = "$progress%",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.width(40.dp),
                )
            }
            -1 -> {
                // Error
                Text(
                    text = "Error",
                    fontSize = 12.sp,
                    color = Color(0xFFF44336),
                )
            }
            -2 -> {
                // Done (will be replaced by FileStatusRow on refresh)
                Text(
                    text = "\u2713",
                    fontSize = 14.sp,
                    color = Color(0xFF4CAF50),
                    fontWeight = FontWeight.Bold,
                )
            }
        }
    }
}

@Composable
private fun MissingFilesHelp() {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors =
            CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.errorContainer,
            ),
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text(
                text = "Missing Required Files",
                fontWeight = FontWeight.Bold,
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text =
                    "Copy D2 files (from Steam/GOG) and/or D1 files to the app:\n" +
                        "  adb push <file> /data/data/com.dxxredux.app/files/\n" +
                        "Filenames are matched case-insensitively.\n" +
                        "Either Descent 2 or Descent 1 files are needed to launch.",
                color = MaterialTheme.colorScheme.onErrorContainer,
                fontSize = 12.sp,
                lineHeight = 16.sp,
            )
        }
    }
}

@Composable
private fun SetManagementDialog(
    fileSetManager: FileSetManager,
    activeSetName: String,
    onSwitchSet: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var newSetName by remember { mutableStateOf("") }
    var showNewSetInput by remember { mutableStateOf(false) }
    var confirmDelete by remember { mutableStateOf(false) }
    var errorMessage by remember { mutableStateOf<String?>(null) }

    val sets = remember { fileSetManager.listSets() }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = { Text("File Sets", fontWeight = FontWeight.Bold) },
        text = {
            Column {
                // Current set info
                Text(
                    "Current: $activeSetName",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                val usage = fileSetManager.diskUsage(activeSetName)
                Text(
                    "Size: ${formatSize(usage)}",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                // Other sets to switch to
                val otherSets = sets.filter { it.name != activeSetName }
                if (otherSets.isNotEmpty()) {
                    otherSets.forEach { set ->
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { onSwitchSet(set.name) }
                                    .padding(vertical = 8.dp, horizontal = 4.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                text = "Switch to \"${set.name}\"",
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.weight(1f),
                            )
                            Text(
                                text = formatSize(fileSetManager.diskUsage(set.name)),
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                }

                // Add new set
                if (showNewSetInput) {
                    OutlinedTextField(
                        value = newSetName,
                        onValueChange = {
                            newSetName = it
                            errorMessage = null
                        },
                        label = { Text("Set name") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                        isError = errorMessage != null,
                        supportingText = errorMessage?.let { { Text(it) } },
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = {
                            try {
                                fileSetManager.createSet(newSetName.trim())
                                onSwitchSet(newSetName.trim())
                            } catch (e: IllegalArgumentException) {
                                errorMessage = e.message
                            }
                        }) {
                            Text("Create", fontSize = 13.sp)
                        }
                        OutlinedButton(onClick = {
                            showNewSetInput = false
                            newSetName = ""
                        }) {
                            Text("Cancel", fontSize = 13.sp)
                        }
                    }
                } else {
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable { showNewSetInput = true }
                                .padding(vertical = 8.dp, horizontal = 4.dp),
                    ) {
                        Text(
                            text = "+ Add new set\u2026",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                }

                // Delete / clear current set
                HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                if (!confirmDelete) {
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable { confirmDelete = true }
                                .padding(vertical = 8.dp, horizontal = 4.dp),
                    ) {
                        Text(
                            text =
                                if (activeSetName == FileSetManager.DEFAULT_SET) {
                                    "Clear all files in \"$activeSetName\""
                                } else {
                                    "Delete \"$activeSetName\""
                                },
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.error,
                        )
                    }
                } else {
                    Column(modifier = Modifier.padding(horizontal = 4.dp)) {
                        Text(
                            text =
                                "Imported files (copied to app data) will be permanently deleted.\n\n" +
                                    "Files added via file picker (leave-in-place) will be unlinked " +
                                    "but not deleted from their original location.",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable {
                                        if (activeSetName == FileSetManager.DEFAULT_SET) {
                                            fileSetManager.clearSet(activeSetName)
                                        } else {
                                            fileSetManager.deleteSet(activeSetName)
                                        }
                                        onSwitchSet(FileSetManager.DEFAULT_SET)
                                    }.padding(vertical = 8.dp),
                        ) {
                            Text(
                                text =
                                    if (activeSetName == FileSetManager.DEFAULT_SET) {
                                        "Confirm clear \"$activeSetName\"?"
                                    } else {
                                        "Confirm delete \"$activeSetName\"?"
                                    },
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.error,
                                fontWeight = FontWeight.Bold,
                            )
                        }
                    }
                }
            }
        },
    )
}

// ── Download helper ─────────────────────────────────────────────────────────

private suspend fun downloadFile(
    url: String,
    destDir: File,
    filename: String,
    onProgress: (Int) -> Unit,
    onDone: (Boolean) -> Unit,
) {
    withContext(Dispatchers.IO) {
        try {
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.connectTimeout = 15_000
            conn.readTimeout = 30_000
            conn.connect()

            if (conn.responseCode != 200) {
                Log.e("DXX-Setup", "Download failed: HTTP ${conn.responseCode} for $url")
                withContext(Dispatchers.Main) { onDone(false) }
                return@withContext
            }

            val totalBytes = conn.contentLength.toLong()
            val tmpFile = File(destDir, "$filename.tmp")
            var downloaded = 0L

            conn.inputStream.use { input ->
                FileOutputStream(tmpFile).use { output ->
                    val buf = ByteArray(8192)
                    while (true) {
                        val n = input.read(buf)
                        if (n <= 0) break
                        output.write(buf, 0, n)
                        downloaded += n
                        if (totalBytes > 0) {
                            val pct = (downloaded * 100 / totalBytes).toInt().coerceIn(0, 100)
                            withContext(Dispatchers.Main) { onProgress(pct) }
                        }
                    }
                }
            }

            // Rename .tmp → final
            val destFile = File(destDir, filename)
            tmpFile.renameTo(destFile)
            Log.i("DXX-Setup", "Downloaded $filename ($downloaded bytes)")
            withContext(Dispatchers.Main) { onDone(true) }
        } catch (e: Exception) {
            Log.e("DXX-Setup", "Download error for $filename", e)
            withContext(Dispatchers.Main) { onDone(false) }
        }
    }
}

// ── GOG installer import dialog ───────────────────────────────────────────

/**
 * Dialog for importing a GOG installer (.exe InnoSetup or .pkg Mac).
 *
 * Flow:
 *  1. Copies installer to temp via content resolver
 *  2. Detects format (InnoSetup / .pkg)
 *  3. Lists game files inside the installer
 *  4. Extracts game files to setDir with progress
 *  5. Detects .gog/.inst audio pair after extraction
 */
@Composable
private fun GogImportDialog(
    installerName: String,
    installerUri: Uri,
    filesDir: File,
    setDir: File,
    context: Context,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var status by remember { mutableStateOf("Analyzing installer\u2026") }
    var format by remember { mutableStateOf<String?>(null) }
    var fileList by remember { mutableStateOf<List<GogImportBridge.GogFile>?>(null) }
    var processing by remember { mutableStateOf(false) }
    var extractedCount by remember { mutableIntStateOf(0) }
    var extractedFileNames by remember { mutableStateOf<List<String>>(emptyList()) }
    var progressFile by remember { mutableStateOf("") }
    var progressPct by remember { mutableStateOf(0f) }
    var tempPath by remember { mutableStateOf<String?>(null) }
    var errorMsg by remember { mutableStateOf<String?>(null) }
    var includeAudio by remember { mutableStateOf(true) }

    // Copy installer to temp + detect format + list files
    LaunchedEffect(installerUri) {
        withContext(Dispatchers.IO) {
            try {
                val tmpDir = File(filesDir, "tmp")
                tmpDir.mkdirs()
                val tmpFile = File(tmpDir, installerName)
                withContext(Dispatchers.Main) { status = "Copying installer\u2026" }
                context.contentResolver.openInputStream(installerUri)?.use { input ->
                    java.io.FileOutputStream(tmpFile).use { output -> input.copyTo(output, bufferSize = 65536) }
                }
                tempPath = tmpFile.absolutePath

                val fmt = GogImportBridge.detectFormat(tmpFile.absolutePath)
                val files = GogImportBridge.listFiles(tmpFile.absolutePath)
                withContext(Dispatchers.Main) {
                    format = fmt
                    fileList = files
                    if (fmt == "unknown") {
                        status = "Not a recognized GOG installer"
                        errorMsg = "This file doesn't appear to be a GOG InnoSetup (.exe) or Mac .pkg installer."
                    } else if (files == null || files.isEmpty()) {
                        status = "No game files found in installer"
                        errorMsg = "The installer was recognized as $fmt but contains no game files."
                    } else {
                        val gameFiles = files.filterNot { GogImportBridge.isAudioFile(it.name) }
                        val totalSize = gameFiles.sumOf { it.size }
                        status = "Found ${gameFiles.size} game file(s) (${formatSize(totalSize)})"
                    }
                }
            } catch (e: Exception) {
                Log.e("DXX-GogImport", "Analysis failed", e)
                withContext(Dispatchers.Main) {
                    status = "Error: ${e.message}"
                    errorMsg = e.message
                }
            }
        }
    }

    AlertDialog(
        onDismissRequest = { if (!processing) onDismiss() },
        confirmButton = {
            if (!processing) {
                TextButton(onClick = {
                    tempPath?.let { File(it).delete() }
                    cleanupTmpDir(filesDir)
                    onDismiss()
                }) { Text("Close") }
            }
        },
        title = { Text("Import GOG Installer", fontWeight = FontWeight.Bold) },
        text = {
            Column {
                // ── Scrollable area: file listing and status ──
                Column(
                    modifier =
                        Modifier
                            .weight(1f, fill = false)
                            .verticalScroll(rememberScrollState()),
                ) {
                    Text(installerName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                    if (format != null && format != "unknown") {
                        Text(
                            "Format: ${if (format == "innosetup") "InnoSetup (.exe)" else "Mac .pkg"}",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(status, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

                    // Error message
                    if (errorMsg != null && extractedCount == 0) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(errorMsg!!, fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
                    }

                    // File listing -- game files only
                    fileList?.let { files ->
                        val gameFiles = files.filterNot { GogImportBridge.isAudioFile(it.name) }
                        if (gameFiles.isNotEmpty()) {
                            Spacer(modifier = Modifier.height(8.dp))
                            gameFiles.forEach { f ->
                                Text(
                                    "${f.name} (${formatSize(f.size)})",
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }

                    // Show extracted file names
                    if (extractedFileNames.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(8.dp))
                        Text("Extracted files:", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                        val displayFiles = extractedFileNames.take(50)
                        displayFiles.forEach { name ->
                            Text(
                                "  $name",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        if (extractedFileNames.size > 50) {
                            Text(
                                "  ... and ${extractedFileNames.size - 50} more",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }

                // ── Fixed area: checkbox, buttons, progress ──
                fileList?.let { files ->
                    val audioFiles = files.filter { GogImportBridge.isAudioFile(it.name) }
                    if (audioFiles.isNotEmpty() ||
                        extractedCount > 0 ||
                        (fileList != null && fileList!!.isNotEmpty() && !processing && extractedCount == 0) ||
                        processing
                    ) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                    }

                    // Audio files checkbox
                    if (audioFiles.isNotEmpty() && extractedCount == 0) {
                        val audioSize = audioFiles.sumOf { it.size }
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Checkbox(
                                checked = includeAudio,
                                onCheckedChange = { includeAudio = it },
                                enabled = !processing,
                            )
                            Text(
                                "Include CD audio (${formatSize(audioSize)})",
                                fontSize = 12.sp,
                                modifier =
                                    Modifier.clickable(enabled = !processing) {
                                        includeAudio = !includeAudio
                                    },
                            )
                        }
                        audioFiles.forEach { f ->
                            Text(
                                "  ${f.name} (${formatSize(f.size)})",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }

                // Extract button with explanatory text
                if (fileList != null && fileList!!.isNotEmpty() && !processing && extractedCount == 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        "Game files will be extracted to \"${setDir.name}\"" +
                            if (includeAudio) ". CD audio will be configured as the active music source" else "",
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = {
                            scope.launch {
                                // Check disk space before extraction
                                val totalNeeded =
                                    fileList!!.sumOf { f ->
                                        if (!includeAudio && GogImportBridge.isAudioFile(f.name)) 0L else f.size
                                    }
                                val available = setDir.usableSpace
                                // Need total + 50MB headroom for temp files and OS overhead
                                if (available < totalNeeded + 50 * 1024 * 1024) {
                                    val needMB = (totalNeeded + 50 * 1024 * 1024) / (1024 * 1024)
                                    val availMB = available / (1024 * 1024)
                                    status = "Not enough disk space: need ~${needMB}MB, have ${availMB}MB free"
                                    return@launch
                                }
                                processing = true
                                status = "Extracting game files\u2026"
                                progressFile = ""
                                progressPct = 0f
                                val filesBefore =
                                    withContext(Dispatchers.IO) {
                                        setDir.list()?.toSet() ?: emptySet()
                                    }
                                withContext(Dispatchers.IO) {
                                    try {
                                        val count =
                                            GogImportBridge.extractFiles(
                                                tempPath!!,
                                                setDir.absolutePath,
                                                object : GogImportBridge.ExtractProgress {
                                                    override fun onProgress(
                                                        currentFile: String,
                                                        bytesDone: Long,
                                                        bytesTotal: Long,
                                                    ): Int {
                                                        val pct =
                                                            if (bytesTotal > 0) {
                                                                bytesDone.toFloat() / bytesTotal
                                                            } else {
                                                                0f
                                                            }
                                                        progressFile = currentFile
                                                        progressPct = pct
                                                        return 0
                                                    }
                                                },
                                                includeAudio = includeAudio,
                                            )
                                        val srcManager = AudioSourceManager(filesDir)
                                        val hasGog =
                                            if (includeAudio) {
                                                findGogPair(setDir) != null
                                            } else {
                                                false
                                            }
                                        if (hasGog) {
                                            enableRedbookInConfig(filesDir, context)
                                            registerGogAudioSource(srcManager, filesDir, setDir, context)
                                        }
                                        val filesAfter = setDir.list()?.toSet() ?: emptySet()
                                        val newFiles = (filesAfter - filesBefore).sorted()
                                        withContext(Dispatchers.Main) {
                                            extractedCount = count
                                            extractedFileNames = newFiles
                                            status =
                                                if (count > 0) {
                                                    val msg = "Extracted $count file(s)"
                                                    if (hasGog) {
                                                        "$msg. CD audio source registered and music mode set to Redbook"
                                                    } else {
                                                        msg
                                                    }
                                                } else {
                                                    "No files extracted"
                                                }
                                        }
                                    } catch (e: Exception) {
                                        Log.e("DXX-GogImport", "Extraction failed", e)
                                        withContext(Dispatchers.Main) {
                                            status = "Extract error: ${e.message}"
                                        }
                                    }
                                }
                                processing = false
                            }
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Extract to \u201c${setDir.name}\u201d", fontSize = 13.sp)
                    }
                }

                // Done button
                if (extractedCount > 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = {
                            tempPath?.let { File(it).delete() }
                            cleanupTmpDir(filesDir)
                            onImported()
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Done", fontSize = 13.sp)
                    }
                }

                // Progress indicator
                if (processing) {
                    Spacer(modifier = Modifier.height(8.dp))
                    if (progressFile.isNotEmpty()) {
                        val pctText = if (progressPct > 0f) " (${(progressPct * 100).toInt()}%)" else ""
                        Text(
                            progressFile + pctText,
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    LinearProgressIndicator(
                        progress = { progressPct },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        },
    )
}

// ── SOW archive import dialog ─────────────────────────────────────────────

@Composable
private fun SowImportDialog(
    sowName: String,
    sowUri: Uri,
    filesDir: File,
    setDir: File,
    context: Context,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var status by remember { mutableStateOf("Preparing\u2026") }
    var processing by remember { mutableStateOf(false) }
    var extractedCount by remember { mutableIntStateOf(0) }
    var tempPath by remember { mutableStateOf<String?>(null) }

    // Copy SOW to temp
    LaunchedEffect(sowUri) {
        withContext(Dispatchers.IO) {
            try {
                val tmpDir = File(filesDir, "tmp")
                tmpDir.mkdirs()
                val tmpFile = File(tmpDir, sowName)
                context.contentResolver.openInputStream(sowUri)?.use { input ->
                    java.io.FileOutputStream(tmpFile).use { output -> input.copyTo(output, bufferSize = 65536) }
                }
                tempPath = tmpFile.absolutePath
                withContext(Dispatchers.Main) {
                    status = "Ready to extract game files from SOW archive"
                }
            } catch (e: Exception) {
                Log.e("DXX-SowImport", "Copy failed", e)
                withContext(Dispatchers.Main) { status = "Error: ${e.message}" }
            }
        }
    }

    AlertDialog(
        onDismissRequest = { if (!processing) onDismiss() },
        confirmButton = {
            if (!processing) {
                TextButton(onClick = {
                    tempPath?.let { File(it).delete() }
                    cleanupTmpDir(filesDir)
                    onDismiss()
                }) { Text("Close") }
            }
        },
        title = { Text("Import SOW Archive", fontWeight = FontWeight.Bold) },
        text = {
            Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
                Text(sowName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                Spacer(modifier = Modifier.height(8.dp))
                Text(status, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

                // Extract button
                if (tempPath != null && !processing && extractedCount == 0) {
                    Spacer(modifier = Modifier.height(12.dp))
                    Button(
                        onClick = {
                            scope.launch {
                                processing = true
                                status = "Extracting game files\u2026"
                                withContext(Dispatchers.IO) {
                                    try {
                                        val count =
                                            DiscImportBridge.extractSowFiles(
                                                tempPath!!,
                                                setDir.absolutePath,
                                                null,
                                            )
                                        withContext(Dispatchers.Main) {
                                            extractedCount = count.coerceAtLeast(0)
                                            status =
                                                if (count > 0) {
                                                    "Extracted $count game file(s)"
                                                } else {
                                                    "No game files found in archive"
                                                }
                                        }
                                    } catch (e: Exception) {
                                        Log.e("DXX-SowImport", "Extraction failed", e)
                                        withContext(Dispatchers.Main) {
                                            status = "Extract error: ${e.message}"
                                        }
                                    }
                                }
                                processing = false
                            }
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Extract to \u201c${setDir.name}\u201d", fontSize = 13.sp)
                    }
                }

                // Done button
                if (extractedCount > 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = {
                            tempPath?.let { File(it).delete() }
                            cleanupTmpDir(filesDir)
                            onImported()
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Done", fontSize = 13.sp)
                    }
                }

                // Progress indicator
                if (processing) {
                    Spacer(modifier = Modifier.height(8.dp))
                    CircularProgressIndicator(modifier = Modifier.size(24.dp))
                }
            }
        },
    )
}

// ── BIN/CUE disc import dialog ────────────────────────────────────────────

/**
 * Dialog for importing a BIN/CUE disc image.
 *
 * Flow:
 *  1. Copies CUE to temp, gets BIN sizes via content resolver
 *  2. Parses CUE to discover tracks (data + audio)
 *  3. Optionally identifies disc via SHA1 hashing
 *  4. Extracts game files from data track
 *  5. Copies BIN to filesDir and registers as audio source
 */
@Composable
private fun DiscImportDialog(
    cueName: String,
    cueUri: Uri,
    binUris: List<Pair<String, Uri>>,
    filesDir: File,
    setDir: File,
    context: Context,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var status by remember { mutableStateOf("Ready to process") }
    var tracks by remember { mutableStateOf<List<DiscImportBridge.CueTrack>?>(null) }
    var processing by remember { mutableStateOf(false) }
    var dataExtracted by remember { mutableIntStateOf(0) }
    var audioRegistered by remember { mutableStateOf(false) }
    var discLabel by remember { mutableStateOf<String?>(null) }
    var discId by remember { mutableStateOf<String?>(null) }
    var legacyDiscId by remember { mutableStateOf(0L) }
    // Temp CUE path for native parsing
    var tempCuePath by remember { mutableStateOf<String?>(null) }

    // Copy CUE + parse tracks on first composition
    LaunchedEffect(cueUri) {
        withContext(Dispatchers.IO) {
            try {
                Log.i("DXX-DiscImport", "Starting disc import: cue=$cueName, bins=${binUris.size}")
                // Copy CUE file to temp
                val tmpDir = File(filesDir, "tmp")
                tmpDir.mkdirs()
                val tmpCue = File(tmpDir, cueName.lowercase())
                context.contentResolver.openInputStream(cueUri)?.use { input ->
                    FileOutputStream(tmpCue).use { output -> input.copyTo(output) }
                }
                tempCuePath = tmpCue.absolutePath
                Log.i("DXX-DiscImport", "CUE copied to ${tmpCue.absolutePath} (${tmpCue.length()} bytes)")

                // Get BIN sizes
                val binSizes =
                    binUris
                        .map { (name, uri) ->
                            val size =
                                context.contentResolver
                                    .query(
                                        uri,
                                        arrayOf(android.provider.OpenableColumns.SIZE),
                                        null,
                                        null,
                                        null,
                                    )?.use { c -> if (c.moveToFirst()) c.getLong(0) else 0L } ?: 0L
                            Log.i("DXX-DiscImport", "BIN '$name' size=$size")
                            size
                        }.toLongArray()

                if (binSizes.isEmpty()) {
                    withContext(Dispatchers.Main) {
                        status = "No BIN files selected \u2014 please select both .cue and .bin files"
                    }
                    return@withContext
                }

                // Parse CUE
                val parsed = DiscImportBridge.parseCue(tmpCue.absolutePath, binSizes)
                Log.i("DXX-DiscImport", "parseCue returned ${parsed?.size ?: "null"} tracks")
                withContext(Dispatchers.Main) {
                    tracks = parsed
                    if (parsed != null) {
                        val dataCount = parsed.count { it.isData }
                        val audioCount = parsed.count { it.isAudio }
                        status = "Found $dataCount data + $audioCount audio track(s)"
                    } else {
                        status = "Failed to parse CUE file"
                    }
                }
            } catch (e: Exception) {
                Log.e("DXX-DiscImport", "CUE parse failed", e)
                withContext(Dispatchers.Main) { status = "Error: ${e.message}" }
            }
        }
    }

    AlertDialog(
        onDismissRequest = { if (!processing) onDismiss() },
        confirmButton = {
            if (!processing) {
                TextButton(onClick = onDismiss) { Text("Close") }
            }
        },
        title = { Text("Import Disc Image", fontWeight = FontWeight.Bold) },
        text = {
            Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
                Text(cueName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                if (binUris.isNotEmpty()) {
                    Text(
                        "BIN: ${binUris.joinToString(", ") { it.first }}",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (discLabel != null) {
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "\u2713 Identified: $discLabel",
                        fontSize = 13.sp,
                        color = Color(0xFF4CAF50),
                    )
                }
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    status,
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                // Track listing
                tracks?.let { trackList ->
                    Spacer(modifier = Modifier.height(8.dp))
                    trackList.forEach { track ->
                        val typeStr = if (track.isData) "DATA" else "AUDIO"
                        val sizeStr = formatSize(track.numSectors.toLong() * 2352)
                        Text(
                            "Track ${track.trackNum}: $typeStr ($sizeStr)" +
                                if (track.title.isNotEmpty()) " - ${track.title}" else "",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                // Action buttons
                if (tracks != null && !processing) {
                    Spacer(modifier = Modifier.height(12.dp))

                    // Extract game files from data track
                    val hasDataTrack = tracks?.any { it.isData } == true
                    if (hasDataTrack && dataExtracted == 0) {
                        Button(
                            onClick = {
                                scope.launch {
                                    processing = true
                                    status = "Extracting game files..."
                                    withContext(Dispatchers.IO) {
                                        try {
                                            val dataTrack = tracks!!.first { it.isData }
                                            // Use first BIN file's fd
                                            val binUri = binUris[dataTrack.fileIndex].second
                                            val pfd = context.contentResolver.openFileDescriptor(binUri, "r")
                                            if (pfd != null) {
                                                val progress =
                                                    object : DiscImportBridge.ExtractProgress {
                                                        override fun onProgress(
                                                            currentFile: String,
                                                            bytesDone: Long,
                                                            bytesTotal: Long,
                                                        ): Int {
                                                            val pct =
                                                                if (bytesTotal > 0L) {
                                                                    ((bytesDone * 100L) / bytesTotal).toInt()
                                                                } else {
                                                                    0
                                                                }
                                                            status = "Extracting $currentFile ($pct%)"
                                                            return 0
                                                        }
                                                    }
                                                val isoExtracted: Int
                                                var macExtracted = 0
                                                val extracted =
                                                    pfd.use {
                                                        isoExtracted =
                                                            DiscImportBridge.extractIsoFiles(
                                                                it.fd,
                                                                dataTrack.startSector,
                                                                dataTrack.numSectors,
                                                                setDir.absolutePath,
                                                                progress,
                                                            )
                                                        if (isoExtracted > 0) {
                                                            isoExtracted
                                                        } else {
                                                            status = "Trying Mac HFS installer..."
                                                            macExtracted =
                                                                DiscImportBridge.extractMacFiles(
                                                                    it.fd,
                                                                    dataTrack.startSector,
                                                                    dataTrack.numSectors,
                                                                    setDir.absolutePath,
                                                                    progress,
                                                                )
                                                            macExtracted
                                                        }
                                                    }
                                                // SOW decompression: scan for .sow files and extract them
                                                var sowExtracted = 0
                                                if (isoExtracted > 0) {
                                                    val sowFiles = DiscImportBridge.scanSowFiles(setDir.absolutePath)
                                                    if (sowFiles != null && sowFiles.isNotEmpty()) {
                                                        withContext(Dispatchers.Main) {
                                                            status =
                                                                "Decompressing ${sowFiles.size} .sow archive(s)..."
                                                        }
                                                        for (sow in sowFiles) {
                                                            sowExtracted +=
                                                                DiscImportBridge
                                                                    .extractSowFiles(
                                                                        sow,
                                                                        setDir.absolutePath,
                                                                        null,
                                                                    ).coerceAtLeast(0)
                                                        }
                                                    }
                                                }
                                                withContext(Dispatchers.Main) {
                                                    dataExtracted = extracted.coerceAtLeast(0) + sowExtracted
                                                    status =
                                                        when {
                                                            isoExtracted > 0 && sowExtracted > 0 ->
                                                                "Extracted $isoExtracted file(s) + $sowExtracted from .sow archives"
                                                            isoExtracted > 0 ->
                                                                "Extracted $isoExtracted game file(s)"
                                                            macExtracted > 0 ->
                                                                "Extracted $macExtracted file(s) from Mac HFS installer"
                                                            else -> "No supported game files found on data track"
                                                        }
                                                }
                                            } else {
                                                withContext(Dispatchers.Main) {
                                                    status = "Could not open BIN file"
                                                }
                                            }
                                        } catch (e: Exception) {
                                            Log.e("DXX-DiscImport", "Extract failed", e)
                                            withContext(Dispatchers.Main) {
                                                status = "Extract error: ${e.message}"
                                            }
                                        }
                                    }
                                    processing = false
                                }
                            },
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            Text("Extract Game Files", fontSize = 13.sp)
                        }
                    }

                    // Register as audio source
                    val hasAudioTracks = tracks?.any { it.isAudio } == true
                    if (hasAudioTracks && !audioRegistered) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Button(
                            onClick = {
                                scope.launch {
                                    processing = true
                                    status = "Registering audio source\u2026"
                                    withContext(Dispatchers.IO) {
                                        try {
                                            val audioCount = tracks!!.count { it.isAudio }
                                            // Copy CUE to filesDir (small file, needed for parsing)
                                            // Use temp name; renamed to unique ${id}.cue after disc identification
                                            val tmpDest = File(filesDir, cueName.lowercase())
                                            tempCuePath?.let { File(it).copyTo(tmpDest, overwrite = true) }

                                            // Take persistable URI permissions on BIN files
                                            val binNames = mutableListOf<String>()
                                            var firstBinUri: Uri? = null
                                            for ((name, uri) in binUris) {
                                                try {
                                                    context.contentResolver.takePersistableUriPermission(
                                                        uri,
                                                        android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION,
                                                    )
                                                } catch (e: SecurityException) {
                                                    Log.w("DXX-DiscImport", "Could not persist URI for $name", e)
                                                }
                                                binNames.add(name.lowercase())
                                                if (firstBinUri == null) firstBinUri = uri
                                            }

                                            // Try to identify the disc via SAF fd
                                            try {
                                                val identifier = DiscIdentifier(context)
                                                val firstAudio = tracks!!.first { it.isAudio }
                                                val binUri = binUris[firstAudio.fileIndex].second
                                                val pfd = context.contentResolver.openFileDescriptor(binUri, "r")
                                                if (pfd != null) {
                                                    val trackBytes = firstAudio.numSectors.toLong() * 2352
                                                    val trackOffset = firstAudio.startSector.toLong() * 2352
                                                    val sha1 =
                                                        pfd.use {
                                                            java.io.FileInputStream(it.fileDescriptor).use { fis ->
                                                                fis.skip(trackOffset)
                                                                DiscIdentifier.sha1Hash(fis, trackBytes)
                                                            }
                                                        }
                                                    val match = identifier.identify(mapOf(firstAudio.trackNum to sha1))
                                                    if (match.matched) {
                                                        discLabel = match.label
                                                        discId = match.disc?.id
                                                        match.disc?.legacyDiscId?.let {
                                                            legacyDiscId = java.lang.Long.decode(it)
                                                        }
                                                    }
                                                }
                                            } catch (e: Exception) {
                                                Log.w("DXX-DiscImport", "Disc identification failed", e)
                                            }

                                            // Rename CUE to unique name now that disc id is known
                                            val srcManager = AudioSourceManager(filesDir)
                                            val id = discId ?: "custom-${System.currentTimeMillis()}"
                                            val destCue = File(filesDir, "$id.cue")
                                            if (tmpDest.exists()) tmpDest.renameTo(destCue)
                                            var trackNames = emptyMap<Int, String>()
                                            try {
                                                if (discId != null) {
                                                    trackNames = FingerprintBridge.lookupTrackNames(context, discId!!)
                                                    Log.i(
                                                        "DXX-DiscImport",
                                                        "Looked up ${trackNames.size} track names for $discId",
                                                    )
                                                }
                                                // Fingerprint matching for unknown discs via SAF fd
                                                if (trackNames.isEmpty() && firstBinUri != null) {
                                                    withContext(Dispatchers.Main) {
                                                        status = "Identifying audio tracks\u2026"
                                                    }
                                                    trackNames =
                                                        FingerprintBridge.fingerprintAndMatchDisc(
                                                            context,
                                                            context.contentResolver,
                                                            firstBinUri,
                                                            tracks!!,
                                                        )
                                                    Log.i(
                                                        "DXX-DiscImport",
                                                        "Fingerprinted ${trackNames.size} track names via SAF fd",
                                                    )
                                                }
                                            } catch (e: Exception) {
                                                Log.w("DXX-DiscImport", "Track name identification failed", e)
                                            }

                                            srcManager.addSource(
                                                AudioSourceManager.AudioSource(
                                                    id = id,
                                                    cuePath = destCue.name,
                                                    binPaths = binNames,
                                                    discLabel = discLabel ?: cueName,
                                                    discId = discId ?: "unknown",
                                                    trackCount = tracks!!.size,
                                                    audioTrackCount = audioCount,
                                                    legacyDiscId = legacyDiscId,
                                                    trackNames = trackNames,
                                                    binContentUri = firstBinUri?.toString(),
                                                    cueContentUri = cueUri.toString(),
                                                ),
                                            )

                                            withContext(Dispatchers.Main) {
                                                audioRegistered = true
                                                status = "Audio source registered" +
                                                    if (discLabel != null) " ($discLabel)" else ""
                                            }
                                            enableRedbookInConfig(filesDir, context)
                                        } catch (e: Exception) {
                                            Log.e("DXX-DiscImport", "Audio registration failed", e)
                                            withContext(Dispatchers.Main) {
                                                status = "Error: ${e.message}"
                                            }
                                        }
                                    }
                                    processing = false
                                }
                            },
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            Text("Add as Audio Source", fontSize = 13.sp)
                        }
                    }

                    // Done state
                    if (dataExtracted > 0 || audioRegistered) {
                        Spacer(modifier = Modifier.height(8.dp))
                        Button(
                            onClick = onImported,
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            Text("Done", fontSize = 13.sp)
                        }
                    }
                }

                if (processing) {
                    Spacer(modifier = Modifier.height(8.dp))
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            }
        },
    )
}

@Composable
private fun IsoImportDialog(
    isoName: String,
    isoUri: Uri,
    setDir: File,
    context: Context,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var status by remember { mutableStateOf("Ready to process") }
    var fileList by remember { mutableStateOf<List<DiscImportBridge.IsoFile>?>(null) }
    var processing by remember { mutableStateOf(false) }
    var extractedCount by remember { mutableIntStateOf(0) }

    LaunchedEffect(isoUri) {
        withContext(Dispatchers.IO) {
            try {
                val pfd = context.contentResolver.openFileDescriptor(isoUri, "r")
                if (pfd == null) {
                    withContext(Dispatchers.Main) {
                        status = "Could not open ISO image"
                    }
                    return@withContext
                }

                val listed =
                    pfd.use {
                        DiscImportBridge.listIsoImageFiles(it.fd)
                    }
                withContext(Dispatchers.Main) {
                    fileList = listed
                    status =
                        if (listed != null) {
                            "Found ${listed.size} file(s) in ISO image"
                        } else {
                            "Failed to read ISO image"
                        }
                }
            } catch (e: Exception) {
                Log.e("DXX-DiscImport", "ISO scan failed", e)
                withContext(Dispatchers.Main) {
                    status = "Error: ${e.message}"
                }
            }
        }
    }

    AlertDialog(
        onDismissRequest = { if (!processing) onDismiss() },
        confirmButton = {
            if (!processing) {
                TextButton(onClick = onDismiss) { Text("Close") }
            }
        },
        title = { Text("Import ISO Image", fontWeight = FontWeight.Bold) },
        text = {
            Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
                Text(isoName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    "Standalone ISO import extracts game data only. CD audio requires a CUE/BIN image.",
                    fontSize = 11.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    status,
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                fileList?.let { entries ->
                    if (entries.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(8.dp))
                        entries.take(12).forEach { entry ->
                            Text(
                                "${entry.path} (${formatSize(entry.size)})",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        if (entries.size > 12) {
                            Text(
                                "...and ${entries.size - 12} more",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }

                if (fileList != null && !processing && extractedCount == 0) {
                    Spacer(modifier = Modifier.height(12.dp))
                    Button(
                        onClick = {
                            scope.launch {
                                processing = true
                                status = "Extracting game files..."
                                withContext(Dispatchers.IO) {
                                    try {
                                        val pfd = context.contentResolver.openFileDescriptor(isoUri, "r")
                                        if (pfd != null) {
                                            val progress =
                                                object : DiscImportBridge.ExtractProgress {
                                                    override fun onProgress(
                                                        currentFile: String,
                                                        bytesDone: Long,
                                                        bytesTotal: Long,
                                                    ): Int {
                                                        val pct =
                                                            if (bytesTotal > 0L) {
                                                                ((bytesDone * 100L) / bytesTotal).toInt()
                                                            } else {
                                                                0
                                                            }
                                                        status = "Extracting $currentFile ($pct%)"
                                                        return 0
                                                    }
                                                }
                                            val isoExtracted =
                                                pfd.use {
                                                    DiscImportBridge.extractIsoImageFiles(
                                                        it.fd,
                                                        setDir.absolutePath,
                                                        progress,
                                                    )
                                                }
                                            val sowExtracted = if (isoExtracted > 0) extractSowArchives(setDir) else 0
                                            withContext(Dispatchers.Main) {
                                                extractedCount = isoExtracted.coerceAtLeast(0) + sowExtracted
                                                status =
                                                    when {
                                                        isoExtracted > 0 && sowExtracted > 0 ->
                                                            "Extracted $isoExtracted file(s) + $sowExtracted from .sow archives"
                                                        isoExtracted > 0 ->
                                                            "Extracted $isoExtracted game file(s)"
                                                        else -> "No supported game files found in ISO image"
                                                    }
                                            }
                                        } else {
                                            withContext(Dispatchers.Main) {
                                                status = "Could not open ISO image"
                                            }
                                        }
                                    } catch (e: Exception) {
                                        Log.e("DXX-DiscImport", "ISO extract failed", e)
                                        withContext(Dispatchers.Main) {
                                            status = "Extract error: ${e.message}"
                                        }
                                    }
                                }
                                processing = false
                            }
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Extract Game Files", fontSize = 13.sp)
                    }
                }

                if (extractedCount > 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = onImported,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Done", fontSize = 13.sp)
                    }
                }

                if (processing) {
                    Spacer(modifier = Modifier.height(8.dp))
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                }
            }
        },
    )
}
