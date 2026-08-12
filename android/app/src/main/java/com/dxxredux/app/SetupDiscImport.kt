package com.dxxredux.app

import android.content.Context
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.util.Log
import java.io.File
import java.io.IOException
import java.util.Locale

internal data class SetupImportSnapshot(
    val kind: String = "",
    val status: String = "idle",
    val resultCount: Int = 0,
    val error: String = "",
)

internal object SetupImportTracker {
    @Volatile
    private var current = SetupImportSnapshot()

    fun begin(kind: String) {
        current = SetupImportSnapshot(kind = kind, status = "running")
    }

    fun complete(
        kind: String,
        resultCount: Int,
    ) {
        current =
            if (resultCount >= 0) {
                SetupImportSnapshot(kind = kind, status = "complete", resultCount = resultCount)
            } else {
                SetupImportSnapshot(kind = kind, status = "failed", resultCount = resultCount, error = "extract_failed")
            }
    }

    fun fail(
        kind: String,
        error: String,
    ) {
        current = SetupImportSnapshot(kind = kind, status = "failed", resultCount = -1, error = error)
    }

    fun snapshot(): SetupImportSnapshot = current

    fun reset() {
        current = SetupImportSnapshot()
    }
}

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
    return findGogPair(files.asList())
}

internal fun findGogPair(files: Collection<String>): GogAudioPair? {
    if (hasAmbiguousGogAudioNames(files)) return null
    val gogByBase =
        files
            .filter { it.endsWith(".gog", ignoreCase = true) }
            .groupBy { it.substringBeforeLast('.').lowercase(Locale.US) }
    val instByBase =
        files
            .filter { it.endsWith(".inst", ignoreCase = true) }
            .groupBy { it.substringBeforeLast('.').lowercase(Locale.US) }
    val pairedBases = (gogByBase.keys intersect instByBase.keys)
    val base =
        pairedBases.firstOrNull { it == "descent_ii" } ?: pairedBases.sorted().firstOrNull() ?: return null
    val gogName = gogByBase[base]?.singleOrNull() ?: return null
    val instName = instByBase[base]?.singleOrNull() ?: return null
    return GogAudioPair(gogName.substringBeforeLast('.'), gogName, instName)
}

internal fun hasAmbiguousGogAudioNames(files: Collection<String>): Boolean =
    files
        .filter { it.endsWith(".gog", ignoreCase = true) || it.endsWith(".inst", ignoreCase = true) }
        .groupBy { it.lowercase(Locale.ROOT) }
        .values
        .any { it.size > 1 }

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
    val existingNames = setDir.list()?.asList() ?: return null
    if (hasAmbiguousGogAudioNames(existingNames)) return null
    ensureKnownGogCueFile(setDir)
    val pair = findGogPair(setDir) ?: return null
    val relDir = setDir.toRelativeString(filesDir)
    val relCue = if (relDir.isEmpty()) pair.instFileName else "$relDir${File.separator}${pair.instFileName}"
    val relBin = if (relDir.isEmpty()) pair.gogFileName else "$relDir${File.separator}${pair.gogFileName}"
    val trackSummary = summarizeCueTracks(File(setDir, pair.instFileName))
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
        trackCount = trackSummary.trackCount,
        audioTrackCount = trackSummary.audioTrackNumbers.size,
        audioTrackNumbers = trackSummary.audioTrackNumbers,
        legacyDiscId = if (sourceId == D2_GOG_AUDIO_SOURCE_ID) D2_GOG_LEGACY_DISC_ID else 0L,
        trackNames = trackNames,
    )
}

private fun ensureKnownGogCueFile(setDir: File) {
    val files = setDir.list() ?: return
    if (files.any { it.endsWith(".inst", ignoreCase = true) }) return

    val gogName = files.firstOrNull { it.equals("DESCENT_II.gog", ignoreCase = true) } ?: return
    val cueFile = File(setDir, "${File(gogName).nameWithoutExtension}.inst")
    AtomicFilePublication.writeUtf8(cueFile, "FILE \"$gogName\" BINARY\n$D2_GOG_CUE_TEXT")
}

