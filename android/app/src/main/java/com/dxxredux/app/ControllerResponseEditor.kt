package com.dxxredux.app

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

@Composable
internal fun ControllerResponseEditor(
    response: ControllerAxisResponse,
    deadzone: Int,
    input: Float,
    unipolar: Boolean,
    onChange: (ControllerAxisResponse) -> Unit,
) {
    Text("Actual response", style = MaterialTheme.typography.labelLarge)
    Text("Center sensitivity: ${(response.center * 100).roundToInt()}%", style = MaterialTheme.typography.labelMedium)
    Slider(
        value = response.center,
        onValueChange = { onChange(response.copy(center = it)) },
        valueRange = 0f..1f,
        steps = 19,
        modifier = Modifier.fillMaxWidth().tvFocusBorder().semantics { contentDescription = "Center sensitivity" },
    )
    Text("Expo: ${(response.expo * 100).roundToInt()}%", style = MaterialTheme.typography.labelMedium)
    Slider(
        value = response.expo,
        onValueChange = { onChange(response.copy(expo = it)) },
        valueRange = 0f..1f,
        steps = 19,
        modifier = Modifier.fillMaxWidth().tvFocusBorder().semantics { contentDescription = "Expo" },
    )
    Text(
        "Lower center sensitivity gives finer control. Higher expo extends that region. Full output stays at 100%.",
        style = MaterialTheme.typography.bodySmall,
    )
    Row {
        TextButton(onClick = { onChange(ControllerAxisResponse(1f, 0f)) }) { Text("Linear") }
        TextButton(onClick = { onChange(ControllerAxisResponse(0.5f, 0f)) }) { Text("Gentle") }
        TextButton(onClick = { onChange(ControllerAxisResponse(0.25f, 0.5f)) }) { Text("Fine") }
    }
    if (response.center == 1f) {
        Text(
            "At 100% center sensitivity the response is linear; expo has no effect.",
            style = MaterialTheme.typography.bodySmall,
        )
    }
    val sample = if (input.isFinite()) input.coerceIn(if (unipolar) 0f else -1f, 1f) else 0f
    val output = applyControllerAxisResponse(sample, deadzone, response)
    Text(
        "Input: ${(sample * 100).roundToInt()}%   Output: ${(output * 100).roundToInt()}%",
        style = MaterialTheme.typography.labelMedium,
    )
    ControllerResponseGraph(response, deadzone, sample, unipolar)
    Text("Solid: controller command  /  Dashed: linear reference", style = MaterialTheme.typography.bodySmall)
}

@Composable
private fun ControllerResponseGraph(
    response: ControllerAxisResponse,
    deadzone: Int,
    input: Float,
    unipolar: Boolean,
) {
    val colors = MaterialTheme.colorScheme
    val measurer = rememberTextMeasurer()
    val labelStyle = TextStyle(color = colors.onSurface, fontSize = 11.sp)
    val graphHeight = if (LocalConfiguration.current.screenHeightDp < 450) 130.dp else 185.dp
    Canvas(
        Modifier.fillMaxWidth().height(graphHeight).semantics {
            contentDescription =
                "Physical input horizontally and controller command vertically, in percent, with dead zone and live sample"
        },
    ) {
        val left = 40.dp.toPx()
        val top = 22.dp.toPx()
        val right = size.width - 14.dp.toPx()
        val bottom = size.height - 36.dp.toPx()
        val minimum = if (unipolar) 0f else -1f

        fun x(v: Float) = left + (v - minimum) / (1f - minimum) * (right - left)

        fun y(v: Float) = bottom - (v - minimum) / (1f - minimum) * (bottom - top)

        fun label(
            text: String,
            at: Offset,
        ) = drawText(measurer, text, at, labelStyle)
        val d = deadzone.coerceIn(0, 95) / 100f
        val deadLeft = x(if (unipolar) 0f else -d)
        drawRect(colors.onSurface.copy(alpha = 0.07f), Offset(deadLeft, top), Size(x(d) - deadLeft, bottom - top))
        val ticks = if (unipolar) listOf(0f, 0.5f, 1f) else listOf(-1f, 0f, 1f)
        for (t in ticks) {
            drawLine(colors.outlineVariant, Offset(left, y(t)), Offset(right, y(t)))
            drawLine(colors.outlineVariant, Offset(x(t), top), Offset(x(t), bottom))
            val text = (t * 100).roundToInt().toString()
            val measured = measurer.measure(text, labelStyle)
            label(text, Offset(x(t) - measured.size.width / 2f, bottom + 3.dp.toPx()))
            label(text, Offset(left - measured.size.width - 5.dp.toPx(), y(t) - measured.size.height / 2f))
        }
        label("Output %", Offset(left, 0f))
        val title = "Physical input %"
        label(
            title,
            Offset(
                (left + right - measurer.measure(title, labelStyle).size.width) / 2f,
                size.height - 15.dp.toPx(),
            ),
        )
        drawLine(
            colors.outline,
            Offset(x(minimum), y(minimum)),
            Offset(x(1f), y(1f)),
            1.dp.toPx(),
            pathEffect = PathEffect.dashPathEffect(floatArrayOf(5.dp.toPx(), 4.dp.toPx())),
        )
        val path = Path()
        for (i in 0..200) {
            val raw = minimum + (1f - minimum) * i / 200f
            val point = Offset(x(raw), y(applyControllerAxisResponse(raw, deadzone, response)))
            if (i == 0) path.moveTo(point.x, point.y) else path.lineTo(point.x, point.y)
        }
        drawPath(path, colors.primary, style = Stroke(2.dp.toPx()))
        drawCircle(
            colors.tertiary,
            4.dp.toPx(),
            Offset(x(input), y(applyControllerAxisResponse(input, deadzone, response))),
        )
    }
}
