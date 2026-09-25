package com.dxxredux.app

internal enum class PreviewStatus { STOPPED, LOADING, PLAYING, PAUSED }

internal data class PreviewSnapshot(
    val status: PreviewStatus,
    val positionMs: Int = 0,
    val durationMs: Int = 0,
)

// Keep this mapping synchronized with STATE_* in MidiPreviewBridge and CdPreviewBridge
internal fun nativePreviewSnapshot(
    state: Int,
    positionMs: Int,
    durationMs: Int,
): PreviewSnapshot =
    PreviewSnapshot(
        when (state) {
            1 -> PreviewStatus.PLAYING
            -1 -> PreviewStatus.PAUSED
            else -> PreviewStatus.STOPPED
        },
        positionMs,
        durationMs,
    )

// Null navigation means a single-track preview; a boundary callback is a no-op
internal class PreviewTransport(
    val snapshot: () -> PreviewSnapshot,
    val start: () -> Unit,
    val pause: () -> Unit,
    val stop: () -> Unit,
    val seek: (Int) -> Unit,
    val navigate: ((Int) -> Unit)? = null,
    val cancelPendingNavigation: () -> Unit = {},
) {
    fun skip(direction: Int) {
        if (navigate != null) {
            navigate(direction)
        } else {
            val state = snapshot()
            if (state.durationMs > 0 && state.status in listOf(PreviewStatus.PLAYING, PreviewStatus.PAUSED)) {
                seek((state.positionMs.toLong() + direction * 10_000L).coerceIn(0, state.durationMs.toLong()).toInt())
            }
        }
    }
}