private fun isKnownD2GogPair(pair: GogAudioPair): Boolean = pair.baseName.equals("DESCENT_II", ignoreCase = true)

private data class CueTrackSummary(
    val trackCount: Int,
    val audioTrackNumbers: List<Int>,
)

private fun summarizeCueTracks(cueFile: File): CueTrackSummary {
    var tracks = 0
    val audioTrackNumbers = mutableListOf<Int>()
    if (!cueFile.isFile) return CueTrackSummary(0, emptyList())
    cueFile.forEachLine { line ->
        val parts = line.trim().split(Regex("\\s+"), limit = 3)
        if (parts.size >= 3 && parts[0].equals("TRACK", ignoreCase = true)) {
            tracks++
            if (parts[2].equals("AUDIO", ignoreCase = true)) {
                parts[1].toIntOrNull()?.let(audioTrackNumbers::add)
            }
        }
    }
    return CueTrackSummary(tracks, audioTrackNumbers)
}

internal fun extractSowArchives(
    setDir: File,
    progress: DiscImportBridge.ExtractProgress? = null,
): Int {
    val sowFiles = DiscImportBridge.scanSowFiles(setDir.absolutePath) ?: return -1
    var sowExtracted = 0
    for (sow in sowFiles) {
        val outputDir = File(sow).parentFile?.absolutePath ?: setDir.absolutePath
        val extracted = DiscImportBridge.extractSowFiles(sow, outputDir, progress)
        if (extracted < 0) return extracted
        sowExtracted += extracted
    }
    return sowExtracted
}

internal fun postProcessImportedDiscFiles(
    setDir: File,
    progress: DiscImportBridge.ExtractProgress? = null,
): Int {
    val sowExtracted = extractSowArchives(setDir, progress)
    if (sowExtracted < 0) return sowExtracted
    if (hoistNestedImportedGameFiles(setDir) < 0) return -1
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
    if (!dest.exists() && source.renameTo(dest)) return true
    return try {
        LauncherFileCopy.copyFileToFile(source, dest)
        source.delete()
        true
    } catch (_: Exception) {
        false
    }
}

