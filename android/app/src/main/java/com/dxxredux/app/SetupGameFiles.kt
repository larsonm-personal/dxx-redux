package com.dxxredux.app

import java.io.File

internal data class GameFileInfo(
    val filename: String,
    val description: String,
    val required: Boolean,
    val alternatives: List<String> = emptyList(),
    val downloadUrl: String? = null,
)

internal data class FileStatus(
    val info: GameFileInfo,
    val found: Boolean,
    val foundName: String?,
    val manifestEntry: AssetManifest.AssetEntry? = null,
    val safUri: String? = null,
    val safSizeBytes: Long = 0,
)

internal data class D1InD2Readiness(
    val needed: Boolean,
    val ready: Boolean,
    val degraded: Boolean,
    val blocked: Boolean,
    val d2Ready: Boolean,
    val d1AssetsReady: Boolean,
    val d1AssetStatuses: List<FileStatus>,
)

internal fun launcherDumpDirectoryState(
    prefix: String,
    dir: File,
) {
    val entries = dir.listFiles()?.sortedBy { it.name.lowercase() } ?: emptyArray<File>().toList()
    LauncherDebugLog.log("$prefix dir=${dir.absolutePath} count=${entries.size}")
    if (entries.isEmpty()) {
        LauncherDebugLog.log("$prefix entry=<none>")
        return
    }
    for (entry in entries) {
        val kind = if (entry.isDirectory) "dir" else "file"
        val size = if (entry.isFile) entry.length() else -1L
        LauncherDebugLog.log(
            "$prefix entry kind=$kind name=${entry.name} size=$size path=${entry.absolutePath}",
        )
    }
}

internal fun launcherDumpStatusList(
    prefix: String,
    statuses: List<FileStatus>,
) {
    LauncherDebugLog.log("$prefix status_count=${statuses.size}")
    for (status in statuses) {
        val alternatives =
            if (status.info.alternatives.isEmpty()) "-" else status.info.alternatives.joinToString("|")
        LauncherDebugLog.log(
            "$prefix filename=${status.info.filename} required=${status.info.required} found=${status.found} found_name=${status.foundName ?: "-"} manifest_filename=${status.manifestEntry?.filename ?: "-"} manifest_source_uri=${status.manifestEntry?.sourceUri ?: "-"} saf_uri=${status.safUri ?: "-"} saf_size=${status.safSizeBytes} alternatives=$alternatives",
        )
    }
}

internal fun launcherDumpFileTable(
    reason: String,
    filesDir: File,
    activeSetName: String,
    setDir: File,
    manifest: AssetManifest,
    safManifest: SafManifest,
) {
    LauncherDebugLog.log(
        "launcher-file-dump reason=$reason active_set=$activeSetName set_dir=${setDir.absolutePath}",
    )
    launcherDumpDirectoryState("launcher-root-files", filesDir)
    launcherDumpDirectoryState("launcher-set-files", setDir)

    val assetEntries = manifest.load().sortedBy { it.filename }
    val assetsPath = File(setDir, "assets.json")
    LauncherDebugLog.log(
        "launcher-asset-manifest file=${assetsPath.absolutePath} exists=${assetsPath.exists()} count=${assetEntries.size}",
    )
    if (assetEntries.isEmpty()) {
        LauncherDebugLog.log("launcher-asset-entry <none>")
    } else {
        for (entry in assetEntries) {
            val matchedName = findFile(setDir, entry.filename)
            val path = matchedName?.let { File(setDir, it) } ?: File(setDir, entry.filename)
            LauncherDebugLog.log(
                "launcher-asset-entry filename=${entry.filename} matched_name=${matchedName ?: "-"} path=${path.absolutePath} exists=${path.exists()} size=${entry.sizeBytes} source_uri=${entry.sourceUri ?: "-"} version=${entry.versionName ?: "-"}",
            )
        }
    }

    val safEntries = safManifest.read().sortedBy { it.filename }
    val safPath = File(setDir, SafManifest.FILENAME)
    LauncherDebugLog.log(
        "launcher-saf-manifest file=${safPath.absolutePath} exists=${safPath.exists()} count=${safEntries.size}",
    )
    if (safEntries.isEmpty()) {
        LauncherDebugLog.log("launcher-saf-entry <none>")
    } else {
        for (entry in safEntries) {
            LauncherDebugLog.log(
                "launcher-saf-entry filename=${entry.filename} uri=${entry.contentUri} size=${entry.sizeBytes}",
            )
        }
    }

    val d2FileList = detectD2FileList(setDir, safManifest)
    val d2Statuses = checkFiles(setDir, d2FileList, manifest, safManifest)
    val d1Statuses = checkFiles(setDir, D1_FILES, manifest, safManifest)
    launcherDumpStatusList("launcher-d2-status", d2Statuses)
    launcherDumpStatusList("launcher-d1-status", d1Statuses)
}

