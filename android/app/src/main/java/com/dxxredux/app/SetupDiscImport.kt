package com.dxxredux.app

import android.content.Context
import android.net.Uri
import android.util.Log
import java.io.File
import java.util.Locale

internal data class GogAudioPair(
    val baseName: String,
    val gogFileName: String,
    val instFileName: String,
)

private const val D2_GOG_AUDIO_SOURCE_ID = "d2-gog-v1.2"
private const val D2_GOG_LEGACY_DISC_ID = 0x7d0ff809L
private const val D2_GOG_CUE_TEXT =
    "  TRACK 01 MODE1/2352\n" +
        "    INDEX 01 00:00:00\n" +
        "  TRACK 02 AUDIO\n" +
        "    INDEX 01 14:18:48\n" +
        "  TRACK 03 AUDIO\n" +
        "    INDEX 01 15:04:48\n" +
        "  TRACK 04 AUDIO\n" +
        "    INDEX 01 20:05:48\n" +
        "  TRACK 05 AUDIO\n" +
        "    INDEX 01 23:47:48\n" +
        "  TRACK 06 AUDIO\n" +
        "    INDEX 01 27:22:48\n" +
        "  TRACK 07 AUDIO\n" +
        "    INDEX 01 29:42:48\n" +
        "  TRACK 08 AUDIO\n" +
        "    INDEX 01 34:09:48\n" +
        "  TRACK 09 AUDIO\n" +
        "    INDEX 01 37:50:48\n"

internal fun findGogPair(dir: File): GogAudioPair? {
    val files = dir.list() ?: return null
    val gogByBase =
        files
            .filter { it.endsWith(".gog", ignoreCase = true) }
            .associateBy { it.substringBeforeLast('.').lowercase(Locale.US) }
    val instByBase =
        files
            .filter { it.endsWith(".inst", ignoreCase = true) }
            .associateBy { it.substringBeforeLast('.').lowercase(Locale.US) }
    val pairedBases = (gogByBase.keys intersect instByBase.keys)
    val base =
        pairedBases.firstOrNull { it == "descent_ii" } ?: pairedBases.sorted().firstOrNull() ?: return null
    val gogName = gogByBase[base] ?: return null
    val instName = instByBase[base] ?: return null
    return GogAudioPair(gogName.substringBeforeLast('.'), gogName, instName)
}

private fun findGogAudioBaseNames(
    files: Collection<String>,
    extension: String,
): Set<String> =
    files
        .asSequence()
        .filter { it.endsWith(extension, ignoreCase = true) }
        .map { it.substringBeforeLast('.').lowercase(Locale.US) }
        .toSet()

internal fun summarizeGogAudioFiles(files: Collection<String>): String {
    val audioFiles = files.filter { AndroidGameFileExtensions.isGogAudioFile(it) }.sortedBy { it.lowercase(Locale.US) }
    return if (audioFiles.isEmpty()) "-" else audioFiles.joinToString(",")
}

internal fun summarizeGogAudioEntrySizes(files: Collection<GogImportBridge.GogFile>): String {
    val audioFiles =
        files
            .asSequence()
            .filter { AndroidGameFileExtensions.isGogAudioFile(it.name) }
            .sortedBy { it.name.lowercase(Locale.US) }
            .toList()
    return if (audioFiles.isEmpty()) {
        "-"
    } else {
        audioFiles.joinToString(",") { "${File(it.name).name}:${it.size}" }
    }
}

internal fun describeNamedFileStates(
    dir: File,
    names: Collection<String>,
): String {
    val orderedNames = names.map { File(it).name }.distinct().sortedBy { it.lowercase(Locale.US) }
    return if (orderedNames.isEmpty()) {
        "-"
    } else {
        orderedNames.joinToString(",") { name ->
            val file = File(dir, name)
            if (file.exists()) "$name:${file.length()}" else "$name:missing"
        }
    }
}

internal fun discLeafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

