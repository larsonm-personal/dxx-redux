package com.dxxredux.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.util.Locale

internal data class BaseGameAsset(
    val name: String,
    val sha256: String?,
    val size: Long,
    val modified: Long,
    val sourceUri: String? = null,
)

internal data class BaseGameAssetAudit(
    val game: String,
    val assets: List<BaseGameAsset>,
    val reasons: List<String>,
) {
    val title: String get() = if (game == "d1") "Descent 1" else "Descent 2"
}

internal fun baseGameAssetNames(game: String): Set<String> =
    (if (game == "d1") D1_FILES else D2_FILES + D2_DEMO_FILES + D2_PARTIAL_FILES)
        .flatMap { listOf(it.filename) + it.alternatives }
        .map(::portableGameFilenameIdentity)
        .toSet()

internal fun classifyBaseGameAssets(
    game: String,
    assets: List<BaseGameAsset>,
    packages: (String, String) -> Set<String> = KnownVersions::matchingVersions,
): BaseGameAssetAudit {
    val reasons = mutableListOf<String>()
    val groups = assets.groupBy { portableGameFilenameIdentity(it.name) }
    for ((name, variants) in groups) {
        if (variants.size > 1 && variants.map { it.sha256 }.distinct().size > 1) {
            reasons += "$name has conflicting copies"
        }
    }
    if (game == "d2" &&
        groups.keys.any { it in setOf("d2demo.hog", "d2demo.ham", "d2demo.pig") } &&
        groups.keys.any { it in setOf("descent2.hog", "descent2.ham", "groupa.pig") }
    ) {
        reasons += "Demo and retail game files are installed together"
    }
    // HAM and sound labels describe their byte revision, not every distribution
    // that ships them: Mac D2 legitimately shares the PC v1.2 HAM and sounds
    // Use known level/texture identities as edition evidence, not those labels
    val known =
        assets
            .filter {
                it.name.substringAfterLast('.').lowercase(Locale.ROOT) in setOf("hog", "pig")
            }.mapNotNull { asset ->
                asset.sha256?.let { packages(asset.name, it).takeIf { candidates -> candidates.isNotEmpty() } }
            }
    if (known.size > 1 && known.reduce { common, candidates -> common intersect candidates }.isEmpty()) {
        reasons += "Known level and texture files do not belong to a single release"
    }
    return BaseGameAssetAudit(game, assets, reasons)
}

internal suspend fun auditBaseGameAssets(
    setDir: File,
    game: String,
): BaseGameAssetAudit =
    withContext(Dispatchers.IO) {
        val names = baseGameAssetNames(game)
        val local =
            setDir
                .listFiles()
                .orEmpty()
                .filter { it.isFile && portableGameFilenameIdentity(it.name) in names }
                .sortedBy { it.name }
                .map { file ->
                    BaseGameAsset(file.name, AssetManifest.computeSha256(file), file.length(), file.lastModified())
                }
        val manifest = AssetManifest(setDir).load().associateBy { portableGameFilenameIdentity(it.filename) }
        val linked =
            SafManifest
                .forDir(setDir)
                .read()
                .filter { portableGameFilenameIdentity(it.filename) in names }
                .sortedBy { it.filename }
                .map { entry ->
                    BaseGameAsset(
                        entry.filename,
                        manifest[portableGameFilenameIdentity(entry.filename)]?.sha256,
                        entry.sizeBytes,
                        0,
                        entry.contentUri,
                    )
                }
        classifyBaseGameAssets(game, local + linked)
    }

/** Called only after the user confirms the displayed game-file inventory */
internal suspend fun clearBaseGameAssets(
    setDir: File,
    reviewed: BaseGameAssetAudit,
) = withContext(Dispatchers.IO) {
    val current = auditBaseGameAssets(setDir, reviewed.game)
    check(current.assets == reviewed.assets) { "Game files changed. Review them again before clearing" }
    check(current.assets.filter { it.sourceUri == null }.all { it.sha256 != null }) {
        "Some game files could not be read. Nothing was cleared"
    }
    AtomicFilePublication.transaction {
        val names = baseGameAssetNames(reviewed.game)
        val local = current.assets.filter { it.sourceUri == null }
        check(
            setDir
                .listFiles()
                .orEmpty()
                .filter { it.isFile && portableGameFilenameIdentity(it.name) in names }
                .map { it.name }
                .sorted() == local.map { it.name }.sorted(),
        ) {
            "Game files changed. Review them again before clearing"
        }
        val manifest = AssetManifest(setDir)
        val saf = SafManifest.forDir(setDir)
        check(
            saf
                .read()
                .filter { portableGameFilenameIdentity(it.filename) in names }
                .sortedBy { it.filename }
                .map { it.filename to it.contentUri } ==
                current.assets
                    .filter { it.sourceUri != null }
                    .map { it.name to it.sourceUri },
        ) { "Linked game files changed. Review them again before clearing" }
        val retainedAssets = manifest.load().filter { portableGameFilenameIdentity(it.filename) !in names }
        val retainedLinks = saf.read().filter { portableGameFilenameIdentity(it.filename) !in names }
        val metadata = listOf(File(setDir, "assets.json"), File(setDir, SafManifest.FILENAME))
        check(metadata.none { it.exists() && !it.isFile }) { "Game metadata is not a regular file" }
        val originalMetadata = metadata.associateWith { if (it.isFile) it.readBytes() else null }
        val backup = AtomicFilePublication.uniqueSibling(setDir, "clear-base")
        check(backup.mkdirs()) { "Could not stage game-file cleanup" }
        val moved = mutableListOf<Pair<File, File>>()
        var preserveBackup = false
        try {
            for (asset in current.assets.filter { it.sourceUri == null }) {
                val file = File(setDir, asset.name)
                check(file.isFile && file.length() == asset.size && file.lastModified() == asset.modified) {
                    "Game files changed. Nothing further was cleared"
                }
                val saved = File(backup, asset.name)
                check(file.renameTo(saved)) { "Could not clear ${asset.name}" }
                moved += saved to file
            }
            manifest.save(retainedAssets)
            saf.write(retainedLinks)
        } catch (error: Exception) {
            val restored = moved.asReversed().map { (saved, original) -> saved.renameTo(original) }.all { it }
            try {
                AtomicFilePublication.writeBytesBatch(
                    originalMetadata.mapNotNull { (file, bytes) ->
                        bytes?.let {
                            file to
                                it
                        }
                    },
                )
                originalMetadata.filterValues { it == null }.keys.forEach { check(!it.exists() || it.delete()) }
            } catch (restoreError: Exception) {
                preserveBackup = true
                error.addSuppressed(restoreError)
            }
            if (!restored) preserveBackup = true
            if (preserveBackup) {
                throw IllegalStateException("Cleanup recovery files remain at ${backup.absolutePath}", error)
            }
            throw error
        } finally {
            if (!preserveBackup) backup.deleteRecursively()
        }
    }
}
