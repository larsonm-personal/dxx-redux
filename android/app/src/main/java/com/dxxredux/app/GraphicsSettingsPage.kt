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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.io.File

@Composable
fun GraphicsSettingsPage(
    filesDir: File,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)

    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val scrollState = rememberScrollState()

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
                TextButton(onClick = onBack) {
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

                        Text(
                            "MSAA and AF take effect on next launch",
                            fontSize = 9.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Spacer(modifier = Modifier.height(16.dp))
                    }
                }
                ScrollArrows(scrollState)
            }
        }
    }
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
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

@Composable
private fun TexFilterSection(filesDir: File) {
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
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

@Composable
private fun ColorDepthSection(filesDir: File) {
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
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
    Text(
        "Takes effect on next launch",
        fontSize = 9.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}

// Shared constant: must match levels in VideoInfoOverlay.cycleMsaa()
// and jni_main.c nativeSetGraphicsOption "msaa_level"
private val MSAA_OPTIONS = listOf("Off" to 0, "2x" to 2, "4x" to 4)

// Shared constant: must match levels in VideoInfoOverlay.cycleAnisotropy()
// and jni_main.c nativeSetGraphicsOption "aniso_level"
private val ANISO_OPTIONS = listOf("Off" to 0, "2x" to 2, "4x" to 4, "8x" to 8, "16x" to 16)

@Composable
private fun MsaaSection(filesDir: File) {
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
                },
            )
            Text(text = label, fontSize = 10.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
}

@Composable
private fun AnisoSection(filesDir: File) {
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
    var menuFilt by remember {
        mutableStateOf((readConfigValue(filesDir, "MenuTexFilt") ?: "0") != "0")
    }
    var hudFilt by remember {
        mutableStateOf((readConfigValue(filesDir, "HudTexFilt") ?: "1") != "0")
    }

    Text("Filter by Context", fontWeight = FontWeight.Bold, fontSize = 11.sp)
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
            },
            modifier = Modifier.height(24.dp),
        )
        Text(
            text = "Menus / briefings / videos / text / reticle",
            fontSize = 10.sp,
            modifier = Modifier.padding(start = 8.dp),
        )
    }
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
    ) {
        Switch(
            checked = hudFilt,
            onCheckedChange = {
                hudFilt = it
                updateAllConfigFiles(filesDir, listOf("HudTexFilt" to if (it) "1" else "0"))
            },
            modifier = Modifier.height(24.dp),
        )
        Text(
            text = "Ship HUD / gauges",
            fontSize = 10.sp,
            modifier = Modifier.padding(start = 8.dp),
        )
    }
    Text(
        "When off, these use nearest-neighbor (pixelated) regardless of texture filter above",
        fontSize = 9.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
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
