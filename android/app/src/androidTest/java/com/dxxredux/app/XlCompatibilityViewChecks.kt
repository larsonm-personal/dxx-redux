package com.dxxredux.app

import android.app.Instrumentation
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Rect
import android.os.Bundle
import android.os.SystemClock
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.accessibility.AccessibilityNodeInfo
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import java.io.File

/** Actual production dialog, using public synthetic reports rather than mission content */
internal class XlCompatibilityViewChecks(
    private val instrumentation: Instrumentation,
) {
    private fun find(text: String): AccessibilityNodeInfo? {
        fun visit(
            node: AccessibilityNodeInfo,
            remaining: Int,
            exact: Boolean,
        ): AccessibilityNodeInfo? {
            val label = node.text?.toString()
            if (if (exact) label == text else label?.contains(text) == true) return node
            if (remaining <= 0) return null
            for (i in 0 until node.childCount) {
                val child = node.getChild(i) ?: continue
                visit(child, remaining - 1, exact)?.let { return it }
            }
            return null
        }
        val root = instrumentation.uiAutomation.rootInActiveWindow ?: return null
        // Prefer the status label over a title such as "Supported fixture"
        return visit(root, 30, true) ?: visit(root, 30, false)
    }

    private fun awaitText(text: String): AccessibilityNodeInfo {
        val deadline = SystemClock.uptimeMillis() + 10000
        while (SystemClock.uptimeMillis() < deadline) {
            find(text)?.let { return it }
            SystemClock.sleep(100)
        }
        error("Missing dialog text: $text")
    }

    private fun click(text: String) {
        var node: AccessibilityNodeInfo? = awaitText(text)
        while (node != null && !node.isClickable) node = node.parent
        check(node != null && node.performAction(AccessibilityNodeInfo.ACTION_CLICK)) {
            "No clickable accessibility ancestor for: $text"
        }
    }

    private fun swipeScrollable(
        node: AccessibilityNodeInfo,
        direction: Int,
    ) {
        val bounds = Rect().also(node::getBoundsInScreen)
        check(bounds.width() > 0 && bounds.height() > 0) { "Scrollable dialog has no touch bounds" }
        val low = bounds.top + bounds.height() * 0.2f
        val high = bounds.top + bounds.height() * 0.8f
        val start = if (direction == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD) low else high
        val end = if (direction == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD) high else low
        val downTime = SystemClock.uptimeMillis()

        fun inject(
            action: Int,
            y: Float,
        ) {
            val event = MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, bounds.exactCenterX(), y, 0)
            event.source = InputDevice.SOURCE_TOUCHSCREEN
            try {
                check(instrumentation.uiAutomation.injectInputEvent(event, true)) { "Dialog swipe injection failed" }
            } finally {
                event.recycle()
            }
        }
        inject(MotionEvent.ACTION_DOWN, start)
        for (step in 1..6) {
            SystemClock.sleep(40)
            inject(MotionEvent.ACTION_MOVE, start + (end - start) * step / 6f)
        }
        // Release at rest so later scroll-position assertions do not race a fling
        SystemClock.sleep(180)
        inject(MotionEvent.ACTION_MOVE, end)
        inject(MotionEvent.ACTION_UP, end)
    }

    private fun scrollTo(
        text: String,
        direction: Int = AccessibilityNodeInfo.ACTION_SCROLL_FORWARD,
    ): AccessibilityNodeInfo {
        repeat(10) { attempt ->
            find(text)?.let { target ->
                if (target.isVisibleToUser) return target
                target.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SHOW_ON_SCREEN.id)
                instrumentation.waitForIdleSync()
                SystemClock.sleep(250)
                find(text)?.takeIf { it.isVisibleToUser }?.let { return it }
            }

            fun scroll(node: AccessibilityNodeInfo): Boolean {
                if (node.isScrollable && node.isVisibleToUser) {
                    // Exercise the user's touch path too, when accessibility scrolling has not revealed the target
                    if (attempt % 2 == 1) {
                        swipeScrollable(node, direction)
                        return true
                    }
                    if (node.performAction(direction)) return true
                }
                for (index in 0 until node.childCount) {
                    val child = node.getChild(index) ?: continue
                    if (scroll(child)) return true
                }
                return false
            }
            scroll(checkNotNull(instrumentation.uiAutomation.rootInActiveWindow))
            instrumentation.waitForIdleSync()
            // A false action result can precede the accessibility update from the previous scroll
            SystemClock.sleep(250)
        }
        find(text)?.takeIf { it.isVisibleToUser }?.let { return it }
        // Only synthetic dialog text is rendered by this test; keep failure evidence bounded
        val labels = mutableListOf<String>()

        fun describe(
            node: AccessibilityNodeInfo,
            depth: Int,
        ) {
            if (depth > 20 || labels.size >= 24) return
            if (node.text != null || node.isScrollable) {
                labels.add(
                    "${node.text?.take(
                        160,
                    )} visible=${node.isVisibleToUser} scroll=${node.isScrollable} bounds=${Rect().also(
                        node::getBoundsInScreen,
                    )}",
                )
            }
            for (index in 0 until node.childCount) node.getChild(index)?.let { describe(it, depth + 1) }
        }
        instrumentation.uiAutomation.rootInActiveWindow?.let { describe(it, 0) }
        error("Text not visible after bounded scroll: $text: ${labels.joinToString(" | ")}")
    }

    private fun settledBounds(text: String): Rect {
        instrumentation.waitForIdleSync()
        var previous: Rect? = null
        var stable = 0
        repeat(30) {
            val node = find(text)
            val current =
                if (node?.isVisibleToUser ==
                    true
                ) {
                    Rect().also(node::getBoundsInScreen).takeUnless { it.isEmpty }
                } else {
                    null
                }
            stable = if (current != null && current == previous) stable + 1 else 0
            if (stable >= 3) return checkNotNull(current)
            previous = current
            SystemClock.sleep(100)
        }
        error("Dialog text did not settle: $text; last=$previous")
    }

    private fun capture(name: String): Bitmap {
        instrumentation.waitForIdleSync()
        SystemClock.sleep(300)
        val bitmap = checkNotNull(instrumentation.uiAutomation.takeScreenshot())
        File(instrumentation.targetContext.getExternalFilesDir(null), name).outputStream().use {
            check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, it))
        }
        return bitmap
    }

    private fun assertColor(
        text: String,
        color: Int,
    ) {
        val label = scrollTo(text)
        val bounds = Rect().also(label::getBoundsInScreen)
        instrumentation.waitForIdleSync()
        SystemClock.sleep(150)
        val bitmap = checkNotNull(instrumentation.uiAutomation.takeScreenshot())
        try {
            check(bounds.intersect(0, 0, bitmap.width, bitmap.height)) { "Status outside screenshot: $text" }
            var pixels = 0
            for (y in bounds.top until bounds.bottom) {
                for (x in bounds.left until bounds.right) {
                    val actual = bitmap.getPixel(x, y)
                    if (listOf(0, 8, 16).all { shift ->
                            kotlin.math.abs(((actual shr shift) and 255) - ((color shr shift) and 255)) <
                                20
                        }
                    ) {
                        pixels++
                    }
                }
            }
            check(pixels > 2) { "Status color missing: $text; matched=${label.text}; bounds=$bounds" }
        } finally {
            bitmap.recycle()
        }
    }

    private fun checkpoint(message: String) {
        instrumentation.sendStatus(0, Bundle().apply { putString("stream", "XL dialog: $message\n") })
    }

    fun run() {
        val context = instrumentation.targetContext
        val launcher =
            instrumentation.startActivitySync(
                Intent(context, SetupActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            ) as SetupActivity
        try {
            instrumentation.runOnMainSync {
                launcher.requestedOrientation =
                    android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
            }
            SystemClock.sleep(500)
            for (dark in listOf(false, true)) {
                checkpoint("theme dark=$dark")
                // Read explanations through the real JNI catalog using only a synthetic descriptor
                val catalog =
                    checkNotNull(
                        XlCompatibility.inspect("synthetic.mn2", "d2x-name = Synthetic UI fixture\n".toByteArray()),
                    )
                check(catalog.rows.all { it.id.isNotBlank() && it.explanation.isNotBlank() })
                val water =
                    catalog.rows.single { it.id == "water_segments" }.copy(
                        requirement = "1",
                        count = 1,
                        status = "partial",
                    )
                val teleports =
                    catalog.rows.single { it.id == "wall_teleports" }.copy(
                        requirement = "3",
                        count = 3,
                        status = "blocked",
                    )
                val rows =
                    listOf(
                        teleports,
                        water,
                        XlCompatibilityRow(
                            "Eligible fixture",
                            "1",
                            "XL-enabled build required",
                            "eligible",
                            "",
                            1,
                            true,
                        ),
                        XlCompatibilityRow(
                            "Supported fixture",
                            "2",
                            "Synthetic supported feature",
                            "full",
                            "",
                            2,
                            true,
                        ),
                    )
                val report =
                    XlCompatibilityReport("Synthetic UI fixture", "blocked", "Fixture only; no mission loaded", rows)
                instrumentation.runOnMainSync {
                    launcher.setContent {
                        MaterialTheme(colorScheme = if (dark) darkColorScheme() else lightColorScheme()) {
                            XlCompatibilityButton(listOf(report))
                        }
                    }
                }
                instrumentation.waitForIdleSync()
                click("D2X-XL compatibility: cannot load")
                awaitText("D2X-XL extensions")
                scrollTo("Wall teleports: 3")
                instrumentation.waitForIdleSync()
                SystemClock.sleep(300)
                val theme = if (dark) "dark" else "light"
                capture("xl-feature-browser-$theme.png").recycle()
                assertColor("Unsupported", if (dark) 0xFFB4AB else 0xB3261E)
                scrollTo("Water segments: 1")
                assertColor("Experimental / partial", if (dark) 0xFFB86C else 0x8A4B00)
                scrollTo("Eligible fixture: 1")
                assertColor("Eligible; runtime checks needed", if (dark) 0xFFB86C else 0x8A4B00)
                scrollTo("Supported fixture: 2")
                assertColor("Supported", if (dark) 0x81C995 else 0x216E39)
                scrollTo("Water segments: 1", AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)
                val waterBounds = settledBounds("Water segments: 1")
                click("Water segments: 1")
                awaitText("What XL does")
                scrollTo("Current Redux support")
                scrollTo("Feature support does not mean this level is playable.")
                awaitText("audible playback and full visual parity remain unqualified")
                capture("xl-feature-water-$theme.png").recycle()
                instrumentation.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK)
                instrumentation.waitForIdleSync()
                awaitText("D2X-XL extensions")
                val restoredBounds = settledBounds("Water segments: 1")
                check(restoredBounds == waterBounds) {
                    "Explanation Back lost the feature-list position: before=$waterBounds after=$restoredBounds"
                }
                checkpoint("water Back preserved $restoredBounds")
                scrollTo("Wall teleports: 3", AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)
                click("Wall teleports: 3")
                awaitText("What XL does")
                awaitText("Unsupported")
                scrollTo("Current Redux support")
                scrollTo("Wall-trigger actions only; object actions are not decoded")
                click("Back to features")
                click("Close")
                instrumentation.waitForIdleSync()
            }
            checkpoint("light/dark explanations and colors passed")
            val unknownRows =
                listOf(
                    XlCompatibilityRow("Missing fixture", "Not inspected", "Unknown", "unknown", "", null, true),
                    XlCompatibilityRow("Absent fixture", "0", "Not present", "not_required", "", 0, true),
                )
            for (rows in listOf(unknownRows, emptyList())) {
                val report = XlCompatibilityReport("Synthetic unknown fixture", "unknown", "No gameplay claim", rows)
                instrumentation.runOnMainSync {
                    launcher.setContent { MaterialTheme { XlCompatibilityButton(listOf(report)) } }
                }
                instrumentation.waitForIdleSync()
                click("D2X-XL compatibility: unverified")
                if (rows.isEmpty()) {
                    awaitText("No feature inventory available; support is unknown")
                } else {
                    awaitText("Missing fixture: ?")
                    awaitText("Not inspected")
                    awaitText("Absent fixture: 0")
                    awaitText("Not present")
                    click("Missing fixture: ?")
                    awaitText("No verified feature explanation is available")
                    click("Back to features")
                }
                click("Close")
                instrumentation.waitForIdleSync()
            }
            checkpoint("unknown/absent/empty states passed")
            val sourceA =
                XlCompatibilityReport("Source A", "eligible", "Assets still need checks", listOf(unknownRows[0]))
            val sourceB = XlCompatibilityReport("Source B", "unknown", "Sibling not inspected", listOf(unknownRows[1]))
            instrumentation.runOnMainSync {
                launcher.setContent { MaterialTheme { XlCompatibilityButton(listOf(sourceA, sourceB)) } }
            }
            instrumentation.waitForIdleSync()
            click("D2X-XL compatibility: unverified")
            awaitText("Missing fixture: ?")
            click("Source B")
            awaitText("Absent fixture: 0")
            awaitText("Sibling not inspected")
            click("Close")
            instrumentation.waitForIdleSync()
            click("D2X-XL compatibility: unverified")
            awaitText("Absent fixture: 0")
            click("Source A")
            awaitText("Missing fixture: ?")
            click("Close")
            instrumentation.waitForIdleSync()
            val catalog =
                checkNotNull(
                    XlCompatibility.inspect("synthetic.mn2", "d2x-name = Synthetic UI fixture\n".toByteArray()),
                )
            val water =
                catalog.rows.single { it.id == "water_segments" }.copy(
                    requirement = "1",
                    count = 1,
                    status = "partial",
                )
            checkpoint("source switching and reopen passed")
            val longRows =
                List(12) { XlCompatibilityRow("Uninspected category $it", "Unknown", "Unqualified", "unknown", "") } +
                    water.copy(counted = false)
            val longReport = XlCompatibilityReport("Synthetic long inventory", "blocked", "No gameplay claim", longRows)
            instrumentation.runOnMainSync {
                launcher.setContent {
                    MaterialTheme {
                        XlCompatibilityButton(
                            listOf(longReport),
                        )
                    }
                }
            }
            instrumentation.waitForIdleSync()
            click("D2X-XL compatibility: cannot load")
            awaitText("D2X-XL extensions")
            click("Uninspected category 0")
            awaitText("No verified feature explanation is available")
            click("Back to features")
            scrollTo("Water segments")
            val before = settledBounds("Water segments")
            click("Water segments")
            awaitText("What XL does")
            click("Back to features")
            awaitText("D2X-XL extensions")
            val after = settledBounds("Water segments")
            check(
                before == after,
            ) { "Closing an explanation lost a scrolled inventory position: before=$before after=$after" }
            click("Close")
        } finally {
            instrumentation.runOnMainSync { launcher.finish() }
        }
    }
}
