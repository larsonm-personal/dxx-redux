package com.dxxredux.app

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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
private fun compatibilityColor(status: String): Color {
    val dark = MaterialTheme.colorScheme.surface.luminance() < 0.5f
    return when (status) {
        "full" -> if (dark) Color(0xFF81C995) else Color(0xFF216E39)
        "partial" -> if (dark) Color(0xFFFFB86C) else Color(0xFF8A4B00)
        "blocked" -> if (dark) Color(0xFFFFB4AB) else Color(0xFFB3261E)
        else -> MaterialTheme.colorScheme.onSurfaceVariant
    }
}

@Composable
internal fun XlCompatibilityButton(reports: List<XlCompatibilityReport>) {
    if (reports.isEmpty()) return
    var open by remember(reports) { mutableStateOf(false) }
    var selected by remember(reports) { mutableStateOf(0) }
    // Unscanned siblings cannot inherit a detected level's compatibility
    val status = if (reports.any { it.status == "blocked" }) "blocked" else "unknown"
    val label = if (status == "blocked") "D2X-XL compatibility: cannot load" else "D2X-XL compatibility: unverified"
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
                    "Green: verified full support. Orange: verified gameplay with presentation differences. Red: unsupported requirement. Gray: unknown or not required.",
                    fontSize = 12.sp,
                )
                Text(
                    "Only inspected sources are listed. Other levels and external assets remain unverified.",
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
                Row(
                    Modifier.fillMaxWidth().padding(vertical = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text("Extension", Modifier.weight(1f), fontWeight = FontWeight.Bold)
                    Text("This source needs", Modifier.weight(1f), fontWeight = FontWeight.Bold)
                    Text("Our support", Modifier.weight(1f), fontWeight = FontWeight.Bold)
                }
                report.rows.forEach { row ->
                    HorizontalDivider()
                    Row(
                        Modifier.fillMaxWidth().padding(vertical = 8.dp),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text(row.feature, Modifier.weight(1f), fontSize = 12.sp)
                        Text(row.requirement, Modifier.weight(1f), fontSize = 12.sp)
                        Column(Modifier.weight(1f)) {
                            Text(
                                when (row.status) {
                                    "blocked" -> "Unsupported"
                                    "full" -> "Verified"
                                    "partial" -> "Partial"
                                    "eligible" -> "Implemented; runtime checks required"
                                    "not_required" -> "Not required"
                                    else -> "Unverified"
                                },
                                color = compatibilityColor(row.status),
                                fontWeight = FontWeight.Bold,
                                fontSize = 12.sp,
                            )
                            Text(row.support, fontSize = 12.sp)
                            if (row.evidence.isNotBlank()) {
                                Text(
                                    row.evidence,
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                }
            }
        },
    )
}
