package com.dxxredux.app.multiplayer

import com.dxxredux.app.MissionContentIdentity
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonPrimitive

internal const val MISSION_REQUIREMENT_SCHEMA = 1
internal const val MISSION_TRANSFER_MAX_BYTES = 256L * 1024L * 1024L

@Serializable
enum class MissionCompatibilityStatus {
    @SerialName("checking")
    CHECKING,

    @SerialName("match")
    MATCH,

    @SerialName("installed_disabled")
    INSTALLED_DISABLED,

    @SerialName("missing")
    MISSING,

    @SerialName("size_mismatch")
    SIZE_MISMATCH,

    @SerialName("hash_mismatch")
    HASH_MISMATCH,

    @SerialName("unsupported_source")
    UNSUPPORTED_SOURCE,

    @SerialName("error")
    ERROR,

    @SerialName("queued")
    QUEUED,

    @SerialName("downloading")
    DOWNLOADING,

    @SerialName("paused")
    PAUSED,

    @SerialName("retrying")
    RETRYING,

    @SerialName("failed_resumable")
    FAILED_RESUMABLE,

    @SerialName("verifying")
    VERIFYING,

    @SerialName("finalizing")
    FINALIZING,
}

@Serializable
data class MissionRequirement(
    val schema: Int = MISSION_REQUIREMENT_SCHEMA,
    val revision: String,
    val game: String,
    @SerialName("mission_key") val missionKey: String,
    @SerialName("display_name") val displayName: String,
    val kind: String,
    @SerialName("descriptor_path") val descriptorPath: String? = null,
    @SerialName("wrapper_filename") val wrapperFilename: String? = null,
    @SerialName("size_bytes") val sizeBytes: Long? = null,
    val sha256: String? = null,
    @SerialName("offer_available") val offerAvailable: Boolean = false,
) {
    val contentId: String get() = if (sha256 != null && sizeBytes != null) "$sha256:$sizeBytes" else revision

    val isWrapper: Boolean get() = kind == KIND_WRAPPER

    val isValid: Boolean
        get() =
            schema == MISSION_REQUIREMENT_SCHEMA &&
                game in setOf("d1", "d2") &&
                missionKey.length <= 8 &&
                missionKey.none { it.isISOControl() || it == '/' || it == '\\' } &&
                displayName.length <= 128 &&
                displayName.none(Char::isISOControl) &&
                revision.length in 1..160 &&
                kind in setOf(KIND_BUILTIN, KIND_WRAPPER, KIND_LOOSE) &&
                if (kind == KIND_WRAPPER) {
                    sizeBytes != null && sizeBytes > 0L &&
                        sha256 != null && MissionContentIdentity.isValidSha256(sha256) &&
                        !wrapperFilename.isNullOrBlank() && wrapperFilename.length <= 255 &&
                        wrapperFilename.none { it.isISOControl() || it == '/' || it == '\\' } &&
                        wrapperFilename !in setOf(".", "..") &&
                        (
                            descriptorPath == null ||
                                (descriptorPath.length <= 512 && descriptorPath.none(Char::isISOControl))
                        ) &&
                        (!offerAvailable || sizeBytes <= MISSION_TRANSFER_MAX_BYTES)
                } else {
                    sizeBytes == null && sha256 == null && !offerAvailable
                }

    companion object {
        const val KIND_BUILTIN = "builtin"
        const val KIND_WRAPPER = "wrapper"
        const val KIND_LOOSE = "loose"
    }
}

@Serializable
data class MissionStatusReport(
    val revision: String,
    val status: MissionCompatibilityStatus,
    @SerialName("verified_bytes") val verifiedBytes: Long = 0L,
    @SerialName("total_bytes") val totalBytes: Long = 0L,
    @SerialName("transfer_id") val transferId: String? = null,
    val attempt: Int = 0,
    @SerialName("failure_code") val failureCode: String? = null,
    @SerialName("bytes_per_second") val bytesPerSecond: Long = 0L,
) {
    val progress: Float
        get() =
            if (totalBytes <=
                0L
            ) {
                0f
            } else {
                (verifiedBytes.toDouble() / totalBytes.toDouble()).toFloat().coerceIn(0f, 1f)
            }

    fun validFor(requirement: MissionRequirement): Boolean =
        revision == requirement.revision &&
            verifiedBytes >= 0L &&
            totalBytes >= 0L &&
            verifiedBytes <= totalBytes &&
            totalBytes == (requirement.sizeBytes ?: 0L) &&
            attempt in 0..100 &&
            bytesPerSecond in 0..MAX_REPORTED_TRANSFER_BYTES_PER_SECOND &&
            (failureCode == null || failureCode.length <= 64)
}

internal const val MAX_REPORTED_TRANSFER_BYTES_PER_SECOND = 1024L * 1024L * 1024L

internal fun MissionStatusReport.remainingSeconds(): Long? {
    if (bytesPerSecond <= 0L || totalBytes <= verifiedBytes) return null
    return ((totalBytes - verifiedBytes) + bytesPerSecond - 1L) / bytesPerSecond
}

internal fun MissionRequirement.toGameInfoFields(): Map<String, JsonElement> =
    mapOf("mission_requirement" to protocolJson.encodeToJsonElement(MissionRequirement.serializer(), this))

internal fun missionRequirementFromGameInfo(gameInfo: JsonObject): MissionRequirement? =
    gameInfo["mission_requirement"]?.let { value ->
        runCatching { protocolJson.decodeFromJsonElement(MissionRequirement.serializer(), value) }
            .getOrNull()
            ?.takeIf(MissionRequirement::isValid)
    }

internal fun legacyMissionRequirement(gameInfo: JsonObject): MissionRequirement? {
    val game = gameInfo["game"]?.jsonPrimitive?.contentOrNull ?: return null
    val mission = gameInfo["mission"]?.jsonPrimitive?.contentOrNull ?: return null
    val revision = "legacy:$game:$mission"
    return MissionRequirement(
        revision = revision,
        game = game,
        missionKey = mission,
        displayName = mission,
        kind = MissionRequirement.KIND_LOOSE,
    )
}
