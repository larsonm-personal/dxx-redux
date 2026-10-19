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
        } finally {
            instrumentation.removeMonitor(external)
            instrumentation.runOnMainSync { activity.finish() }
            root.deleteRecursively()
        }
    }
}
