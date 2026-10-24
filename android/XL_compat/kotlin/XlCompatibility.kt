package com.dxxredux.app

import org.json.JSONObject

internal data class XlCompatibilityRow(
    val feature: String,
    val requirement: String,
    val support: String,
    val status: String,
    val evidence: String,
    val count: Int? = null,
    val counted: Boolean = false,
    val id: String = "",
    val explanation: String = "",
    val limitations: String = "",
)

internal data class XlCompatibilityReport(
    val source: String,
    val status: String,
    val summary: String,
    val rows: List<XlCompatibilityRow>,
)

internal object XlCompatibility {
    private val available by lazy { runCatching { System.loadLibrary("xl_compat_metadata") }.isSuccess }

    fun inspect(
        name: String,
        bytes: ByteArray,
    ): XlCompatibilityReport? {
        if (!available) return null
        val raw = runCatching { nativeInspect(name, bytes) }.getOrNull() ?: return null
        return parse(raw)
    }

    internal fun parse(raw: String): XlCompatibilityReport? =
        runCatching {
            val json = JSONObject(raw)
            if (!json.getBoolean("detected")) return null
            val rows = json.getJSONArray("rows")
            XlCompatibilityReport(
                json.getString("source"),
                json.getString("status"),
                json.getString("summary"),
                List(rows.length()) { index ->
                    val row = rows.getJSONObject(index)
                    val counted = row.has("count")
                    val count = (row.opt("count") as? Int)?.takeIf { it >= 0 }
                    XlCompatibilityRow(
                        row.getString("feature"),
                        row.getString("requirement"),
                        row.getString("support"),
                        if (counted && count == null) "unknown" else row.getString("status"),
                        row.getString("evidence"),
                        count,
                        counted,
                        row.optString("id", ""),
                        row.optString("explanation", ""),
                        row.optString("limitations", ""),
                    )
                },
            )
        }.getOrNull()

    private external fun nativeInspect(
        name: String,
        bytes: ByteArray,
    ): String?
}

internal fun XlCompatibilityRow.shortTitle(): String = if (counted) "$feature: ${count?.toString() ?: "?"}" else feature

internal fun compatibilityStatusLabel(status: String): String =
    when (status) {
        "full" -> "Supported"
        "partial" -> "Experimental / partial"
        "blocked" -> "Unsupported"
        "eligible" -> "Eligible; runtime checks needed"
        "not_required" -> "Not present"
        else -> "Not inspected"
    }

internal fun XlCompatibilityRow.xlExplanation(): String =
    explanation.ifBlank {
        "No verified feature explanation is available in this report. Older or unrecognized metadata does not establish XL behavior or support."
    }

internal fun XlCompatibilityRow.reduxExplanation(): String =
    buildString {
        append(limitations.ifBlank { support }.ifBlank { "Support has not been verified." })
        when (status) {
            "eligible" -> {
                append(
                    " Requires an XL-enabled build and mission/resource checks at load time; this is not verified gameplay or campaign support.",
                )
            }

            "unknown" -> {
                append(" This feature has not been fully inspected for this source.")
            }

            "not_required" -> {
                append(
                    " No records of this scoped feature were found; this does not qualify the level for gameplay.",
                )
            }
        }
    }

// Missing or unknown sibling evidence must not inherit another level's eligibility
internal fun compatibilityGroupStatus(reports: List<XlCompatibilityReport>): String =
    when {
        reports.any { it.status == "blocked" } -> "blocked"
        reports.isNotEmpty() && reports.all { it.status == "eligible" } -> "eligible"
        else -> "unknown"
    }
