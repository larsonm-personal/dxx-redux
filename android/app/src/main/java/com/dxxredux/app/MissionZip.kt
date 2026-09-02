package com.dxxredux.app

import java.io.File
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.charset.CharacterCodingException
import java.nio.charset.Charset
import java.nio.charset.CodingErrorAction
import java.util.Locale
import java.util.zip.ZipInputStream
import javax.xml.parsers.DocumentBuilderFactory

object MissionZip {
    const val KIND = "mission_zip"
    const val CATEGORY_LEVELS = "levels"
    const val DURABLE_EXTRACT_THRESHOLD_BYTES = 10L * 1024L * 1024L
    const val SMALL_IN_MEMORY_LIMIT_BYTES = 100L * 1024L * 1024L
    const val SMALL_NESTED_ARCHIVE_LIMIT_BYTES = 32L * 1024L * 1024L
    private val INLINE_README_EXTENSIONS = setOf("txt", "docx")
    private val EXTERNAL_README_EXTENSIONS = setOf("pdf", "rtf", "doc")
    private const val MAX_DOCX_XML_BYTES = 4L * 1024L * 1024L
    private const val MAX_DOCX_TOTAL_BYTES = 16L * 1024L * 1024L

    const val UNSUPPORTED_D2XXL_HOG_MESSAGE =
        "This level pack uses the D2X-XL extended HOG format, which DXX Redux does not currently support"

    class UnsupportedD2xxlHogException : IllegalArgumentException(UNSUPPORTED_D2XXL_HOG_MESSAGE)

    data class Constituent(
        val path: String,
        val name: String,
        val role: String,
        val sizeBytes: Long,
        val compressedSizeBytes: Long?,
        val archiveEntries: Set<String>? = null,
    )

    data class ScanResult(
        val constituents: List<Constituent>,
        val mission: GameFileFormats.MissionDescriptor,
        val missionSets: List<MissionSet>,
        val variantSelection: MissionVariantSelection? = null,
        val game: String,
        val category: String = CATEGORY_LEVELS,
        val totalSizeBytes: Long,
        val importMode: String,
        val readmes: List<Constituent> = emptyList(),
        val archiveFormat: String = "zip",
    ) {
        val readme: Constituent? get() = readmes.firstOrNull()

        val effectiveMissionSets: List<MissionSet>
            get() {
                val excludedPaths =
                    variantSelection
                        ?.excluded
                        ?.map { it.missionPath.lowercase(Locale.US) }
                        ?.toSet()
                        ?: return missionSets
                return missionSets.filterNot { it.mission.path.lowercase(Locale.US) in excludedPaths }
            }
    }

    data class MissionSet(
        val mission: GameFileFormats.MissionDescriptor,
        val constituents: List<Constituent>,
    )

    data class MissionVariant(
        val label: String,
        val missionPath: String,
    )

    data class MissionVariantSelection(
        val selected: MissionVariant,
        val excluded: List<MissionVariant>,
    )

    data class TextFileContent(
        val text: String,
        val truncated: Boolean,
        val problem: String? = null,
    )

