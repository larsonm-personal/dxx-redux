package com.dxxredux.app.multiplayer

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

@Composable
internal fun MissionStatusIndicator(
    report: MissionStatusReport?,
    modifier: Modifier = Modifier,
) {
    if (report == null) return
    val isProblem =
        report.status in
            setOf(
                MissionCompatibilityStatus.MISSING,
                MissionCompatibilityStatus.SIZE_MISMATCH,
                MissionCompatibilityStatus.HASH_MISMATCH,
                MissionCompatibilityStatus.ERROR,
                MissionCompatibilityStatus.FAILED_RESUMABLE,
            )
    Column(modifier = modifier) {
        Text(
            report.status.userLabel(report),
            style = MaterialTheme.typography.bodySmall,
            color =
                when {
                    isProblem -> MaterialTheme.colorScheme.error
                    report.status == MissionCompatibilityStatus.MATCH -> MaterialTheme.colorScheme.primary
                    else -> MaterialTheme.colorScheme.onSurfaceVariant
                },
        )
        if (report.status in TRANSFER_PROGRESS_STATES && report.totalBytes > 0L) {
            LinearProgressIndicator(
                progress = { report.progress },
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

private val TRANSFER_PROGRESS_STATES =
    setOf(
        MissionCompatibilityStatus.QUEUED,
        MissionCompatibilityStatus.DOWNLOADING,
        MissionCompatibilityStatus.PAUSED,
        MissionCompatibilityStatus.RETRYING,
        MissionCompatibilityStatus.FAILED_RESUMABLE,
        MissionCompatibilityStatus.VERIFYING,
    )
