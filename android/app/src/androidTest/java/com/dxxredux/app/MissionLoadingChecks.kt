package com.dxxredux.app

import android.app.Instrumentation
import android.content.Intent
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.accessibility.AccessibilityNodeInfo
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.mutableStateOf
import com.dxxredux.app.multiplayer.CreateGameDialog
import com.dxxredux.app.multiplayer.MissionCatalog
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

internal class MissionLoadingChecks(
    private val instrumentation: Instrumentation,
) {
    private fun textNode(text: String): AccessibilityNodeInfo? {
        fun find(node: AccessibilityNodeInfo): AccessibilityNodeInfo? {
            if (node.text?.contains(text) == true || node.contentDescription?.contains(text) == true) return node
            for (index in 0 until node.childCount) {
                node.getChild(index)?.let { child -> find(child)?.let { return it } }
            }
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
        waitFor("Missing control: $text") {
            var node = textNode(text)
            while (node != null && !node.isClickable) node = node.parent
            node?.performAction(AccessibilityNodeInfo.ACTION_CLICK) == true
        }
    }

    fun run() {
        val activity =
            instrumentation.startActivitySync(
                Intent(
                    instrumentation.targetContext,
                    SetupActivity::class.java,
                ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            ) as SetupActivity
        val visible = mutableStateOf(true)
        val opened = CountDownLatch(1)
        val release = CountDownLatch(1)
        val calls = AtomicInteger()
        val cancelled = CountDownLatch(1)
        val failNext =
            java.util.concurrent.atomic
                .AtomicBoolean(false)
        try {
            instrumentation.runOnMainSync {
                activity.setContent {
                    MaterialTheme {
                        if (visible.value) {
                            CreateGameDialog(
                                title = "Host LAN Game",
                                confirmLabel = "Host",
                                onCreate = {
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _,
                                    ->
                                    error("Unexpected host launch")
                                },
                                onDismiss = {
                                    visible.value = false
                                    cancelled.countDown()
                                },
                                loadCatalog = { context, game ->
                                    check(Looper.myLooper() != Looper.getMainLooper()) { "Catalog loaded on UI thread" }
                                    if (calls.incrementAndGet() == 1) {
                                        opened.countDown()
                                        check(release.await(15, TimeUnit.SECONDS)) { "Fixture was never released" }
                                    }
                                    currentCoroutineContext().ensureActive()
                                    if (failNext.getAndSet(false)) error("Injected catalog read failure")
                                    MissionCatalog.load(context, game)
                                },
                            )
                        }
                    }
                }
            }
            check(opened.await(10, TimeUnit.SECONDS))
            waitFor("Dialog did not show loading promptly") { textNode("Loading missions and saves") != null }
            val heartbeat = CountDownLatch(1)
            Handler(Looper.getMainLooper()).post { heartbeat.countDown() }
            check(heartbeat.await(1, TimeUnit.SECONDS)) { "Blocked catalog scan froze UI" }
            click("Cancel")
            check(cancelled.await(1, TimeUnit.SECONDS)) { "Cancel blocked on catalog scan" }
            waitFor("Dialog did not dismiss") { textNode("Host LAN Game") == null }
            release.countDown()
            failNext.set(true)
            instrumentation.runOnMainSync { visible.value = true }
            waitFor("Catalog failure was not visible") { textNode("Could not load missions") != null }
            click("Retry")
            waitFor("Retry never loaded missions and saves") {
                calls.get() >= 3 && textNode("Loading missions and saves") == null &&
                    textNode("Could not load missions") == null
            }
            val afterLoad = calls.get()
            click("Anarchy")
            click("Coop")
            instrumentation.waitForIdleSync()
            check(calls.get() == afterLoad) { "Mode changes rescanned the catalog" }
            click("Cancel")
        } finally {
            release.countDown()
            instrumentation.runOnMainSync { activity.finish() }
            instrumentation.waitForIdleSync()
        }
    }
}