internal fun describeGogPairState(files: Collection<String>): String {
    val gogBases = findGogAudioBaseNames(files, ".gog")
    val instBases = findGogAudioBaseNames(files, ".inst")
    val paired = (gogBases intersect instBases).sorted()
    return when {
        paired.isNotEmpty() -> "paired:${paired.joinToString(",")}"
        gogBases.isEmpty() && instBases.isEmpty() -> "missing-both"
        gogBases.isEmpty() -> "missing-gog inst-only:${instBases.sorted().joinToString(",")}"
        instBases.isEmpty() -> "missing-inst gog-only:${gogBases.sorted().joinToString(",")}"
        else -> "mismatched gog:${gogBases.sorted().joinToString(",")} inst:${instBases.sorted().joinToString(",")}"
    }
}

internal fun registerGogAudioSource(
    srcManager: AudioSourceManager,
    filesDir: File,
    setDir: File,
    context: Context? = null,
): Boolean {
    val source = buildGogAudioSource(filesDir, setDir, context) ?: return false
    srcManager.addSource(source)
    return true
}

internal fun buildGogAudioSource(
    filesDir: File,
    setDir: File,
    context: Context? = null,
): AudioSourceManager.AudioSource? {
    ensureKnownGogCueFile(setDir)
    val pair = findGogPair(setDir) ?: return null
    val relDir = setDir.toRelativeString(filesDir)
    val relCue = if (relDir.isEmpty()) pair.instFileName else "$relDir${File.separator}${pair.instFileName}"
    val relBin = if (relDir.isEmpty()) pair.gogFileName else "$relDir${File.separator}${pair.gogFileName}"
    val trackCounts = countCueTracks(File(setDir, pair.instFileName))
    val sourceId =
        if (isKnownD2GogPair(pair)) {
            D2_GOG_AUDIO_SOURCE_ID
        } else {
            "gog-${sanitizeCdAudioImportStem(pair.baseName)}"
        }
    val discLabel = if (sourceId == D2_GOG_AUDIO_SOURCE_ID) "Descent II (GOG)" else pair.baseName
    val trackNames =
        context?.let {
            try {
                FingerprintBridge.lookupTrackNames(it, sourceId)
            } catch (e: Exception) {
                emptyMap()
            }
        } ?: emptyMap()
    return AudioSourceManager.AudioSource(
        id = sourceId,
        cuePath = relCue,
        binPaths = listOf(relBin),
        discLabel = discLabel,
        discId = sourceId,
        trackCount = trackCounts.first,
        audioTrackCount = trackCounts.second,
        legacyDiscId = if (sourceId == D2_GOG_AUDIO_SOURCE_ID) D2_GOG_LEGACY_DISC_ID else 0L,
        trackNames = trackNames,
    )
}

private fun ensureKnownGogCueFile(setDir: File) {
    val files = setDir.list() ?: return
    if (files.any { it.endsWith(".inst", ignoreCase = true) }) return

    val gogName = files.firstOrNull { it.equals("DESCENT_II.gog", ignoreCase = true) } ?: return
    val cueFile = File(setDir, "${File(gogName).nameWithoutExtension}.inst")
    cueFile.writeText("FILE \"$gogName\" BINARY\n$D2_GOG_CUE_TEXT")
}

private fun isKnownD2GogPair(pair: GogAudioPair): Boolean = pair.baseName.equals("DESCENT_II", ignoreCase = true)

private fun countCueTracks(cueFile: File): Pair<Int, Int> {
    var tracks = 0
    var audio = 0
    if (!cueFile.isFile) return 0 to 0
    cueFile.forEachLine { line ->
        val parts = line.trim().split(Regex("\\s+"), limit = 3)
        if (parts.size >= 3 && parts[0].equals("TRACK", ignoreCase = true)) {
            tracks++
            if (parts[2].equals("AUDIO", ignoreCase = true)) audio++
        }
    }
    return tracks to audio
}

internal fun extractSowArchives(
    setDir: File,
    progress: DiscImportBridge.ExtractProgress? = null,
): Int {
    val sowFiles = DiscImportBridge.scanSowFiles(setDir.absolutePath) ?: return 0
    var sowExtracted = 0
    for (sow in sowFiles) {
        val outputDir = File(sow).parentFile?.absolutePath ?: setDir.absolutePath
        sowExtracted += DiscImportBridge.extractSowFiles(sow, outputDir, progress).coerceAtLeast(0)
    }
    return sowExtracted
}

