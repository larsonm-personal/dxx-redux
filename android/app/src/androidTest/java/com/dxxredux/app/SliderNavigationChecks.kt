package com.dxxredux.app

import android.app.Instrumentation
import android.content.Intent
import android.content.pm.ActivityInfo
import android.graphics.Rect
import android.os.SystemClock
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.requiredWidth
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.unit.dp
import java.io.File

/** Exercises production settings with real controller key dispatch and focus diagnostics */
internal class SliderNavigationChecks(
    private val instrumentation: Instrumentation,
) {
    private fun <T> onMain(block: () -> T): T {
        var result: Result<T>? = null
        instrumentation.runOnMainSync { result = runCatching(block) }
        return result!!.getOrThrow()
    }

    private fun composeView(view: View): View? {
        if (view.accessibilityNodeProvider != null && view.javaClass.simpleName.contains("Compose")) return view
        if (view is ViewGroup) {
            for (i in 0 until view.childCount) composeView(view.getChildAt(i))?.let { return it }
        }
        return null
    }

    private fun logFocus(launcher: SetupActivity) =
        onMain {
            val provider = checkNotNull(composeView(launcher.window.decorView)?.accessibilityNodeProvider)
            for (id in -1..16383) {
                val node = provider.createAccessibilityNodeInfo(id) ?: continue
                if (node.isFocused || node.rangeInfo != null) {
                    val bounds = Rect().also(node::getBoundsInScreen)
                    Log.i(
                        "DXX-SliderTest",
                        "Android focus: id=$id text=${node.text} range=${node.rangeInfo?.current} bounds=$bounds",
                    )
                }
            }
        }

    private fun key(
        launcher: SetupActivity,
        code: Int,
    ) {
        onMain {
            val now = SystemClock.uptimeMillis()
            for (action in listOf(KeyEvent.ACTION_DOWN, KeyEvent.ACTION_UP)) {
                launcher.dispatchKeyEvent(KeyEvent(now, now, action, code, 0, 0, -1, 0, 0, InputDevice.SOURCE_GAMEPAD))
            }
        }
        Thread.sleep(150)
        instrumentation.waitForIdleSync()
    }

    fun run(capabilitiesOnly: Boolean = false) {
        val launcher =
            instrumentation.startActivitySync(
                Intent(instrumentation.targetContext, SetupActivity::class.java).apply {
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                },
            ) as SetupActivity
        val configs =
            listOf("descent.cfg", "d1x-redux/descent.cfg", "d2x-redux/descent.cfg").map {
                File(launcher.filesDir, it)
            }
        val backups = configs.associateWith { if (it.isFile) it.readBytes() else null }
        val originalOrientation = onMain { launcher.requestedOrientation }
        try {
            if (capabilitiesOnly) {
                graphicsCapabilityDetails(launcher)
                return
            }
            for (orientation in listOf(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE,
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT,
            )) {
                onMain { launcher.requestedOrientation = orientation }
                Thread.sleep(500)
                for (game in listOf("d1", "d2")) {
                    for (wide in listOf(true, false)) {
                        graphicsNavigation(launcher, game, wide)
                    }
                }
            }
            graphicsCapabilityDetails(launcher)
            touchSliderNavigation(launcher)
        } finally {
            for ((file, bytes) in backups) {
                if (bytes != null) file.writeBytes(bytes) else file.delete()
            }
            onMain { launcher.requestedOrientation = originalOrientation }
        }
    }

    @OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)
    private fun graphicsCapabilityDetails(launcher: SetupActivity) {
        // Render the production sections with isolated config files and real semantics
        val directory = File(launcher.cacheDir, "graphics-capability-ui-test").apply { mkdirs() }
        val unsupported =
            GraphicsCapabilities(
                1,
                0,
                0,
                "AF is unavailable on this GPU",
                "MSAA is unavailable for this display format",
                "Test GPU",
            )
        val limited = GraphicsCapabilities(8, 4, 4, "", "", "Test GPU")
        try {
            for ((caps, enabledCount) in listOf(unsupported to 2, limited to 7, null to 8)) {
                onMain {
                    launcher.setContent {
                        MaterialTheme {
                            CompositionLocalProvider(
                                androidx.compose.material3.LocalMinimumInteractiveComponentSize provides 0.dp,
                            ) {
                                Column {
                                    MsaaSection(directory, caps)
                                    AnisoSection(directory, caps)
                                }
                            }
                        }
                    }
                }
                Thread.sleep(350)
                instrumentation.waitForIdleSync()
                onMain {
                    val provider = checkNotNull(composeView(launcher.window.decorView)?.accessibilityNodeProvider)
                    val texts = mutableListOf<String>()
                    var enabled = 0
                    var options = 0
                    for (id in -1..16383) {
                        val node = provider.createAccessibilityNodeInfo(id) ?: continue
                        node.text?.let { texts.add(it.toString()) }
                        if (node.className == "android.widget.RadioButton") {
                            options++
                            if (node.isEnabled) enabled++
                        }
                    }
                    check(options == 8 && enabled == enabledCount) { "Graphics options: $enabled enabled of $options" }
                    check(
                        texts.contains(caps?.msaaDetail() ?: GraphicsCapabilities.UNKNOWN_DETAIL),
                    ) { "Missing MSAA details: $texts" }
                    check(
                        texts.contains(caps?.anisoDetail() ?: GraphicsCapabilities.UNKNOWN_DETAIL),
                    ) { "Missing AF details: $texts" }
                }
            }
        } finally {
            directory.deleteRecursively()
        }
    }

    private fun graphicsNavigation(
        launcher: SetupActivity,
        game: String,
        wide: Boolean,
    ) {
        Log.i(
            "DXX-SliderTest",
            "Android graphics navigation: game=$game wide=$wide orientation=${launcher.resources.configuration.orientation}",
        )
        onMain {
            updateAllConfigFiles(launcher.filesDir, listOf("MainViewFov" to "0", "TexFilt" to "2"))
            launcher.setContent {
                MaterialTheme {
                    // Exercise a wide settings layout, where geometric searches can skip the slider
                    Box(
                        if (wide) {
                            Modifier
                                .wrapContentWidth(
                                    Alignment.Start,
                                    unbounded = true,
                                ).requiredWidth(1080.dp)
                        } else {
                            Modifier
                        },
                    ) {
                        androidx.compose.runtime.key(
                            game to wide,
                        ) { GraphicsSettingsPage(game, launcher.filesDir, onBack = {}) }
                    }
                }
            }
        }
        Thread.sleep(1500)
        instrumentation.waitForIdleSync()
        // Rotation can return Android to touch input mode; seed controller focus before traversal
        key(launcher, KeyEvent.KEYCODE_DPAD_UP)
        Thread.sleep(600)
        check(onMain { launcher.collectAccessibleButtons().any { it.text == "< Back" && it.focused } }) {
            "Graphics did not seed focus on Back"
        }
        val resolutionCount = onMain { computeResolutionOptions(launcher).size }
        repeat(resolutionCount) { key(launcher, KeyEvent.KEYCODE_DPAD_DOWN) }
        key(launcher, KeyEvent.KEYCODE_DPAD_DOWN)
        logFocus(launcher)

        fun fov(expected: Int) {
            val actual = onMain { readConfigValue(launcher.filesDir, "MainViewFov") }
            check(actual == expected.toString()) { "FOV expected $expected, got $actual (game=$game wide=$wide)" }
        }
        key(launcher, KeyEvent.KEYCODE_DPAD_RIGHT)
        fov(100)
        key(launcher, KeyEvent.KEYCODE_DPAD_DOWN)
        key(launcher, KeyEvent.KEYCODE_BUTTON_A)
        fov(100)
        check(
            onMain { readConfigValue(launcher.filesDir, "TexFilt") } == "0",
        ) { "Down from FOV skipped first texture filter" }
        key(launcher, KeyEvent.KEYCODE_DPAD_UP)
        key(launcher, KeyEvent.KEYCODE_DPAD_RIGHT)
        fov(110)
        key(launcher, KeyEvent.KEYCODE_DPAD_UP)
        key(launcher, KeyEvent.KEYCODE_DPAD_LEFT)
        fov(110)
        key(launcher, KeyEvent.KEYCODE_DPAD_DOWN)
        key(launcher, KeyEvent.KEYCODE_DPAD_LEFT)
        fov(100)
        repeat(3) { key(launcher, KeyEvent.KEYCODE_DPAD_RIGHT) }
        fov(120)
        repeat(4) { key(launcher, KeyEvent.KEYCODE_DPAD_LEFT) }
        fov(0)
    }

    private fun touchSliderNavigation(launcher: SetupActivity) {
        var floatValue by mutableFloatStateOf(50f)
        var intValue by mutableIntStateOf(25)
        val initialFocus = FocusRequester()
        onMain {
            launcher.setContent {
                MaterialTheme {
                    RequestLauncherControllerFocus(initialFocus, true)
                    Column(Modifier.repeatVerticalDpadFocus(useTraversalOrder = true)) {
                        TextButton({}, Modifier.focusRequester(initialFocus)) { Text("Before") }
                        LabeledSlider("Float", floatValue, 0f, 100f) { floatValue = it }
                        LabeledIntSlider("Deadzone", intValue, 0, 50, steps = 49) { intValue = it }
                        TextButton({}) { Text("After") }
                    }
                }
            }
        }
        Thread.sleep(1000)
        key(launcher, KeyEvent.KEYCODE_DPAD_DOWN)
        key(launcher, KeyEvent.KEYCODE_DPAD_RIGHT)
        check(onMain { floatValue == 51f && intValue == 25 }) { "Float slider was skipped or cannot adjust" }
        key(launcher, KeyEvent.KEYCODE_DPAD_DOWN)
        key(launcher, KeyEvent.KEYCODE_DPAD_RIGHT)
        check(onMain { floatValue == 51f && intValue == 26 }) { "Down changed the float value instead of navigating" }
        key(launcher, KeyEvent.KEYCODE_DPAD_DOWN)
        key(launcher, KeyEvent.KEYCODE_DPAD_UP)
        key(launcher, KeyEvent.KEYCODE_DPAD_LEFT)
        check(onMain { floatValue == 51f && intValue == 25 }) { "Integer slider cannot be re-entered" }
        key(launcher, KeyEvent.KEYCODE_DPAD_UP)
        key(launcher, KeyEvent.KEYCODE_DPAD_LEFT)
        check(onMain { floatValue == 50f && intValue == 25 }) { "Up changed the integer value instead of navigating" }
        key(launcher, KeyEvent.KEYCODE_DPAD_UP)
        check(onMain { floatValue == 50f && intValue == 25 }) { "Up could not leave the float slider" }
    }
}