internal fun hoistNestedImportedGameFiles(setDir: File): Int {
    if (!setDir.isDirectory) return 0

    val candidates =
        setDir
            .walkTopDown()
            .filter { it.isFile && isDirectGameDataImportName(it.name.lowercase(Locale.US)) }
            .toList()
    if (candidates.groupBy { it.name.lowercase(Locale.US) }.values.any { it.size > 1 }) {
        runCatching { Log.w("DXX-DiscImport", "Refusing to hoist ambiguous case-folded game filenames") }
        return -1
    }
    var hoisted = 0

    candidates
        .filter { it.parentFile != setDir && it.length() > 1L }
        .forEach { file ->
            val dest = File(setDir, file.name)
            if (moveImportedGameFileToRoot(file, dest)) {
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
        val key = File(nameOf(entry)).name.lowercase(Locale.ROOT)
        entriesByName.getOrPut(key) { java.util.ArrayDeque() }.addLast(entry)
    }

    val orderedEntries = mutableListOf<T>()
    val missingNames = mutableListOf<String>()
    for (referencedName in referencedNames) {
        val queue = entriesByName[referencedName.lowercase(Locale.ROOT)]
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
            val trackOffset = firstAudio.startSector.toLong() * firstAudio.sectorSize
            val trackBytes = firstAudio.numSectors.toLong() * firstAudio.sectorSize
            val audioBinFile = orderedBinFiles.getOrNull(firstAudio.fileIndex)
            if (audioBinFile == null) {
                Log.w(
                    "DXX-DiscImport",
                    "Disc identification missing fileIndex=${firstAudio.fileIndex} for $cuePath",
                )
            } else {
                audioBinFile.inputStream().use { input ->
                    when (val hash = DiscIdentifier.sha1Hash(input, trackOffset, trackBytes)) {
                        is DiscIdentifier.Sha1HashResult.Complete -> {
                            val match = identifier.identify(mapOf(firstAudio.trackNum to hash.sha1))
                            if (match.matched) {
                                discLabel = match.label
                                discId = match.disc?.id
                                match.disc?.legacyDiscId?.let {
                                    legacyDiscId = java.lang.Long.decode(it)
                                }
                            }
                        }

                        is DiscIdentifier.Sha1HashResult.Failed -> {
                            Log.w("DXX-DiscImport", "Disc track hash failed: ${hash.problem}")
                        }

                        DiscIdentifier.Sha1HashResult.Canceled -> {
                            Log.w("DXX-DiscImport", "Disc track hash canceled")
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
            binPaths = orderedBinFiles.map { it.name.lowercase(Locale.ROOT) },
            discLabel = discLabel ?: File(cuePath).nameWithoutExtension,
            discId = discId ?: "unknown",
            trackCount = tracks.size,
            audioTrackCount = tracks.count { it.isAudio },
            audioTrackNumbers = tracks.filter { it.isAudio }.map { it.trackNum },
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
    val temporaryBin = AtomicFilePublication.uniqueSibling(destBin, "copy")
    val temporaryCue = AtomicFilePublication.uniqueSibling(destCue, "copy")

    ImportStorageGuard.requireFreeSpace(
        artifactDir,
        binSizes.sum(),
        "merged CD audio source",
    )

    try {
        java.io.FileOutputStream(temporaryBin).use { output ->
            binUris.forEachIndexed { index, (name, uri) ->
                onStatus("Copying CD audio file ${index + 1}/${binUris.size}: $name")
                val totalBytes = binSizes.getOrElse(index) { 0L }
                context.contentResolver.openInputStream(uri)?.use { input ->
                    LauncherFileCopy.copyStream(input, output, totalBytes, name)
                } ?: throw java.io.IOException("Could not open selected file: $name")
            }
            output.flush()
            output.fd.sync()
        }
        AtomicFilePublication.writeUtf8(temporaryCue, buildMergedCueText(destBin.name, mergedCueTracks))
        AtomicFilePublication.publishFiles(listOf(temporaryBin to destBin, temporaryCue to destCue))
        return StagedMergedSafDiscAudioSource(destCue, destBin, mergedCueTracks)
    } finally {
        temporaryBin.delete()
        temporaryCue.delete()
    }
}

internal data class CueDataTrackAttempt(
    val isoExtracted: Int,
    val macExtracted: Int,
)

internal data class CueDataTrackExtractionResult(
    val isoExtracted: Int = 0,
    val macExtracted: Int = 0,
    val sowExtracted: Int = 0,
    val processedTracks: Int = 0,
    val totalTracks: Int = 0,
    val failedTrackNumber: Int? = null,
    val cancelled: Boolean = false,
) {
    val succeeded: Boolean get() = failedTrackNumber == null && !cancelled
    val primaryExtracted: Int get() = isoExtracted + macExtracted
    val totalExtracted: Int get() = primaryExtracted + sowExtracted
}

internal fun cueDataTrackStorageBytes(tracks: List<DiscImportBridge.CueTrack>): Long =
    cueDataTrackPeakStorageBytes(tracks, 0L)

internal fun cueDataTrackPeakStorageBytes(
    tracks: List<DiscImportBridge.CueTrack>,
    stagedImageBytes: Long,
): Long {
    require(stagedImageBytes >= 0L) { "Invalid staged image size" }
    val extractedBytes =
        tracks
            .asSequence()
            .filter { it.isData }
            .fold(0L) { total, track ->
                require(track.numSectors > 0) { "Invalid data track length" }
                require(track.sectorSize == 2048 || track.sectorSize == 2352) { "Invalid data sector size" }
                Math.addExact(total, Math.multiplyExact(track.numSectors.toLong(), track.sectorSize.toLong()))
            }
    return Math.addExact(extractedBytes, stagedImageBytes)
}

internal class PreparedDiscImages(
    val stagedImageBytes: Long,
    private val sources: MutableMap<Int, ParcelFileDescriptor>,
) : AutoCloseable {
    fun openSeekable(
        context: Context,
        imageIndex: Int,
        image: Pair<String, Uri>,
    ): ParcelFileDescriptor {
        val source =
            sources.remove(imageIndex)
                ?: context.contentResolver.openFileDescriptor(image.second, "r")
                ?: throw IOException("Could not open ${image.first}")
        return SafDescriptorStager.openSeekable(source, context.cacheDir, image.first)
    }

    override fun close() {
        sources.values.forEach { runCatching { it.close() } }
        sources.clear()
    }
}

internal fun prepareDiscImages(
    context: Context,
    tracks: List<DiscImportBridge.CueTrack>,
    imageUris: List<Pair<String, Uri>>,
    imageSizes: List<Long>,
): PreparedDiscImages {
    var maximum = 0L
    val sources = mutableMapOf<Int, ParcelFileDescriptor>()
    try {
        tracks
            .asSequence()
            .filter { it.isData }
            .map { it.fileIndex }
            .distinct()
            .forEach { imageIndex ->
                val image = imageUris.getOrNull(imageIndex) ?: throw IOException("Missing disc image $imageIndex")
                val size = imageSizes.getOrNull(imageIndex) ?: throw IOException("Missing size for ${image.first}")
                val source =
                    context.contentResolver.openFileDescriptor(image.second, "r")
                        ?: throw IOException("Could not open ${image.first}")
                sources[imageIndex] = source
                if (SafDescriptorStager.needsStaging(source)) {
                    if (size <= 0L) throw IOException("Could not determine size of ${image.first}")
                    maximum = maxOf(maximum, size)
                }
            }
        return PreparedDiscImages(maximum, sources)
    } catch (failure: Exception) {
        sources.values.forEach { runCatching { it.close() } }
        throw failure
    }
}

internal fun openSeekableDiscImage(
    context: Context,
    image: Pair<String, Uri>,
): ParcelFileDescriptor {
    val source =
        context.contentResolver.openFileDescriptor(image.second, "r")
            ?: throw IOException("Could not open ${image.first}")
    return SafDescriptorStager.openSeekable(source, context.cacheDir, image.first)
}

internal fun extractPickedCueDataTracks(
    context: Context,
    setDir: File,
    tracks: List<DiscImportBridge.CueTrack>,
    orderedBinUris: List<Pair<String, Uri>>,
    preparedImages: PreparedDiscImages,
    postUpdate: (() -> Unit) -> Unit,
    onStatus: (String) -> Unit,
    onProgress: (Long, Long) -> Unit,
): CueDataTrackExtractionResult {
    val progress =
        object : DiscImportBridge.ExtractProgress {
            override fun onProgress(
                currentFile: String,
                bytesDone: Long,
                bytesTotal: Long,
            ): Int {
                val pct = if (bytesTotal > 0L) ((bytesDone * 100L) / bytesTotal).toInt() else 0
                postUpdate {
                    onStatus("Extracting $currentFile ($pct%)")
                    onProgress(bytesDone, bytesTotal)
                }
                return 0
            }
        }
    return extractCueDataTracks(
        setDir = setDir,
        tracks = tracks,
        imageCount = orderedBinUris.size,
        progress = progress,
        extractTrack = { track, outputDir, trackProgress ->
            val image = orderedBinUris.getOrNull(track.fileIndex)
            if (image == null) {
                CueDataTrackAttempt(-1, -1)
            } else {
                postUpdate { onStatus("Extracting data track ${track.trackNum}...") }
                preparedImages.openSeekable(context, track.fileIndex, image).use {
                    val iso =
                        DiscImportBridge.extractIsoFiles(
                            it.fd,
                            track.startSector,
                            track.numSectors,
                            track.sectorSize,
                            track.userDataOffset,
                            outputDir.absolutePath,
                            trackProgress,
                        )
                    val mac =
                        if (iso == DiscImportBridge.EXTRACT_CANCELLED) {
                            DiscImportBridge.EXTRACT_CANCELLED
                        } else if (iso > 0) {
                            0
                        } else if (track.sectorMode == DiscImportBridge.CUE_SECTOR_MODE1_2352) {
                            postUpdate { onStatus("Trying Mac HFS track ${track.trackNum}...") }
                            DiscImportBridge.extractMacFiles(
                                it.fd,
                                track.startSector,
                                track.numSectors,
                                outputDir.absolutePath,
                                trackProgress,
                            )
                        } else {
                            -1
                        }
                    CueDataTrackAttempt(iso, mac)
                }
            }
        },
    )
}

private class CueDataTrackProgress(
    private val tracks: List<DiscImportBridge.CueTrack>,
    private val delegate: DiscImportBridge.ExtractProgress?,
) {
    private val trackBytes =
        tracks.map { Math.multiplyExact(it.numSectors.toLong(), it.sectorSize.toLong()) }
    private val offsets =
        trackBytes.runningFold(0L) { total, bytes -> Math.addExact(total, bytes) }
    val totalBytes: Long = offsets.last()
    var cancelled = false
        private set

    fun forTrack(index: Int): DiscImportBridge.ExtractProgress? {
        val callback = delegate ?: return null
        return object : DiscImportBridge.ExtractProgress {
            override fun onProgress(
                currentFile: String,
                bytesDone: Long,
                bytesTotal: Long,
            ): Int {
                val localBytes = trackBytes[index]
                val scaled =
                    if (bytesTotal > 0L) {
                        (bytesDone.coerceIn(0L, bytesTotal).toDouble() / bytesTotal.toDouble() * localBytes)
                            .toLong()
                    } else {
                        0L
                    }
                val result = callback.onProgress(currentFile, offsets[index] + scaled, totalBytes)
                if (result != 0) cancelled = true
                return result
            }
        }
    }
}

internal fun extractCueDataTracks(
    setDir: File,
    tracks: List<DiscImportBridge.CueTrack>,
    imageCount: Int,
    progress: DiscImportBridge.ExtractProgress? = null,
    extractTrack: (
        track: DiscImportBridge.CueTrack,
        outputDir: File,
        progress: DiscImportBridge.ExtractProgress?,
    ) -> CueDataTrackAttempt,
    postProcess: (File, DiscImportBridge.ExtractProgress?) -> Int = ::postProcessImportedDiscFiles,
): CueDataTrackExtractionResult {
    val dataTracks = tracks.filter { it.isData }
    if (dataTracks.isEmpty()) {
        return CueDataTrackExtractionResult(totalTracks = 0, failedTrackNumber = 0)
    }
    val invalidTrack =
        dataTracks.firstOrNull {
            it.fileIndex !in 0 until imageCount ||
                it.startSector < 0 ||
                it.numSectors <= 0 ||
                when (it.sectorMode) {
                    DiscImportBridge.CUE_SECTOR_MODE1_2352 -> {
                        it.sectorSize != 2352 || it.userDataOffset != 16
                    }

                    DiscImportBridge.CUE_SECTOR_MODE1_2048 -> {
                        it.sectorSize != 2048 || it.userDataOffset != 0
                    }

                    DiscImportBridge.CUE_SECTOR_MODE2_2352 -> {
                        it.sectorSize != 2352 || it.userDataOffset != 24
                    }

                    else -> {
                        true
                    }
                }
        }
    if (invalidTrack != null) {
        return CueDataTrackExtractionResult(
            totalTracks = dataTracks.size,
            failedTrackNumber = invalidTrack.trackNum,
        )
    }

    val parent =
        setDir.parentFile
            ?: return CueDataTrackExtractionResult(
                totalTracks = dataTracks.size,
                failedTrackNumber = dataTracks.first().trackNum,
            )
    val stagingDir = File(parent, ".disc-import-${System.nanoTime()}")
    if (!stagingDir.mkdirs()) {
        return CueDataTrackExtractionResult(
            totalTracks = dataTracks.size,
            failedTrackNumber = dataTracks.first().trackNum,
        )
    }

    val aggregateProgress = CueDataTrackProgress(dataTracks, progress)
    var isoExtracted = 0
    var macExtracted = 0
    var processed = 0
    try {
        for ((index, track) in dataTracks.withIndex()) {
            val attempt = extractTrack(track, stagingDir, aggregateProgress.forTrack(index))
            if (aggregateProgress.cancelled) {
                return CueDataTrackExtractionResult(
                    isoExtracted = isoExtracted,
                    macExtracted = macExtracted,
                    processedTracks = processed,
                    totalTracks = dataTracks.size,
                    failedTrackNumber = track.trackNum,
                    cancelled = true,
                )
            }
            if (attempt.isoExtracted < 0 && attempt.macExtracted < 0) {
                return CueDataTrackExtractionResult(
                    isoExtracted = isoExtracted,
                    macExtracted = macExtracted,
                    processedTracks = processed,
                    totalTracks = dataTracks.size,
                    failedTrackNumber = track.trackNum,
                )
            }
            isoExtracted += attempt.isoExtracted.coerceAtLeast(0)
            macExtracted += attempt.macExtracted.coerceAtLeast(0)
            processed++
        }

        val primaryExtracted = isoExtracted + macExtracted
        val sowExtracted = if (primaryExtracted > 0) postProcess(stagingDir, progress) else 0
        if (sowExtracted < 0) {
            return CueDataTrackExtractionResult(
                isoExtracted = isoExtracted,
                macExtracted = macExtracted,
                sowExtracted = sowExtracted,
                processedTracks = processed,
                totalTracks = dataTracks.size,
                failedTrackNumber = dataTracks.last().trackNum,
            )
        }
        if (primaryExtracted > 0 || sowExtracted > 0) {
            if (!setDir.isDirectory && !setDir.mkdirs()) {
                return CueDataTrackExtractionResult(
                    isoExtracted = isoExtracted,
                    macExtracted = macExtracted,
                    sowExtracted = sowExtracted,
                    processedTracks = processed,
                    totalTracks = dataTracks.size,
                    failedTrackNumber = dataTracks.last().trackNum,
                )
            }
            publishStagedArchiveFiles(stagingDir, setDir)
        }
        return CueDataTrackExtractionResult(
            isoExtracted = isoExtracted,
            macExtracted = macExtracted,
            sowExtracted = sowExtracted,
            processedTracks = processed,
            totalTracks = dataTracks.size,
        )
    } finally {
        stagingDir.deleteRecursively()
    }
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

    val dataTracks = tracks.filter { it.isData }
    if (dataTracks.isEmpty()) {
        Log.w("DXX-DiscImport", "importDiscImageFromPath: no data track for $cuePath")
        return -1
    }

    ImportStorageGuard.requireFreeSpace(
        setDir,
        cueDataTrackStorageBytes(dataTracks),
        "extract disc game files",
    )
    val result =
        extractCueDataTracks(
            setDir = setDir,
            tracks = tracks,
            imageCount = orderedImageFiles.size,
            extractTrack = { track, outputDir, progress ->
                val image = orderedImageFiles[track.fileIndex]
                val iso =
                    DiscImportBridge.extractIsoFiles(
                        image.absolutePath,
                        track.startSector,
                        track.numSectors,
                        track.sectorSize,
                        track.userDataOffset,
                        outputDir.absolutePath,
                        progress,
                    )
                val mac =
                    if (iso == DiscImportBridge.EXTRACT_CANCELLED) {
                        DiscImportBridge.EXTRACT_CANCELLED
                    } else if (iso > 0) {
                        0
                    } else if (track.sectorMode == DiscImportBridge.CUE_SECTOR_MODE1_2352) {
                        DiscImportBridge.extractMacFiles(
                            image.absolutePath,
                            track.startSector,
                            track.numSectors,
                            outputDir.absolutePath,
                            progress,
                        )
                    } else {
                        -1
                    }
                CueDataTrackAttempt(iso, mac)
            },
        )
    if (!result.succeeded) {
        Log.w(
            "DXX-DiscImport",
            "importDiscImageFromPath: data track ${result.failedTrackNumber} failed for $cuePath",
        )
        return -1
    }

    if (includeAudio && result.primaryExtracted > 0 && tracks.any { it.isAudio }) {
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
        "importDiscImageFromPath: cue=$cuePath images=${orderedImageFiles.size} data_tracks=${result.processedTracks} iso=${result.isoExtracted} mac=${result.macExtracted} sow=${result.sowExtracted} audio=$includeAudio",
    )
    return result.totalExtracted
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
    return if (isoExtracted < 0 || sowExtracted < 0) -1 else isoExtracted + sowExtracted
}
