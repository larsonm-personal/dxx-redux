package com.dxxredux.app

import android.content.Context
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dxxredux.app.multiplayer.HostGameDefaults
import java.io.File

internal const val PREF_GUIDEBOT_HELPER_LINE = "guidebot_helper_line_enabled"
internal const val PREF_NEAREST_PLAYER_LINE = "nearest_player_line_enabled"
internal const val PREF_SKIP_INTRO_MOVIE = "skip_intro_movie"
internal const val PREF_REWIND_SUPPORT_ENABLED = "rewind_support_enabled"
internal const val PREF_REWIND_TARGET_SECONDS = "rewind_target_seconds"
internal const val PREF_HEADLIGHT_OFF_BY_DEFAULT = "headlight_off_by_default"
internal const val PREF_SHOW_RESUME_OFFER = "show_resume_offer"
internal const val PREF_SAVE_EXPLORER_PANEL_EXPANDED = "save_explorer_panel_expanded"
internal const val PREF_SHOW_DEMO_INSTALLER_OFFER = "show_demo_installer_offer"

// Keep in sync with android_rewind_policy.h.
internal const val DEFAULT_REWIND_TARGET_SECONDS = 10
internal val REWIND_TARGET_SECONDS_OPTIONS = listOf(5, DEFAULT_REWIND_TARGET_SECONDS, 20)

internal fun sanitizeRewindTargetSeconds(value: Int): Int =
    if (value in REWIND_TARGET_SECONDS_OPTIONS) value else DEFAULT_REWIND_TARGET_SECONDS

private const val CM_FULL_COCKPIT = 0
private const val CM_STATUS_BAR = 2
private const val CM_FULL_SCREEN = 3

private val COCKPIT_MODE_OPTIONS =
    listOf(
        "Cockpit" to CM_FULL_COCKPIT,
        "Status Bar" to CM_STATUS_BAR,
        "Fullscreen" to CM_FULL_SCREEN,
    )

