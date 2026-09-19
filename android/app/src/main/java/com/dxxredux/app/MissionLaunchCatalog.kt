package com.dxxredux.app

import java.io.File
import java.util.Locale

/** Identity for native mission selection; titles and legacy short names are not unique */
internal data class MissionLaunchKey(
    val owner: String,
    val descriptor: String,
    val game: String,
)

internal data class MissionLaunchEntry(
    val key: MissionLaunchKey,
    val title: String,
    val activationError: String = "",
)

/** An extracted file retains its owner even if it is a DXA or generated song list */
internal data class MissionLaunchResource(
    val source: File,
    val virtualPath: String,
    val sha256: String,
    // Empty means package-shared, never globally shared
    val missions: Set<MissionLaunchKey>,
)

internal data class MissionLaunchPackage(
    val owner: String,
    val revision: String,
    val missions: List<MissionLaunchEntry>,
    val resources: List<MissionLaunchResource>,
)

/**
 * Pure selection policy for the forthcoming native asset-context boundary
 *
 * Building or browsing this catalog does not mount anything. A null selection
 * represents the base game, with no external mission-owned resources
 */
internal class MissionLaunchCatalog(
    packages: List<MissionLaunchPackage>,
) {
    private val packages =
        packages.map { pack ->
            pack.copy(
                missions = pack.missions.toList(),
                resources = pack.resources.map { it.copy(missions = it.missions.toSet()) },
            )
        }
    val missions: List<MissionLaunchEntry> get() = packages.flatMap { it.missions }

    fun plus(other: MissionLaunchCatalog): MissionLaunchCatalog = MissionLaunchCatalog(packages + other.packages)

    init {
        require(
            this.packages
                .map { it.owner }
                .distinct()
                .size == this.packages.size,
        ) { "Duplicate mission package owner" }
        for (pack in this.packages) {
            require(pack.owner.isNotBlank() && pack.revision.isNotBlank()) { "Missing mission package identity" }
            require(pack.missions.isNotEmpty()) { "Mission package has no playable missions: ${pack.owner}" }
            val keys = pack.missions.map { it.key }.toSet()
            require(
                keys.size == pack.missions.size && keys.all { it.owner == pack.owner },
            ) { "Invalid mission identities" }
            for (resource in pack.resources) {
                require(resource.missions.all { it in keys }) { "Resource references an unknown mission" }
                require(resource.sha256.isNotBlank()) { "Missing resource fingerprint: ${resource.virtualPath}" }
                requireSafeMissionPath(resource.virtualPath)
            }
            for (mission in pack.missions) {
                requireSafeMissionPath(mission.key.descriptor)
                require(pack.resources.any { it.virtualPath == mission.key.descriptor && mission.key in it.missions }) {
                    "Missing owned descriptor: ${mission.key}"
                }
            }
        }
    }

    fun resourcesFor(selection: MissionLaunchKey?): List<MissionLaunchResource> {
        if (selection == null) return emptyList()
        val pack = packages.singleOrNull { it.owner == selection.owner }
        require(pack != null && pack.missions.any { it.key == selection }) { "Unknown mission selection: $selection" }
        val resources = pack.resources.filter { it.missions.isEmpty() || selection in it.missions }
        val paths = resources.groupBy { it.virtualPath.lowercase(Locale.US) }
        require(
            paths.values.all { candidates ->
                candidates.map { it.sha256.lowercase(Locale.US) }.distinct().size == 1 &&
                    candidates.map { it.missions }.distinct().size == 1
            },
        ) {
            buildString {
                append("Conflicting resource paths for $selection")
                resources
                    .groupBy { it.virtualPath.lowercase(Locale.US) }
                    .filterValues { it.size > 1 }
                    .toSortedMap()
                    .forEach { (path, candidates) ->
                        val identical = candidates.map { it.sha256.lowercase(Locale.US) }.distinct().size == 1
                        append("\nresource-path-conflict path='$path' identical_sha256=$identical")
                        candidates.sortedBy { it.virtualPath }.forEach { resource ->
                            append("\n  virtual='${resource.virtualPath}' source='${resource.source.absolutePath}'")
                            append(" exists=${resource.source.isFile} bytes=${resource.source.length()}")
                            append(" sha256=${resource.sha256}")
                            append(" scope=${if (resource.missions.isEmpty()) "package-shared" else "mission-owned"}")
                        }
                    }
            }
        }
        // Some discs contain identical case variants; preserve the descriptor's exact spelling
        return paths.values.map { candidates ->
            candidates.minWith(
                compareBy<MissionLaunchResource> { it.virtualPath != selection.descriptor }
                    .thenBy { it.virtualPath }
                    .thenBy { it.source.absolutePath },
            )
        }
    }

    fun revisionFor(selection: MissionLaunchKey): String {
        require(missions.any { it.key == selection }) { "Unknown mission selection: $selection" }
        return packages.single { it.owner == selection.owner }.revision
    }

    /** Legacy save/demo lookup must not choose a package by enumeration order */
    fun resolveLegacy(
        shortName: String,
        game: String,
    ): MissionLaunchKey? {
        val matches =
            missions.filter {
                it.key.game == game &&
                    it.key.descriptor
                        .substringAfterLast(
                            '/',
                        ).substringBeforeLast('.')
                        .equals(shortName, ignoreCase = true)
            }
        require(matches.size <= 1) { "Ambiguous mission '$shortName' for $game; select its package explicitly" }
        return matches.singleOrNull()?.key
    }
}