internal fun findFile(
    dir: File,
    name: String,
): String? {
    val files = dir.listFiles() ?: return null
    return files.firstOrNull { it.name.equals(name, ignoreCase = true) }?.name
}

internal fun launchDataReadyForGame(
    game: String,
    setDir: File,
    manifest: AssetManifest,
    safManifest: SafManifest,
): Boolean {
    if (game == "d1" && isD1TestFlightSet(setDir, manifest, safManifest)) return false
    val fileList = if (game == "d1") D1_FILES else detectD2FileList(setDir, safManifest)
    return checkFiles(setDir, fileList, manifest, safManifest)
        .filter { it.info.required }
        .all { it.found }
}

internal fun d1InD2Readiness(
    filesDir: File,
    setDir: File,
    manifest: AssetManifest,
    safManifest: SafManifest,
): D1InD2Readiness {
    val d2Ready = launchDataReadyForGame("d2", setDir, manifest, safManifest)
    val d1Ready = launchDataReadyForGame("d1", setDir, manifest, safManifest)
    val needed = ModManager(filesDir).hasEnabledD1MissionZipForD2()
    return D1InD2Readiness(
        needed = needed,
        ready = !needed || (d2Ready && d1Ready),
        degraded = needed && d2Ready && !d1Ready,
        blocked = needed && !d2Ready,
        d2Ready = d2Ready,
        d1AssetsReady = d1Ready,
        d1AssetStatuses = checkFiles(setDir, D1_FILES, manifest, safManifest),
    )
}

// Duplicates known_versions.json5 so readiness can reject this old demo before native launch.
private const val D1_TEST_FLIGHT_HOG_SHA256 =
    "40c5754bb1e4cc0b0e176d50154568cb754d689df434511e0d8bdc1053f4de4a"
private const val D1_TEST_FLIGHT_PIG_SHA256 =
    "4a7b57482030ca18aa50accfaee6ee20ff24c077fb1b5adcffcf2fbb8dc91c21"
private const val D1_TEST_FLIGHT_HOG_SIZE = 1626232L
private const val D1_TEST_FLIGHT_PIG_SIZE = 28518L

internal fun isD1TestFlightSet(
    setDir: File,
    manifest: AssetManifest,
    safManifest: SafManifest,
): Boolean {
    val hogEntry = manifest.getEntry("descent.hog")
    val pigEntry = manifest.getEntry("descent.pig")
    if (hogEntry?.sha256 == D1_TEST_FLIGHT_HOG_SHA256 &&
        pigEntry?.sha256 == D1_TEST_FLIGHT_PIG_SHA256
    ) {
        return true
    }

    val safEntries = safManifest.read()
    return fileSizeForLaunchCheck(setDir, safEntries, "descent.hog") == D1_TEST_FLIGHT_HOG_SIZE &&
        fileSizeForLaunchCheck(setDir, safEntries, "descent.pig") == D1_TEST_FLIGHT_PIG_SIZE
}

private fun fileSizeForLaunchCheck(
    setDir: File,
    safEntries: List<SafManifest.SafFileEntry>,
    filename: String,
): Long? {
    findFile(setDir, filename)?.let { return File(setDir, it).length() }
    return safEntries.firstOrNull { it.filename.equals(filename, ignoreCase = true) }?.sizeBytes
}

