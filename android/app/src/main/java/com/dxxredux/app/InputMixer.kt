package com.dxxredux.app

/**
 * Combines input from multiple sources (touch, controller, gyro) into a single
 * button state and axis value per game action before forwarding to JNI.
 *
 * Buttons: OR-mixed. Any source pressing = active. Release when all release.
 * Axes: additive with clamp to [-1, 1]. Gyro adds on top of stick.
 */
class InputMixer(
    private val buttonCallback: (button: Int, pressed: Int) -> Unit,
    private val axisCallback: (axis: Int, value: Float, touchActive: Boolean) -> Unit,
    private val axisBatchCallback: ((axes: IntArray, values: FloatArray, touchActive: BooleanArray) -> Unit)? = null,
) {
    // Per-button: actionId -> { sourceTag -> pressed }
    private val buttonSources = HashMap<Int, HashMap<String, Boolean>>()

    // Per-button combined state
    private val buttonState = HashMap<Int, Boolean>()

    // Per-axis: axisId -> { sourceTag -> value }
    private val axisSources = HashMap<Int, HashMap<String, Float>>()

    // Last mixed state sent to JNI
    private val axisState = HashMap<Int, Float>()
    private val axisTouchState = HashMap<Int, Boolean>()

    /** Update a button source and dispatch the OR-mixed result if it changed. */
    @Synchronized
    fun setButton(
        actionId: Int,
        sourceTag: String,
        pressed: Boolean,
    ) {
        val sources = buttonSources.getOrPut(actionId) { HashMap() }
        val old = sources[sourceTag] ?: false
        if (old == pressed) return
        sources[sourceTag] = pressed
        val combined = sources.values.any { it }
        val prev = buttonState[actionId] ?: false
        if (combined != prev) {
            buttonState[actionId] = combined
            buttonCallback(actionId, if (combined) 1 else 0)
        }
    }

    /** Update an axis source and dispatch the additive-clamped result. */
    @Synchronized
    fun setAxis(
        axisId: Int,
        sourceTag: String,
        value: Float,
    ) {
        val sources = axisSources.getOrPut(axisId) { HashMap() }
        sources[sourceTag] = value
        val sum = sources.values.fold(0f) { acc, v -> acc + v }.coerceIn(-1f, 1f)
        val touchActive = sources.any { (tag, axisValue) -> tag.startsWith("touch") && axisValue != 0f }
        updateAxisIfChanged(axisId, sum, touchActive)?.let { axisCallback(it.first, it.second, it.third) }
    }

    /** Update one source across a complete producer vector and dispatch it atomically. */
    @Synchronized
    fun setAxes(
        sourceTag: String,
        values: Map<Int, Float>,
    ) {
        val axisIds = values.keys.sorted()
        for (axisId in axisIds) {
            axisSources.getOrPut(axisId) { HashMap() }[sourceTag] = values.getValue(axisId)
        }
        val changes =
            axisIds.mapNotNull { axisId ->
                val sources = axisSources.getValue(axisId)
                val sum = sources.values.fold(0f) { acc, sourceValue -> acc + sourceValue }.coerceIn(-1f, 1f)
                val touchActive = sources.any { (tag, sourceValue) -> tag.startsWith("touch") && sourceValue != 0f }
                updateAxisIfChanged(axisId, sum, touchActive)
            }
        dispatchAxes(changes)
    }

    /** Release all buttons and zero all axes for sources matching [prefix]. */
    @Synchronized
    fun clearSources(prefix: String) {
        val axisChanges = mutableListOf<Triple<Int, Float, Boolean>>()
        val btnIter = buttonSources.entries.iterator()
        while (btnIter.hasNext()) {
            val (actionId, sources) = btnIter.next()
            sources.keys.removeAll { it.startsWith(prefix) }
            val combined = sources.values.any { it }
            val prev = buttonState[actionId] ?: false
            if (prev && !combined) {
                buttonState[actionId] = false
                buttonCallback(actionId, 0)
            }
            if (sources.isEmpty()) btnIter.remove()
        }
        val axisIter = axisSources.entries.iterator()
        while (axisIter.hasNext()) {
            val (axisId, sources) = axisIter.next()
            val hadSources = sources.keys.removeAll { it.startsWith(prefix) }
            if (hadSources) {
                val sum = sources.values.fold(0f) { acc, v -> acc + v }.coerceIn(-1f, 1f)
                val touchActive = sources.any { (tag, axisValue) -> tag.startsWith("touch") && axisValue != 0f }
                updateAxisIfChanged(axisId, sum, touchActive)?.let { axisChanges += it }
            }
            if (sources.isEmpty()) axisIter.remove()
        }
        dispatchAxes(axisChanges)
    }

    /** Release all mixed input before the activity stops. */
    @Synchronized
    fun releaseAll() {
        val pressedButtons = buttonState.filterValues { it }.keys.toList()
        val activeAxes =
            axisState
                .filter { (axisId, value) -> value != 0f || axisTouchState[axisId] == true }
                .keys
                .toList()
        buttonSources.clear()
        buttonState.clear()
        axisSources.clear()
        axisState.clear()
        axisTouchState.clear()
        pressedButtons.forEach { buttonCallback(it, 0) }
        dispatchAxes(activeAxes.sorted().map { Triple(it, 0f, false) })
    }

    private fun updateAxisIfChanged(
        axisId: Int,
        value: Float,
        touchActive: Boolean,
    ): Triple<Int, Float, Boolean>? {
        if ((axisState[axisId] ?: 0f) == value &&
            (axisTouchState[axisId] ?: false) == touchActive
        ) {
            return null
        }
        axisState[axisId] = value
        axisTouchState[axisId] = touchActive
        return Triple(axisId, value, touchActive)
    }

    private fun dispatchAxes(changes: List<Triple<Int, Float, Boolean>>) {
        if (changes.isEmpty()) return
        val batchCallback = axisBatchCallback
        if (batchCallback == null) {
            changes.forEach { axisCallback(it.first, it.second, it.third) }
            return
        }
        batchCallback(
            changes.map { it.first }.toIntArray(),
            changes.map { it.second }.toFloatArray(),
            changes.map { it.third }.toBooleanArray(),
        )
    }
}
