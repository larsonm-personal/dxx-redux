package com.dxxredux.app.lobby

internal enum class LanLaunchStage { PREPARING, COMMITTED, CANCELLED }

internal data class LanLaunchPreparation(
    val lobbyId: String,
    val hostAddress: String,
    val attempt: Long,
    val stage: LanLaunchStage,
    val reason: String? = null,
)

// Attempt numbers increase within a lobby. Terminal decisions cannot be undone by UDP reordering
internal fun acceptLanLaunchUpdate(
    current: LanLaunchPreparation?,
    next: LanLaunchPreparation,
): Boolean {
    if (current == null || current.lobbyId != next.lobbyId || current.hostAddress != next.hostAddress) return true
    if (next.attempt != current.attempt) return next.attempt > current.attempt
    return current.stage == LanLaunchStage.PREPARING && next.stage != LanLaunchStage.PREPARING
}
