package com.dxxredux.app

import android.app.Activity
import android.app.Instrumentation
import android.content.Intent
import android.os.SystemClock
import android.util.Log
import android.view.KeyEvent
import android.view.accessibility.AccessibilityNodeInfo
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.mutableStateOf
import kotlinx.coroutines.runBlocking
import java.io.File
import java.util.concurrent.atomic.AtomicInteger
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/** Exercise production metadata and recommended dialogs using the accessibility/introspection API */
internal class PackageDocumentChecks(
    private val instrumentation: Instrumentation,
) {
    private fun node(text: String): AccessibilityNodeInfo? {
        fun find(node: AccessibilityNodeInfo): AccessibilityNodeInfo? {
            if (node.text?.contains(text) == true || node.contentDescription?.contains(text) == true) return node
            for (i in 0 until node.childCount) node.getChild(i)?.let { child -> find(child)?.let { return it } }
            return null
        }
        return instrumentation.uiAutomation.rootInActiveWindow?.let(::find)
    }

    private fun waitFor(
        message: String,
        condition: () -> Boolean,
    ) {
        val deadline = SystemClock.elapsedRealtime() + 15_000
        while (!condition()) {
            if (SystemClock.elapsedRealtime() >= deadline) {
                fun dump(
                    current: AccessibilityNodeInfo,
                    depth: Int,
                ) {
                    Log.i(
                        "DXX-Documents",
                        "${" ".repeat(
                            depth,
                        )}text=${current.text} description=${current.contentDescription} scrollable=${current.isScrollable}",
                    )
                    for (i in 0 until current.childCount) current.getChild(i)?.let { dump(it, depth + 1) }
                }
                instrumentation.uiAutomation.rootInActiveWindow?.let { dump(it, 0) }
                instrumentation.uiAutomation.takeScreenshot()?.let { bitmap ->
                    File(
                        instrumentation.targetContext.getExternalFilesDir(null),
                        "package-documents-failure.png",
                    ).outputStream().use {
                        bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, it)
                    }
                    bitmap.recycle()
                }
            }
            check(SystemClock.elapsedRealtime() < deadline) { message }
            Thread.sleep(40)
        }
    }

    private fun click(text: String) {
        waitFor("Missing clickable control: $text") {
            var control = node(text)
            while (control != null && !control.isClickable) control = control.parent
            control?.performAction(AccessibilityNodeInfo.ACTION_CLICK) == true
        }
        instrumentation.waitForIdleSync()
    }

    private fun visible(text: String) = waitFor("Missing text: $text") { node(text) != null }

    private fun reveal(text: String) {
        fun scroll(node: AccessibilityNodeInfo): Boolean {
            if (node.isScrollable && node.performAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD)) return true
            for (i in 0 until node.childCount) {
                if (node.getChild(i)?.let(::scroll) == true) return true
            }
            return false
        }
        waitFor("Could not scroll to control: $text") {
            if (node(text) != null) {
                true
            } else {
                instrumentation.uiAutomation.rootInActiveWindow?.let(::scroll)
                instrumentation.waitForIdleSync()
                Thread.sleep(100)
                false
            }
        }
    }

    private fun runMissionFileList(
        activity: SetupActivity,
        root: File,
    ) {
        val fixtureRoot = File(root, "mission-files").apply { mkdirs() }
        val archive = File(fixtureRoot, "mods/files.zip").apply { parentFile!!.mkdirs() }
        ZipOutputStream(archive.outputStream()).use { zip ->
            fun entry(
                name: String,
                bytes: ByteArray,
            ) {
                zip.putNextEntry(ZipEntry(name))
                zip.write(bytes)
                zip.closeEntry()
            }
            entry("files.mn2", "name = File browser fixture\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            entry("files.hog", "DHF".toByteArray())
            entry("level01.rl2", ByteArray(16))
            for (index in 0..29) entry("00tone%02d.wav".format(index), ByteArray(16))
        }
        val scan = checkNotNull(MissionZip.inspect(archive))
        MissionZipExtractionStore(fixtureRoot).ensureExtracted(archive.name, archive, scan)
        val mod =
            ModManager.ModInfo(
                archive.name,
                "File browser fixture",
                false,
                0,
                archive.length(),
                "d2",
                0,
                MissionZip.KIND,
            )
        val details = ModManager(fixtureRoot).getModDetails(mod, fixtureRoot)
        check(details.missionZip != null)
        instrumentation.runOnMainSync {
            activity.setContent { MaterialTheme { ModDetailsDialog(mod, details, false, null, 0, fixtureRoot) {} } }
        }
        reveal("00tone00.wav")
        var list = checkNotNull(node("00tone00.wav"))
        while (!list.isScrollable) list = checkNotNull(list.parent)

        fun hasArrow(
            node: AccessibilityNodeInfo,
            label: String,
        ): Boolean {
            node.refresh()
            if (node.contentDescription?.toString() == label) return true
            return (0 until node.childCount).any { node.getChild(it)?.let { child -> hasArrow(child, label) } == true }
        }
        waitFor("Files list has no independent down arrow") {
            list.refresh()
            val outer = checkNotNull(list.parent)
            if (hasArrow(outer, "Scroll down")) {
                true
            } else {
                outer.performAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD)
                instrumentation.waitForIdleSync()
                Thread.sleep(200)
                false
            }
        }
        click("tone00.wav")
        visible("No additional metadata for this file type")
        check(node("Reading extracted file metadata") == null)
        click("Close")
        visible("File browser fixture")
        reveal("tone00.wav")
        list = checkNotNull(node("tone00.wav"))
        while (!list.isScrollable) list = checkNotNull(list.parent)
        check(list.performAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD))
        instrumentation.waitForIdleSync()
        waitFor("Files list has no independent up arrow") {
            list.refresh()
            hasArrow(checkNotNull(list.parent), "Scroll up")
        }
        val limitation =
            runBlocking {
                scanMissionMovies(
                    instrumentation.targetContext,
                    LevelMetadataTarget(displayName = "Pumo fixture", game = "d2", sourceType = "mission"),
                    listOf(XlCompatibilityReport("pmines.hog", "eligible", "", emptyList(), true)),
                )
            }
        check(limitation.status == "unsupported_format")
        Log.i("DXX-Documents", "PASS: extracted media details, independent Files arrows and XL movie boundary")
    }

    fun run(verifyPublished: Boolean = false) {
        val context = instrumentation.targetContext
        val root = File(context.cacheDir, "document-integration-${System.nanoTime()}").apply { mkdirs() }
        if (verifyPublished) {
            try {
                runBlocking {
                    for (recommended in RECOMMENDED_MODS) {
                        val docs = loadRecommendedModDocuments(File(root, "published"), recommended)
                        check(docs.warning == null)
                        check(docs.documents.isNotEmpty())
                        for (document in docs.documents.filter { PackageDocuments.isInlineReadmeCandidate(it.path) }) {
                            val content = PackageDocuments.readTextFile(docs.archive, document.path)
                            check(content.problem == null && content.text.isNotBlank())
                        }
                        Log.i(
                            "DXX-Documents",
                            "Verified published ${recommended.filename}: ${docs.documents.map { it.path }}",
                        )
                    }
                }
            } catch (error: Throwable) {
                root.deleteRecursively()
                throw error
            }
        }
        val archive = File(root, "mods/fixture.dxa").apply { parentFile!!.mkdirs() }
        val originals =
            linkedMapOf(
                "README.md" to "Original author instructions",
                "README.dxx-redux.md" to "Redux conversion details",
                "original.rtf" to "{\\rtf1 Original RTF bytes}",
            )
        ZipOutputStream(archive.outputStream()).use { zip ->
            originals.forEach { (name, text) ->
                zip.putNextEntry(ZipEntry(name))
                zip.write(text.toByteArray())
                zip.closeEntry()
            }
        }
        val mod = ModManager.ModInfo(archive.name, "Document fixture", false, 0, archive.length(), "d1", 0)
        val details = ModManager(root).getModDetails(mod, root)
        check(details.documents.size == 3 && details.missionZip == null)
        val activity =
            instrumentation.startActivitySync(
                Intent(context, SetupActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            ) as SetupActivity
        val mode = mutableStateOf(0)
        val installed = mutableStateOf(false)
        val loadCalls = AtomicInteger()
        val externalCalls = AtomicInteger()
        val external =
            object : Instrumentation.ActivityMonitor() {
                override fun onStartActivity(intent: Intent): Instrumentation.ActivityResult? {
                    if (intent.action != Intent.ACTION_VIEW) return null
                    check(intent.type == "application/rtf")
                    check(intent.flags and Intent.FLAG_GRANT_READ_URI_PERMISSION != 0)
                    val bytes =
                        context.contentResolver
                            .openInputStream(
                                requireNotNull(intent.data),
                            )!!
                            .use { it.readBytes() }
                    check(bytes.contentEquals(originals.getValue("original.rtf").toByteArray()))
                    externalCalls.incrementAndGet()
                    return Instrumentation.ActivityResult(Activity.RESULT_OK, null)
                }
            }
        instrumentation.addMonitor(external)
        try {
            instrumentation.runOnMainSync {
                activity.setContent {
                    MaterialTheme {
                        when (mode.value) {
                            0, 1, 2 -> {
                                ModDetailsDialog(
                                    mod,
                                    details.copy(
                                        documents =
                                            when (mode.value) {
                                                1 -> details.documents.take(1)
                                                2 -> emptyList()
                                                else -> details.documents
                                            },
                                    ),
                                    false,
                                    null,
                                    0,
                                    root,
                                    onDismiss = { mode.value = -1 },
                                )
                            }

                            3 -> {
                                RecommendedModsDialog(
                                    installedNames =
                                        if (installed.value) {
                                            setOf(
                                                RECOMMENDED_MODS.first().filename,
                                            )
                                        } else {
                                            emptySet()
                                        },
                                    downloadProgress = emptyMap(),
                                    onDownload = { error("Docs must not download or install a mod") },
                                    onDismiss = { mode.value = -1 },
                                    loadDocuments = { _, requested ->
                                        check(requested == RECOMMENDED_MODS.first())
                                        when (loadCalls.incrementAndGet()) {
                                            1 -> {
                                                error("Injected unavailable documentation")
                                            }

                                            2 -> {
                                                RecommendedModDocuments.Loaded(archive, details.documents)
                                            }

                                            3 -> {
                                                RecommendedModDocuments.Loaded(
                                                    archive,
                                                    details.documents.take(1),
                                                    "Saved copy: could not check for updates",
                                                )
                                            }

                                            else -> {
                                                RecommendedModDocuments.Loaded(archive, details.documents.take(1))
                                            }
                                        }
                                    },
                                )
                            }
                        }
                    }
                }
            }
            click("View docs")
            visible("Documents")
            visible("Redux adaptation notes")
            click("README.dxx-redux.md")
            visible("Redux conversion details")
            instrumentation.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK)
            visible("Documents")
            click("README.md")
            visible("Original author instructions")
            click("Close")
            visible("Documents")
            click("original.rtf")
            waitFor("External viewer was not invoked") { externalCalls.get() == 1 }
            visible("Documents")
            click("Close")
            visible("View docs")

            instrumentation.runOnMainSync { mode.value = 1 }
            click("View readme")
            visible("Original author instructions")
            click("Close")
            visible("View readme")
            instrumentation.runOnMainSync { mode.value = 2 }
            instrumentation.waitForIdleSync()
            waitFor("Documentation controls remained visible for a document-free mod") {
                node("Document fixture") != null && node("View docs") == null && node("View readme") == null
            }

            instrumentation.runOnMainSync { mode.value = 3 }
            visible("Download")
            reveal("View docs")
            click("View docs")
            visible("Injected unavailable documentation")
            click("Retry")
            visible("Published documentation")
            click("README.dxx-redux.md")
            visible("Redux conversion details")
            click("Close")
            visible("Documents")
            check(loadCalls.get() == 2) { "Returning to docs repeated the download" }
            click("Close")
            visible("Recommended mods")
            instrumentation.runOnMainSync { installed.value = true }
            visible("Installed")
            reveal("View docs")
            click("View docs")
            visible("Original author instructions")
            visible("Published documentation")
            visible("Saved copy")
            click("Retry")
            visible("Original author instructions")
            waitFor("Retry left a stale cache warning") { node("Saved copy") == null }
            check(loadCalls.get() == 4)
            click("Close")
            visible("Recommended mods")
            runMissionFileList(activity, root)
        } finally {
            instrumentation.removeMonitor(external)
            instrumentation.runOnMainSync { activity.finish() }
            root.deleteRecursively()
        }
    }
}
