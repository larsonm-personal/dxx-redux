package com.dxxredux.app.lobby

internal const val LAN_BROADCAST_FAILURE_DIAGNOSTIC =
    "Broadcasts failing -- check Wi-Fi and Nearby Devices permission"

internal fun lanDiagnosticAfterBroadcastRecovery(current: String): String =
    if (current == LAN_BROADCAST_FAILURE_DIAGNOSTIC) "" else current

internal fun shouldRefreshLanDiscoveryAfterResume(
    isDiscovering: Boolean,
    wasBackgrounded: Boolean,
    socketUnavailable: Boolean,
): Boolean = isDiscovering && (wasBackgrounded || socketUnavailable)

internal fun shouldShowBroadcastFailureWarning(appBackgrounded: Boolean): Boolean = !appBackgrounded

internal fun lanPlayerMatchesSender(
    player: LanPlayer,
    callsign: String,
    clientId: String?,
    senderAddress: String,
): Boolean {
    if (player.address == "127.0.0.1") return false
    if (player.address != senderAddress) return false
    if (!clientId.isNullOrBlank() && !player.clientId.isNullOrBlank()) {
        return clientId == player.clientId
    }
    return player.callsign.equals(callsign, ignoreCase = true)
}

internal fun lanPlayerMatchesJoinIdentity(
    player: LanPlayer,
    callsign: String,
    clientId: String?,
    senderAddress: String,
): Boolean {
    if (player.address == "127.0.0.1") return false
    if (!clientId.isNullOrBlank() && !player.clientId.isNullOrBlank()) return clientId == player.clientId
    return player.address == senderAddress && player.callsign.equals(callsign, ignoreCase = true)
}

internal fun refreshLanPlayerLeasesAfterResume(
    players: List<LanPlayer>,
    nowMs: Long,
): List<LanPlayer> =
    players.map { player ->
        if (player.address == "127.0.0.1") player else player.copy(lastSeenMs = nowMs)
    }