@Composable
fun EnginePreferencesPage(
    gameVariant: String,
    filesDir: File,
    controllerFocusActive: Boolean = true,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)

    val context = LocalContext.current
    val prefs =
        remember {
            context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        }
    val scrollState = rememberScrollState()
    val initialFocus = remember { FocusRequester() }
    var cockpitMode by remember { mutableIntStateOf(CM_FULL_COCKPIT) }
    var savedCockpitMode by remember { mutableIntStateOf(CM_FULL_COCKPIT) }
    var autoLeveling by remember { mutableStateOf(true) }
    var savedAutoLeveling by remember { mutableStateOf(true) }
    var showRobotHostageCounts by remember { mutableStateOf(false) }
    var savedShowRobotHostageCounts by remember { mutableStateOf(false) }
    var showBossHealthBar by remember { mutableStateOf(true) }
    var savedShowBossHealthBar by remember { mutableStateOf(true) }
    var mapCheatsAccessible by remember { mutableStateOf(true) }
    var savedMapCheatsAccessible by remember { mutableStateOf(true) }
    var originalHoming by remember { mutableStateOf(false) }
    var savedOriginalHoming by remember { mutableStateOf(false) }
    var statusMessage by remember { mutableStateOf("") }
    var hasPilotFile by remember { mutableStateOf(false) }
    var showGuidebotLine by remember {
        mutableStateOf(prefs.getBoolean(PREF_GUIDEBOT_HELPER_LINE, true))
    }
    var savedShowGuidebotLine by remember { mutableStateOf(showGuidebotLine) }
    var mainViewFov by remember {
        mutableIntStateOf(
            (readConfigValue(filesDir, "MainViewFov") ?: "0").toIntOrNull() ?: 0,
        )
    }
    var savedMainViewFov by remember { mutableIntStateOf(mainViewFov) }
    var presetNeedsSave by remember { mutableStateOf(false) }
    var pendingPreset by remember { mutableStateOf<GameSettingsPreset?>(null) }
    var showNearestPlayerLine by remember {
        mutableStateOf(prefs.getBoolean(PREF_NEAREST_PLAYER_LINE, true))
    }
    var skipIntroMovie by remember {
        mutableStateOf(prefs.getBoolean(PREF_SKIP_INTRO_MOVIE, false))
    }
    var savedSkipIntroMovie by remember { mutableStateOf(skipIntroMovie) }
    var rewindSupportEnabled by remember {
        mutableStateOf(prefs.getBoolean(PREF_REWIND_SUPPORT_ENABLED, true))
    }
    var savedRewindSupportEnabled by remember { mutableStateOf(rewindSupportEnabled) }
    var serverCoopQol by remember { mutableStateOf(HostGameDefaults.load(context).coopQol) }
    var savedServerCoopQol by remember { mutableStateOf(serverCoopQol) }
    var textureFilter by remember {
        mutableIntStateOf(
            (readConfigValue(filesDir, "TexFilt") ?: "0").toIntOrNull() ?: 0,
        )
    }
    var savedTextureFilter by remember { mutableIntStateOf(textureFilter) }
    var hudFiltering by remember { mutableStateOf((readConfigValue(filesDir, "HudTexFilt") ?: "1") != "0") }
    var savedHudFiltering by remember { mutableStateOf(hudFiltering) }
    var rewindTargetSeconds by remember {
        mutableIntStateOf(
            sanitizeRewindTargetSeconds(
                prefs.getInt(PREF_REWIND_TARGET_SECONDS, DEFAULT_REWIND_TARGET_SECONDS),
            ),
        )
    }
    var headlightOffByDefault by remember {
        mutableStateOf(prefs.getBoolean(PREF_HEADLIGHT_OFF_BY_DEFAULT, true))
    }
    var showDemoInstallerOffer by remember {
        mutableStateOf(prefs.getBoolean(PREF_SHOW_DEMO_INSTALLER_OFFER, true))
    }
    val hasChanges =
        presetNeedsSave ||
            cockpitMode != savedCockpitMode ||
            autoLeveling != savedAutoLeveling ||
            showRobotHostageCounts != savedShowRobotHostageCounts ||
            showBossHealthBar != savedShowBossHealthBar ||
            mapCheatsAccessible != savedMapCheatsAccessible ||
            originalHoming != savedOriginalHoming ||
            showGuidebotLine != savedShowGuidebotLine ||
            mainViewFov != savedMainViewFov ||
            skipIntroMovie != savedSkipIntroMovie ||
            rewindSupportEnabled != savedRewindSupportEnabled ||
            serverCoopQol != savedServerCoopQol ||
            textureFilter != savedTextureFilter ||
            hudFiltering != savedHudFiltering

    fun loadPrefs() {
        val data = NativePilotPreferences.readEnginePrefsForAll(gameVariant, filesDir.absolutePath)
        val homingData = NativePilotPreferences.readOriginalHomingPrefsForAll(gameVariant, filesDir.absolutePath)
        cockpitMode = data.cockpitMode
        savedCockpitMode = data.cockpitMode
        autoLeveling = data.autoLeveling
        savedAutoLeveling = data.autoLeveling
        showRobotHostageCounts = data.showRobotHostageCounts
        savedShowRobotHostageCounts = data.showRobotHostageCounts
        showBossHealthBar = data.showBossHealthBar
        savedShowBossHealthBar = data.showBossHealthBar
        mapCheatsAccessible = data.mapCheatsAccessible
        savedMapCheatsAccessible = data.mapCheatsAccessible
        originalHoming = homingData.enabled
        savedOriginalHoming = homingData.enabled
        hasPilotFile = data.hasPilotFile
        statusMessage =
            when {
                !data.hasPilotFile -> {
                    "No pilot files found - showing defaults"
                }

                data.hasPotentialConflicts -> {
                    "Multiple pilots exist. Shown values come from one pilot; saving applies these values to all pilots"
                }

                else -> {
                    ""
                }
            }
    }

    RequestLauncherControllerFocus(initialFocus, controllerFocusActive)
    LaunchedEffect(Unit) { loadPrefs() }

    pendingPreset?.let { preset ->
        AlertDialog(
            onDismissRequest = { pendingPreset = null },
            title = { Text(preset.title) },
            text = {
                Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
                    Text("Set these preferences for both games and all existing pilots:")
                    Spacer(modifier = Modifier.height(8.dp))
                    preset.confirmationLines.forEach { Text("- $it") }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        "Confirm to edit these values, then press Save to apply. You can change each setting individually.",
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    showRobotHostageCounts = preset.helpersEnabled
                    showBossHealthBar = preset.helpersEnabled
                    mapCheatsAccessible = preset.helpersEnabled
                    showGuidebotLine = preset.helpersEnabled
                    mainViewFov = preset.mainViewFov
                    serverCoopQol = preset.serverCoopQol
                    rewindSupportEnabled = preset.rewindEnabled
                    skipIntroMovie = preset.skipIntroMovie
                    textureFilter = preset.textureFilter
                    hudFiltering = preset.hudFiltering
                    if (preset == GameSettingsPreset.DEFAULTS) {
                        cockpitMode = CM_FULL_COCKPIT
                        autoLeveling = true
                        originalHoming = false
                    }
                    presetNeedsSave = true
                    pendingPreset = null
                    statusMessage = "${preset.title} selected - customize settings or press Save"
                }) { Text("Confirm") }
            },
            dismissButton = {
                TextButton(onClick = { pendingPreset = null }) { Text("Cancel") }
            },
        )
    }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier =
                Modifier
                    .fillMaxSize()
                    .safeDrawingPadding()
                    .padding(16.dp)
                    .repeatVerticalDpadFocus(),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TextButton(onClick = onBack, modifier = Modifier.focusRequester(initialFocus).tvFocusBorder()) {
                    Text("< Back", fontSize = 12.sp)
                }
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    "Game Preferences",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                )
            }

            Spacer(modifier = Modifier.height(6.dp))

            Column(
                modifier =
                    Modifier
                        .fillMaxSize()
                        .verticalScroll(scrollState),
            ) {
                Text("Presets", fontWeight = FontWeight.Bold, fontSize = 11.sp)
                Spacer(modifier = Modifier.height(6.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    GameSettingsPreset.entries.forEach { preset ->
                        OutlinedButton(
                            onClick = { pendingPreset = preset },
                            modifier = Modifier.weight(1f).tvFocusBorder(),
                        ) {
                            Text(
                                if (preset ==
                                    GameSettingsPreset.ORIGINAL
                                ) {
                                    "Set preset: Original Descent"
                                } else {
                                    "Restore defaults"
                                },
                                fontSize = 12.sp,
                            )
                        }
                    }
                }
                Spacer(modifier = Modifier.height(8.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(8.dp))

                Text("Launcher", fontWeight = FontWeight.Bold, fontSize = 11.sp)
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    "Launcher-only options that control setup-screen behavior before the game starts",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(modifier = Modifier.height(6.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = showDemoInstallerOffer,
                        onCheckedChange = { checked ->
                            showDemoInstallerOffer = checked
                            prefs.edit().putBoolean(PREF_SHOW_DEMO_INSTALLER_OFFER, checked).apply()
                        },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text(
                            "Show D1/D2 demo installer offer on launch",
                            fontSize = 10.sp,
                            fontWeight = FontWeight.SemiBold,
                        )
                        Text(
                            "When game data is missing, offer to install the hosted Mac demo packages",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(8.dp))

                Text("Pilot-backed Preferences", fontWeight = FontWeight.Bold, fontSize = 11.sp)
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    "These settings apply to every pilot file found for Descent 1 and Descent 2. Press Save to apply changes",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(modifier = Modifier.height(6.dp))

                Text("HUD Size", fontWeight = FontWeight.SemiBold, fontSize = 10.sp)
                COCKPIT_MODE_OPTIONS.forEach { (label, value) ->
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        RadioButton(
                            selected = cockpitMode == value,
                            onClick = { cockpitMode = value },
                            modifier = Modifier.tvFocusBorder(),
                        )
                        Text(label, fontSize = 10.sp)
                    }
                }

                Spacer(modifier = Modifier.height(4.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = autoLeveling,
                        onCheckedChange = { autoLeveling = it },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Auto-level", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Matches the in-game auto-level toggle stored in player files",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = originalHoming,
                        onCheckedChange = { originalHoming = it },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Original homing (Single/Coop)", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "D1 is similar at 25 Hz; D2 tracks and reacquires more strongly",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = showRobotHostageCounts,
                        onCheckedChange = { showRobotHostageCounts = it },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Robot / hostage / secret counts", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Shows robot progress, hostage status, and secrets below the score line",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = showBossHealthBar,
                        onCheckedChange = { showBossHealthBar = it },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Boss health bar", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Shows boss health after the boss and player exchange fire",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = mapCheatsAccessible,
                        onCheckedChange = { mapCheatsAccessible = it },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Map cheats accessible", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Secrets view button, reactor extend, and objectives extend options",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                MainViewFovControl(value = mainViewFov, onValueChange = { mainViewFov = it })
                TextureFilterControl(value = textureFilter, onValueChange = { textureFilter = it })
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Switch(
                        checked = hudFiltering,
                        onCheckedChange = { hudFiltering = it },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Text("HUD filtering", fontSize = 10.sp)
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Switch(
                        checked = serverCoopQol,
                        onCheckedChange = { serverCoopQol = it },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Column {
                        Text("New-server Coop QoL", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Default for teammate arrows, Guidebot and warp when creating a server. Existing sessions are unchanged",
                            fontSize = 9.sp,
                        )
                    }
                }
                Row(modifier = Modifier.fillMaxWidth()) {
                    Button(
                        onClick = {
                            val count =
                                NativePilotPreferences.writeEngineAndHomingPrefsToAll(
                                    filesDir.absolutePath,
                                    cockpitMode,
                                    autoLeveling,
                                    showRobotHostageCounts,
                                    showBossHealthBar,
                                    mapCheatsAccessible,
                                    !headlightOffByDefault,
                                    originalHoming,
                                )
                            if (count < 0) {
                                statusMessage = "Could not save pilot preferences; original files were restored"
                            } else if (count > 0) {
                                try {
                                    updateAllConfigFiles(
                                        filesDir,
                                        listOf(
                                            "MainViewFov" to mainViewFov.toString(),
                                            "TexFilt" to textureFilter.toString(),
                                            "HudTexFilt" to if (hudFiltering) "1" else "0",
                                        ),
                                    )
                                    prefs
                                        .edit()
                                        .putBoolean(PREF_GUIDEBOT_HELPER_LINE, showGuidebotLine)
                                        .putBoolean(PREF_REWIND_SUPPORT_ENABLED, rewindSupportEnabled)
                                        .putBoolean(PREF_SKIP_INTRO_MOVIE, skipIntroMovie)
                                        .putBoolean(HostGameDefaults.COOP_QOL_PREF, serverCoopQol)
                                        .putLong(
                                            PREF_GRAPHICS_SETTINGS_GENERATION,
                                            prefs.getLong(PREF_GRAPHICS_SETTINGS_GENERATION, 0L) + 1L,
                                        ).apply()
                                    savedServerCoopQol = serverCoopQol
                                    savedRewindSupportEnabled = rewindSupportEnabled
                                    savedSkipIntroMovie = skipIntroMovie
                                    savedTextureFilter = textureFilter
                                    savedHudFiltering = hudFiltering
                                    savedMainViewFov = mainViewFov
                                    savedShowGuidebotLine = showGuidebotLine
                                } catch (_: Exception) {
                                    statusMessage =
                                        "Pilot preferences saved, but graphics settings could not be saved. Press Save to retry"
                                    return@Button
                                }
                                savedCockpitMode = cockpitMode
                                savedAutoLeveling = autoLeveling
                                savedShowRobotHostageCounts = showRobotHostageCounts
                                savedShowBossHealthBar = showBossHealthBar
                                savedMapCheatsAccessible = mapCheatsAccessible
                                savedOriginalHoming = originalHoming
                                presetNeedsSave = false
                                hasPilotFile = true
                                statusMessage = "Saved to $count pilot file(s) across both games"
                            } else {
                                statusMessage = "No pilot files found to save"
                            }
                        },
                        enabled = hasChanges,
                        modifier = Modifier.weight(1f).height(32.dp).tvFocusBorder(),
                    ) {
                        Text("Save", fontSize = 12.sp)
                    }
                }

                if (statusMessage.isNotEmpty()) {
                    Spacer(modifier = Modifier.height(6.dp))
                    Text(
                        statusMessage,
                        fontSize = 10.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }

                if (!hasPilotFile) {
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "Launch the game and create or select a pilot before these can be written",
                        fontSize = 9.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }

                Spacer(modifier = Modifier.height(8.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(8.dp))

                Text("Launch Intro", fontWeight = FontWeight.Bold, fontSize = 11.sp)
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    "Applies to the launch intro only. Other movies stay tap-to-skip",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(modifier = Modifier.height(6.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = skipIntroMovie,
                        onCheckedChange = { checked ->
                            skipIntroMovie = checked
                            statusMessage = "Skip intro movie changed - press Save to apply"
                        },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Skip intro movie on launch", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Skips the D1/D2 startup intro sequence, but leaves other movies skippable by tap. Press Save above to apply",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(8.dp))

                Text("Gameplay", fontWeight = FontWeight.Bold, fontSize = 11.sp)
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    "Android runtime options that affect in-level helper features",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(modifier = Modifier.height(6.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = headlightOffByDefault,
                        onCheckedChange = { checked ->
                            headlightOffByDefault = checked
                            prefs.edit().putBoolean(PREF_HEADLIGHT_OFF_BY_DEFAULT, checked).apply()
                        },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Headlight off by default", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "In D2, picking up the headlight keeps it off until toggled on",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(4.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = rewindSupportEnabled,
                        onCheckedChange = { checked ->
                            rewindSupportEnabled = checked
                            statusMessage = "Rewind support changed - press Save to apply"
                        },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Enable rewind support", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Keeps rewind points available. When off, rewind overlay controls are hidden. Press Save above to apply",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                if (rewindSupportEnabled) {
                    Spacer(modifier = Modifier.height(6.dp))
                    Text("Rewind amount", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                    Text(
                        "Targets the first rewind point at least this far back. Actual distance snaps to saved 5 second points",
                        fontSize = 9.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(modifier = Modifier.height(2.dp))
                    REWIND_TARGET_SECONDS_OPTIONS.forEach { seconds ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            RadioButton(
                                selected = rewindTargetSeconds == seconds,
                                onClick = {
                                    rewindTargetSeconds = seconds
                                    prefs.edit().putInt(PREF_REWIND_TARGET_SECONDS, seconds).apply()
                                },
                                modifier = Modifier.tvFocusBorder(),
                            )
                            Text("$seconds seconds", fontSize = 10.sp)
                        }
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
                HorizontalDivider()
                Spacer(modifier = Modifier.height(8.dp))

                Text("Local Visual Helpers", fontWeight = FontWeight.Bold, fontSize = 11.sp)
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    "These are local launcher toggles layered on top of the host's coop QoL setting",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(modifier = Modifier.height(6.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = showGuidebotLine,
                        onCheckedChange = { checked ->
                            showGuidebotLine = checked
                            statusMessage = "Guidebot helper line changed - press Save to apply"
                        },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Guidebot helper line", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Shows the guidebot path line in D2 when that helper is available. Press Save above to apply",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(4.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Switch(
                        checked = showNearestPlayerLine,
                        onCheckedChange = { checked ->
                            showNearestPlayerLine = checked
                            prefs.edit().putBoolean(PREF_NEAREST_PLAYER_LINE, checked).apply()
                        },
                        modifier = Modifier.tvFocusBorder(),
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Nearest-player line", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Shows the nearest coop teammate path line when the host enables coop QoL",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))
            }
        }
    }
}
