package com.dxxredux.app.multiplayer

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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * Step-by-step ICE negotiation progress panel for the lobby screen.
 * Shows each phase: STUN discovery, UPnP, peer exchange, connectivity probe,
 * and an overall readiness summary.
 */
@Composable
internal fun IceProgressPanel(state: MatchmakingState) {
    val ice = state.iceStatus
    if (ice.phase == IcePhase.IDLE && state.peerCandidates.isEmpty()) return

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors =
            CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f),
            ),
    ) {
        Column(modifier = Modifier.padding(8.dp)) {
            Text(
                "Connection Setup",
                style = MaterialTheme.typography.titleSmall,
                fontFamily = FontFamily.Monospace,
            )
            Spacer(Modifier.height(4.dp))

            // Step 1: STUN Discovery
            val stunStatus =
                when {
                    ice.phase == IcePhase.STUN_DISCOVERY -> StepState.RUNNING

                    ice.phase > IcePhase.STUN_DISCOVERY -> StepState.DONE

                    ice.phase == IcePhase.FAILED &&
                        ice.stunNatType == null -> StepState.FAILED

                    else -> StepState.PENDING
                }
            val stunDetail =
                when (stunStatus) {
                    StepState.DONE -> "${ice.stunNatType ?: "?"}, ${ice.stunCandidateCount} candidates"
                    StepState.RUNNING -> "querying STUN servers..."
                    StepState.FAILED -> ice.errorMessage ?: "failed"
                    StepState.PENDING, StepState.SKIPPED -> ""
                }
            StepRow("STUN Discovery", stunStatus, stunDetail)

            // Step 2: UPnP
            val upnpStatus =
                when {
                    ice.phase < IcePhase.STUN_COMPLETE -> StepState.PENDING
                    ice.upnpMapped -> StepState.DONE
                    ice.phase >= IcePhase.STUN_COMPLETE -> StepState.SKIPPED
                    else -> StepState.PENDING
                }
            val upnpDetail =
                when {
                    ice.upnpMapped -> "mapped ${ice.upnpAddr ?: "?"}"
                    upnpStatus == StepState.SKIPPED -> "not available"
                    else -> ""
                }
            StepRow("UPnP / NAT-PMP", upnpStatus, upnpDetail)

            // Step 3: Peer Candidate Exchange
            val peerStatus =
                when {
                    ice.phase < IcePhase.STUN_COMPLETE -> StepState.PENDING
                    state.connectivityPairs.isNotEmpty() -> StepState.DONE
                    state.peerCandidates.isNotEmpty() -> StepState.RUNNING
                    ice.phase >= IcePhase.STUN_COMPLETE -> StepState.RUNNING
                    else -> StepState.PENDING
                }
            val peerDetail =
                when {
                    state.connectivityPairs.isNotEmpty() -> {
                        "${state.peerCandidates.size} peers, ${state.connectivityPairs.size} pairs"
                    }

                    state.peerCandidates.isNotEmpty() -> {
                        "${state.peerCandidates.size} peers received, waiting for check..."
                    }

                    peerStatus == StepState.RUNNING -> {
                        "waiting for peer results..."
                    }

                    else -> {
                        ""
                    }
                }
            StepRow("Peer Exchange", peerStatus, peerDetail)

            // Step 4: Connectivity Probe
            val probeStatus =
                when (ice.phase) {
                    IcePhase.PROBING -> StepState.RUNNING
                    IcePhase.COMPLETE -> StepState.DONE
                    IcePhase.FAILED -> if (ice.probeResult != null) StepState.DONE else StepState.FAILED
                    else -> StepState.PENDING
                }
            val probeDetail =
                when {
                    ice.probeResult != null -> {
                        val rtt = ice.probeRttMs?.let { " (${it}ms)" } ?: ""
                        "${ice.probeResult}$rtt"
                    }

                    probeStatus == StepState.RUNNING -> {
                        "sending probes..."
                    }

                    probeStatus == StepState.FAILED -> {
                        ice.errorMessage ?: "failed"
                    }

                    else -> {
                        ""
                    }
                }
            StepRow("Connectivity Probe", probeStatus, probeDetail)

            // Overall summary
            Spacer(Modifier.height(4.dp))
            val summary =
                when (ice.phase) {
                    IcePhase.COMPLETE -> {
                        val method = ice.probeResult ?: "unknown"
                        val relay = method == "relay"
                        if (relay) "Ready (relay)" else "Ready (direct: $method)"
                    }

                    IcePhase.FAILED -> {
                        "Failed: ${ice.errorMessage ?: "unknown error"}"
                    }

                    IcePhase.IDLE -> {
                        "Waiting for players..."
                    }

                    else -> {
                        "Negotiating..."
                    }
                }
            val summaryColor =
                when (ice.phase) {
                    IcePhase.COMPLETE -> {
                        if (ice.probeResult == "relay") {
                            Color(0xFFFFAA44)
                        } else {
                            Color(0xFF44FF44)
                        }
                    }

                    IcePhase.FAILED -> {
                        MaterialTheme.colorScheme.error
                    }

                    else -> {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    }
                }
            Text(
                summary,
                fontSize = 11.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                color = summaryColor,
            )
        }
    }
}

private enum class StepState {
    PENDING,
    RUNNING,
    DONE,
    FAILED,
    SKIPPED,
}

@Composable
private fun StepRow(
    label: String,
    state: StepState,
    detail: String,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.padding(vertical = 1.dp),
    ) {
        val icon =
            when (state) {
                StepState.DONE -> "[ok]"
                StepState.RUNNING -> "[..]"
                StepState.FAILED -> "[!!]"
                StepState.SKIPPED -> "[--]"
                StepState.PENDING -> "[  ]"
            }
        val iconColor =
            when (state) {
                StepState.DONE -> Color(0xFF44FF44)
                StepState.RUNNING -> Color(0xFFFFFF44)
                StepState.FAILED -> Color(0xFFFF4444)
                StepState.SKIPPED -> Color(0xFF888888)
                StepState.PENDING -> Color(0xFF666666)
            }
        Text(
            icon,
            fontSize = 10.sp,
            fontFamily = FontFamily.Monospace,
            color = iconColor,
        )
        Spacer(Modifier.width(4.dp))
        Text(
            label,
            fontSize = 10.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (detail.isNotEmpty()) {
            Spacer(Modifier.width(6.dp))
            Text(
                detail,
                fontSize = 10.sp,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
            )
        }
    }
}
