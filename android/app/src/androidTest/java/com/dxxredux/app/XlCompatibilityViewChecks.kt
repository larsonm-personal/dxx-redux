package com.dxxredux.app

import android.app.Instrumentation
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Rect
import android.os.SystemClock
import android.view.KeyEvent
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

    private fun scrollTo(
        text: String,
        direction: Int = AccessibilityNodeInfo.ACTION_SCROLL_FORWARD,
    ): AccessibilityNodeInfo {
        var lastDiagnostics = ""
        repeat(10) {
            find(text)?.let { target ->
                if (target.isVisibleToUser) return target
                target.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SHOW_ON_SCREEN.id)
                instrumentation.waitForIdleSync()
                SystemClock.sleep(250)
                find(text)?.takeIf { it.isVisibleToUser }?.let { return it }
            }

            fun scroll(node: AccessibilityNodeInfo): Boolean {
                val vertical =
                    if (direction == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD) {
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP.id
                    } else {
                        AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN.id
                    }
                if (node.isScrollable && node.actionList.any { it.id == vertical } &&
                    node.performAction(vertical)
                ) {
                    return true
                }
                for (index in 0 until node.childCount) {
                    val child = node.getChild(index) ?: continue
                    if (scroll(child)) return true
                }
                return false
            }
            val moved = scroll(checkNotNull(instrumentation.uiAutomation.rootInActiveWindow))
            instrumentation.waitForIdleSync()
            SystemClock.sleep(250)
            if (!moved) {
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
                            )} visible=${node.isVisibleToUser} scroll=${node.isScrollable} " +
                                "actions=${node.actionList.map {
                                    it.id
                                }} bounds=${Rect().also(
                                    node::getBoundsInScreen,
                                )}",
                        )
                    }
                    for (index in 0 until node.childCount) node.getChild(index)?.let { describe(it, depth + 1) }
                }
                instrumentation.uiAutomation.rootInActiveWindow?.let { describe(it, 0) }
                lastDiagnostics = labels.joinToString(" | ")
            }
        }
        error("Text not visible after bounded scroll: $text; $lastDiagnostics")
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
                awaitText("Supported fixture: 2")
                awaitText("Wall teleports: 3")
                awaitText("Water segments: 1")
                instrumentation.waitForIdleSync()
                SystemClock.sleep(300)
                val theme = if (dark) "dark" else "light"
                capture("xl-feature-browser-$theme.png").recycle()
                assertColor("Unsupported", if (dark) 0xFFB4AB else 0xB3261E)
                assertColor("Experimental / partial", if (dark) 0xFFB86C else 0x8A4B00)
                assertColor("Eligible; runtime checks needed", if (dark) 0xFFB86C else 0x8A4B00)
                assertColor("Supported", if (dark) 0x81C995 else 0x216E39)
                val waterBounds =
                    Rect().also(
                        scrollTo("Water segments: 1", AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)::getBoundsInScreen,
                    )
                click("Water segments: 1")
                awaitText("What XL does")
                scrollTo("Current Redux support")
                scrollTo("Feature support does not mean this level is playable.")
                awaitText("audible playback and full visual parity remain unqualified")
                capture("xl-feature-water-$theme.png").recycle()
                instrumentation.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK)
                instrumentation.waitForIdleSync()
                val restoredBounds = Rect().also(awaitText("Water segments: 1")::getBoundsInScreen)
                check(restoredBounds == waterBounds) { "Explanation Back lost the feature-list position" }
                scrollTo("Wall teleports: 3", AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)
                click("Wall teleports: 3")
                awaitText("What XL does")
                scrollTo("Current Redux support")
                awaitText("Unsupported")
                click("Back to features")
                click("Close")
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
            click("Uninspected category 0")
            awaitText("No verified feature explanation is available")
            click("Back to features")
            awaitText("Close")
            val before = Rect().also(scrollTo("Water segments")::getBoundsInScreen)
            click("Water segments")
            awaitText("What XL does")
            click("Back to features")
            awaitText("Close")
            val after = Rect().also(awaitText("Water segments")::getBoundsInScreen)
            check(before == after) { "Closing an explanation lost a scrolled inventory position: $before -> $after" }
            click("Close")
        } finally {
            instrumentation.runOnMainSync { launcher.finish() }
        }
    }

    /** Optional original-package witness; requires an already imported, unchanged fixture */
    fun runDragonPackage(archivePath: String) {
        val context = instrumentation.targetContext
        val archive = File(archivePath)
        val digest = java.security.MessageDigest.getInstance("SHA-256")
        archive.inputStream().use { input ->
            val buffer = ByteArray(65536)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                digest.update(buffer, 0, count)
            }
        }
        check(
            digest.digest().joinToString("") { "%02x".format(it.toInt() and 255) } ==
                "b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719",
        )
        val store = checkNotNull(missionZipExtractedStoreForArchivePath(archivePath))
        val record = checkNotNull(store.reusableRecord(archive.name, archive))
        val scan = checkNotNull(MissionZip.inspectExtracted(record))
        val mission = scan.missionSets.single { it.mission.path == "missions/spotlight/drgpit.msn" }
        val target = checkNotNull(store.extractedTarget(archivePath, context.filesDir, scan, mission))
        val catalog = checkNotNull(target.packageCompatibility)
        check(!catalog.blocksAnalysis && catalog.status == "unknown")
        check(catalog.rows.all { it.status == "unknown" })
        val sound = catalog.rows.single { it.id == "scoped_sounds" }
        check(sound.requirement == "2 catalogued candidate files")
        check(
            sound.evidence.contains(
                "mods/drgpit/sounds/boss01.wav [SHA-256 f448d3506def983bd2ba3e9d79a24386930ff7aeba956f2eaed666b443201480]",
            ),
        )
        check(
            sound.evidence.contains(
                "mods/drgpit/sounds/boss02.wav [SHA-256 91fa0e3fb5ff3187929e6511ebfebff002b8ef47c3225d5b99be42fd67571f3d]",
            ),
        )
        val reports = kotlinx.coroutines.runBlocking { inspectXlCompatibilityTarget(target) }
        check(reports.contains(catalog))
        check(
            reports.analysisBlockers().any { report ->
                report.rows.any {
                    it.id == "companion_lighting" &&
                        it.status == "blocked"
                }
            },
        )
        check(compatibilityGroupStatus(reports) == "blocked")
        File(context.getExternalFilesDir(null), "xl-feature-dragon-evidence.txt").writeText(
            reports.joinToString("\n\n") { report ->
                "${report.source}\nstatus=${report.status} blocks_analysis=${report.blocksAnalysis}\n" +
                    report.rows.joinToString("\n") { "${it.id}: ${it.requirement} [${it.status}]\n${it.evidence}" }
            },
        )
        val launcher =
            instrumentation.startActivitySync(
                Intent(context, SetupActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            ) as SetupActivity
        try {
            instrumentation.runOnMainSync {
                launcher.requestedOrientation = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                launcher.setContent { MaterialTheme { XlCompatibilityButton(reports) } }
            }
            instrumentation.waitForIdleSync()
            click("D2X-XL compatibility: cannot load")
            scrollTo("Mission WAV effects and sound banks")
            click("Mission WAV effects and sound banks")
            awaitText("What XL does")
            scrollTo("2 catalogued candidate files")
            scrollTo("mods/drgpit/sounds/boss01.wav")
            awaitText("Back to features")
            capture("xl-feature-dragon-sounds.png").recycle()
            check(find("Supported") == null)
            click("Back to features")
            click("Close")
        } finally {
            instrumentation.runOnMainSync { launcher.finish() }
        }
    }
}
