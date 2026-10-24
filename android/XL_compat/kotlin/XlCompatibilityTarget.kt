package com.dxxredux.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

internal suspend fun xlCompatibilityAnalysisFailure(target: LevelMetadataTarget): LevelMetadataResult? {
    // Metadata analysis has no XL runtime/assets; feature eligibility is not analysis support
    val blockers = inspectXlCompatibilityTarget(target)
    if (blockers.isEmpty()) return null
    return LevelMetadataResult.failed(
        source = target.displayName,
        game = target.game,
        problem = "XL metadata analysis is unavailable. See runtime eligibility in the compatibility table.",
        diagnostics = blockers.map { "${it.source}: ${it.summary}" },
        failureKind = "unsupported_format",
    )
}

internal suspend fun inspectXlCompatibilityTarget(target: LevelMetadataTarget): List<XlCompatibilityReport> =
    withContext(Dispatchers.IO) {
        buildList {
            val archive = target.archivePath?.let(::File)
            if (archive?.isFile == true) {
                target.archiveEntries.distinct().forEach { path ->
                    if (GameFileFormats.extensionOf(path) in setOf("hog", "mn2", "rl2")) {
                        val summary =
                            runCatching {
                                GameFileMetadata.summarizeZipConstituent(
                                    archive,
                                    path,
                                    path.substringAfterLast('/'),
                                )
                            }.getOrNull()
                        addAll(summary?.xlCompatibility.orEmpty())
                    }
                }
            } else {
                val source = target.sourcePath?.let(::File)
                if (source?.isFile == true) {
                    addAll(
                        runCatching {
                            GameFileMetadata.summarizeLocalFile(
                                source,
                            )
                        }.getOrNull()?.xlCompatibility.orEmpty(),
                    )
                } else {
                    val root = source?.takeIf { it.isDirectory } ?: target.extraDataDir?.let(::File)
                    if (root != null) {
                        val names =
                            target.hogFiles +
                                listOfNotNull(target.hogFile, target.missionFilename, target.levelFile) +
                                target.normalLevelFiles +
                                target.secretLevelFiles
                        names.distinct().forEach { name ->
                            val file = File(root, name).canonicalFile
                            if (file.toPath().startsWith(root.canonicalFile.toPath()) && file.isFile) {
                                addAll(
                                    runCatching {
                                        GameFileMetadata.summarizeLocalFile(
                                            file,
                                        )
                                    }.getOrNull()?.xlCompatibility.orEmpty(),
                                )
                            }
                        }
                    }
                }
            }
        }.distinctBy { it.source }
    }
