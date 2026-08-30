package com.dxxredux.app.multiplayer

import android.content.Context
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.ModManager
import com.dxxredux.app.formatBinaryRate

internal object MissionCompatibilityResolver {
    fun resolve(
        context: Context,
        requirement: MissionRequirement,
        mode: String,
    ): MissionStatusReport {
        if (!requirement.isValid) return report(requirement, MissionCompatibilityStatus.ERROR, "invalid_requirement")
        val fileSets = FileSetManager(context.filesDir)
        val setDir = fileSets.getSetDir(fileSets.getActive())
        val catalog = MissionScanner.scan(context.filesDir, setDir, requirement.game, mode)
        val local = resolveMissionSelection(catalog, requirement.missionKey)
        if (local == null) {
            if (requirement.isWrapper && matchingDisabledWrapper(context, setDir, requirement)) {
                return report(requirement, MissionCompatibilityStatus.INSTALLED_DISABLED)
            }
            return report(requirement, MissionCompatibilityStatus.MISSING)
        }
        return when (requirement.kind) {
            MissionRequirement.KIND_BUILTIN -> {
                if (local.isBuiltin) {
                    report(requirement, MissionCompatibilityStatus.MATCH)
                } else {
                    report(requirement, MissionCompatibilityStatus.HASH_MISMATCH, "builtin_shadowed")
                }
            }

            MissionRequirement.KIND_LOOSE -> {
                report(requirement, MissionCompatibilityStatus.MATCH)
            }

            MissionRequirement.KIND_WRAPPER -> {
                when {
                    local.archiveSizeBytes == null || local.archiveSha256 == null -> {
                        report(requirement, MissionCompatibilityStatus.MISSING)
                    }

                    local.archiveSizeBytes != requirement.sizeBytes -> {
                        report(requirement, MissionCompatibilityStatus.SIZE_MISMATCH)
                    }

                    local.archiveSha256 != requirement.sha256 -> {
                        report(requirement, MissionCompatibilityStatus.HASH_MISMATCH)
                    }

                    else -> {
                        report(requirement, MissionCompatibilityStatus.MATCH)
                    }
                }
            }

            else -> {
                report(requirement, MissionCompatibilityStatus.ERROR, "unsupported_kind")
            }
        }
    }

    fun enableMatchingWrapper(
        context: Context,
        requirement: MissionRequirement,
        mode: String,
    ): MissionStatusReport {
        if (!requirement.isValid || !requirement.isWrapper) {
            return report(requirement, MissionCompatibilityStatus.ERROR, "invalid_requirement")
        }
        val fileSets = FileSetManager(context.filesDir)
        val setDir = fileSets.getSetDir(fileSets.getActive())
        val manager = ModManager(context.filesDir, context, setDir)
        val matching =
            manager.listMods().firstOrNull { mod ->
                !mod.enabled &&
                    mod.kind == ModManager.MOD_KIND_MISSION_ZIP &&
                    manager.ensureMissionContentIdentity(mod.filename)?.let { identity ->
                        identity.sizeBytes == requirement.sizeBytes && identity.sha256 == requirement.sha256
                    } == true
            } ?: return report(requirement, MissionCompatibilityStatus.MISSING)
        manager.setEnabled(matching.filename, true)
        return resolve(context, requirement, mode)
    }

    private fun matchingDisabledWrapper(
        context: Context,
        setDir: java.io.File,
        requirement: MissionRequirement,
    ): Boolean {
        val manager = ModManager(context.filesDir, context, setDir)
        return manager.listMods().any { mod ->
            !mod.enabled &&
                mod.kind == ModManager.MOD_KIND_MISSION_ZIP &&
                manager.ensureMissionContentIdentity(mod.filename)?.let { identity ->
                    identity.sizeBytes == requirement.sizeBytes && identity.sha256 == requirement.sha256
                } == true
        }
    }

    private fun report(
        requirement: MissionRequirement,
        status: MissionCompatibilityStatus,
        failureCode: String? = null,
    ): MissionStatusReport =
        MissionStatusReport(
            revision = requirement.revision,
            status = status,
            totalBytes = requirement.sizeBytes ?: 0L,
            failureCode = failureCode,
        )
}

internal fun MissionCompatibilityStatus.userLabel(progress: MissionStatusReport? = null): String =
    when (this) {
        MissionCompatibilityStatus.CHECKING -> "Checking mission"
        MissionCompatibilityStatus.MATCH -> "Mission ready"
        MissionCompatibilityStatus.INSTALLED_DISABLED -> "Matching mission is disabled"
        MissionCompatibilityStatus.MISSING -> "Missing mission"
        MissionCompatibilityStatus.SIZE_MISMATCH -> "Warning: mission size differs"
        MissionCompatibilityStatus.HASH_MISMATCH -> "Warning: mission hash differs"
        MissionCompatibilityStatus.UNSUPPORTED_SOURCE -> "Mission cannot be synchronized"
        MissionCompatibilityStatus.ERROR -> "Mission check failed"
        MissionCompatibilityStatus.QUEUED -> "Mission download queued"
        MissionCompatibilityStatus.DOWNLOADING -> transferStatusLabel("Downloading mission", progress)
        MissionCompatibilityStatus.PAUSED -> "Mission download paused"
        MissionCompatibilityStatus.RETRYING -> "Retrying mission download"
        MissionCompatibilityStatus.FAILED_RESUMABLE -> "Mission download interrupted"
        MissionCompatibilityStatus.VERIFYING -> transferStatusLabel("Verifying mission", progress)
        MissionCompatibilityStatus.FINALIZING -> transferStatusLabel("Finalizing mission", progress)
    }

internal fun MissionStatusReport.percentText(): String = "${(progress * 100f).toInt()}%"

private fun transferStatusLabel(
    prefix: String,
    report: MissionStatusReport?,
): String {
    if (report == null) return prefix
    val parts = mutableListOf(prefix, report.percentText())
    report.remainingSeconds()?.let { parts += "${formatRemainingTime(it)} remaining" }
    if (report.bytesPerSecond > 0L) parts += formatTransferRate(report.bytesPerSecond)
    return parts.joinToString(" - ")
}

internal fun formatRemainingTime(seconds: Long): String =
    when {
        seconds < 60L -> "${seconds}s"
        seconds < 3600L -> "${seconds / 60L}m ${seconds % 60L}s"
        else -> "${seconds / 3600L}h ${(seconds % 3600L) / 60L}m"
    }

internal fun formatTransferRate(bytesPerSecond: Long): String = formatBinaryRate(bytesPerSecond)
