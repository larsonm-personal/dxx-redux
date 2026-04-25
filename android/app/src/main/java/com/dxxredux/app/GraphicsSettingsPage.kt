package com.dxxredux.app

import android.content.SharedPreferences
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.*
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.io.File

internal const val PREF_GRAPHICS_SETTINGS_GENERATION = "graphics_settings_generation"
internal const val PREF_GRAPHICS_ALPHA_EFFECTS = "graphics_alpha_effects"
internal const val PREF_GRAPHICS_DYNLIGHT_COLOR = "graphics_dynlight_color"

@Composable
fun GraphicsSettingsPage(
    gameVariant: String,
    filesDir: File,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)

    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val scrollState = rememberScrollState()
    val initialFocus = remember { FocusRequester() }
    LaunchedEffect(Unit) { initialFocus.requestFocus() }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier =
                Modifier
                    .fillMaxSize()
                    .safeDrawingPadding()
                    .padding(16.dp),
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
                    "Graphics",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                )
            }

            Spacer(modifier = Modifier.height(3.dp))

            Box(modifier = Modifier.weight(1f)) {
                @OptIn(ExperimentalMaterial3Api::class)
                CompositionLocalProvider(
                    LocalMinimumInteractiveComponentSize provides 0.dp,
                ) {
                    Column(
                        modifier =
                            Modifier
                                .fillMaxSize()
                                .verticalScroll(scrollState),
                    ) {
                        // -- Render Resolution --
                        ResolutionSection(filesDir = filesDir, prefs = prefs)

                        Spacer(modifier = Modifier.height(3.dp))
                        HorizontalDivider()
                        Spacer(modifier = Modifier.height(3.dp))

                        // -- Texture Filtering --
                        TexFilterSection(filesDir = filesDir)

                        Spacer(modifier = Modifier.height(3.dp))
                        HorizontalDivider()
                        Spacer(modifier = Modifier.height(3.dp))

                        // -- Color Depth --
                        ColorDepthSection(filesDir = filesDir)

                        Spacer(modifier = Modifier.height(3.dp))
                        HorizontalDivider()
                        Spacer(modifier = Modifier.height(3.dp))

                        // -- Anti-Aliasing (MSAA) --
                        MsaaSection(filesDir = filesDir)

                        Spacer(modifier = Modifier.height(3.dp))
                        HorizontalDivider()
                        Spacer(modifier = Modifier.height(3.dp))

                        // -- Anisotropic Filtering --
                        AnisoSection(filesDir = filesDir)

                        Spacer(modifier = Modifier.height(3.dp))
                        HorizontalDivider()
                        Spacer(modifier = Modifier.height(3.dp))

                        // -- Selective Filtering (menu/HUD) --
                        SelectiveFilterSection(filesDir = filesDir)

                        Spacer(modifier = Modifier.height(3.dp))
                        HorizontalDivider()
                        Spacer(modifier = Modifier.height(3.dp))

                        // -- Original game visual options --
                        OriginalVisualOptionsSection(
                            gameVariant = gameVariant,
                            filesDir = filesDir,
                            prefs = prefs,
                        )

                        Spacer(modifier = Modifier.height(3.dp))
                        HorizontalDivider()
                        Spacer(modifier = Modifier.height(3.dp))

                        // -- Debug options --
                        DebugOptionsSection(prefs = prefs)

                        Spacer(modifier = Modifier.height(16.dp))
                    }
                }
                ScrollArrows(scrollState)
            }
        }
    }
}

private fun bumpGraphicsSettingsGeneration(prefs: SharedPreferences) {
    val next = prefs.getLong(PREF_GRAPHICS_SETTINGS_GENERATION, 0L) + 1L
    prefs.edit().putLong(PREF_GRAPHICS_SETTINGS_GENERATION, next).apply()
}