internal fun postProcessImportedDiscFiles(
    setDir: File,
    progress: DiscImportBridge.ExtractProgress? = null,
): Int {
    val sowExtracted = extractSowArchives(setDir, progress)
    hoistNestedImportedGameFiles(setDir)
    return sowExtracted
}

internal fun buildDiscExtractSummary(
    primaryCount: Int,
    primaryLabel: String,
    sowExtracted: Int,
): String {
    val parts = mutableListOf("$primaryCount $primaryLabel")
    if (sowExtracted > 0) {
        parts += "$sowExtracted from .sow archives"
    }
    return "Extracted ${parts.joinToString(" + ")}"
}

private fun moveImportedGameFileToRoot(
    source: File,
    dest: File,
): Boolean {
    if (source.renameTo(dest)) return true
    return try {
        source.copyTo(dest, overwrite = true)
        source.delete()
        true
    } catch (_: Exception) {
        false
    }
}

internal fun hoistNestedImportedGameFiles(setDir: File): Int {
    if (!setDir.isDirectory) return 0

    val rootFiles =
        (setDir.listFiles() ?: emptyArray())
            .filter { it.isFile }
            .associateBy { it.name.lowercase() }
            .toMutableMap()
    var hoisted = 0

    setDir
        .walkTopDown()
        .filter { it.isFile && it.parentFile != setDir }
        .forEach { file ->
            val lowercaseName = file.name.lowercase()
            if (lowercaseName !in ALL_GAME_FILENAMES || file.length() <= 1L) return@forEach

            val existing = rootFiles[lowercaseName]
            if (existing != null && existing.absolutePath == file.absolutePath) return@forEach
            if (existing != null && existing.length() >= file.length()) {
                file.delete()
                return@forEach
            }

            existing?.delete()
            val dest = File(setDir, file.name)
            if (moveImportedGameFileToRoot(file, dest)) {
                rootFiles[lowercaseName] = dest
                hoisted++
            } else {
                runCatching {
                    Log.w("DXX-DiscImport", "Could not hoist ${file.absolutePath} to ${dest.absolutePath}")
                }
            }
        }

    setDir
        .walkBottomUp()
        .filter { it.isDirectory && it != setDir }
        .forEach { dir ->
            if ((dir.listFiles()?.isEmpty() ?: false)) dir.delete()
        }

    if (hoisted > 0) {
        runCatching {
            Log.i("DXX-DiscImport", "Hoisted $hoisted nested game file(s) into ${setDir.absolutePath}")
        }
    }
    return hoisted
}

private val CUE_FILE_LINE_REGEX = Regex("^\\s*FILE\\s+\"([^\"]+)\"", RegexOption.IGNORE_CASE)

internal data class OrderedCueEntries<T>(
    val orderedEntries: List<T>,
    val missingNames: List<String>,
    val extraNames: List<String> = emptyList(),
)

internal fun summarizeDiscImageNames(
    names: List<String>,
    limit: Int = 3,
): String {
    val uniqueNames = names.map { File(it).name }.distinct()
    if (uniqueNames.isEmpty()) return ""
    val shownNames = uniqueNames.take(limit)
    val remainingCount = uniqueNames.size - shownNames.size
    return if (remainingCount > 0) {
        "${shownNames.joinToString(", ")}, and $remainingCount more"
    } else {
        shownNames.joinToString(", ")
    }
}

internal fun buildMissingDiscImageSelectionMessage(missingNames: List<String>): String {
    val noun = if (missingNames.size == 1) "file" else "files"
    return "Missing CUE-referenced image $noun: ${summarizeDiscImageNames(missingNames)}. " +
        "Select the .cue and every referenced .bin/.img file"
}

