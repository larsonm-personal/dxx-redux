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