/** Reuse the existing importer's mission inventory and verified extraction record */
internal fun missionLaunchPackage(
    owner: String,
    scan: MissionZip.ScanResult,
    record: MissionZipExtractionRecord,
    game: String,
    includeD1ForD2: Boolean,
): MissionLaunchPackage? {
    val missionSets =
        scan.effectiveMissionSets.filter {
            it.mission.game == game || (game == "d2" && includeD1ForD2 && it.mission.game == "d1")
        }
    if (missionSets.isEmpty()) return null

    fun key(set: MissionZip.MissionSet) =
        MissionLaunchKey(owner, stagedRelativePath(scan, set.mission.path), set.mission.game)
    val resources =
        record.files.mapNotNull { file ->
            val sourceOwners = missionResourceOwners(scan, file.sourceEntryPath)
            val owners =
                missionSets
                    .filter { it in sourceOwners }
                    .map(::key)
                    .toSet()
            // A filtered-out variant's files must not become package-shared
            if (sourceOwners.isNotEmpty() && owners.isEmpty()) return@mapNotNull null
            requireSafeMissionPath(file.relativePath)
            val source = File(record.rootDir, file.relativePath).canonicalFile
            require(source.toPath().startsWith(record.rootDir.canonicalFile.toPath()) && source.isFile) {
                "Missing or escaped mission resource: ${file.relativePath}"
            }
            MissionLaunchResource(source, file.relativePath, file.contentSha256, owners)
        }
    return MissionLaunchPackage(
        owner,
        record.ownerSha256,
        missionSets.map { MissionLaunchEntry(key(it), it.mission.displayName, it.activationError) },
        resources,
    )
}

private fun missionResourceOwners(
    scan: MissionZip.ScanResult,
    sourcePath: String,
): List<MissionZip.MissionSet> {
    val path = sourcePath.lowercase(Locale.US)
    val direct =
        scan.missionSets.filter { set ->
            set.mission.path.equals(path, ignoreCase = true) ||
                set.constituents.any { it.path.equals(path, ignoreCase = true) }
        }
    if (direct.isNotEmpty()) return direct
    val sidecars =
        scan.missionSets.filter { set ->
            val descriptor = set.mission.path.lowercase(Locale.US)
            val stem = descriptor.substringAfterLast('/').substringBeforeLast('.')
            (path.substringBeforeLast('.', path) == descriptor.substringBeforeLast('.')) ||
                path.startsWith("mods/$stem/")
        }
    if (sidecars.isNotEmpty()) return sidecars
    // Files beneath a variant's directory inherit that variant, including files
    // not listed as level payloads by the importer. The nearest directory wins
    val directories =
        scan.missionSets.mapNotNull { set ->
            val directory =
                set.mission.path
                    .substringBeforeLast('/', "")
                    .lowercase(Locale.US)
            if (directory.isNotEmpty() && path.startsWith("$directory/")) directory.length to set else null
        }
    val longest = directories.maxOfOrNull { it.first } ?: return emptyList()
    return directories.filter { it.first == longest }.map { it.second }
}

private fun requireSafeMissionPath(path: String) {
    require(
        path.isNotEmpty() && '\\' !in path && ':' !in path &&
            path.split('/').none { it.isEmpty() || it == "." || it == ".." },
    ) {
        "Invalid mission resource path: $path"
    }
}
