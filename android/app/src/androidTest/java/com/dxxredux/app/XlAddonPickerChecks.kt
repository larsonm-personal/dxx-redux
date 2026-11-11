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
        retainForGameplay: Boolean = false,
        originalMission: String = "wshock20",
    ) {
        require(originalMission == "wshock20" || originalMission == "dinter")
        val context = instrumentation.targetContext
        val sets = FileSetManager(context.filesDir)
        val config = File(context.filesDir, "file_sets.json")
        val oldConfig = config.takeIf { it.exists() }?.readBytes()
        val active = sets.getActive()
        val setName = "xl-picker-check"
        check(
            !retainForGameplay ||
                (originalDistribution && File(context.filesDir, "xl-addon-import-backup/state.tar").isFile),
        ) {
            "Gameplay import requires the protected host runner"
        }
        check(
            !retainForGameplay ||
                ImportLocationManager(context.filesDir).getActiveRoot().canonicalFile ==
                File(context.filesDir, "imported").canonicalFile,
        ) {
            "Gameplay import requires internal disposable storage"
        }
        check(sets.listSets().none { it.name == setName }) { "Previous picker check remains" }
        sets.createSet(setName)
        sets.setActive(setName)
        var launcher: SetupActivity? = null
        var retained = false
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
                val deadline =
                    SystemClock.uptimeMillis() +
                        if (retainForGameplay) {
                            240000
                        } else if (originalDistribution) {
                            120000
                        } else {
                            30000
                        }
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
                    for ((name, hash) in mapOf(
                        "rain.png" to "2fa050b3cf433a363595a41e6604336d053b7d9d7e9e938540a844d7cde110d3",
                        "simplesmoke.png" to "8c39135fddbc35dc85e2132ece9fb0449625ae9b86df3e3eae8cf7fa44bf8c48",
                    )) {
                        check(MissionContentIdentity.compute(File(pack, "textures/d2x-xl/$name")).sha256 == hash)
                    }
                    for ((name, hash) in mapOf(
                        "slowmotion#0.png" to "3133036494c8ef656433842850598072ab4dadde58870389c41745f73977113e",
                        "bullettime#0.png" to "6b0de9039a3a90312e0193d88f9aa804a50d2334454afc52e1a5c5e74eadc1c0",
                    )) {
                        check(MissionContentIdentity.compute(File(pack, "textures/d2/$name")).sha256 == hash)
                    }
                    File(pack, ".resource_inventory.json").copyTo(
                        File(context.getExternalFilesDir(null), "xl-addon-original-inventory.json"),
                        overwrite = true,
                    )
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
            if (retainForGameplay) {
                val names =
                    listOf(
                        "descent2.ham",
                        "descent2.hog",
                        "descent2.s11",
                        "descent2.s22",
                        "groupa.pig",
                        "alien1.pig",
                        "alien2.pig",
                        "fire.pig",
                        "ice.pig",
                        "water.pig",
                        "$originalMission.hog",
                        "$originalMission.mn2",
                        "d2x.ham",
                    )
                val inputs = names.map { File(archive.parentFile, it) }
                check(inputs.all { it.isFile }) { "Original gameplay inputs are missing" }
                val missionHashes =
                    if (originalMission == "dinter") {
                        listOf(
                            "b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13",
                            "89ea5005ce971a3c7462dbfe727bbfa9b17b8766432e5f418e93c50c76ec72b1",
                        )
                    } else {
                        listOf(
                            "2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c",
                            "e4ee4da1dfc16b48b91be3409216e73cf43dd74e6e20d1be40b6bb310fe5a42a",
                        )
                    }
                for ((extension, hash) in listOf("hog", "mn2").zip(missionHashes)) {
                    check(
                        MissionContentIdentity.compute(inputs[names.indexOf("$originalMission.$extension")]).sha256 ==
                            hash,
                    )
                }
                pickFiles(inputs) {
                    click("Import All")
                    val target = sets.getSetDir(setName)
                    val deadline = SystemClock.uptimeMillis() + 30000

                    fun ready(): Boolean =
                        inputs.all { source ->
                            target.walkTopDown().any {
                                it.isFile && it.name == source.name && it.length() == source.length()
                            }
                        }
                    while (!ready() && SystemClock.uptimeMillis() < deadline) SystemClock.sleep(100)
                    check(ready()) { "Ordinary retail/mission import did not finish" }
                    val manager = ModManager.forActiveSet(context.filesDir, context)
                    check(manager.listMods().any { it.filename == "xl-resources" && it.enabled })
                    check(
                        MissionContentIdentity.compute(File(manager.modFile("xl-resources"), "d2x.ham")).sha256 ==
                            MissionContentIdentity.compute(File(archive.parentFile, "d2x.ham")).sha256,
                    )
                    instrumentation.waitForIdleSync()
                }
                retained = true
            }
        } finally {
            launcher?.let { activity -> instrumentation.runOnMainSync { activity.finish() } }
            instrumentation.waitForIdleSync()
            if (!retained) {
                sets.setActive(active)
                sets.deleteSet(setName, context)
                if (oldConfig == null) config.delete() else config.writeBytes(oldConfig)
            }
        }
    }
}