@Composable
private fun ResolutionSection(
    filesDir: File,
    prefs: SharedPreferences,
) {
    Text("Render Resolution", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(1.dp))

    val ctx = LocalContext.current
    val options = remember { computeResolutionOptions(ctx) }
    val validValues = remember(options) { options.map { it.first }.toSet() }
    val defaultValue = remember(options) { options.firstOrNull()?.first ?: "640x480" }
    var selected by remember {
        val stored = prefs.getString("render_resolution", null) ?: ""
        mutableStateOf(if (stored in validValues) stored else defaultValue)
    }
    options.forEach { (value, label) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().padding(vertical = 0.dp),
        ) {
            RadioButton(
                selected = selected == value,
                onClick = {
                    selected = value
                    prefs.edit().putString("render_resolution", value).apply()
                    updateDescentCfgResolution(filesDir, value)
                    bumpGraphicsSettingsGeneration(prefs)
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

@Composable
private fun TexFilterSection(filesDir: File) {
    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val texFilterOptions = listOf("None (nearest)" to "0", "Bilinear" to "1", "Trilinear" to "2")
    var texFilter by remember {
        val cur = readConfigValue(filesDir, "TexFilt") ?: "0"
        mutableStateOf(if (texFilterOptions.any { it.second == cur }) cur else "0")
    }

    Text("Texture Filtering", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(1.dp))
    texFilterOptions.forEach { (label, value) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().padding(vertical = 0.dp),
        ) {
            RadioButton(
                selected = texFilter == value,
                onClick = {
                    texFilter = value
                    updateAllConfigFiles(filesDir, listOf("TexFilt" to value))
                    bumpGraphicsSettingsGeneration(prefs)
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

@Composable
private fun ColorDepthSection(filesDir: File) {
    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val colorDepthOptions = listOf("16-bit (RGB565)" to "0", "24-bit (RGBA8888)" to "1")
    var colorDepth by remember {
        val cur = readConfigValue(filesDir, "ColorDepth") ?: "0"
        mutableStateOf(if (colorDepthOptions.any { it.second == cur }) cur else "0")
    }

    Text("Color Depth", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(1.dp))
    colorDepthOptions.forEach { (label, value) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().padding(vertical = 0.dp),
        ) {
            RadioButton(
                selected = colorDepth == value,
                onClick = {
                    colorDepth = value
                    updateAllConfigFiles(filesDir, listOf("ColorDepth" to value))
                    bumpGraphicsSettingsGeneration(prefs)
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

// Shared constant: must match levels in VideoInfoOverlay.cycleMsaa()
// and jni_main.c nativeSetGraphicsOption "msaa_level"
private val MSAA_OPTIONS = listOf("Off" to 0, "2x" to 2, "4x" to 4)

// Shared constant: must match levels in VideoInfoOverlay.cycleAnisotropy()
// and jni_main.c nativeSetGraphicsOption "aniso_level"
private val ANISO_OPTIONS = listOf("Off" to 0, "2x" to 2, "4x" to 4, "8x" to 8, "16x" to 16)

@Composable
private fun MsaaSection(filesDir: File) {
    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    var msaaLevel by remember {
        mutableIntStateOf((readConfigValue(filesDir, "MsaaLevel") ?: "0").toIntOrNull() ?: 0)
    }

    Text("Anti-Aliasing (MSAA)", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(1.dp))
    MSAA_OPTIONS.forEach { (label, value) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().padding(vertical = 0.dp),
        ) {
            RadioButton(
                selected = msaaLevel == value,
                onClick = {
                    msaaLevel = value
                    updateAllConfigFiles(filesDir, listOf("MsaaLevel" to value.toString()))
                    bumpGraphicsSettingsGeneration(prefs)
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

@Composable
private fun AnisoSection(filesDir: File) {
    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    var anisoLevel by remember {
        mutableIntStateOf((readConfigValue(filesDir, "AnisoLevel") ?: "0").toIntOrNull() ?: 0)
    }

    Text("Anisotropic Filtering (AF)", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(1.dp))
    ANISO_OPTIONS.forEach { (label, value) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().padding(vertical = 0.dp),
        ) {
            RadioButton(
                selected = anisoLevel == value,
                onClick = {
                    anisoLevel = value
                    updateAllConfigFiles(filesDir, listOf("AnisoLevel" to value.toString()))
                    bumpGraphicsSettingsGeneration(prefs)
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

// Shared constant: selective filtering config keys
// Must match config.c string constants for MenuTexFilt / HudTexFilt
@Composable
private fun SelectiveFilterSection(filesDir: File) {
    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    var menuFilt by remember {
        mutableStateOf((readConfigValue(filesDir, "MenuTexFilt") ?: "0") != "0")
    }
    var hudFilt by remember {
        mutableStateOf((readConfigValue(filesDir, "HudTexFilt") ?: "1") != "0")
    }

    Text("Enable filters for...", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(2.dp))
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
    ) {
        Switch(
            checked = menuFilt,
            onCheckedChange = {
                menuFilt = it
                updateAllConfigFiles(filesDir, listOf("MenuTexFilt" to if (it) "1" else "0"))
                bumpGraphicsSettingsGeneration(prefs)
            },
            modifier = Modifier.height(24.dp),
        )
        Text(
            text = "Menus / briefings / videos / text / reticle",
            fontSize = 10.sp,
            modifier = Modifier.padding(start = 8.dp),
        )
    }
    Spacer(modifier = Modifier.height(4.dp))
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
    ) {
        Switch(
            checked = hudFilt,
            onCheckedChange = {
                hudFilt = it
                updateAllConfigFiles(filesDir, listOf("HudTexFilt" to if (it) "1" else "0"))
                bumpGraphicsSettingsGeneration(prefs)
            },
            modifier = Modifier.height(24.dp),
        )
        Text(
            text = "Ship HUD / gauges",
            fontSize = 10.sp,
            modifier = Modifier.padding(start = 8.dp),
        )
    }
}

@Composable
private fun OriginalVisualOptionsSection(
    gameVariant: String,
    filesDir: File,
    prefs: SharedPreferences,
) {
    var classicDepth by remember {
        mutableStateOf((readConfigValue(filesDir, "ClassicDepth") ?: "0") != "0")
    }
    var movieFilter by remember {
        mutableStateOf((readConfigValueForGame(filesDir, "d2", "MovieTexFilt") ?: "0") != "0")
    }
    var alphaEffects by remember { mutableStateOf(prefs.getBoolean(PREF_GRAPHICS_ALPHA_EFFECTS, false)) }
    var dynLightColor by remember { mutableStateOf(prefs.getBoolean(PREF_GRAPHICS_DYNLIGHT_COLOR, false)) }
    var hasPilotFile by remember { mutableStateOf(false) }
    var statusText by remember { mutableStateOf("") }

    LaunchedEffect(gameVariant, filesDir.absolutePath) {
        try {
            val data = NativePilotPreferences.readVisualPrefsForAll(gameVariant, filesDir.absolutePath)
            hasPilotFile = data.hasPilotFile
            alphaEffects = data.alphaEffects
            dynLightColor = data.dynLightColor
            prefs
                .edit()
                .putBoolean(PREF_GRAPHICS_ALPHA_EFFECTS, alphaEffects)
                .putBoolean(PREF_GRAPHICS_DYNLIGHT_COLOR, dynLightColor)
                .apply()
            statusText = if (data.hasPilotFile) "" else "No pilot files found"
        } catch (e: Exception) {
            statusText = "Pilot visual settings unavailable: ${e.message ?: "native bridge error"}"
        }
    }

    Text("Original Visual Options", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(2.dp))

    DebugOptionRow(
        checked = classicDepth,
        title = "Classic depth ordering",
        detail = "Matches the in-game Classic Depth Ordering option",
        onCheckedChange = {
            classicDepth = it
            updateAllConfigFiles(filesDir, listOf("ClassicDepth" to if (it) "1" else "0"))
            bumpGraphicsSettingsGeneration(prefs)
        },
    )

    if (gameVariant == "d2") {
        Spacer(modifier = Modifier.height(4.dp))
        DebugOptionRow(
            checked = movieFilter,
            title = "Movie filter",
            detail = "Matches the D2 in-game Movie Filter option",
            onCheckedChange = {
                movieFilter = it
                updateConfigFilesForGame(filesDir, "d2", listOf("MovieTexFilt" to if (it) "1" else "0"))
                bumpGraphicsSettingsGeneration(prefs)
            },
        )
    }

    Spacer(modifier = Modifier.height(4.dp))
    DebugOptionRow(
        checked = alphaEffects,
        title = "Transparency effects",
        detail = "Matches the pilot-backed in-game Transparency Effects option",
        enabled = hasPilotFile,
        onCheckedChange = {
            alphaEffects = it
            val count = NativePilotPreferences.writeVisualPrefsToAll(filesDir.absolutePath, alphaEffects, dynLightColor)
            prefs.edit().putBoolean(PREF_GRAPHICS_ALPHA_EFFECTS, alphaEffects).apply()
            bumpGraphicsSettingsGeneration(prefs)
            hasPilotFile = count > 0
            statusText = if (count > 0) "Saved to $count pilot file(s)" else "No pilot files found"
        },
    )

    Spacer(modifier = Modifier.height(4.dp))
    DebugOptionRow(
        checked = dynLightColor,
        title = "Colored dynamic light",
        detail = "Matches the pilot-backed in-game Colored Dynamic Light option",
        enabled = hasPilotFile,
        onCheckedChange = {
            dynLightColor = it
            val count = NativePilotPreferences.writeVisualPrefsToAll(filesDir.absolutePath, alphaEffects, dynLightColor)
            prefs.edit().putBoolean(PREF_GRAPHICS_DYNLIGHT_COLOR, dynLightColor).apply()
            bumpGraphicsSettingsGeneration(prefs)
            hasPilotFile = count > 0
            statusText = if (count > 0) "Saved to $count pilot file(s)" else "No pilot files found"
        },
    )

    if (statusText.isNotEmpty()) {
        Spacer(modifier = Modifier.height(4.dp))
        Text(statusText, fontSize = 9.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun DebugOptionsSection(prefs: SharedPreferences) {
    val ctx = LocalContext.current
    var showVideoInfoDebugOptions by remember {
        mutableStateOf(prefs.getBoolean(PREF_SHOW_VIDEO_INFO_DEBUG_OPTIONS, false))
    }
    var graphicsDebugLogging by remember {
        mutableStateOf(
            DebugLog.isCategoryEnabled(ctx, DebugLogCategory.GRAPHICS) ||
                DebugLog.isCategoryEnabled(ctx, DebugLogCategory.TEXTURE),
        )
    }
    var forceLegacyTexmerge by remember {
        mutableStateOf(prefs.getBoolean(PREF_FORCE_LEGACY_MERGED_WALL_TEXMERGE, false))
    }

    Text("Debug Options", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    Spacer(modifier = Modifier.height(2.dp))

    DebugOptionRow(
        checked = showVideoInfoDebugOptions,
        title = "Show debug options in Video Info",
        detail = "Adds merged-wall controls to the in-game Video Info overlay",
        onCheckedChange = {
            showVideoInfoDebugOptions = it
            prefs.edit().putBoolean(PREF_SHOW_VIDEO_INFO_DEBUG_OPTIONS, it).apply()
        },
    )

    Spacer(modifier = Modifier.height(4.dp))

    DebugOptionRow(
        checked = graphicsDebugLogging,
        title = "Graphics / merged-wall debug logging",
        detail = "Enables the Graphics and Texture log categories used by merged-wall diagnostics",
        onCheckedChange = {
            graphicsDebugLogging = it
            DebugLog.setCategoryEnabled(ctx, DebugLogCategory.GRAPHICS, it)
            DebugLog.setCategoryEnabled(ctx, DebugLogCategory.TEXTURE, it)
        },
    )

    Spacer(modifier = Modifier.height(4.dp))

    DebugOptionRow(
        checked = forceLegacyTexmerge,
        title = "Force legacy CPU texmerge",
        detail = "Applies the merged-wall legacy texmerge experiment on launch or resume",
        onCheckedChange = {
            forceLegacyTexmerge = it
            prefs.edit().putBoolean(PREF_FORCE_LEGACY_MERGED_WALL_TEXMERGE, it).apply()
        },
    )
}

@Composable
private fun DebugOptionRow(
    checked: Boolean,
    title: String,
    detail: String,
    enabled: Boolean = true,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
    ) {
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            enabled = enabled,
            modifier = Modifier.height(24.dp),
        )
        Column(modifier = Modifier.padding(start = 8.dp)) {
            Text(text = title, fontSize = 10.sp)
            Text(text = detail, fontSize = 9.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
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
