package com.dxxredux.app

import org.json.JSONObject
import kotlin.math.*

/** Frequency response of the RBJ cascade in shared/music_eq.h, using the shipped preset asset */
internal object MusicEqResponse {
    fun curve(
        preset: String,
        asset: JSONObject,
        rate: Int = 48000,
    ): List<Pair<Float, Float>> {
        val index = MusicEq.nativeId(preset) - 1
        val profile = if (index >= 0) asset.getJSONArray("profiles").getJSONObject(index) else null
        val bands = profile?.getJSONArray("bands")
        val filters = (0 until (bands?.length() ?: 0)).map { coefficients(bands!!.getJSONObject(it), rate) }
        return (0..240).map { i ->
            val frequency = 20.0 * 1000.0.pow(i / 240.0)
            val w = 2 * PI * frequency / rate

            fun power(
                c: DoubleArray,
                offset: Int,
            ): Double {
                val real = c[offset] + c[offset + 1] * cos(w) + c[offset + 2] * cos(2 * w)
                val imaginary = -c[offset + 1] * sin(w) - c[offset + 2] * sin(2 * w)
                return real * real + imaginary * imaginary
            }
            val db =
                (profile?.getDouble("preamp_db") ?: 0.0) + filters.sumOf { 10 * log10(power(it, 0) / power(it, 3)) }
            frequency.toFloat() to db.toFloat()
        }
    }

    private fun coefficients(
        band: JSONObject,
        rate: Int,
    ): DoubleArray {
        val a = 10.0.pow(band.getDouble("gain_db") / 40)
        val w = 2 * PI * min(band.getDouble("frequency_hz"), rate * .45) / rate
        val c = cos(w)
        val s = sin(w)
        val width = band.getDouble("width")
        if (band.getString("type") == "peak") {
            val alpha = s / (2 * width)
            return doubleArrayOf(1 + alpha * a, -2 * c, 1 - alpha * a, 1 + alpha / a, -2 * c, 1 - alpha / a)
        }
        val t = sqrt(a) * s * sqrt((a + 1 / a) * (1 / width - 1) + 2)
        return if (band.getString("type") == "low_shelf") {
            doubleArrayOf(
                a * ((a + 1) - (a - 1) * c + t),
                2 * a * ((a - 1) - (a + 1) * c),
                a * ((a + 1) - (a - 1) * c - t),
                (a + 1) + (a - 1) * c + t,
                -2 * ((a - 1) + (a + 1) * c),
                (a + 1) + (a - 1) * c - t,
            )
        } else {
            doubleArrayOf(
                a * ((a + 1) + (a - 1) * c + t),
                -2 * a * ((a - 1) + (a + 1) * c),
                a * ((a + 1) + (a - 1) * c - t),
                (a + 1) - (a - 1) * c + t,
                2 * ((a - 1) - (a + 1) * c),
                (a + 1) - (a - 1) * c - t,
            )
        }
    }
}
