package com.dxxredux.app.multiplayer

import android.app.Activity
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import com.dxxredux.app.TextButton

@Composable
internal fun LanHostQr() {
    val context = LocalContext.current
    val source = remember { LanHostAddresses(context) }
    val addresses by source.addresses.collectAsState()
    var preferred by remember { mutableStateOf(LanHostAddresses.preferred(context)) }
    val selected = addresses.find { it.address == preferred } ?: addresses.firstOrNull()
    val square = remember { LanJoinQrView(context) }
    DisposableEffect(context) {
        source.onNetworkLost = square::conceal
        val lifecycle = (context as LifecycleOwner).lifecycle
        val observer =
            LifecycleEventObserver { _, event ->
                if (event == Lifecycle.Event.ON_RESUME) source.start()
                if (event == Lifecycle.Event.ON_PAUSE) {
                    square.conceal()
                    source.stop()
                }
            }
        lifecycle.addObserver(observer)
        if (lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED)) source.start()
        onDispose {
            lifecycle.removeObserver(observer)
            source.stop()
            square.conceal()
        }
    }
    Column {
        AndroidView(factory = { square }, modifier = Modifier.size(132.dp), update = {
            it.setAddress(selected?.address)
            selected?.let { address -> LanHostAddresses.select(context, address.address) }
        })
        if (addresses.size > 1) {
            TextButton(onClick = {
                val index = addresses.indexOf(selected)
                preferred = addresses[(index + 1) % addresses.size].address
            }) { Text(selected?.label.orEmpty()) }
        }
    }
}

/** Small actual View bounds keep all touches outside the QR on the SDL/menu layers */
internal class LanJoinQrOverlay(
    private val activity: Activity,
    frame: FrameLayout,
    preferredAddress: String?,
) {
    private val source = LanHostAddresses(activity)
    internal val square = LanJoinQrView(activity)
    private var preferred = preferredAddress
    private var active = false

    init {
        val density = activity.resources.displayMetrics.density
        val side = (112 * density).toInt()
        val margin = (8 * density).toInt()
        frame.addView(
            square,
            FrameLayout.LayoutParams(side, side, Gravity.BOTTOM or Gravity.LEFT).apply {
                leftMargin = margin
                bottomMargin = margin
            },
        )
        square.visibility = View.GONE
        frame.addOnLayoutChangeListener { _, left, top, right, bottom, _, _, _, _ ->
            // Keep clear of the native player checkboxes, including the eighth row
            val fittedSide =
                minOf(
                    side,
                    ((right - left) * 0.10f).toInt(),
                    ((bottom - top) * 0.28f).toInt(),
                ).coerceAtLeast(1)
            val params = square.layoutParams as FrameLayout.LayoutParams
            if (params.width != fittedSide) {
                params.width = fittedSide
                params.height = fittedSide
                square.layoutParams = params
                square.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX, minOf(13 * density, fittedSide / 6f))
            }
        }
        ViewCompat.setOnApplyWindowInsetsListener(square) { view, insets ->
            val safe = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            val params = view.layoutParams as FrameLayout.LayoutParams
            params.leftMargin = safe.left + margin
            params.bottomMargin = safe.bottom + margin
            view.layoutParams = params
            insets
        }
        source.onChanged = ::updateAddress
        source.onNetworkLost = square::conceal
    }

    private fun updateAddress(addresses: List<LanHostAddress>) {
        val selected = addresses.find { it.address == preferred } ?: addresses.firstOrNull()
        preferred = selected?.address
        square.setAddress(selected?.address)
    }

    fun show(visible: Boolean) {
        if (visible == active) return
        active = visible
        if (visible) {
            source.start()
            updateAddress(source.addresses.value)
            square.visibility = View.VISIBLE
            ViewCompat.requestApplyInsets(square)
        } else {
            square.conceal()
            square.visibility = View.GONE
            source.stop()
        }
    }
}
