package com.dxxredux.app

import java.io.File
import java.util.Locale

enum class MissionDownloadPolicy(
    val explanation: String?,
) {
    USER_SUPPLIED(null),
    PROPRIETARY("Contains proprietary game data; each player must import their own copy"),
    UNVERIFIED("This archive's contents could not be verified for sharing"),
}

/** Content placement and permission to distribute it are independent. */
object MissionDistributionPolicy {
    private fun leaf(path: String): String = path.replace('\\', '/').substringAfterLast('/').lowercase(Locale.ROOT)

    fun isVertigoMission(path: String): Boolean = leaf(path) in setOf("d2x", "d2x.mn2")

    private fun isProprietaryPath(path: String): Boolean {
        val name = leaf(path)
        return name in setOf("d2x.hog", "d2x.mn2", "d2x.ham") ||
            Regex("d2xlvl(?:[0-9]{2}|s[1-3])\\.rl2").matches(name)
    }

    fun missionPolicy(path: String): MissionDownloadPolicy =
        if (isVertigoMission(path)) MissionDownloadPolicy.PROPRIETARY else MissionDownloadPolicy.USER_SUPPLIED

    fun archivePolicy(scan: MissionZip.ScanResult): MissionDownloadPolicy {
        // The whole wrapper is transferred, including unselected missions and variants
        val names =
            scan.constituents.flatMap { listOf(it.path) + it.archiveEntries.orEmpty() } +
                scan.missionSets.flatMap {
                    listOf(
                        it.mission.path,
                    ) + it.mission.levelNames + it.mission.secretLevelNames
                }
        if (names.any(::isProprietaryPath)) return MissionDownloadPolicy.PROPRIETARY
        // Do not approve opaque nested archives or HOG/DXA files whose catalogs failed
        if (names.any(ArchiveFiles::isSupportedArchiveName) ||
            scan.constituents.any {
                it.role in setOf(GameFileFormats.MISSION_ZIP_HOG, GameFileFormats.MISSION_ZIP_MOD_ARCHIVE) &&
                    it.archiveEntries == null
            }
        ) {
            return MissionDownloadPolicy.UNVERIFIED
        }
        return MissionDownloadPolicy.USER_SUPPLIED
    }

    internal fun archivePolicy(file: File): MissionDownloadPolicy =
        runCatching { MissionZip.inspect(file)?.let(::archivePolicy) }.getOrNull() ?: MissionDownloadPolicy.UNVERIFIED
}