internal fun checkFiles(
    dir: File,
    fileList: List<GameFileInfo>,
    manifest: AssetManifest? = null,
    safManifest: SafManifest? = null,
): List<FileStatus> {
    val safEntries = safManifest?.read() ?: emptyList()
    return fileList.map { info ->
        val primaryMatch = findFile(dir, info.filename)
        val altMatch =
            if (primaryMatch == null) {
                info.alternatives.firstNotNullOfOrNull { findFile(dir, it) }
            } else {
                null
            }
        val foundName = primaryMatch ?: altMatch
        val safEntry =
            if (foundName == null) {
                safEntries.firstOrNull { it.filename.equals(info.filename, ignoreCase = true) }
                    ?: info.alternatives.firstNotNullOfOrNull { alt ->
                        safEntries.firstOrNull { it.filename.equals(alt, ignoreCase = true) }
                    }
            } else {
                null
            }
        val entry =
            if (foundName != null) {
                manifest?.getEntry(foundName)
            } else {
                manifest?.getEntry(info.filename)
            }
        FileStatus(
            info,
            found = foundName != null || safEntry != null,
            foundName = foundName ?: if (safEntry != null) info.filename else null,
            manifestEntry = entry,
            safUri = safEntry?.contentUri,
            safSizeBytes = safEntry?.sizeBytes ?: 0,
        )
    }
}

internal fun descriptionForFile(filename: String): String {
    val lower = filename.lowercase()
    val allFiles = D2_FILES + D2_DEMO_FILES + D2_PARTIAL_FILES + D1_FILES
    return allFiles
        .firstOrNull { info ->
            info.filename.equals(lower, ignoreCase = true) ||
                info.alternatives.any { it.equals(lower, ignoreCase = true) }
        }?.description ?: launcherFileTypeLabel(filename)
}

internal fun describeExtension(filename: String): String = launcherExtensionDescription(filename)

internal val D2_FILES =
    listOf(
        GameFileInfo(
            "descent2.hog",
            "Main game data",
            required = true,
            alternatives = listOf("d2demo.hog"),
        ),
        GameFileInfo(
            "descent2.ham",
            "Models & objects",
            required = true,
            alternatives = listOf("d2demo.ham"),
        ),
        GameFileInfo(
            "groupa.pig",
            "Main textures",
            required = true,
            alternatives = listOf("d2demo.pig"),
        ),
        GameFileInfo(
            "descent2.s22",
            "Sound effects (22 kHz)",
            required = true,
            alternatives = listOf("descent2.s11"),
        ),
        GameFileInfo("alien1.pig", "Alien 1 level textures", required = true),
        GameFileInfo("alien2.pig", "Alien 2 level textures", required = true),
        GameFileInfo("fire.pig", "Fire level textures", required = true),
        GameFileInfo("ice.pig", "Ice level textures", required = true),
        GameFileInfo("water.pig", "Water level textures", required = true),
        GameFileInfo(
            "intro-h.mvl",
            "Intro movie",
            required = false,
            alternatives = listOf("intro-l.mvl"),
        ),
        GameFileInfo(
            "other-h.mvl",
            "Cutscene movies",
            required = false,
            alternatives = listOf("other-l.mvl"),
        ),
        GameFileInfo(
            "robots-h.mvl",
            "Robot movies",
            required = false,
            alternatives = listOf("robots-l.mvl"),
        ),
        GameFileInfo("d2x.hog", "Vertigo expansion", required = false),
        GameFileInfo("hoard.ham", "Hoard multiplayer mode", required = false),
    )

internal val D2_DEMO_FILES =
    listOf(
        GameFileInfo("d2demo.hog", "Demo game data", required = true),
        GameFileInfo("d2demo.ham", "Demo models & objects", required = true),
        GameFileInfo("d2demo.pig", "Demo textures", required = true),
        GameFileInfo("d2demo.dem", "Demo playback", required = false),
        GameFileInfo("descent2.s11", "Mac demo sound effects", required = false),
        GameFileInfo("exit.ham", "Mac demo exit data", required = false),
    )

// The first size must match OEM_MISSION_HOGSIZE in d2/main/mission.h. The
// Quartzon 3D release wraps the same A/B-level data in a larger HOG.
private val D2_PARTIAL_HOG_SIZES = setOf(6132957L, 14024077L)

