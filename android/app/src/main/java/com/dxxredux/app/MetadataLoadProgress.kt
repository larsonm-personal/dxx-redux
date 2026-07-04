package com.dxxredux.app

internal data class MetadataLoadProgress(
    val label: String,
    val completed: Int,
    val total: Int,
) {
    val fraction: Float?
        get() = total.takeIf { it > 0 }?.let { (completed.toFloat() / it.toFloat()).coerceIn(0f, 1f) }
}

internal fun formatMetadataLoadProgress(progress: MetadataLoadProgress): String {
    val total = progress.total.coerceAtLeast(1)
    val completed = progress.completed.coerceIn(0, total)
    return "${progress.label} $completed/$total"
}
