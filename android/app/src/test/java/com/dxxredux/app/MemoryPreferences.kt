package com.dxxredux.app

import android.content.SharedPreferences
import java.lang.reflect.Proxy

/** Minimal JVM preference backend for exercising real asset/selection workflows. */
internal fun memoryPreferences(): SharedPreferences {
    val values = mutableMapOf<String, Any>()
    return Proxy.newProxyInstance(SharedPreferences::class.java.classLoader, arrayOf(SharedPreferences::class.java)) { _, method, args ->
        when (method.name) {
            "getString", "getBoolean", "getInt" -> values[args!![0]] ?: args[1]
            "contains" -> values.containsKey(args!![0])
            "getAll" -> values.toMap()
            "edit" -> {
                val pending = mutableMapOf<String, Any?>()
                lateinit var editor: SharedPreferences.Editor
                editor = Proxy.newProxyInstance(SharedPreferences.Editor::class.java.classLoader, arrayOf(SharedPreferences.Editor::class.java)) { _, edit, arguments ->
                    when (edit.name) {
                        "putString", "putBoolean", "putInt" -> { pending[arguments!![0] as String] = arguments[1]; editor }
                        "commit", "apply" -> {
                            pending.forEach { (key, value) -> if (value == null) values.remove(key) else values[key] = value }
                            if (edit.name == "commit") true else null
                        }
                        else -> error("Unexpected preference edit: ${edit.name}")
                    }
                } as SharedPreferences.Editor
                editor
            }
            else -> error("Unexpected preference operation: ${method.name}")
        }
    } as SharedPreferences
}
