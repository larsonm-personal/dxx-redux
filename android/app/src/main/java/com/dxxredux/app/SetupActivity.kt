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
import android.provider.DocumentsContract
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.widget.Toast
import androidx.activity.ComponentActivity
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
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_files --es path /sdcard/DESCENT2.HOG
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_default_config
    //   adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command write_autoselect --es game d2 --es primary "8,9,7,6,5,4,3,2,1,0,255" --es secondary "9,8,4,3,1,5,0,255,7,6,2"
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
                            FileSetManager(filesDir).writeActiveSetPath()
                            AudioSourceManager(filesDir).writePlaylist()
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
                    "create_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        val fsm = FileSetManager(filesDir)
                        try {
                            val dir = fsm.createSet(name)
                            Log.i("DXX-Setup", "create_set '$name': ${dir.absolutePath}")
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
                    }
                    "clear_set" -> {
                        val name = intent.getStringExtra("name") ?: return
                        val fsm = FileSetManager(filesDir)
                        val dir = fsm.getSetDir(name)
                        val count = dir.listFiles()?.count { it.isFile && it.delete() } ?: 0
                        Log.i("DXX-Setup", "clear_set '$name': deleted $count file(s)")
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
                        }.start()
                    }
                    "import_sow" -> {
                        val path = intent.getStringExtra("path") ?: return
                        Thread {
                            val fsm = FileSetManager(filesDir)
                            val setDir = fsm.getSetDir(fsm.getActive())
                            val count = DiscImportBridge.extractSowFiles(path, setDir.absolutePath, null)
                            Log.i("DXX-Setup", "import_sow '$path' -> $count file(s) to ${setDir.name}")
                        }.start()
                    }
                    "import_files" -> {
                        val path = intent.getStringExtra("path") ?: return
                        val fsm = FileSetManager(filesDir)
                        val setDir = fsm.getSetDir(fsm.getActive())
                        val src = File(path)
                        if (src.isFile) {
                            src.copyTo(File(setDir, src.name), overwrite = true)
                            Log.i("DXX-Setup", "import_files: copied ${src.name} to ${setDir.name}")
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
                        val gameInfo =
                            JsonObject(
                                mapOf(
                                    "mission" to JsonPrimitive(mission),
                                    "mode" to JsonPrimitive(mode),
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
                            )
                        Log.i(
                            "DXX-MP",
                            "lan_launch: $mpMode $game/$mission lvl=$levelNum diff=$difficulty host=$hostAddr:$hostPort",
                        )
                        launchMultiplayerGame(info)
                    }
                    else -> Log.w("DXX-MP", "Unknown MP command: $cmd")
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
            return true
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
            for (i in 0 until n) {
                var value = if (joyArr != null && i < joyArr.length()) joyArr.getInt(i) else 255
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
        // Close lobby socket before handing off to engine
        com.dxxredux.app.lobby.LobbyService
            .stopDiscovery()
        FileSetManager(filesDir).writeActiveSetPath()
        AudioSourceManager(filesDir).writePlaylist()
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
        } else {
            mpIntent.putExtra("mp_mode", "join")
            if (info.lanHostAddr != null) {
                // LAN joiner: route through proxy for packet stats
                com.dxxredux.app.multiplayer.MatchmakingService.createLanProxy(
                    info.lanHostAddr,
                    info.lanHostPort,
                )
                mpIntent.putExtra("mp_host_addr", "127.0.0.1")
                mpIntent.putExtra("mp_host_port", NetworkConstants.PROXY_PORT_BASE)
            } else {
                // Online: use existing proxy from matchmaking
                val hostAddr = mpJoinHostAddrOverride ?: "127.0.0.1"
                val hostPort = mpJoinHostPortOverride ?: NetworkConstants.PROXY_PORT_BASE
                mpIntent.putExtra("mp_host_addr", hostAddr)
                mpIntent.putExtra("mp_host_port", hostPort)
            }
            mpIntent.putExtra("mp_my_port", NetworkConstants.ENGINE_PORT)
        }
        if (info.isLan) mpIntent.putExtra("mp_is_lan", true)
        // Pass current net log path so :game process appends to the same file
        com.dxxredux.app.multiplayer.NetLog.currentFilePath()?.let {
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
                    if (gameRunning || isGameProcessAlive()) {
                        finish() // return to the already-running game
                    } else {
                        FileSetManager(filesDir).writeActiveSetPath()
                        AudioSourceManager(filesDir).writePlaylist()
                        writeInitialGameConfig()
                        writeMusicConfigForLaunch()
                        val intent = Intent(this, MainActivity::class.java)
                        intent.putExtra("game", game)
                        startActivity(intent)
                        // Don't finish() — stay in back stack so quitting
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
        val cfgFile = File(filesDir, "descent.cfg")
        if (cfgFile.exists()) return // user already has a config — don't overwrite

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

        cfgFile.writeText(
            "AspectX=$aspectX\n" +
                "AspectY=$aspectY\n" +
                "ResolutionX=$resW\n" +
                "ResolutionY=$resH\n",
        )

        // Store matching preference so the picker shows the right selection
        getSharedPreferences("dxx_prefs", MODE_PRIVATE)
            .edit()
            .putString("render_resolution", "${resW}x$resH")
            .apply()
        Log.i("DXX-Setup", "First launch: default resolution ${resW}x$resH")
    }

    /**
     * Write controller_config.json from bundled defaults if it doesn't exist yet.
     * Called during first launch (and during tests after the runner deletes config
     * files) so the JSON default pipeline is always exercised.
     */
    private fun writeDefaultControllerConfig() {
        if (File(filesDir, "controller_config.json").exists()) return
        val bindings = loadDefaultBindings(applicationContext)
        saveConfig(applicationContext, bindings, emptySet())
        Log.i("DXX-Setup", "First launch: wrote default controller config from bundled defaults")
    }

    override fun onResume() {
        super.onResume()
        mpGameLaunching = false
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
        // Required – core D1 files
        GameFileInfo("descent.hog", "D1 game data", required = true),
        GameFileInfo("descent.pig", "D1 textures", required = true),
        // Optional – downloadable extras
        GameFileInfo(
            "d1xr-mac-demo-sounds.dxa",
            "Optional sound file",
            required = false,
            downloadUrl = "https://dxx-redux.com/dl/d1xr-mac-demo-sounds.dxa",
        ),
        GameFileInfo(
            "d1xr-hires.dxa",
            "Optional D1 high-res file",
            required = false,
            downloadUrl = "https://dxx-redux.com/dl/d1xr-hires.dxa",
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
                } else if (displayName.lowercase() in ALL_GAME_FILENAMES) {
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
        val destFile = File(destDir, canonicalName)
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

    // ── Startup: hash any stale/new files ───────────────────
    LaunchedEffect(activeSetName) {
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
                manifest.upsert(file.name, sha256, file.length())
            }
            hashingFile = null
            onRefresh()
        }
    }

    val d2RequiredOk = d2Statuses.filter { it.info.required }.all { it.found }
    val d1RequiredOk = d1Statuses.filter { it.info.required }.all { it.found }
    val canLaunch = d2RequiredOk || d1RequiredOk

    val context = androidx.compose.ui.platform.LocalContext.current

    // ── Startup: prune audio sources whose files are gone ───
    var prunedSourceNames by remember { mutableStateOf<List<String>>(emptyList()) }
    var prunedDataFiles by remember { mutableStateOf<List<String>>(emptyList()) }
    LaunchedEffect(activeSetName) {
        val srcManager = AudioSourceManager(filesDir)
        val pruned = srcManager.pruneMissingSources(setDir)
        if (pruned.isNotEmpty()) {
            prunedSourceNames = pruned
        }
        // Prune stale asset manifest entries
        val prunedAssets = manifest.pruneStaleEntries()
        // Prune stale SAF manifest entries
        val prunedSaf = safManifest.pruneStaleEntries(context)
        val allPrunedData = prunedAssets + prunedSaf
        if (allPrunedData.isNotEmpty()) {
            prunedDataFiles = allPrunedData
        }
        if (pruned.isNotEmpty() || allPrunedData.isNotEmpty()) {
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

    // ── GOG installer import state ──────────────────────
    var gogImportUri by remember { mutableStateOf<Uri?>(null) }
    var gogImportName by remember { mutableStateOf<String?>(null) }

    // ── SOW archive import state ────────────────────────
    var sowImportUri by remember { mutableStateOf<Uri?>(null) }
    var sowImportName by remember { mutableStateOf<String?>(null) }

    // ── Audio file auto-import state ────────────────────
    var audioImportUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
    var audioImporting by remember { mutableStateOf(false) }

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
                    var gogUri: Pair<String, Uri>? = null
                    var sowUri: Pair<String, Uri>? = null
                    // Track raw .gog/.inst pairs (GOG CD images picked directly)
                    var gogDiscUri: Pair<String, Uri>? = null // .gog BIN file
                    var instDiscUri: Pair<String, Uri>? = null // .inst CUE sheet
                    val unhandledFiles = mutableListOf<String>()
                    val audioFileUris = mutableListOf<Uri>()
                    for (uri in uris) {
                        val name = getDisplayName(context, uri)
                        if (name != null) {
                            val lname = name.lowercase()
                            when {
                                lname.endsWith(".zip") || lname.endsWith(".7z") -> zipUris.add(name to uri)
                                lname.endsWith(".cue") -> cueUris.add(name to uri)
                                lname.endsWith(".inst") -> instDiscUri = name to uri
                                lname.endsWith(".gog") -> gogDiscUri = name to uri
                                lname.endsWith(".bin") -> binUris.add(name to uri)
                                lname.endsWith(".exe") || lname.endsWith(".pkg") -> gogUri = name to uri
                                lname.endsWith(".sow") -> sowUri = name to uri
                                lname in ALL_GAME_FILENAMES -> gameUris.add(FoundFile(name, uri))
                                lname.endsWith(".mp3") || lname.endsWith(".ogg") || lname.endsWith(".flac") ->
                                    audioFileUris.add(uri)
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
                        scanning = false
                    }
                    // Handle ZIP/7z files
                    if (zipUris.isNotEmpty()) {
                        withContext(Dispatchers.Main) { zipExtracting = true }
                        val tmpDir = File(filesDir, "tmp")
                        val allExtracted = mutableListOf<ExtractedFile>()
                        var anyAudio = false
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
                        }
                        // Identify package
                        val fileHashes = allExtracted.associate { it.name to it.sha256 }
                        val pkgName = KnownVersions.identifyPackage(fileHashes)
                        withContext(Dispatchers.Main) {
                            zipExtracted = allExtracted
                            zipPackageName = pkgName
                            zipHadAudioFiles = anyAudio
                            zipExtracting = false
                            zipProgressFile = ""
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

    // ── Page navigation state ────────────────────────────
    var showControllerPage by remember { mutableStateOf(false) }
    var showTouchEditorPage by remember { mutableStateOf(false) }
    var showAdvancedPage by remember { mutableStateOf(false) }
    var showMultiplayerPage by remember { mutableStateOf(false) }
    var showAutoselectPage by remember { mutableStateOf(false) }
    var showMusicPage by remember { mutableStateOf(false) }

    MaterialTheme(colorScheme = darkColorScheme()) {
        if (showControllerPage) {
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
            TouchEditorPage(
                gameVariant = selectedGame,
                onBack = { showTouchEditorPage = false },
            )
            return@MaterialTheme
        }
        if (showAdvancedPage) {
            AdvancedSettingsPage(
                filesDir = filesDir,
                fileSetManager = fileSetManager,
                onBack = { showAdvancedPage = false },
            )
            return@MaterialTheme
        }
        if (showMultiplayerPage) {
            com.dxxredux.app.multiplayer.MultiplayerScreen(
                onBack = { showMultiplayerPage = false },
                onLaunchGame = onMultiplayerLaunch,
            )
            return@MaterialTheme
        }
        if (showAutoselectPage) {
            AutoselectEditorPage(
                gameVariant = selectedGame,
                filesDir = filesDir.absolutePath,
                onBack = { showAutoselectPage = false },
            )
            return@MaterialTheme
        }
        if (showMusicPage) {
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

                if (showAbout) {
                    AlertDialog(
                        onDismissRequest = { showAbout = false },
                        confirmButton = {
                            TextButton(onClick = { showAbout = false }) { Text("OK") }
                        },
                        title = { Text("DXX-Redux") },
                        text = {
                            val arch = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown"
                            Text(
                                "Build ${BuildInfo.GIT_COMMIT_COUNT}" +
                                    " (${BuildInfo.GIT_SHORT_HASH})" +
                                    " ${BuildInfo.BUILD_TYPE}\n" +
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
                                        File(setDir, entry.filename).delete()
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

                // ── Audio file auto-import dialog ──
                if (audioImportUris.isNotEmpty()) {
                    val customMgr = remember { CustomAudioSetManager(filesDir) }
                    var customSets by remember { mutableStateOf(customMgr.getSets()) }
                    AddToSetDialog(
                        existingSets = customSets,
                        defaultName = "Set ${customSets.size + 1}",
                        onDismiss = { audioImportUris = emptyList() },
                        onConfirm = { targetSetId, newName, copyToStorage ->
                            val uris = audioImportUris
                            audioImportUris = emptyList()
                            audioImporting = true
                            scope.launch {
                                importAudioFiles(
                                    context,
                                    filesDir,
                                    customMgr,
                                    newName,
                                    uris,
                                    targetSetId,
                                    copyToStorage,
                                )
                                customSets = customMgr.getSets()
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
                            fontSize = 14.sp,
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
                                                            val destFile = File(setDir, canonicalName)
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
                                    Text(
                                        text =
                                            if (zipHadAudioFiles) {
                                                "This archive contains audio files but no game files. To import music, use the Music tab"
                                            } else {
                                                "No game files found in archive"
                                            },
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
                                                    if (success) onRefresh()
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
                                .height(40.dp),
                        colors =
                            ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.secondaryContainer,
                                contentColor = MaterialTheme.colorScheme.onSecondaryContainer,
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
    val cfgFile = File(filesDir, "descent.cfg")

    if (cfgFile.exists()) {
        var text = cfgFile.readText()
        val rxRegex = Regex("^ResolutionX=.*$", RegexOption.MULTILINE)
        val ryRegex = Regex("^ResolutionY=.*$", RegexOption.MULTILINE)
        text =
            if (rxRegex.containsMatchIn(text)) {
                rxRegex.replace(text, "ResolutionX=$w")
            } else {
                text.trimEnd() + "\nResolutionX=$w\n"
            }
        text =
            if (ryRegex.containsMatchIn(text)) {
                ryRegex.replace(text, "ResolutionY=$h")
            } else {
                text.trimEnd() + "\nResolutionY=$h\n"
            }
        cfgFile.writeText(text)
    } else {
        cfgFile.writeText("ResolutionX=$w\nResolutionY=$h\n")
    }
    Log.i("DXX-Setup", "Updated descent.cfg: ResolutionX=$w ResolutionY=$h")
}

/**
 * Apply key=value settings to all descent.cfg files: root (first-launch fallback),
 * d1x-redux/ and d2x-redux/ (per-game configs created after first run).
 * Each game's PHYSFS reads only its own subdir config, so we must write to all.
 */
private fun updateAllConfigFiles(
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
 * Returns the lowercase base filename (e.g. "descent_ii") or null if not found.
 */
private fun findGogPair(dir: File): String? {
    val files = dir.list() ?: return null
    val lower = files.map { it.lowercase() }.toSet()
    if ("descent_ii.gog" in lower && "descent_ii.inst" in lower) return "descent_ii"
    return null
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

    // ── Weapon Autoselect / Advanced ──
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = onEditAutoselect,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Weapon Autoselect", fontSize = 12.sp)
        }
        OutlinedButton(
            onClick = onAdvancedSettings,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp).padding(vertical = 2.dp),
        ) {
            Text("Advanced Settings", fontSize = 12.sp)
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
            Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
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

                // File listing — split game files and audio files
                fileList?.let { files ->
                    val gameFiles = files.filterNot { GogImportBridge.isAudioFile(it.name) }
                    val audioFiles = files.filter { GogImportBridge.isAudioFile(it.name) }

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

                    // Audio files with opt-in checkbox
                    if (audioFiles.isNotEmpty() && extractedCount == 0) {
                        Spacer(modifier = Modifier.height(8.dp))
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
                                    status = "Extracting game files\u2026"
                                    withContext(Dispatchers.IO) {
                                        try {
                                            val dataTrack = tracks!!.first { it.isData }
                                            // Use first BIN file's fd
                                            val binUri = binUris[dataTrack.fileIndex].second
                                            val pfd = context.contentResolver.openFileDescriptor(binUri, "r")
                                            if (pfd != null) {
                                                val extracted =
                                                    pfd.use {
                                                        DiscImportBridge.extractIsoFiles(
                                                            it.fd,
                                                            dataTrack.startSector,
                                                            dataTrack.numSectors,
                                                            setDir.absolutePath,
                                                        )
                                                    }
                                                // SOW decompression: scan for .sow files and extract them
                                                var sowExtracted = 0
                                                if (extracted > 0) {
                                                    val sowFiles = DiscImportBridge.scanSowFiles(setDir.absolutePath)
                                                    if (sowFiles != null && sowFiles.isNotEmpty()) {
                                                        withContext(Dispatchers.Main) {
                                                            status =
                                                                "Decompressing ${sowFiles.size} .sow archive(s)\u2026"
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
                                                    dataExtracted = extracted + sowExtracted
                                                    status =
                                                        when {
                                                            extracted > 0 && sowExtracted > 0 ->
                                                                "Extracted $extracted file(s) + $sowExtracted from .sow archives"
                                                            extracted > 0 ->
                                                                "Extracted $extracted game file(s)"
                                                            else -> "No game files found on data track"
                                                        }
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
                                    status = "Copying BIN file(s) for audio\u2026"
                                    withContext(Dispatchers.IO) {
                                        try {
                                            val audioCount = tracks!!.count { it.isAudio }
                                            // Copy CUE to filesDir
                                            val destCue = File(filesDir, cueName.lowercase())
                                            tempCuePath?.let { File(it).copyTo(destCue, overwrite = true) }

                                            // Copy BIN file(s) to filesDir
                                            val destBinPaths = mutableListOf<String>()
                                            for ((name, uri) in binUris) {
                                                val destBin = File(filesDir, name.lowercase())
                                                withContext(Dispatchers.Main) {
                                                    status = "Copying ${name}\u2026"
                                                }
                                                context.contentResolver.openInputStream(uri)?.use { input ->
                                                    FileOutputStream(destBin).use { output ->
                                                        input.copyTo(output, bufferSize = 65536)
                                                    }
                                                }
                                                destBinPaths.add(destBin.name)
                                            }

                                            // Try to identify the disc
                                            try {
                                                val identifier = DiscIdentifier(context)
                                                // Quick identification by hashing first audio track
                                                val firstAudio = tracks!!.first { it.isAudio }
                                                val binFile = File(filesDir, destBinPaths[firstAudio.fileIndex])
                                                val trackBytes = firstAudio.numSectors.toLong() * 2352
                                                val trackOffset = firstAudio.startSector.toLong() * 2352
                                                val sha1 =
                                                    binFile.inputStream().use { fis ->
                                                        fis.skip(trackOffset)
                                                        DiscIdentifier.sha1Hash(fis, trackBytes)
                                                    }
                                                val match = identifier.identify(mapOf(firstAudio.trackNum to sha1))
                                                if (match.matched) {
                                                    discLabel = match.label
                                                    discId = match.disc?.id
                                                    match.disc?.legacyDiscId?.let {
                                                        legacyDiscId = java.lang.Long.decode(it)
                                                    }
                                                }
                                            } catch (e: Exception) {
                                                Log.w("DXX-DiscImport", "Disc identification failed", e)
                                            }

                                            // Register audio source
                                            val srcManager = AudioSourceManager(filesDir)
                                            val id = discId ?: "custom-${System.currentTimeMillis()}"

                                            // Get track names: direct lookup for known discs,
                                            // fingerprint matching for unknown ones
                                            var trackNames = emptyMap<Int, String>()
                                            try {
                                                if (discId != null) {
                                                    trackNames = FingerprintBridge.lookupTrackNames(context, discId!!)
                                                    Log.i(
                                                        "DXX-DiscImport",
                                                        "Looked up ${trackNames.size} track names for $discId",
                                                    )
                                                }
                                                if (trackNames.isEmpty()) {
                                                    withContext(Dispatchers.Main) {
                                                        status = "Identifying audio tracks\u2026"
                                                    }
                                                    val binFile = File(filesDir, destBinPaths[0])
                                                    trackNames =
                                                        FingerprintBridge.fingerprintAndMatchDisc(
                                                            context,
                                                            binFile.absolutePath,
                                                            tracks!!,
                                                        )
                                                    Log.i(
                                                        "DXX-DiscImport",
                                                        "Fingerprinted ${trackNames.size} track names",
                                                    )
                                                }
                                            } catch (e: Exception) {
                                                Log.w("DXX-DiscImport", "Track name identification failed", e)
                                            }

                                            srcManager.addSource(
                                                AudioSourceManager.AudioSource(
                                                    id = id,
                                                    cuePath = destCue.name,
                                                    binPaths = destBinPaths,
                                                    discLabel = discLabel ?: cueName,
                                                    discId = discId ?: "unknown",
                                                    trackCount = tracks!!.size,
                                                    audioTrackCount = audioCount,
                                                    legacyDiscId = legacyDiscId,
                                                    trackNames = trackNames,
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