internal fun buildDiscImageTrackSummary(
    dataTrackCount: Int,
    audioTrackCount: Int,
    imageCount: Int,
    extraNames: List<String> = emptyList(),
): String {
    val imageNoun = if (imageCount == 1) "image file" else "image files"
    val ignoredSuffix =
        if (extraNames.isEmpty()) {
            ""
        } else {
            val noun = if (extraNames.size == 1) "file" else "files"
            " Ignoring extra selected image $noun: ${summarizeDiscImageNames(extraNames)}"
        }
    return "Found $dataTrackCount data + $audioTrackCount audio track(s) across $imageCount $imageNoun" +
        ignoredSuffix
}

private fun parseCueReferencedFilenames(cueFile: File): List<String> {
    if (!cueFile.isFile) return emptyList()
    return runCatching {
        cueFile.useLines { lines ->
            lines
                .mapNotNull { line ->
                    CUE_FILE_LINE_REGEX
                        .find(line)
                        ?.groupValues
                        ?.getOrNull(1)
                        ?.let { File(it).name }
                }.toList()
        }
    }.getOrElse {
        Log.w("DXX-DiscImport", "Failed to read CUE file order for ${cueFile.absolutePath}", it)
        emptyList()
    }
}

internal fun <T> orderCueEntries(
    cueFile: File,
    entries: List<T>,
    nameOf: (T) -> String,
): OrderedCueEntries<T> {
    val referencedNames = parseCueReferencedFilenames(cueFile)
    if (referencedNames.isEmpty()) {
        return OrderedCueEntries(entries, emptyList(), emptyList())
    }

    val entriesByName = mutableMapOf<String, java.util.ArrayDeque<T>>()
    for (entry in entries) {
        val key = File(nameOf(entry)).name.lowercase()
        entriesByName.getOrPut(key) { java.util.ArrayDeque() }.addLast(entry)
    }

    val orderedEntries = mutableListOf<T>()
    val missingNames = mutableListOf<String>()
    for (referencedName in referencedNames) {
        val queue = entriesByName[referencedName.lowercase()]
        if (queue == null || queue.isEmpty()) {
            missingNames.add(referencedName)
            continue
        }
        orderedEntries.add(queue.removeFirst())
    }

    val extraNames =
        entriesByName.values
            .flatMap { queue -> queue.map { entry -> File(nameOf(entry)).name } }
            .distinct()

    return OrderedCueEntries(
        orderedEntries = if (orderedEntries.isEmpty()) entries else orderedEntries,
        missingNames = missingNames.distinct(),
        extraNames = extraNames,
    )
}

