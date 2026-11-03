package com.dxxredux.app

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
private fun compatibilityColor(status: String): Color {
    val dark = MaterialTheme.colorScheme.surface.luminance() < 0.5f
    return when (status) {
        "full", "supported", "not_required" -> if (dark) Color(0xFF81C995) else Color(0xFF216E39)
        "partial", "eligible", "unknown" -> if (dark) Color(0xFFFFB86C) else Color(0xFF8A4B00)
        "blocked" -> if (dark) Color(0xFFFFB4AB) else Color(0xFFB3261E)
        else -> MaterialTheme.colorScheme.onSurfaceVariant
    }
}

@Composable
internal fun XlCompatibilityButton(reports: List<XlCompatibilityReport>) {
    if (reports.isEmpty()) return
    var open by remember(reports) { mutableStateOf(false) }
    var explanation by remember(reports) { mutableStateOf<XlCompatibilityRow?>(null) }
    // Unscanned siblings cannot inherit a detected level's compatibility
    val status = compatibilityGroupStatus(reports)
    val label = "D2X-XL feature support"
    val rows = remember(reports) { compatibilitySummaryRows(reports) }
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
                    "Green: supported or not required. Yellow: partial or unverified. Red: unsupported. Feature support does not establish campaign playability.",
                    fontSize = 12.sp,
                )
                Text(
                    "Whole-package summary from ${reports.size} source reports. Counts cover inspected records; uninspected levels and external assets remain unverified. Tap a feature for details.",
                    fontSize = 12.sp,
                    modifier = Modifier.padding(vertical = 8.dp),
                )
                if (rows.isEmpty()) Text("No feature inventory available", fontSize = 12.sp)
                rows.forEach { row ->
                    HorizontalDivider()
                    Column(
                        Modifier
                            .fillMaxWidth()
                            .heightIn(min = 48.dp)
                            .clickable(
                                role = Role.Button,
                                onClickLabel = "Explain ${row.feature}",
                                onClick = { explanation = row },
                            ).padding(vertical = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                    ) {
                        Text(row.feature, fontWeight = FontWeight.Bold)
                        Text(compatibilityStatusLabel(row.status), color = compatibilityColor(row.status))
                        if (row.counted && row.count != null) Text("${row.count} inspected records", fontSize = 12.sp)
                        if (row.count != 0) Text(row.support, fontSize = 12.sp)
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
