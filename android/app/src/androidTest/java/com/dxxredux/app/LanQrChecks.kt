package com.dxxredux.app

import android.app.Instrumentation
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.net.Uri
import com.dxxredux.app.multiplayer.LanInvitation
import com.dxxredux.app.multiplayer.LanJoinQrView
import com.dxxredux.app.multiplayer.LanJoinRequest
import com.google.zxing.BinaryBitmap
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.qrcode.QRCodeReader

/** Exercises the real rendered square and the exported link router on Android */
internal class LanQrChecks(private val instrumentation: Instrumentation) {
    private fun <T> onMain(block: () -> T): T {
        var result: Result<T>? = null
        instrumentation.runOnMainSync { result = runCatching(block) }
        return result!!.getOrThrow()
    }
    private fun decode(bitmap: Bitmap): String? {
        val pixels = IntArray(bitmap.width * bitmap.height)
        bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
        return try {
            QRCodeReader().decode(BinaryBitmap(HybridBinarizer(RGBLuminanceSource(bitmap.width, bitmap.height, pixels)))).text
        } catch (_: com.google.zxing.ReaderException) { null }
    }

    fun run() {
        val context = instrumentation.targetContext
        val launcher = instrumentation.startActivitySync(Intent(context, SetupActivity::class.java).apply {
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }) as SetupActivity
        onMain {
            val square = LanJoinQrView(launcher)
            square.layoutParams = android.widget.FrameLayout.LayoutParams(264, 264)
            square.layout(0, 0, 264, 264)
            fun render(): Bitmap = Bitmap.createBitmap(264, 264, Bitmap.Config.ARGB_8888).also {
                square.draw(Canvas(it))
            }
            square.setAddress("192.168.1.42")
            check(decode(render()) == null) { "QR exposed before reveal" }
            square.requestFocus()
            check(decode(render()) == null) { "Focus revealed QR" }
            // Exercise explicit activation synchronously; an unattached View queues touch clicks
            square.performClick()
            check(square.revealState.revealed)
            check(decode(render()) == "descent://192.168.1.42") { "Revealed square cannot be decoded" }
            check(square.width == 264 && square.height == 264)
            square.performClick()
            check(decode(render()) == null) { "Second tap did not hide QR" }
            square.performClick()
            check(decode(render()) == "descent://192.168.1.42")
            check(square.performLongClick())
            val expanded = LanJoinQrView::class.java.getDeclaredField("expanded").apply { isAccessible = true }
                .get(square) as android.app.AlertDialog
            check(expanded.isShowing) { "Long press did not enlarge QR" }
            expanded.dismiss()
            check(decode(render()) == "descent://192.168.1.42") { "Long press hid the inline QR" }
            square.setAddress("192.168.1.43")
            check(decode(render()) == null) { "Address change exposed QR" }
            square.performClick()
            check(decode(render()) == "descent://192.168.1.43")
            square.conceal()
            check(decode(render()) == null) { "Conceal left QR pixels" }
            square.setAddress("123.123.123.123")
            square.performClick()
            check(decode(render()) == LanInvitation.encode("123.123.123.123"))
            square.setAddress(null)
            square.performClick()
            check(decode(render()) == null)
        }

        val implicit = Intent(Intent.ACTION_VIEW, Uri.parse("descent://192.168.1.42"))
            .setPackage(context.packageName).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        val resolved = context.packageManager.resolveActivity(implicit, 0)
        check(resolved?.activityInfo?.name == "com.dxxredux.app.multiplayer.LanJoinLinkActivity")
        // Hold the request behind the existing lobby confirmation, without making a network connection
        onMain {
            com.dxxredux.app.lobby.LobbyService.startDiscovery(context, "QrTest")
            com.dxxredux.app.lobby.LobbyService.hostLobby("QrTest", "d2", "d2", "anarchy", 4)
        }
        try {
            context.startActivity(implicit)
            val pendingField = SetupActivity::class.java.getDeclaredField("pendingLanJoin").apply { isAccessible = true }
            var request: LanJoinRequest? = null
            val deadline = System.currentTimeMillis() + 10_000
            while (request == null && System.currentTimeMillis() < deadline) {
                onMain {
                    @Suppress("UNCHECKED_CAST")
                    val state = pendingField.get(launcher) as androidx.compose.runtime.MutableState<LanJoinRequest?>
                    request = state.value
                }
                if (request == null) Thread.sleep(50)
            }
            check(request?.address == "192.168.1.42") { "Router did not deliver to existing launcher" }
            check(com.dxxredux.app.lobby.LobbyService.isHosting.value) { "Link left the active lobby without consent" }
            onMain {
                SetupActivity::class.java.getDeclaredMethod("consumeLanJoin").apply { isAccessible = true }.invoke(launcher)
                com.dxxredux.app.lobby.LobbyService.stopDiscovery()
            }
            // Open the actual scanner through the same accessible button used by a person
            fun click(label: String) {
                val until = System.currentTimeMillis() + 15_000
                while (System.currentTimeMillis() < until) {
                    if (onMain { launcher.performAccessibilityClick(label, exactOnly = true) }) return
                    kotlinx.coroutines.runBlocking(kotlinx.coroutines.Dispatchers.Main) { launcher.scrollDown() }
                    Thread.sleep(150)
                }
                error("Missing button: $label")
            }
            click("Multiplayer")
            val scannerMonitor = instrumentation.addMonitor("com.journeyapps.barcodescanner.CaptureActivity", null, false)
            try {
                click("Read QR code")
                val scanner = instrumentation.waitForMonitorWithTimeout(scannerMonitor, 10_000)
                    ?: error("Read QR code did not launch the scanner")
                onMain {
                    scanner.setResult(android.app.Activity.RESULT_OK, Intent().apply {
                        putExtra(com.google.zxing.client.android.Intents.Scan.RESULT, "https://example.invalid/")
                        putExtra(com.google.zxing.client.android.Intents.Scan.RESULT_FORMAT, "QR_CODE")
                    })
                    scanner.finish()
                }
                val until = System.currentTimeMillis() + 10_000
                var rejected = false
                while (!rejected && System.currentTimeMillis() < until) {
                    rejected = onMain { launcher.findButtonByText("Scan again", exactOnly = true) != null }
                    if (!rejected) Thread.sleep(100)
                }
                check(rejected) { "Scanner result did not reach the invitation validator" }
                click("Cancel")
            } finally {
                instrumentation.removeMonitor(scannerMonitor)
            }
        } finally {
            onMain {
                com.dxxredux.app.lobby.LobbyService.stopDiscovery()
                launcher.finish()
            }
        }
    }
}
