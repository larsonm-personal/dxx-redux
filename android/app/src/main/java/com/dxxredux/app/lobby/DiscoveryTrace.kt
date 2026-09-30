package com.dxxredux.app.lobby

import org.json.JSONObject

// Optional discovery fields only; no full payloads or player/mission data in traces
internal fun discoveryQueryReply(
    data: ByteArray,
    query: JSONObject,
): ByteArray {
    val reply = parsePacket(data, data.size) ?: return data
    for (key in listOf("trace_id", "trace_ms", "trace_strategy", "trace_run", "trace_phase")) {
        if (query.has(key)) reply.put("reply_$key", query.get(key))
    }
    return reply.toString().toByteArray(Charsets.UTF_8)
}

internal fun discoveryTraceSummary(json: JSONObject): String =
    "type=${json.optString("type")} id=${json.optString("trace_id")} " +
        "sent_ms=${json.optLong("trace_ms")} strategy=${json.optString("trace_strategy")} " +
        "run=${json.optString("trace_run")} phase=${json.optString("trace_phase")} " +
        "reply_to=${json.optString("reply_trace_id")} reply_sent_ms=${json.optLong("reply_trace_ms")} " +
        "reply_strategy=${json.optString("reply_trace_strategy")} " +
        "reply_run=${json.optString("reply_trace_run")} reply_phase=${json.optString("reply_trace_phase")} " +
        "status=${json.optString("status")} query_reply=${json.optBoolean("query_reply") }"
