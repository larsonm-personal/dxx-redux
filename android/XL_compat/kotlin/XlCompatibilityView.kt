package com.dxxredux.app

import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
private fun compatibilityColor(status: String): Color {
    val dark = MaterialTheme.colorScheme.surface.luminance() < 0.5f
    return when (status) {
        "full" -> if (dark) Color(0xFF81C995) else Color(0xFF216E39)
        "partial", "eligible" -> if (dark) Color(0xFFFFB86C) else Color(0xFF8A4B00)
        "blocked" -> if (dark) Color(0xFFFFB4AB) else Color(0xFFB3261E)
        else -> MaterialTheme.colorScheme.onSurfaceVariant
    }
}

@Composable
internal fun XlCompatibilityButton(reports: List<XlCompatibilityReport>) {
    if (reports.isEmpty()) return
    var open by remember(reports) { mutableStateOf(false) }
    var selected by remember(reports) { mutableStateOf(0) }
    var explanation by remember(reports) { mutableStateOf<XlCompatibilityRow?>(null) }
    // Unscanned siblings cannot inherit a detected level's compatibility
    val status = compatibilityGroupStatus(reports)
    val label =
        when (status) {
            "blocked" -> "D2X-XL compatibility: cannot load"
            "eligible" -> "D2X-XL compatibility: runtime checks needed"
            else -> "D2X-XL compatibility: unverified"
        }
    OutlinedButton(
        onClick = { open = true },
        colors = ButtonDefaults.outlinedButtonColors(contentColor = compatibilityColor(status)),
        modifier = Modifier.fillMaxWidth(),
    ) { Text(label) }
    if (!open) return
    AlertDialog(
        onDismissRequest = { open = false },
        confirmButton = { TextButton(onClick = { open = false }) { Text("Close") } },
        title = { Text("D2X-XL extensions") },
        text = {
            Column(Modifier.heightIn(max = 520.dp).verticalScroll(rememberScrollState())) {
                Text(
                    "Green: supported feature. Orange: experimental, partial, or eligible pending runtime checks. Red: unsupported feature. Gray: not inspected or not present. Feature support does not mean this level is playable.",
                    fontSize = 12.sp,
                )
                Text(
                    "Only inspected sources are listed. Other levels and external assets remain unverified. Tap a row for its explanation.",
                    fontSize = 12.sp,
                    modifier = Modifier.padding(vertical = 8.dp),
                )
                Row(Modifier.horizontalScroll(rememberScrollState())) {
                    reports.forEachIndexed { index, report ->
                        TextButton(onClick = { selected = index }) {
                            Text(
                                report.source,
                                fontWeight =
                                    if (index ==
                                        selected
                                    ) {
                                        FontWeight.Bold
                                    } else {
                                        FontWeight.Normal
                                    },
                            )
                        }
                    }
                }
                val report = reports[selected]
                Text(report.summary, color = compatibilityColor(report.status), fontWeight = FontWeight.Bold)
                if (report.rows.isEmpty()) Text("No feature inventory available; support is unknown", fontSize = 12.sp)
                report.rows.sortedByDescending { it.counted }.forEach { row ->
                    HorizontalDivider()
                    Column(
                        Modifier.fillMaxWidth().heightIn(min = 48.dp).clickable(
                            role = Role.Button,
                            onClickLabel = "Explain ${row.feature}",
                            onClick = { explanation = row },
                        ),
                    ) {
                        if (row.counted) {
                            Column(Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
                                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    Text(row.shortTitle(), Modifier.weight(1f), fontWeight = FontWeight.Bold)
                                    Text(
                                        compatibilityStatusLabel(row.status),
                                        color = compatibilityColor(row.status),
                                        fontSize = 12.sp,
                                    )
                                }
                                if (row.count == null) Text("Not inspected", fontSize = 12.sp)
                                if (row.count != 0) Text(row.support, fontSize = 12.sp)
                            }
                        } else {
                            Row(
                                Modifier.fillMaxWidth().padding(vertical = 8.dp),
                                horizontalArrangement = Arrangement.spacedBy(8.dp),
                            ) {
                                Text(row.shortTitle(), Modifier.weight(1f), fontSize = 12.sp)
                                Text(row.requirement, Modifier.weight(1f), fontSize = 12.sp)
                                Column(Modifier.weight(1f)) {
                                    Text(
                                        compatibilityStatusLabel(row.status),
                                        color = compatibilityColor(row.status),
                                        fontWeight = FontWeight.Bold,
                                        fontSize = 12.sp,
                                    )
                                    Text(row.support, fontSize = 12.sp, maxLines = 3, overflow = TextOverflow.Ellipsis)
                                    if (row.evidence.isNotBlank()) {
                                        Text(
                                            row.evidence,
                                            fontSize = 11.sp,
                                            maxLines = 3,
                                            overflow = TextOverflow.Ellipsis,
                                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }
        },
    )
    explanation?.let { row ->
        AlertDialog(
            onDismissRequest = { explanation = null },
            confirmButton = {
                TextButton(onClick = { explanation = null }) { Text("Back to features") }
            },
            title = { Text(row.feature) },
            text = {
                Column(
                    Modifier.heightIn(max = 520.dp).verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    Text(row.shortTitle(), fontWeight = FontWeight.Bold)
                    Text(compatibilityStatusLabel(row.status), color = compatibilityColor(row.status))
                    Text("Inspected requirement", fontWeight = FontWeight.Bold)
                    Text(row.requirement)
                    Text("What XL does", fontWeight = FontWeight.Bold)
                    Text(row.xlExplanation())
                    Text("Current Redux support", fontWeight = FontWeight.Bold)
                    Text(row.reduxExplanation())
                    if (row.evidence.isNotBlank()) {
                        Text("Evidence", fontWeight = FontWeight.Bold)
                        Text(row.evidence)
                    }
                    Text("Feature support does not mean this level is playable.", fontSize = 12.sp)
                }
            },
        )
    }
}
