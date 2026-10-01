package com.dxxredux.app.multiplayer

import android.app.AlertDialog
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.view.Gravity
import android.widget.ImageView
import androidx.appcompat.widget.AppCompatTextView
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter
import com.google.zxing.qrcode.decoder.ErrorCorrectionLevel

internal fun lanQrBitmap(
    address: String,
    size: Int,
): Bitmap {
    val matrix =
        QRCodeWriter().encode(
            LanInvitation.encode(address),
            BarcodeFormat.QR_CODE,
            size,
            size,
            mapOf(EncodeHintType.ERROR_CORRECTION to ErrorCorrectionLevel.M, EncodeHintType.MARGIN to 4),
        )
    val pixels =
        IntArray(matrix.width * matrix.height) { index ->
            if (matrix[index % matrix.width, index / matrix.width]) Color.BLACK else Color.WHITE
        }
    return Bitmap.createBitmap(pixels, matrix.width, matrix.height, Bitmap.Config.ARGB_8888)
}

/** Shared by Compose and the native lobby; only this square receives reveal taps */
internal class LanJoinQrView(
    context: Context,
) : AppCompatTextView(context) {
    internal val revealState = LanQrRevealState()
    private var expanded: AlertDialog? = null
    private var bitmap: Bitmap? = null

    init {
        gravity = Gravity.CENTER
        textSize = 13f
        setTextColor(Color.WHITE)
        isFocusable = true
        setOnClickListener {
            if (revealState.revealed) {
                enlarge()
            } else if (revealState.reveal()) {
                render()
            }
        }
        render()
    }

    fun setAddress(address: String?) {
        if (address == revealState.address) return
        expanded?.dismiss()
        revealState.setAddress(address)
        bitmap = null
        render()
    }

    fun conceal() {
        expanded?.dismiss()
        expanded = null
        revealState.conceal()
        render()
    }

    private fun render() {
        val address = revealState.address
        isEnabled = address != null
        if (revealState.revealed && address != null && width > 0) {
            val code = bitmap ?: lanQrBitmap(address, width).also { bitmap = it }
            background = BitmapDrawable(resources, code).apply { isFilterBitmap = false }
            text = ""
            contentDescription = "Scan to join $address. Activate to enlarge QR code"
        } else {
            setBackgroundColor(0xFF30343A.toInt())
            text = if (address == null) "QR unavailable\nNo LAN address" else "Tap to show\nQR code"
            contentDescription = text
        }
    }

    private fun enlarge() {
        val address = revealState.address ?: return
        val size =
            (resources.displayMetrics.widthPixels * 0.75f)
                .toInt()
                .coerceAtMost((resources.displayMetrics.heightPixels * 0.65f).toInt())
                .coerceAtLeast(1)
        val image =
            ImageView(context).apply {
                setImageBitmap(lanQrBitmap(address, size))
                adjustViewBounds = true
                contentDescription = "Scan to join $address"
            }
        expanded =
            AlertDialog
                .Builder(context)
                .setTitle("Scan to join")
                .setMessage("$address\nSame Wi-Fi or local network")
                .setView(image)
                .setPositiveButton("Close", null)
                .setNeutralButton("Hide code") { _, _ -> conceal() }
                .show()
    }

    override fun onSizeChanged(
        w: Int,
        h: Int,
        oldw: Int,
        oldh: Int,
    ) {
        super.onSizeChanged(w, h, oldw, oldh)
        bitmap = null
        render()
    }

    override fun onDetachedFromWindow() {
        conceal()
        super.onDetachedFromWindow()
    }
}
