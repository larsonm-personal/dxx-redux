package com.dxxredux.app

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

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

@Composable
internal fun MetadataLoadProgressView(
    progress: MetadataLoadProgress,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            CircularProgressIndicator(modifier = Modifier.size(18.dp), strokeWidth = 2.dp)
            Spacer(modifier = Modifier.width(10.dp))
            Text(
                formatMetadataLoadProgress(progress),
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Spacer(modifier = Modifier.height(6.dp))
        progress.fraction?.let { fraction ->
            LinearProgressIndicator(
                progress = { fraction },
                modifier = Modifier.fillMaxWidth().height(4.dp),
            )
        } ?: LinearProgressIndicator(modifier = Modifier.fillMaxWidth().height(4.dp))
    }
}

@Composable
internal fun LevelMetadataAnalysisProgressView(
    progress: LevelMetadataAnalysisProgress,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            CircularProgressIndicator(modifier = Modifier.size(18.dp), strokeWidth = 2.dp)
            Spacer(modifier = Modifier.width(10.dp))
            Text(
                "Analyzing level metadata",
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Spacer(modifier = Modifier.height(8.dp))
        MetadataLinearProgress(progress.overall)
        progress.estimatedLevel?.let { estimatedLevel ->
            Spacer(modifier = Modifier.height(10.dp))
            MetadataLinearProgress(estimatedLevel)
        }
        progress.currentLevel?.let { currentLevel ->
            Spacer(modifier = Modifier.height(10.dp))
            MetadataLinearProgress(currentLevel)
        }
    }
}

@Composable
private fun MetadataLinearProgress(progress: MetadataLoadProgress) {
    Text(
        if (progress.total > 0) formatMetadataLoadProgress(progress) else progress.label,
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(4.dp))
    progress.fraction?.let { fraction ->
        LinearProgressIndicator(
            progress = { fraction },
            modifier = Modifier.fillMaxWidth().height(4.dp),
        )
    } ?: LinearProgressIndicator(modifier = Modifier.fillMaxWidth().height(4.dp))
}
