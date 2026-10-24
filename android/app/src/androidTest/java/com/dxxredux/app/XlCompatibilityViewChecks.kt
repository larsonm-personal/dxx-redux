package com.dxxredux.app

import android.app.Instrumentation
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Rect
import android.os.SystemClock
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
        ): AccessibilityNodeInfo? {
            if (node.text?.toString() == text) return node
            if (remaining <= 0) return null
            for (i in 0 until node.childCount) {
                val child = node.getChild(i) ?: continue
                visit(child, remaining - 1)?.let { return it }
            }
            return null
        }
        val root = instrumentation.uiAutomation.rootInActiveWindow ?: return null
        return visit(root, 30)
    }

    private fun awaitText(text: String): AccessibilityNodeInfo {
        val deadline = SystemClock.uptimeMillis() + 10000
        while (SystemClock.uptimeMillis() < deadline) {
            find(text)?.let { return it }
            SystemClock.sleep(100)
        }
        error("Missing dialog text: $text")
    }

    private fun assertColor(
        bitmap: Bitmap,
        text: String,
        color: Int,
    ) {
        val bounds = Rect().also(awaitText(text)::getBoundsInScreen)
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
        check(pixels > 2) { "Status color missing from rendered label: $text" }
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
                val rows =
                    listOf(
                        XlCompatibilityRow(
                            "Unsupported fixture",
                            "3",
                            "Synthetic wall actions",
                            "blocked",
                            "",
                            3,
                            true,
                        ),
                        XlCompatibilityRow(
                            "Partial fixture",
                            "1",
                            "Synthetic experimental feature",
                            "partial",
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
                val button = awaitText("D2X-XL compatibility: cannot load")
                check(button.performAction(AccessibilityNodeInfo.ACTION_CLICK)) { "Compatibility button did not open" }
                awaitText("Supported fixture: 2")
                awaitText("Unsupported fixture: 3")
                awaitText("Partial fixture: 1")
                instrumentation.waitForIdleSync()
                SystemClock.sleep(300)
                val bitmap = checkNotNull(instrumentation.uiAutomation.takeScreenshot())
                try {
                    File(
                        context.getExternalFilesDir(null),
                        "xl-feature-browser-${if (dark) "dark" else "light"}.png",
                    ).outputStream().use {
                        check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, it))
                    }
                    assertColor(bitmap, "Unsupported", if (dark) 0xFFB4AB else 0xB3261E)
                    assertColor(bitmap, "Experimental / partial", if (dark) 0xFFB86C else 0x8A4B00)
                    assertColor(bitmap, "Supported", if (dark) 0x81C995 else 0x216E39)
                } finally {
                    bitmap.recycle()
                }
                check(awaitText("Close").performAction(AccessibilityNodeInfo.ACTION_CLICK))
                instrumentation.waitForIdleSync()
            }
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
                check(awaitText("D2X-XL compatibility: unverified").performAction(AccessibilityNodeInfo.ACTION_CLICK))
                if (rows.isEmpty()) {
                    awaitText("No feature inventory available; support is unknown")
                } else {
                    awaitText("Missing fixture: ?")
                    awaitText("Not inspected")
                    awaitText("Absent fixture: 0")
                    awaitText("Not present")
                }
                check(awaitText("Close").performAction(AccessibilityNodeInfo.ACTION_CLICK))
                instrumentation.waitForIdleSync()
            }
        } finally {
            instrumentation.runOnMainSync { launcher.finish() }
        }
    }
}