    fun inspect(file: File): ScanResult? {
        if (!file.isFile) return null
        ArchiveFiles.open(file).use { archive ->
            val budget = ExtractionBudget()
            val metadataBudget = descriptorBudget()
            val constituents = mutableListOf<Constituent>()
            val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
            for (entry in archive.entries) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.sizeBytes,
                    if (entry.isDirectory) 0 else entry.compressedSizeBytes ?: 0,
                    entry.path,
                )
                if (entry.isDirectory) continue
                val name = leafName(entry.path)
                val role = GameFileFormats.missionZipRoleForFile(name)
                val archiveEntries =
                    if (isMissionArchiveRole(role)) {
                        archive.openInputStream(entry).use { MissionAssetCatalog.read(role, it, entry.path) }
                    } else {
                        null
                    }
                constituents +=
                    Constituent(
                        path = normalizePath(entry.path),
                        name = name,
                        role = role,
                        sizeBytes = entry.sizeBytes,
                        compressedSizeBytes = entry.compressedSizeBytes,
                        archiveEntries = archiveEntries,
                    )
                if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                    val bytes =
                        archive.openInputStream(entry).use {
                            it
                                .readBytesBounded(
                                    ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                    entry.path,
                                    metadataBudget,
                                    entry.compressedSizeBytes ?: -1,
                                    expectedSizeBytes = entry.sizeBytes,
                                )
                        }
                    missions += parseMissionDescriptor(entry.path, bytes)
                }
            }
            return buildResult(
                constituents,
                missions,
                file.length(),
                file.name.substringBeforeLast('.'),
                archive.format,
            )
        }
    }

    fun inspect(input: InputStream): ScanResult? {
        val budget = ExtractionBudget()
        val metadataBudget = descriptorBudget()
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory) {
                    val name = leafName(entry.name)
                    val role = GameFileFormats.missionZipRoleForFile(name)
                    val archiveEntries =
                        if (isMissionArchiveRole(role)) MissionAssetCatalog.read(role, zip, entry.name) else null
                    val size = entry.size.coerceAtLeast(0)
                    constituents +=
                        Constituent(
                            path = normalizePath(entry.name),
                            name = name,
                            role = role,
                            sizeBytes = size,
                            compressedSizeBytes = entry.compressedSize.takeIf { it >= 0 },
                            archiveEntries = archiveEntries,
                        )
                    if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                        missions +=
                            parseMissionDescriptor(
                                entry.name,
                                zip.readBytesBounded(
                                    ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                    entry.name,
                                    metadataBudget,
                                    entry.compressedSize,
                                    expectedSizeBytes = entry.size,
                                ),
                            )
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return buildResult(
            constituents,
            missions,
            constituents.sumOf { it.sizeBytes },
            zipStem = null,
            archiveFormat = "zip",
        )
    }

    internal fun inspectExtracted(record: MissionZipExtractionRecord): ScanResult? {
        val budget = ExtractionBudget()
        val metadataBudget = descriptorBudget()
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        for (file in record.files) {
            val entryPath = normalizePath(file.entryPath).takeIf { it.isNotBlank() } ?: continue
            val name = leafName(entryPath)
            val role = GameFileFormats.missionZipRoleForFile(name)
            budget.registerEntry(file.sizeBytes, file.sizeBytes, entryPath)
            val diskFile = File(record.rootDir, file.relativePath.replace('/', File.separatorChar))
            val archiveEntries =
                if (diskFile.isFile && isMissionArchiveRole(role)) {
                    diskFile.inputStream().use { MissionAssetCatalog.read(role, it, entryPath) }
                } else {
                    null
                }
            constituents +=
                Constituent(
                    path = entryPath,
                    name = name,
                    role = role,
                    sizeBytes = file.sizeBytes,
                    compressedSizeBytes = null,
                    archiveEntries = archiveEntries,
                )
            if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                if (diskFile.isFile) {
                    missions +=
                        diskFile.inputStream().use {
                            parseMissionDescriptor(
                                entryPath,
                                it.readBytesBounded(
                                    ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                    entryPath,
                                    metadataBudget,
                                    file.sizeBytes,
                                    expectedSizeBytes = file.sizeBytes,
                                ),
                            )
                        }
                }
            }
        }
        return buildResult(
            constituents,
            missions,
            record.ownerSizeBytes,
            record.ownerFilename.substringBeforeLast('.'),
            record.archiveFormat.ifBlank { "zip" },
        )?.copy(importMode = record.importMode.ifBlank { "extracted_bundle" })
    }

    fun isImportCandidate(
        input: InputStream,
        stagingDirectory: File? = null,
        maxSourceBytes: Long = ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES,
    ): Boolean {
        val budget = ExtractionBudget()
        val metadataBudget = descriptorBudget()
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        val nestedVariantChildren = mutableListOf<String>()
        openZipInputStreamSkippingPreamble(input, stagingDirectory, maxSourceBytes).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory) {
                    val name = leafName(entry.name)
                    val role = GameFileFormats.missionZipRoleForFile(name)
                    val archiveEntries =
                        if (isMissionArchiveRole(role)) MissionAssetCatalog.read(role, zip, entry.name) else null
                    constituents +=
                        Constituent(
                            path = normalizePath(entry.name),
                            name = name,
                            role = role,
                            sizeBytes = entry.size.coerceAtLeast(0),
                            compressedSizeBytes = entry.compressedSize.takeIf { it >= 0 },
                            archiveEntries = archiveEntries,
                        )
                    if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                        missions +=
                            parseMissionDescriptor(
                                entry.name,
                                zip.readBytesBounded(
                                    ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                    entry.name,
                                    metadataBudget,
                                    entry.compressedSize,
                                    expectedSizeBytes = entry.size,
                                ),
                            )
                    }
                    if (GameFileFormats.extensionOf(name) == "zip" &&
                        missionVariantForArchiveFilename(name) != null
                    ) {
                        nestedVariantChildren += name
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        val nestedChoice =
            selectPreferredMissionVariant(nestedVariantChildren, ::missionVariantForArchiveFilename)
        return nestedChoice?.preference?.supportedByRedux == true ||
            playableMissionSets(sortedConstituents(constituents), missions).isNotEmpty()
    }

    fun containsUnsupportedD2xxlHog(file: File): Boolean {
        if (!file.isFile) return false
        ArchiveFiles.open(file).use { archive ->
            val budget = ExtractionBudget()
            for (entry in archive.entries) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.sizeBytes,
                    if (entry.isDirectory) 0 else entry.compressedSizeBytes ?: 0,
                    entry.path,
                )
                if (!entry.isDirectory && isMissionHogName(entry.path)) {
                    if (archive.openInputStream(entry).use(::hasD2xxlHogSignature)) return true
                }
            }
        }
        return false
    }

    fun containsUnsupportedD2xxlHog(
        input: InputStream,
        stagingDirectory: File? = null,
        maxSourceBytes: Long = ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES,
    ): Boolean {
        val budget = ExtractionBudget()
        openZipInputStreamSkippingPreamble(input, stagingDirectory, maxSourceBytes).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory && isMissionHogName(entry.name) && hasD2xxlHogSignature(zip)) return true
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return false
    }

    private fun descriptorBudget() =
        ExtractionBudget(
            maxEntryBytes = ExtractionLimits.MAX_DESCRIPTOR_BYTES,
            maxTotalBytes = ExtractionLimits.MAX_METADATA_BYTES,
        )

    fun readTextFile(
        file: File,
        path: String,
        maxBytes: Long = 1024L * 1024L,
    ): TextFileContent {
        if (!file.isFile) return TextFileContent("", truncated = false, problem = "Mission ZIP is missing")
        return try {
            ArchiveFiles.open(file).use { archive ->
                val entry = archive.findEntry(path) ?: return TextFileContent("", false, "Text file is missing")
                if (!isInlineReadmeCandidate(entry.path)) {
                    return TextFileContent("", false, "Only .txt and .docx files can be viewed in the launcher")
                }
                val requestedLimit =
                    if (GameFileFormats.extensionOf(entry.path) == "docx") MAX_DOCX_TOTAL_BYTES else maxBytes
                val limit = requestedLimit.coerceAtLeast(1L).coerceAtMost((Int.MAX_VALUE - 1).toLong()).toInt()
                val bytes =
                    archive.openInputStream(entry).use { input ->
                        val buffer = ByteArray(limit + 1)
                        var total = 0
                        while (total < buffer.size) {
                            val read = input.read(buffer, total, buffer.size - total)
                            if (read <= 0) break
                            total += read
                        }
                        buffer.copyOf(total)
                    }
                decodeInlineDocument(entry.path, bytes, limit)
            }
        } catch (e: Exception) {
            TextFileContent("", truncated = false, problem = e.message ?: e.javaClass.simpleName)
        }
    }

    fun readExtractedDocument(
        file: File,
        path: String,
        maxBytes: Long = 1024L * 1024L,
    ): TextFileContent {
        if (!isInlineReadmeCandidate(path)) {
            return TextFileContent("", false, "Only .txt and .docx files can be viewed in the launcher")
        }
        if (!file.isFile) return TextFileContent("", truncated = false, problem = "Document file is missing")
        return try {
            val requestedLimit = if (GameFileFormats.extensionOf(path) == "docx") MAX_DOCX_TOTAL_BYTES else maxBytes
            val limit = requestedLimit.coerceAtLeast(1L).coerceAtMost((Int.MAX_VALUE - 1).toLong()).toInt()
            file.inputStream().use { input ->
                val bytes = input.readBytesBounded(limit.toLong(), path)
                decodeInlineDocument(path, bytes, limit)
            }
        } catch (e: Exception) {
            TextFileContent("", truncated = false, problem = e.message ?: e.javaClass.simpleName)
        }
    }

    fun isInlineReadmeCandidate(path: String): Boolean = GameFileFormats.extensionOf(path) in INLINE_README_EXTENSIONS

    fun isExternalReadmeCandidate(path: String): Boolean =
        GameFileFormats.extensionOf(path) in EXTERNAL_README_EXTENSIONS

    fun isReadmeCandidate(path: String): Boolean = isInlineReadmeCandidate(path) || isExternalReadmeCandidate(path)

    fun externalViewMimeType(path: String): String =
        when (GameFileFormats.extensionOf(path)) {
            "pdf" -> "application/pdf"
            "rtf" -> "application/rtf"
            "doc" -> "application/msword"
            "docx" -> "application/vnd.openxmlformats-officedocument.wordprocessingml.document"
            "txt" -> "text/plain"
            else -> "application/octet-stream"
        }

    fun parseMissionDescriptor(
        path: String,
        text: String,
    ): GameFileFormats.MissionDescriptor = GameFileFormats.parseMissionDescriptor(path, text)

    internal fun parseMissionDescriptor(
        path: String,
        bytes: ByteArray,
    ): GameFileFormats.MissionDescriptor = GameFileFormats.parseMissionDescriptor(path, MissionDescriptorPolicy.decode(bytes))

    private fun buildResult(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
        totalSizeBytes: Long,
        zipStem: String?,
        archiveFormat: String,
    ): ScanResult? {
        val sortedConstituents = sortedConstituents(constituents)
        val missionSets = playableMissionSets(sortedConstituents, missions)
        if (missionSets.isEmpty()) return null
        val variantSelection = selectMissionVariant(missionSets)
        val mission =
            variantSelection
                ?.selected
                ?.missionPath
                ?.let { path -> missionSets.firstOrNull { it.mission.path.equals(path, ignoreCase = true) } }
                ?.mission
                ?: missionSets.first().mission
        val game =
            missionSets
                .map { it.mission }
                .map { it.game }
                .distinct()
                .singleOrNull()
                ?: "both"
        return ScanResult(
            constituents = sortedConstituents,
            mission = mission,
            missionSets = missionSets,
            variantSelection = variantSelection,
            game = game,
            totalSizeBytes = totalSizeBytes,
            importMode =
                if (shouldStoreArchive(
                        archiveFormat,
                        totalSizeBytes,
                        sortedConstituents,
                    )
                ) {
                    "stored_zip"
                } else {
                    "extracted_bundle"
                },
            readmes = chooseReadmes(sortedConstituents, zipStem),
            archiveFormat = archiveFormat,
        )
    }

    private fun shouldStoreArchive(
        archiveFormat: String,
        totalSizeBytes: Long,
        constituents: List<Constituent>,
    ): Boolean {
        if (archiveFormat != "zip") return false
        if (totalSizeBytes > DURABLE_EXTRACT_THRESHOLD_BYTES) return false
        return constituents.none {
            it.role in
                setOf(
                    GameFileFormats.MISSION_ZIP_HOG,
                    GameFileFormats.MISSION_ZIP_MOD_ARCHIVE,
                ) &&
                it.sizeBytes > SMALL_NESTED_ARCHIVE_LIMIT_BYTES
        }
    }

    private fun playableMissionSets(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
    ): List<MissionSet> =
        missions
            .filter { it.valid }
            .sortedWith(
                compareBy<GameFileFormats.MissionDescriptor> { it.path.lowercase(Locale.US) }
                    .thenBy { it.path },
            ).mapNotNull { mission ->
                val stem = leafName(mission.path).substringBeforeLast('.').lowercase(Locale.US)
                val missionDir = parentPath(mission.path)
                val requiredLevelNames =
                    mission.levelNames
                        .map { leafName(it).lowercase(Locale.US) }
                        .toSet()
                val referencedLevelNames =
                    (mission.levelNames + mission.secretLevelNames)
                        .map { leafName(it).lowercase(Locale.US) }
                        .toSet()
                val inMissionDirectory =
                    constituents.filter { constituent ->
                        constituent.path.equals(mission.path, ignoreCase = true) ||
                            (
                                parentPath(constituent.path).equals(missionDir, ignoreCase = true) &&
                                    isMissionPayload(constituent, stem, referencedLevelNames)
                            )
                    }
                val directLevels =
                    inMissionDirectory
                        .filter { it.name.lowercase(Locale.US) in referencedLevelNames }
                        .map { it.name.lowercase(Locale.US) }
                        .toSet()
                val archivedLevels =
                    inMissionDirectory
                        .filter(::isMissionArchive)
                        .flatMap { it.archiveEntries.orEmpty() }
                        .toSet()
                // The paired engines admit missions with a missing optional secret
                // level and report the failure only if that secret is selected.
                // Every ordinary level must still be present before the launcher
                // advertises the set as playable.
                if (!(directLevels + archivedLevels).containsAll(requiredLevelNames)) return@mapNotNull null
                MissionSet(mission, inMissionDirectory)
            }

    private data class VariantMissionSet(
        val missionSet: MissionSet,
        val preference: MissionVariantPreference?,
        val rootPath: String,
    )

    private fun selectMissionVariant(missionSets: List<MissionSet>): MissionVariantSelection? {
        val variants =
            missionSets.map { missionSet ->
                val variantDir = parentPath(missionSet.mission.path)
                val preference = missionVariantForDirectoryName(leafName(variantDir))
                VariantMissionSet(missionSet, preference, parentPath(variantDir))
            }
        val selections =
            variants
                .groupBy { variant ->
                    "${variant.rootPath.lowercase(
                        Locale.US,
                    )}\u0000${leafName(variant.missionSet.mission.path).lowercase(Locale.US)}"
                }.values
                .filter { group -> group.all { it.preference != null } }
                .mapNotNull(::selectEquivalentMissionVariantGroup)
        val selection = selections.singleOrNull() ?: return null
        val selectedRoot =
            variants
                .single {
                    it.missionSet.mission.path
                        .equals(selection.selected.missionPath, ignoreCase = true)
                }.rootPath
        val auxiliaryTests =
            missionSets.mapNotNull { missionSet ->
                val missionDir = parentPath(missionSet.mission.path)
                if (!leafName(missionDir).equals("TEST", ignoreCase = true) ||
                    !parentPath(missionDir).equals(selectedRoot, ignoreCase = true) ||
                    !hasPairedMissionArchive(missionSet)
                ) {
                    return@mapNotNull null
                }
                MissionVariant("Test", missionSet.mission.path)
            }
        return selection.copy(excluded = selection.excluded + auxiliaryTests)
    }

    private fun selectEquivalentMissionVariantGroup(variants: List<VariantMissionSet>): MissionVariantSelection? {
        val preferences = variants.map { it.preference }
        if (preferences.size < 2 || preferences.size != preferences.distinct().size) return null
        if (variants.any { !hasPairedMissionArchive(it.missionSet) }) return null
        val identity = missionVariantIdentity(variants.first().missionSet.mission)
        if (variants.drop(1).any { missionVariantIdentity(it.missionSet.mission) != identity }) return null
        val choice = selectPreferredMissionVariant(variants) { it.preference } ?: return null
        val selected = choice.value
        return MissionVariantSelection(
            selected = MissionVariant(choice.preference.displayName, selected.missionSet.mission.path),
            excluded =
                variants
                    .filterNot { it === selected }
                    .map { variant ->
                        MissionVariant(requireNotNull(variant.preference).displayName, variant.missionSet.mission.path)
                    },
        )
    }

    private fun hasPairedMissionArchive(missionSet: MissionSet): Boolean {
        val mission = missionSet.mission
        val missionDir = parentPath(mission.path)
        val missionStem = leafName(mission.path).substringBeforeLast('.')
        return missionSet.constituents.any { constituent ->
            parentPath(constituent.path).equals(missionDir, ignoreCase = true) &&
                constituent.name.substringBeforeLast('.').equals(missionStem, ignoreCase = true) &&
                isMissionArchive(constituent)
        }
    }

    private fun missionVariantIdentity(mission: GameFileFormats.MissionDescriptor): List<Any?> =
        listOf(
            mission.game.lowercase(Locale.US),
            mission.type?.trim()?.lowercase(Locale.US),
            mission.displayName.trim().lowercase(Locale.US),
            mission.levelNames.map(::normalizedMissionReference),
            mission.secretLevelNames.map(::normalizedMissionReference),
            mission.secretLevelOrigins,
        )

    private fun normalizedMissionReference(path: String): String = normalizePath(path).trim().lowercase(Locale.US)

    private fun isMissionPayload(
        constituent: Constituent,
        missionStem: String,
        levelNames: Set<String>,
    ): Boolean =
        constituent.name.lowercase(Locale.US) in levelNames ||
            (
                constituent.name.substringBeforeLast('.').lowercase(Locale.US) == missionStem &&
                    isMissionArchive(constituent)
            )

    private fun isMissionArchive(constituent: Constituent): Boolean =
        isMissionArchiveRole(constituent.role) && constituent.archiveEntries != null

    private fun isMissionArchiveRole(role: String): Boolean =
        role == GameFileFormats.MISSION_ZIP_HOG || role == GameFileFormats.MISSION_ZIP_MOD_ARCHIVE

    private fun isMissionHog(constituent: Constituent): Boolean = constituent.role == GameFileFormats.MISSION_ZIP_HOG

    private fun isMissionHogName(path: String): Boolean =
        GameFileFormats.missionZipRoleForFile(leafName(path)) == GameFileFormats.MISSION_ZIP_HOG

    private fun hasD2xxlHogSignature(input: InputStream): Boolean {
        val signature = ByteArray(3)
        var offset = 0
        while (offset < signature.size) {
            val read = input.read(signature, offset, signature.size - offset)
            if (read < 0) return false
            if (read == 0) continue
            offset += read
        }
        return signature.toString(Charsets.US_ASCII) == "D2X"
    }

    private fun parentPath(path: String): String = normalizePath(path).substringBeforeLast('/', "")

    private fun sortedConstituents(constituents: List<Constituent>): List<Constituent> =
        constituents.sortedWith(
            compareBy<Constituent> { readmeSortGroup(it.name) }
                .thenBy { it.path.lowercase(Locale.US) },
        )

    private fun chooseReadme(
        constituents: List<Constituent>,
        zipStem: String?,
    ): Constituent? {
        chooseReadmeFromCandidates(constituents.filter { isInlineReadmeCandidate(it.name) }, zipStem)?.let {
            return it
        }
        return chooseReadmeFromCandidates(constituents.filter { isExternalReadmeCandidate(it.name) }, zipStem)
    }

    private fun chooseReadmes(
        constituents: List<Constituent>,
        zipStem: String?,
    ): List<Constituent> {
        val candidates = constituents.filter { isReadmeCandidate(it.name) }
        val preferred = chooseReadme(candidates, zipStem) ?: return emptyList()
        return listOf(preferred) + candidates.filterNot { it.path.equals(preferred.path, ignoreCase = true) }
    }

    private fun chooseReadmeFromCandidates(
        candidates: List<Constituent>,
        zipStem: String?,
    ): Constituent? {
        if (candidates.size <= 1) return candidates.firstOrNull()
        candidates.firstOrNull { isReadmeNamed(it.name) }?.let { return it }
        val zipPrefix = zipStem?.takeIf { it.isNotBlank() }?.lowercase(Locale.US)
        if (zipPrefix != null) {
            candidates.firstOrNull { it.name.lowercase(Locale.US).startsWith(zipPrefix) }?.let { return it }
        }
        return candidates.sortedWith(compareByDescending<Constituent> { it.sizeBytes }.thenBy { it.path }).firstOrNull()
    }

    private fun readmeSortGroup(path: String): Int =
        when {
            isInlineReadmeCandidate(path) -> 0
            isExternalReadmeCandidate(path) -> 1
            else -> 2
        }

    private fun isReadmeNamed(path: String): Boolean =
        leafName(path).substringBeforeLast('.').equals("README", ignoreCase = true) && isReadmeCandidate(path)

    private fun readDocxText(bytes: ByteArray): TextFileContent {
        val budget =
            ExtractionBudget(
                maxEntryBytes = MAX_DOCX_XML_BYTES,
                maxTotalBytes = MAX_DOCX_TOTAL_BYTES,
                maxEntries = 256,
            )
        ZipInputStream(bytes.inputStream().buffered()).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                val path = normalizePath(entry.name)
                budget.registerEntry(entry.size, entry.compressedSize, path)
                if (!entry.isDirectory) {
                    val content =
                        zip.readBytesBounded(
                            MAX_DOCX_XML_BYTES,
                            path,
                            budget,
                            entry.compressedSize,
                            entry.size,
                        )
                    if (path.equals("word/document.xml", ignoreCase = true)) {
                        return TextFileContent(parseDocxDocumentXml(content), truncated = false)
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return TextFileContent("", truncated = false, problem = "Word document content is missing")
    }

    private fun decodeInlineDocument(
        path: String,
        bytes: ByteArray,
        limit: Int,
    ): TextFileContent =
        if (GameFileFormats.extensionOf(path) == "docx") {
            if (bytes.size > limit) {
                TextFileContent("", truncated = true, problem = "Word document exceeds $limit bytes")
            } else {
                readDocxText(bytes)
            }
        } else {
            val truncated = bytes.size > limit
            TextFileContent(decodeLegacyText(bytes.copyOf(minOf(bytes.size, limit)), truncated), truncated)
        }

    private fun parseDocxDocumentXml(bytes: ByteArray): String {
        val factory = DocumentBuilderFactory.newInstance()
        factory.isNamespaceAware = true
        factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true)
        factory.setFeature("http://xml.org/sax/features/external-general-entities", false)
        factory.setFeature("http://xml.org/sax/features/external-parameter-entities", false)
        factory.setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd", false)
        val document = factory.newDocumentBuilder().parse(bytes.inputStream())
        val paragraphs = document.getElementsByTagNameNS("*", "p")
        return buildString {
            for (paragraphIndex in 0 until paragraphs.length) {
                val children = paragraphs.item(paragraphIndex).childNodes
                appendDocxText(children)
                if (isNotEmpty() && last() != '\n') append('\n')
            }
        }.trimEnd()
    }

    private fun StringBuilder.appendDocxText(nodes: org.w3c.dom.NodeList) {
        for (index in 0 until nodes.length) {
            val node = nodes.item(index)
            when (node.localName) {
                "t" -> append(node.textContent)
                "tab" -> append('\t')
                "br", "cr" -> append('\n')
                else -> appendDocxText(node.childNodes)
            }
        }
    }

    internal fun decodeLegacyText(
        bytes: ByteArray,
        truncated: Boolean = false,
    ): String {
        decodeStrictUtf8(bytes)?.let { return it }
        if (truncated) {
            val incompleteBytes = incompleteUtf8SuffixLength(bytes)
            if (incompleteBytes > 0) {
                decodeStrictUtf8(bytes.copyOf(bytes.size - incompleteBytes))?.let { return it }
            }
        }
        return bytes.toString(Charset.forName("windows-1252"))
    }

    private fun decodeStrictUtf8(bytes: ByteArray): String? =
        try {
            Charsets.UTF_8
                .newDecoder()
                .onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT)
                .decode(ByteBuffer.wrap(bytes))
                .toString()
        } catch (_: CharacterCodingException) {
            null
        }

    private fun incompleteUtf8SuffixLength(bytes: ByteArray): Int {
        if (bytes.isEmpty()) return 0
        var leadIndex = bytes.lastIndex
        while (leadIndex >= 0 && bytes[leadIndex].toInt() and 0xc0 == 0x80) leadIndex--
        if (leadIndex < 0) return 0
        val lead = bytes[leadIndex].toInt() and 0xff
        val expectedContinuationBytes =
            when (lead) {
                in 0xc2..0xdf -> 1
                in 0xe0..0xef -> 2
                in 0xf0..0xf4 -> 3
                else -> return 0
            }
        val actualContinuationBytes = bytes.lastIndex - leadIndex
        return if (actualContinuationBytes < expectedContinuationBytes) bytes.size - leadIndex else 0
    }

    private fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')
}
