package com.dxxredux.app.multiplayer

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * Collapsible panel showing network events: connection status, NAT type,
 * peer candidates, connectivity results, relay info, and per-peer latency.
 *
 * Intended for use in SetupActivity multiplayer screens (always present).
 */
@Composable
internal fun NetworkEventsPanel(state: MatchmakingState) {
    var expanded by rememberSaveable { mutableStateOf(true) }
    val hasNetInfo =
        state.stunAddrs.isNotEmpty() ||
            state.peerCandidates.isNotEmpty() ||
            state.connectivityPairs.isNotEmpty() ||
            state.connectionInfo.isNotEmpty() ||
            state.relayInfo != null

    // Only show if connected or there is network info to display
    if (state.status == ConnectionStatus.DISCONNECTED && !hasNetInfo) return

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors =
            CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f),
            ),
    ) {
        Column(modifier = Modifier.padding(8.dp)) {
            // Header row (clickable to expand/collapse)
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .clickable { expanded = !expanded },
            ) {
                Text(
                    if (expanded) "[-] Network" else "[+] Network",
                    style = MaterialTheme.typography.titleSmall,
                    fontFamily = FontFamily.Monospace,
                )
                Spacer(Modifier.width(8.dp))
                StatusBadge(state.status)
                if (state.connectionInfo.isNotEmpty()) {
                    Spacer(Modifier.width(8.dp))
                    val methods = state.connectionInfo.map { it.method }.distinct()
                    Text(
                        methods.joinToString(", "),
                        style = MaterialTheme.typography.bodySmall,
                        fontFamily = FontFamily.Monospace,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }

            AnimatedVisibility(visible = expanded) {
                Column {
                    Spacer(Modifier.height(4.dp))

                    // STUN addresses
                    if (state.stunAddrs.isNotEmpty()) {
                        NetLabel("STUN: ${state.stunAddrs.joinToString(", ")}")
                    }

                    // Relay info
                    state.relayInfo?.let { relay ->
                        NetLabel("Relay: ${relay.relayAddr}")
                    }

                    // Peer candidates and NAT types
                    if (state.peerCandidates.isNotEmpty()) {
                        Spacer(Modifier.height(2.dp))
                        for ((_, info) in state.peerCandidates) {
                            val types =
                                info.candidates
                                    .map { it.candidateType }
                                    .distinct()
                                    .joinToString(",")
                            NetLabel("${info.peerId.take(8)}: NAT=${info.natType} cands=[$types]")
                        }
                    }

                    // Connectivity pairs
                    if (state.connectivityPairs.isNotEmpty()) {
                        Spacer(Modifier.height(2.dp))
                        for (pair in state.connectivityPairs) {
                            NetLabel(
                                "${pair.peerId.take(8)}: " +
                                    "${pair.localType}->${pair.remoteType} " +
                                    "${pair.remoteAddr} pri=${pair.priority}",
                            )
                        }
                    }

                    // Per-peer connection info (method + latency)
                    if (state.connectionInfo.isNotEmpty()) {
                        Spacer(Modifier.height(2.dp))
                        for (ci in state.connectionInfo) {
                            val latency = ci.estimatedLatencyMs?.let { " ${it}ms" } ?: ""
                            val relay = if (ci.serverRelay) " [relay]" else ""
                            val detail = ci.detail?.let { " ($it)" } ?: ""
                            NetLabel("${ci.peerCallsign}: ${ci.method}$relay$latency$detail")
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun StatusBadge(status: ConnectionStatus) {
    val color =
        when (status) {
            ConnectionStatus.CONNECTED -> MaterialTheme.colorScheme.primary

            ConnectionStatus.DISCONNECTED -> MaterialTheme.colorScheme.error

            ConnectionStatus.CONNECTING,
            ConnectionStatus.AUTHENTICATING,
            ConnectionStatus.RECONNECTING,
            -> MaterialTheme.colorScheme.tertiary
        }
    Text(
        status.name,
        style = MaterialTheme.typography.labelSmall,
        fontWeight = FontWeight.Bold,
        color = color,
        fontFamily = FontFamily.Monospace,
    )
}

@Composable
private fun NetLabel(text: String) {
    Text(
        text,
        fontSize = 10.sp,
        fontFamily = FontFamily.Monospace,
        maxLines = 1,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}
