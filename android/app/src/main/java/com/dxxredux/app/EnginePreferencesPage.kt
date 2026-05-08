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
import java.io.File

internal const val PREF_GUIDEBOT_HELPER_LINE = "guidebot_helper_line_enabled"
internal const val PREF_NEAREST_PLAYER_LINE = "nearest_player_line_enabled"
internal const val PREF_SKIP_INTRO_MOVIE = "skip_intro_movie"

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
    var statusMessage by remember { mutableStateOf("") }
    var hasPilotFile by remember { mutableStateOf(false) }
    var showGuidebotLine by remember {
        mutableStateOf(prefs.getBoolean(PREF_GUIDEBOT_HELPER_LINE, true))
    }
    var showNearestPlayerLine by remember {
        mutableStateOf(prefs.getBoolean(PREF_NEAREST_PLAYER_LINE, true))
    }
    var skipIntroMovie by remember {
        mutableStateOf(prefs.getBoolean(PREF_SKIP_INTRO_MOVIE, false))
    }

    val hasChanges = cockpitMode != savedCockpitMode || autoLeveling != savedAutoLeveling

    fun loadPrefs() {
        val data = NativePilotPreferences.readEnginePrefsForAll(gameVariant, filesDir.absolutePath)
        cockpitMode = data.cockpitMode
        savedCockpitMode = data.cockpitMode
        autoLeveling = data.autoLeveling
        savedAutoLeveling = data.autoLeveling
        hasPilotFile = data.hasPilotFile
        statusMessage = if (data.hasPilotFile) "" else "No pilot files found - showing defaults"
    }

    LaunchedEffect(Unit) {
        initialFocus.requestFocus()
        loadPrefs()
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
                TextButton(onClick = onBack, modifier = Modifier.focusRequester(initialFocus)) {
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
                Text("Pilot-backed Preferences", fontWeight = FontWeight.Bold, fontSize = 11.sp)
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    "These settings apply to every pilot file found for Descent 1 and Descent 2",
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
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    OutlinedButton(
                        onClick = {
                            cockpitMode = CM_FULL_COCKPIT
                            autoLeveling = true
                        },
                        modifier = Modifier.weight(1f).height(32.dp),
                    ) {
                        Text("Reset to Defaults", fontSize = 12.sp)
                    }
                    Button(
                        onClick = {
                            val count =
                                NativePilotPreferences.writeEnginePrefsToAll(
                                    filesDir.absolutePath,
                                    cockpitMode,
                                    autoLeveling,
                                )
                            if (count > 0) {
                                savedCockpitMode = cockpitMode
                                savedAutoLeveling = autoLeveling
                                hasPilotFile = true
                                statusMessage = "Saved to $count pilot file(s) across both games"
                            } else {
                                statusMessage = "No pilot files found to save"
                            }
                        },
                        enabled = hasChanges,
                        modifier = Modifier.weight(1f).height(32.dp),
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
                            prefs.edit().putBoolean(PREF_SKIP_INTRO_MOVIE, checked).commit()
                        },
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Skip intro movie on launch", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Skips the D1/D2 startup intro sequence, but leaves other movies skippable by tap",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
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
                            prefs.edit().putBoolean(PREF_GUIDEBOT_HELPER_LINE, checked).apply()
                        },
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Guidebot helper line", fontSize = 10.sp, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Shows the guidebot path line in D2 when that helper is available",
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
