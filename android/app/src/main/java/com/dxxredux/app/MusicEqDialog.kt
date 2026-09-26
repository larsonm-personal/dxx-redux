package com.dxxredux.app

import android.graphics.Paint
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import org.json.JSONObject
import kotlin.math.log10

@Composable
internal fun MusicEqDialog(
    name: String,
    profile: String,
    preset: String,
    busy: Boolean,
    error: String?,
    onSelect: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val asset =
        remember(
            context,
        ) {
            context.assets
                .open("music_eq_sc55.json")
                .bufferedReader()
                .use { JSONObject(it.readText()) }
        }
    val curve = remember(preset, asset) { MusicEqResponse.curve(preset, asset) }
    var expanded by remember { mutableStateOf(false) }
    val measured = MusicEq.supportsProfile(profile)
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("eq $name") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Frequency response", style = MaterialTheme.typography.labelLarge)
                EqCurve(curve)
                val choices =
                    listOf(MusicEq.FLAT to "Flat (no EQ)") +
                        if (measured) {
                            listOf(
                                MusicEq.BALANCED to "Measured EQ - match SC-55 recordings",
                            )
                        } else {
                            emptyList()
                        }
                choices.forEach { (id, label) ->
                    val selected = if (id == MusicEq.FLAT) preset == id else preset != MusicEq.FLAT
                    Row(
                        Modifier.fillMaxWidth().selectable(
                            selected,
                            enabled = !busy,
                            role = Role.RadioButton,
                            onClick = { onSelect(id) },
                        ),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        RadioButton(selected = selected, onClick = null, enabled = !busy)
                        Text(label)
                    }
                }
                if (measured && preset != MusicEq.FLAT) {
                    Text("Curve smoothing", style = MaterialTheme.typography.labelLarge)
                    Box {
                        OutlinedButton(
                            onClick = { expanded = true },
                            enabled = !busy,
                            modifier = Modifier.fillMaxWidth().tvFocusBorder(),
                        ) {
                            Text(MusicEq.smoothing.getValue(preset))
                        }
                        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                            MusicEq.smoothing.forEach { (id, label) ->
                                DropdownMenuItem(text = { Text(label) }, onClick = {
                                    expanded = false
                                    onSelect(id)
                                })
                            }
                        }
                    }
                    Text(
                        "Broad smooths more; Detail follows the measured curve more closely.",
                    )
                }
                Text(
                    if (measured) {
                        "Average correction from 28 D1/D2 recordings for this SC-55 bank."
                    } else if (profile ==
                        MusicEq.OPL3
                    ) {
                        "OPL3 uses Flat. The recordings do not yet justify a measured correction."
                    } else {
                        "Flat is the default for this soundfont. No measured correction is available for this bank."
                    },
                    style = MaterialTheme.typography.bodySmall,
                )
                Text(
                    "Saved per profile. Changing EQ stops the preview. Games use it on the next launch.",
                    style = MaterialTheme.typography.bodySmall,
                )
                error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss, enabled = !busy, modifier = Modifier.tvFocusBorder()) { Text("Close") }
        },
    )
}

@Composable
private fun EqCurve(curve: List<Pair<Float, Float>>) {
    val colors = MaterialTheme.colorScheme
    Canvas(
        Modifier.fillMaxWidth().height(190.dp).semantics {
            contentDescription = "EQ frequency response, 20 Hz to 20 kHz, minus 12 to plus 12 dB"
        },
    ) {
        val left = 34.dp.toPx()
        val right = size.width - 12.dp.toPx()
        val top = 16.dp.toPx()
        val bottom = size.height - 27.dp.toPx()

        fun x(hz: Float) = left + (log10(hz / 20f) / 3f) * (right - left)

        fun y(db: Float) = top + (12f - db) / 24f * (bottom - top)
        val paint =
            Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = colors.onSurfaceVariant.toArgb()
                textSize =
                    10.dp.toPx()
            }
        drawRect(
            colors.surfaceVariant,
            topLeft = Offset(left, top),
            size =
                androidx.compose.ui.geometry
                    .Size(right - left, bottom - top),
        )
        for (db in listOf(-12, -6, 0, 6, 12)) {
            drawLine(
                colors.onSurfaceVariant.copy(
                    alpha =
                        if (db ==
                            0
                        ) {
                            .65f
                        } else {
                            .18f
                        },
                ),
                Offset(left, y(db.toFloat())),
                Offset(right, y(db.toFloat())),
            )
            paint.textAlign = Paint.Align.RIGHT
            drawContext.canvas.nativeCanvas.drawText(
                if (db >
                    0
                ) {
                    "+$db"
                } else {
                    "$db"
                },
                left - 5.dp.toPx(),
                y(db.toFloat()) + 3.dp.toPx(),
                paint,
            )
        }
        for ((hz, label) in listOf(20f to "20", 100f to "100", 1000f to "1k", 5000f to "5k", 20000f to "20k")) {
            drawLine(colors.onSurfaceVariant.copy(alpha = .18f), Offset(x(hz), top), Offset(x(hz), bottom))
            paint.textAlign =
                when (hz) {
                    20f -> Paint.Align.LEFT
                    20000f -> Paint.Align.RIGHT
                    else -> Paint.Align.CENTER
                }
            drawContext.canvas.nativeCanvas.drawText(label, x(hz), bottom + 15.dp.toPx(), paint)
        }
        paint.textAlign = Paint.Align.LEFT
        drawContext.canvas.nativeCanvas.drawText("dB", 0f, top - 4.dp.toPx(), paint)
        paint.textAlign = Paint.Align.RIGHT
        drawContext.canvas.nativeCanvas.drawText("Hz", right, size.height - 1.dp.toPx(), paint)
        val path = Path()
        curve.forEachIndexed { i, (hz, db) -> if (i == 0) path.moveTo(x(hz), y(db)) else path.lineTo(x(hz), y(db)) }
        drawPath(path, colors.primary, style = Stroke(width = 2.5.dp.toPx()))
    }
}
