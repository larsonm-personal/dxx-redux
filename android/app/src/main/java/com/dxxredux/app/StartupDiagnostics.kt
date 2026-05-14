package com.dxxredux.app

import android.content.Intent
import android.os.Bundle
import android.os.Process
import android.os.SystemClock
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject

private const val STARTUP_DIAGNOSTIC_TAG = "DXX-Startup"

internal fun logGameStartupDiagnostic(
    event: String,
    details: JSONObject = JSONObject(),
) {
    val payload =
        JSONObject()
            .put("event", event)
            .put("pid", Process.myPid())
            .put("elapsed_ms", SystemClock.elapsedRealtime())
            .put("details", details)
    val line = payload.toString()
    DebugLog.log(DebugLogCategory.GAME, "startup diagnostic: $line")
    Log.i(STARTUP_DIAGNOSTIC_TAG, line)
}

internal fun Intent.startupJson(): JSONObject =
    JSONObject()
        .putNullable("action", action)
        .putNullable("component", component?.flattenToShortString())
        .putNullable("data", dataString)
        .put("flags", flags)
        .put("extras", extras.startupJson())

internal fun JSONObject.putNullable(
    name: String,
    value: Any?,
): JSONObject = put(name, value ?: JSONObject.NULL)

internal fun Bundle?.startupJson(): JSONObject {
    val bundle = this ?: return JSONObject()
    return JSONObject().apply {
        for (key in bundle.keySet().sorted()) {
            put(key, startupValueToJson(startupBundleValue(bundle, key)))
        }
    }
}

@Suppress("DEPRECATION")
private fun startupBundleValue(
    bundle: Bundle,
    key: String,
): Any? = bundle.get(key)

private fun startupValueToJson(value: Any?): Any =
    when (value) {
        null -> JSONObject.NULL
        is String -> value
        is Number -> value
        is Boolean -> value
        is Bundle -> value.startupJson()
        is IntArray -> JSONArray().also { arr -> value.forEach { arr.put(it) } }
        is LongArray -> JSONArray().also { arr -> value.forEach { arr.put(it) } }
        is FloatArray -> JSONArray().also { arr -> value.forEach { arr.put(it) } }
        is DoubleArray -> JSONArray().also { arr -> value.forEach { arr.put(it) } }
        is BooleanArray -> JSONArray().also { arr -> value.forEach { arr.put(it) } }
        is Array<*> -> JSONArray().also { arr -> value.forEach { arr.put(startupValueToJson(it)) } }
        else -> value.toString()
    }
