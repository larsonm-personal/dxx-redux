package com.dxxredux.app

import android.content.Context
import org.json.JSONArray

internal data class RobotNameEntry(
    val number: Int,
    val name: String,
    val sourceCodeName: String,
)

internal object RobotNameCatalog {
    private const val D1_COUNT = 24
    private const val D2_COUNT = 66
    private val cache = mutableMapOf<String, List<RobotNameEntry>>()

    fun load(
        context: Context,
        game: String,
    ): List<RobotNameEntry> =
        synchronized(cache) {
            cache.getOrPut(game) {
                val (filename, count) =
                    when (game) {
                        GameFileFormats.GAME_D1 -> "robot_names_d1.jsonc" to D1_COUNT
                        GameFileFormats.GAME_D2 -> "robot_names_d2.jsonc" to D2_COUNT
                        else -> throw IllegalArgumentException("Unsupported robot-name game $game")
                    }
                val text =
                    context.assets
                        .open(filename)
                        .bufferedReader()
                        .use { it.readText() }
                parse(text, count)
            }
        }

    internal fun parse(
        text: String,
        expectedCount: Int,
    ): List<RobotNameEntry> {
        val array = JSONArray(Jsonc.strip(text))
        require(array.length() == expectedCount) { "Expected $expectedCount robot-name entries" }
        return buildList {
            for (index in 0 until array.length()) {
                val item = array.getJSONObject(index)
                require(item.getInt("number") == index) { "Robot-name entry $index is out of order" }
                add(
                    RobotNameEntry(
                        index,
                        item.getString("name").trim(),
                        item.optString("source_code_name").trim(),
                    ),
                )
            }
        }
    }

    fun displayName(
        entries: List<RobotNameEntry>,
        number: Int,
        fallback: String,
    ): String {
        val name =
            entries
                .getOrNull(number)
                ?.takeIf { it.number == number }
                ?.name
                ?.takeUnless { it == number.toString() }
                .orEmpty()
        return if (name.isBlank()) fallback else "$name (Robot $number)"
    }
}
