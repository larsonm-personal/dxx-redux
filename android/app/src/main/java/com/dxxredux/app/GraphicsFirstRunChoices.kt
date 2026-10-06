package com.dxxredux.app

import android.content.Context
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import org.json.JSONObject

/** Large versions of the Video Info cycling selectors, sharing their choice model */
internal class GraphicsFirstRunChoices(
    context: Context,
    private val change: (String, Int) -> Unit,
) : LinearLayout(context) {
    private val names = listOf("tex_filt", "aniso_level", "msaa_level")
    private val keys = listOf("TexFilt", "AnisoLevel", "MsaaLevel")
    private val labels = listOf("Texture filtering", "AF", "MSAA")
    private var choices = listOf(GraphicsOptionChoices.texture, listOf(0), listOf(0))
    private var values = listOf(0, 0, 0)
    private val buttons =
        names.mapIndexed { index, name ->
            TextView(context).apply {
                textSize = 19f
                setTextColor(PauseOverlayStyle.TEXT_COLOR)
                gravity = Gravity.CENTER_VERTICAL
                setPadding(dp(14), dp(10), dp(14), dp(10))
                minHeight = dp(56)
                isClickable = true
                isFocusable = true
                background = PauseOverlayStyle.choiceBackground(resources.displayMetrics.density)
                setOnClickListener { change(name, GraphicsOptionChoices.next(choices[index], values[index])) }
            }
        }

    init {
        orientation = VERTICAL
        buttons.forEach { addView(it, LayoutParams(-1, -2).apply { bottomMargin = dp(8) }) }
        addView(
            TextView(context).apply {
                text = "These can be edited later live in Settings > Video Info, or in the launcher's Graphics page"
                textSize = 13f
                setTextColor(PauseOverlayStyle.SECONDARY_TEXT_COLOR)
                setPadding(0, dp(4), 0, dp(4))
            },
            LayoutParams(-1, -2),
        )
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    fun update(
        state: JSONObject,
        selection: Int,
    ) {
        val caps = state.optJSONObject("capabilities")?.let(GraphicsCapabilities::fromReport)
        choices =
            listOf(
                GraphicsOptionChoices.texture,
                GraphicsOptionChoices.anisotropy(caps?.anisoMax ?: 1),
                GraphicsOptionChoices.msaa(caps),
            )
        values = keys.map { state.optJSONObject("candidate")?.optInt(it) ?: 0 }
        val current = state.optJSONObject("current")
        buttons.forEachIndexed { index, button ->
            val supported = choices[index].size > 1
            val value = values[index]
            val display =
                if (index ==
                    0
                ) {
                    GraphicsOptionChoices.textureLabel(value)
                } else {
                    GraphicsOptionChoices.multiplierLabel(value)
                }
            val detail =
                when {
                    !supported -> if (index == 1) caps?.anisoDetail() else caps?.msaaDetail()

                    current?.optInt(keys[index]) != value -> "Applying..."

                    index == 2 && caps != null && caps.effectiveMsaa(
                        value,
                    ) > value -> "Uses ${caps.effectiveMsaa(value)}x on this GPU"

                    else -> null
                }
            val suffix = if (detail.isNullOrEmpty()) "" else "\n$detail"
            button.text = "${labels[index]}: ${if (supported) display else "Unavailable"}$suffix"
            button.isEnabled = supported && state.optString("phase") == "editing"
            button.alpha = if (supported) 1f else 0.55f
            button.isSelected = selection == index
        }
    }

    fun availableIndices(): List<Int> = buttons.indices.filter { buttons[it].isEnabled }

    fun activate(index: Int) {
        buttons.getOrNull(index)?.takeIf { it.isEnabled }?.performClick()
    }

    fun button(index: Int): View = buttons[index]

    fun navigationState(): Map<String, Any> = keys.indices.associate { "graphics_preview_${names[it]}" to values[it] }
}
