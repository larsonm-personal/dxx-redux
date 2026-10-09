package com.dxxredux.app

/** Product limits also used by android_graphics_safety_preview_ready in native code */
internal object GraphicsOptionChoices {
    val texture = listOf(0, 1, 2)
    val msaaLevels = listOf(0, 2, 4)

    fun anisotropy(max: Int): List<Int> = listOf(0, 2, 4, 8, 16).filter { it == 0 || it <= max }

    fun msaa(capabilities: GraphicsCapabilities?): List<Int> =
        msaaLevels.filter { it == 0 || capabilities?.supportsMsaa(it) == true }

    fun next(
        values: List<Int>,
        current: Int,
    ): Int = values[(values.indexOf(current) + 1) % values.size]

    fun textureLabel(value: Int): String = listOf("Nearest", "Bilinear", "Trilinear").getOrElse(value) { "Nearest" }

    fun multiplierLabel(value: Int): String = if (value == 0) "Off" else "${value}x"
}
