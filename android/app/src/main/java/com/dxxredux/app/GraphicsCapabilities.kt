package com.dxxredux.app

import android.os.Build
import org.json.JSONObject
import java.io.File

// Shared schema and filenames: native android_gpu_capabilities.cpp
internal data class GraphicsCapabilities(
    val anisoMax: Int,
    val msaa2: Int,
    val msaa4: Int,
    val anisoReason: String,
    val msaaReason: String,
    val renderer: String,
) {
    fun supportsMsaa(value: Int): Boolean = value == 0 || effectiveMsaa(value) >= value

    fun effectiveMsaa(value: Int): Int =
        when (value) {
            2 -> msaa2
            4 -> msaa4
            else -> 0
        }

    fun supportsAniso(value: Int): Boolean = value == 0 || value <= anisoMax

    fun msaaDetail(): String {
        if (msaa2 == 0 && msaa4 == 0) return msaaReason.ifBlank { "MSAA is unavailable for this color mode" }
        val details =
            listOf(2, 4).map { requested ->
                val actual = effectiveMsaa(requested)
                when {
                    actual == 0 -> "${requested}x is unavailable"
                    actual != requested -> "${requested}x uses ${actual}x on this GPU"
                    else -> "${requested}x supported"
                }
            }
        return details.joinToString(". ")
    }

    fun anisoDetail(): String =
        if (anisoMax > 1) {
            "Supported up to ${anisoMax}x on this GPU"
        } else {
            anisoReason.ifBlank { "Anisotropic filtering is unavailable on this graphics driver" }
        }

    companion object {
        const val UNKNOWN_DETAIL = "Start a game with this color mode to check graphics support."

        fun read(
            filesDir: File,
            colorDepth: Int,
            fingerprint: String = Build.FINGERPRINT,
        ): GraphicsCapabilities? =
            runCatching {
                val data = JSONObject(File(filesDir, "graphics-capabilities-$colorDepth.json").readText())
                require(data.getInt("schema") == 1 && data.getInt("color_depth") == colorDepth)
                require(data.getString("fingerprint") == fingerprint)
                val max = data.getDouble("aniso_max")
                val two = data.getInt("msaa_2")
                val four = data.getInt("msaa_4")
                require(max.isFinite() && max >= 1 && max <= 1024)
                require(two == 0 || two in 2..1024)
                require(four == 0 || four in 4..1024)
                GraphicsCapabilities(
                    max.toInt(),
                    two,
                    four,
                    data.getString("aniso_reason"),
                    data.getString("msaa_reason"),
                    data.getString("renderer"),
                )
            }.getOrNull()
    }
}
