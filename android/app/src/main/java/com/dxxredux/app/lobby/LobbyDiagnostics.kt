package com.dxxredux.app.lobby

internal const val LAN_BROADCAST_FAILURE_DIAGNOSTIC =
    "Broadcasts failing -- check Wi-Fi and Nearby Devices permission"
internal const val LAN_RECONNECTING_DIAGNOSTIC = "Connection interrupted, reconnecting to host"
internal const val LAN_PEER_TIMEOUT_MS = 10_000L
internal const val LAN_RECONNECT_GRACE_MS = 2L * 60L * 1000L

internal enum class LanPlayerLeaseAction {
    NONE,
    MARK_RECONNECTING,
    REMOVE,
}

internal fun lanPlayerLeaseAction(
    player: LanPlayer,
    nowMs: Long,
): LanPlayerLeaseAction {
    if (player.address == "127.0.0.1") return LanPlayerLeaseAction.NONE
    if (player.connected) {
        return if (nowMs - player.lastSeenMs > LAN_PEER_TIMEOUT_MS) {
            LanPlayerLeaseAction.MARK_RECONNECTING
        } else {
            LanPlayerLeaseAction.NONE
        }
    }
    val disconnectedAtMs = player.disconnectedAtMs ?: return LanPlayerLeaseAction.REMOVE
    return if (nowMs - disconnectedAtMs > LAN_RECONNECT_GRACE_MS) {
        LanPlayerLeaseAction.REMOVE
    } else {
        LanPlayerLeaseAction.NONE
    }
}

internal fun lanDiagnosticAfterBroadcastRecovery(current: String): String =
    if (current == LAN_BROADCAST_FAILURE_DIAGNOSTIC) "" else current

internal fun shouldRefreshLanDiscoveryAfterResume(
    isDiscovering: Boolean,
    wasBackgrounded: Boolean,
    socketUnavailable: Boolean,
): Boolean = isDiscovering && (wasBackgrounded || socketUnavailable)

internal fun shouldShowBroadcastFailureWarning(appBackgrounded: Boolean): Boolean = !appBackgrounded

internal fun lanTransportRecoveryReason(
    isDiscovering: Boolean,
    appBackgrounded: Boolean,
    socketAvailable: Boolean,
    receiveLoopActive: Boolean,
): String? {
    if (!isDiscovering || appBackgrounded) return null
    if (!socketAvailable) return "socket unavailable"
    if (!receiveLoopActive) return "receive loop stopped"
    return null
}

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

internal fun lanLobbyHasClientIdConflict(
    players: List<LanPlayer>,
    callsign: String,
    clientId: String?,
): Boolean =
    !clientId.isNullOrBlank() &&
        players.any { player ->
            player.clientId == clientId && !player.callsign.equals(callsign, ignoreCase = true)
        }

internal fun refreshLanPlayerLeasesAfterResume(
    players: List<LanPlayer>,
    nowMs: Long,
): List<LanPlayer> =
    players.map { player ->
        if (player.address == "127.0.0.1") player else player.copy(lastSeenMs = nowMs)
    }
