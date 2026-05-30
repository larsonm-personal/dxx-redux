package com.dxxredux.app.multiplayer

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
internal fun MultiplayerResumeOfferCard(
    record: MultiplayerResumeRecord,
    primaryLabel: String,
    onPrimary: () -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val missionLabel = record.mission.ifBlank { if (record.game == "d1") "First Strike" else "Counterstrike!" }
    val restoreLabel = record.coopRestoreSlot?.let { "Save slot $it" } ?: "Fresh start"
    val networkLabel = if (record.transport == "lan") "LAN" else "Online"
    val title = if (record.role == "host") "Last $networkLabel Coop" else "Last $networkLabel Host"
    val detailLabel =
        if (record.role == "host") {
            restoreLabel
        } else {
            "Host ${record.hostCallsign ?: record.lanHostAddr ?: "unknown"}"
        }

    Card(modifier = modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(title, style = MaterialTheme.typography.titleSmall)
            Text(
                "${record.game.uppercase()} $missionLabel, level ${record.levelNum}",
                style = MaterialTheme.typography.bodySmall,
            )
            Text(
                detailLabel,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = onPrimary, modifier = Modifier.weight(1f)) {
                    Text(primaryLabel, fontSize = 12.sp)
                }
                OutlinedButton(onClick = onDismiss, modifier = Modifier.weight(1f)) {
                    Text("Dismiss", fontSize = 12.sp)
                }
            }
        }
    }
}