internal val D2_PARTIAL_FILES =
    listOf(
        GameFileInfo("descent2.hog", "OEM game data", required = true),
        GameFileInfo("descent2.ham", "Models & objects", required = true),
        GameFileInfo("groupa.pig", "Quartzon textures", required = true),
        GameFileInfo("water.pig", "Brimspark textures", required = true),
        GameFileInfo(
            "descent2.s22",
            "Sound effects",
            required = true,
            alternatives = listOf("descent2.s11"),
        ),
    )

internal fun detectD2FileList(
    dir: File,
    safManifest: SafManifest? = null,
): List<GameFileInfo> {
    val demoFiles = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig")
    val hasDemoOnDisk = demoFiles.any { findFile(dir, it) != null }
    val hasDemoInSaf =
        safManifest?.let { sm ->
            val entries = sm.read()
            demoFiles.any { demo -> entries.any { it.filename.equals(demo, ignoreCase = true) } }
        } ?: false
    if (hasDemoOnDisk || hasDemoInSaf) return D2_DEMO_FILES

    val safEntries = safManifest?.read() ?: emptyList()
    val hogSize = fileSizeForLaunchCheck(dir, safEntries, "descent2.hog")
    return if (hogSize in D2_PARTIAL_HOG_SIZES) D2_PARTIAL_FILES else D2_FILES
}

internal val D1_FILES =
    listOf(
        GameFileInfo("descent.hog", "D1 game data", required = true),
        GameFileInfo("descent.pig", "D1 textures", required = true),
    )

internal data class RecommendedMod(
    val filename: String,
    val displayName: String,
    val description: String,
    val downloadUrl: String,
    val game: String,
)

internal val RECOMMENDED_MODS =
    listOf(
        RecommendedMod(
            "d1xr-mac-demo-sounds.dxa",
            "D1 Mac Demo Sounds",
            "Sound replacements from the Mac demo",
            "https://dxx-redux.com/dl/d1xr-mac-demo-sounds.dxa",
            "d1",
        ),
        RecommendedMod(
            "d1xr-hires.dxa",
            "D1 High-Res Pack",
            "High-resolution textures for D1",
            "https://dxx-redux.com/dl/d1xr-hires.dxa",
            "d1",
        ),
    )

internal data class DemoPackage(
    val game: String,
    val name: String,
    val url: String,
    val downloadFilename: String,
    val archiveName: String,
    val description: String,
    val sizeBytes: Long,
    val files: List<String>,
)

internal val DEMO_DOWNLOADS =
    listOf(
        DemoPackage(
            game = "d2",
            name = "D2 Demo",
            url =
                "https://github.com/larsonm-personal/dxx-redux/releases/download/" +
                    "demo_installers/Descent.II.Preview.sit",
            downloadFilename = "Descent.II.Preview.sit",
            archiveName = "Descent II Preview.sit",
            description = "Descent 2 Mac preview demo",
            sizeBytes = 7_753_518L,
            files = listOf("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham"),
        ),
        DemoPackage(
            game = "d1",
            name = "D1 Demo",
            url =
                "https://github.com/larsonm-personal/dxx-redux/releases/download/" +
                    "demo_installers/Descent.Shareware.sit",
            downloadFilename = "Descent.Shareware.sit",
            archiveName = "Descent Shareware.sit",
            description = "Descent 1 Mac shareware demo",
            sizeBytes = 4_735_288L,
            files = listOf("descent.hog", "descent.pig"),
        ),
    )

internal fun visibleDemoInstallerOffers(
    showDemoInstallerOffer: Boolean,
    d1Ready: Boolean,
    d2Ready: Boolean,
): List<DemoPackage> =
    if (!showDemoInstallerOffer) {
        emptyList()
    } else {
        DEMO_DOWNLOADS.filter { demo ->
            when (demo.game) {
                "d1" -> !d1Ready
                "d2" -> !d2Ready
                else -> false
            }
        }
    }

internal val ALL_GAME_FILENAMES: Set<String> by lazy {
    (D2_FILES + D2_DEMO_FILES + D2_PARTIAL_FILES + D1_FILES)
        .flatMap { info ->
            listOf(info.filename) + info.alternatives
        }.map { it.lowercase() }
        .toSet()
}
