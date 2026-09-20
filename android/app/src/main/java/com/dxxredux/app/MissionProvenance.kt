package com.dxxredux.app

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.json.JSONArray
import org.json.JSONObject

// Matches provenance_file_dates consumed by shared/mission_provenance.hpp
internal data class ArchiveProvenanceDate(
    val file: String,
    val modified: String,
    val kind: String,
) {
    fun toJson(): JSONObject = JSONObject().put("file", file).put("modified", modified).put("kind", kind)
}

internal fun provenanceDatesJson(dates: List<ArchiveProvenanceDate>): JSONArray =
    JSONArray().apply { dates.sortedBy { it.file }.forEach { put(it.toJson()) } }

internal fun provenanceDisplayText(provenance: JSONObject): String =
    buildString {
        val estimate = provenance.optJSONObject("date_estimate") ?: JSONObject()
        val year = estimate.optInt("year")
        appendLine("Estimated vintage: ${if (year > 0) year.toString() else "Unknown"}")
        val basis =
            when (estimate.optString("basis")) {
                "archive_modification_dates" -> "Archive file modification dates"
                "declared_release_or_completion" -> "Declared release or completion date"
                "conflicting_dates" -> "Dates disagree; no single year inferred"
                else -> "No usable date evidence"
            }
        appendLine("Basis: $basis")
        if (year > 0) appendLine("Confidence: ${estimate.optString("confidence")}")
        estimate.optJSONArray("file_year_range")?.let {
            appendLine("File years: ${it.optInt(0)} - ${it.optInt(1)}")
        }
        val credits = provenance.optJSONArray("credits") ?: JSONArray()
        for (index in 0 until credits.length()) {
            val credit = credits.getJSONObject(index)
            val label =
                when (val field = credit.optString("field")) {
                    "nickname" -> "NickName"
                    "real_name" -> "RealName"
                    else -> field.replace('_', ' ').replaceFirstChar { it.uppercase() }
                }
            appendLine()
            appendLine("$label: ${credit.optString("value")}")
            val sources = credit.optJSONArray("sources") ?: JSONArray()
            appendLine("Source: ${(0 until sources.length()).joinToString(", ") { sources.getString(it) }}")
        }
        val dates = provenance.optJSONArray("file_dates") ?: JSONArray()
        if (dates.length() > 0) appendLine("\nArchive file dates:")
        for (index in 0 until dates.length()) {
            val date = dates.getJSONObject(index)
            appendLine("${date.optString("file")}: ${date.optString("modified")}")
            if (date.has("excluded")) appendLine("  Excluded from estimate: invalid or implausible year")
        }
        val notes = provenance.optJSONArray("notes") ?: JSONArray()
        for (index in 0 until notes.length()) appendLine("\n${notes.getString(index)}")
    }

@Composable
internal fun MissionProvenanceDialog(
    provenance: JSONObject,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Mission provenance") },
        confirmButton = { TextButton(onClick = onDismiss) { Text("Close") } },
        text = {
            Column(Modifier.heightIn(max = 420.dp).verticalScroll(rememberScrollState())) {
                SelectionContainer { Text(provenanceDisplayText(provenance)) }
            }
        },
    )
}