internal fun registerDiscAudioSourceFromPath(
    srcManager: AudioSourceManager,
    filesDir: File,
    context: Context,
    cuePath: String,
    binPaths: List<String>,
    tracks: List<DiscImportBridge.CueTrack>,
) {
    val orderedBinFiles = binPaths.map(::File)
    if (orderedBinFiles.isEmpty()) {
        Log.w("DXX-DiscImport", "registerDiscAudioSourceFromPath: no disc image files for $cuePath")
        return
    }

    var discLabel: String? = null
    var discId: String? = null
    var legacyDiscId = 0L
    val firstAudio = tracks.firstOrNull { it.isAudio }
    val trackNames = mutableMapOf<Int, String>()

    if (firstAudio != null) {
        try {
            val identifier = DiscIdentifier(context)
            val trackOffset = firstAudio.startSector.toLong() * 2352L
            val trackBytes = firstAudio.numSectors.toLong() * 2352L
            val audioBinFile = orderedBinFiles.getOrNull(firstAudio.fileIndex)
            if (audioBinFile == null) {
                Log.w(
                    "DXX-DiscImport",
                    "Disc identification missing fileIndex=${firstAudio.fileIndex} for $cuePath",
                )
            } else {
                audioBinFile.inputStream().use { input ->
                    input.channel.position(trackOffset)
                    val sha1 = DiscIdentifier.sha1Hash(input, trackBytes)
                    val match = identifier.identify(mapOf(firstAudio.trackNum to sha1))
                    if (match.matched) {
                        discLabel = match.label
                        discId = match.disc?.id
                        match.disc?.legacyDiscId?.let {
                            legacyDiscId = java.lang.Long.decode(it)
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Log.w("DXX-DiscImport", "Disc identification failed for $cuePath", e)
        }
    }

    try {
        discId?.let { resolvedDiscId ->
            trackNames.putAll(FingerprintBridge.lookupTrackNames(context, resolvedDiscId))
        }
        if (trackNames.isEmpty() && tracks.any { it.isAudio }) {
            val contentPaths = orderedBinFiles.map { it.absolutePath }
            val matchedTrackNames =
                if (contentPaths.size == 1) {
                    FingerprintBridge.fingerprintAndMatchDisc(context, contentPaths.first(), tracks)
                } else {
                    FingerprintBridge.fingerprintAndMatchDisc(context, contentPaths, tracks)
                }
            trackNames.putAll(matchedTrackNames)
        }
    } catch (e: Exception) {
        Log.w("DXX-DiscImport", "Track name identification failed for $cuePath", e)
    }

    val id = discId ?: "custom-${System.currentTimeMillis()}"
    val sourceFileStem =
        chooseUniqueCdAudioImportStem(
            preferredStem = File(cuePath).nameWithoutExtension,
            existingFileNames = filesDir.list()?.toSet() ?: emptySet(),
        )
    val destCue = File(filesDir, "$sourceFileStem.cue")
    LauncherFileCopy.copyFileToFile(File(cuePath), destCue)
    srcManager.addSource(
        AudioSourceManager.AudioSource(
            id = id,
            cuePath = destCue.name,
            binPaths = orderedBinFiles.map { it.name.lowercase() },
            discLabel = discLabel ?: File(cuePath).nameWithoutExtension,
            discId = discId ?: "unknown",
            trackCount = tracks.size,
            audioTrackCount = tracks.count { it.isAudio },
            legacyDiscId = legacyDiscId,
            trackNames = trackNames,
            binContentUri = orderedBinFiles.first().absolutePath,
            binContentUris = orderedBinFiles.map { it.absolutePath },
        ),
    )
}

internal data class StagedMergedSafDiscAudioSource(
    val cueFile: File,
    val binFile: File,
    val mergedTracks: List<DiscImportBridge.CueTrack>,
)

internal suspend fun stageMergedSafDiscAudioSource(
    filesDir: File,
    context: Context,
    sourceFileStem: String,
    binUris: List<Pair<String, Uri>>,
    tracks: List<DiscImportBridge.CueTrack>,
    binSizes: List<Long>,
    onStatus: suspend (String) -> Unit = {},
): StagedMergedSafDiscAudioSource {
    val mergedCueTracks = normalizeCueTracksForMergedBin(tracks, binSizes)
    val artifactDir = generatedCdAudioArtifactsDir(filesDir)
    val destBin = File(artifactDir, "$sourceFileStem.bin")
    val destCue = File(artifactDir, "$sourceFileStem.cue")

    ImportStorageGuard.requireFreeSpace(
        artifactDir,
        binSizes.sum(),
        "merged CD audio source",
    )

    java.io.FileOutputStream(destBin).use { output ->
        binUris.forEachIndexed { index, (name, uri) ->
            onStatus("Copying CD audio file ${index + 1}/${binUris.size}: $name")
            val totalBytes = binSizes.getOrElse(index) { 0L }
            context.contentResolver.openInputStream(uri)?.use { input ->
                LauncherFileCopy.copyStream(input, output, totalBytes, name)
            } ?: throw java.io.IOException("Could not open selected file: $name")
        }
    }

    destCue.writeText(buildMergedCueText(destBin.name, mergedCueTracks))
    return StagedMergedSafDiscAudioSource(destCue, destBin, mergedCueTracks)
}

internal fun importDiscImageFromPath(
    filesDir: File,
    setDir: File,
    context: Context,
    cuePath: String,
    binPaths: List<String>,
    includeAudio: Boolean,
): Int {
    val cueFile = File(cuePath)
    val imageFiles = binPaths.map(::File)

    if (!cueFile.isFile || imageFiles.isEmpty() || imageFiles.any { !it.isFile }) {
        Log.w(
            "DXX-DiscImport",
            "importDiscImageFromPath: missing cue/image files ($cuePath, ${binPaths.joinToString()})",
        )
        return -1
    }

    val orderedImages = orderCueEntries(cueFile, imageFiles) { it.name }
    if (orderedImages.missingNames.isNotEmpty()) {
        Log.w(
            "DXX-DiscImport",
            "importDiscImageFromPath: ${buildMissingDiscImageSelectionMessage(orderedImages.missingNames)}",
        )
        return -1
    }
    if (orderedImages.extraNames.isNotEmpty()) {
        Log.i(
            "DXX-DiscImport",
            "importDiscImageFromPath: ignoring extra image files for $cuePath: ${summarizeDiscImageNames(
                orderedImages.extraNames,
            )}",
        )
    }
    val orderedImageFiles = orderedImages.orderedEntries

    val tracks = DiscImportBridge.parseCue(cueFile.absolutePath, orderedImageFiles.map { it.length() }.toLongArray())
    if (tracks.isNullOrEmpty()) {
        Log.w("DXX-DiscImport", "importDiscImageFromPath: parseCue failed for $cuePath")
        return -1
    }

    val dataTrack = tracks.firstOrNull { it.isData }
    if (dataTrack == null) {
        Log.w("DXX-DiscImport", "importDiscImageFromPath: no data track for $cuePath")
        return -1
    }

    val dataImageFile = orderedImageFiles.getOrNull(dataTrack.fileIndex)
    if (dataImageFile == null) {
        Log.w(
            "DXX-DiscImport",
            "importDiscImageFromPath: missing data image for $cuePath (fileIndex=${dataTrack.fileIndex})",
        )
        return -1
    }

    val isoExtracted =
        DiscImportBridge.extractIsoFiles(
            dataImageFile.absolutePath,
            dataTrack.startSector,
            dataTrack.numSectors,
            setDir.absolutePath,
            null,
        )
    val macExtracted =
        if (isoExtracted > 0) {
            0
        } else {
            DiscImportBridge.extractMacFiles(
                dataImageFile.absolutePath,
                dataTrack.startSector,
                dataTrack.numSectors,
                setDir.absolutePath,
                null,
            )
        }

    var sowExtracted = 0
    if (isoExtracted > 0 || macExtracted > 0) {
        sowExtracted = postProcessImportedDiscFiles(setDir)
    }

    val extracted = if (isoExtracted > 0) isoExtracted else macExtracted.coerceAtLeast(0)
    if (includeAudio && extracted > 0 && tracks.any { it.isAudio }) {
        registerDiscAudioSourceFromPath(
            srcManager = AudioSourceManager(filesDir),
            filesDir = filesDir,
            context = context,
            cuePath = cueFile.absolutePath,
            binPaths = orderedImageFiles.map { it.absolutePath },
            tracks = tracks,
        )
        enableRedbookInConfig(filesDir, context)
    }

    Log.i(
        "DXX-DiscImport",
        "importDiscImageFromPath: cue=$cuePath images=${orderedImageFiles.size} iso=$isoExtracted mac=$macExtracted sow=$sowExtracted audio=$includeAudio",
    )
    return extracted + sowExtracted
}

internal fun importIsoImageFromPath(
    setDir: File,
    isoPath: String,
): Int {
    val isoFile = File(isoPath)

    if (!isoFile.isFile) {
        Log.w("DXX-DiscImport", "importIsoImageFromPath: missing iso ($isoPath)")
        return -1
    }

    val isoExtracted = DiscImportBridge.extractIsoImageFiles(isoFile.absolutePath, setDir.absolutePath, null)
    val sowExtracted = if (isoExtracted > 0) postProcessImportedDiscFiles(setDir) else 0

    Log.i(
        "DXX-DiscImport",
        "importIsoImageFromPath: iso=$isoPath files=$isoExtracted sow=$sowExtracted",
    )
    return if (isoExtracted < 0) isoExtracted else isoExtracted + sowExtracted
}
