package com.dxxredux.app

import android.app.Activity
import android.app.Instrumentation
import android.content.ClipData
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.os.SystemClock
import android.view.accessibility.AccessibilityNodeInfo
import java.io.File

/** General Import Files button and its real picker callback, with a disposable file set */
internal class XlAddonPickerChecks(
    private val instrumentation: Instrumentation,
) {
    private fun find(text: String): AccessibilityNodeInfo? {
        val automation = instrumentation.uiAutomation
        if (android.os.Build.VERSION.SDK_INT >= 34) automation.clearCache()

        fun visit(
            node: AccessibilityNodeInfo,
            remaining: Int,
        ): AccessibilityNodeInfo? {
            if (node.text?.toString() == text) return node
            if (remaining <= 0) return null
            for (index in 0 until node.childCount) {
                node.getChild(index)?.let { visit(it, remaining - 1) }?.let { return it }
            }
            return null
        }
        return automation.rootInActiveWindow?.let { visit(it, 30) }
    }

    fun reveal(text: String): AccessibilityNodeInfo {
        val deadline = SystemClock.uptimeMillis() + 15000
        var direction = AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD
        while (SystemClock.uptimeMillis() < deadline) {
            val node = find(text)
            if (node != null) {
                node.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SHOW_ON_SCREEN.id)
                return node
            }

            fun scroll(node: AccessibilityNodeInfo): Boolean {
                if (node.isScrollable && node.performAction(direction)) return true
                for (index in 0 until node.childCount) {
                    if (node.getChild(index)?.let(::scroll) == true) return true
                }
                return false
            }
            instrumentation.uiAutomation.rootInActiveWindow?.let {
                if (!scroll(it)) direction = AccessibilityNodeInfo.ACTION_SCROLL_FORWARD
            }
            SystemClock.sleep(250)
        }
        val labels = mutableListOf<String>()

        fun collect(
            node: AccessibilityNodeInfo,
            remaining: Int,
        ) {
            node.text?.toString()?.let(labels::add)
            if (remaining > 0) {
                for (index in 0 until node.childCount) node.getChild(index)?.let { collect(it, remaining - 1) }
            }
        }
        instrumentation.uiAutomation.rootInActiveWindow?.let { collect(it, 30) }
        error("Missing launcher button: $text; visible labels: $labels")
    }

    private fun click(text: String) {
        val deadline = SystemClock.uptimeMillis() + 15000
        while (SystemClock.uptimeMillis() < deadline) {
            var node: AccessibilityNodeInfo? = reveal(text)
            while (node != null && !node.isClickable) node = node.parent
            if (node?.isEnabled == true && node.performAction(AccessibilityNodeInfo.ACTION_CLICK)) return
            SystemClock.sleep(100)
        }
        error("Cannot click $text")
    }

    fun pickFiles(
        files: List<File>,
        completed: () -> Unit,
    ) {
        check(files.isNotEmpty() && files.all { it.isFile })
        val context = instrumentation.targetContext
        val uris = files.map { Uri.parse("content://${context.packageName}.saf-test/seekable/${it.name}") }
        val result = Intent().setData(uris.first()).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        if (uris.size > 1) {
            result.clipData =
                ClipData.newRawUri("Original test files", uris.first()).apply {
                    uris.drop(1).forEach { addItem(ClipData.Item(it)) }
                }
        }
        val monitor =
            instrumentation.addMonitor(
                IntentFilter(Intent.ACTION_OPEN_DOCUMENT).apply {
                    addCategory(Intent.CATEGORY_OPENABLE)
                    addDataType("*/*")
                },
                Instrumentation.ActivityResult(Activity.RESULT_OK, result),
                true,
            )
        try {
            click("\uD83D\uDCC2 Select Game Files or Archive to Import")
            click("Pick One or More Files")
            completed()
            check(monitor.hits == 1) { "General import did not launch its document picker" }
        } finally {
            instrumentation.removeMonitor(monitor)
        }
    }

    fun run(
        archive: File,
        originalDistribution: Boolean = false,
    ) {
        val context = instrumentation.targetContext
        val sets = FileSetManager(context.filesDir)
        val config = File(context.filesDir, "file_sets.json")
        val oldConfig = config.takeIf { it.exists() }?.readBytes()
        val active = sets.getActive()
        val setName = "xl-picker-check"
        check(sets.listSets().none { it.name == setName }) { "Previous picker check remains" }
        sets.createSet(setName)
        sets.setActive(setName)
        var launcher: SetupActivity? = null
        try {
            launcher =
                instrumentation.startActivitySync(
                    Intent(
                        context,
                        SetupActivity::class.java,
                    ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK),
                ) as SetupActivity
            instrumentation.waitForIdleSync()
            pickFiles(listOf(archive)) {
                val deadline = SystemClock.uptimeMillis() + if (originalDistribution) 120000 else 30000
                while (ModManager.forActiveSet(context.filesDir, context).listMods().isEmpty() &&
                    SystemClock.uptimeMillis() < deadline
                ) {
                    SystemClock.sleep(100)
                }
                val manager = ModManager.forActiveSet(context.filesDir, context)
                val imported = manager.listMods().single()
                check(imported.filename == "xl-resources" && imported.enabled)
                val pack = manager.modFile("xl-resources")
                if (originalDistribution) {
                    check(
                        MissionContentIdentity.compute(File(pack, "d2x-xl.hog")).sha256 ==
                            "e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6",
                    )
                    check(!File(pack, "d2x.ham").exists())
                    check(File(pack, "textures/d2x-xl/smokingfire.png").isFile)
                    check(File(pack, "sounds/d2x-xl/lightning.wav").isFile)
                } else {
                    check(File(pack, "d2x-xl.hog").readText() == "camera bytes")
                    check(File(pack, "d2x.ham").readText() == "vertigo bytes")
                    check(!File(pack, "hoard.ham").exists())
                }
                reveal("Imported mod: XL resources")
                instrumentation.waitForIdleSync()
                val screenshot = checkNotNull(instrumentation.uiAutomation.takeScreenshot())
                try {
                    val image = if (originalDistribution) "xl-addon-picker-original.png" else "xl-addon-picker.png"
                    File(context.getExternalFilesDir(null), image).outputStream().use {
                        check(screenshot.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, it))
                    }
                } finally {
                    screenshot.recycle()
                }
            }
        } finally {
            launcher?.let { activity -> instrumentation.runOnMainSync { activity.finish() } }
            instrumentation.waitForIdleSync()
            sets.setActive(active)
            sets.deleteSet(setName, context)
            if (oldConfig == null) config.delete() else config.writeBytes(oldConfig)
        }
    }
}
