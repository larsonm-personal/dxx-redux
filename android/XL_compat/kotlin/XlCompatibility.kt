package com.dxxredux.app

import org.json.JSONObject

internal data class XlCompatibilityRow(
    val feature: String,
    val requirement: String,
    val support: String,
    val status: String,
    val evidence: String,
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
        return runCatching {
            val json = JSONObject(nativeInspect(name, bytes) ?: return null)
            if (!json.getBoolean("detected")) return null
            val rows = json.getJSONArray("rows")
            XlCompatibilityReport(
                json.getString("source"),
                json.getString("status"),
                json.getString("summary"),
                List(rows.length()) { index ->
                    val row = rows.getJSONObject(index)
                    XlCompatibilityRow(
                        row.getString("feature"),
                        row.getString("requirement"),
                        row.getString("support"),
                        row.getString("status"),
                        row.getString("evidence"),
                    )
                },
            )
        }.getOrNull()
    }

    private external fun nativeInspect(
        name: String,
        bytes: ByteArray,
    ): String?
}
