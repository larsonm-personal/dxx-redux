package com.dxxredux.app

import java.io.File
import java.util.Locale

internal data class FileSetMissionEntry(
    val displayName: String,
    val game: String,
    val descriptor: File,
    val archive: File?,
    val versionName: String?,
    val sourceUri: String?,
    val ownedFiles: List<File> = listOfNotNull(descriptor, archive),
    val contentId: String? = null,
) {
    val files: List<File> = ownedFiles
    val totalBytes: Long = files.sumOf { it.length() }
}

internal object FileSetMissionInventory {
    fun scan(setDir: File): List<FileSetMissionEntry> {
        val stored = FileSetContentManager(setDir).listEntries()
        val storedIds = stored.map { it.id }.toSet()
        return (stored + FileSetContentCatalog.scan(setDir).filter { it.id !in storedIds })
            .filter { it.kind == FileSetContentCatalog.KIND_LOOSE_MISSION && it.problem == null }
            .mapNotNull { content ->
                val descriptor =
                    content.files.firstOrNull { GameFileFormats.isMissionDescriptor(it.name) }
                        ?: return@mapNotNull null
                val archive =
                    content.files.firstOrNull {
                        it.extension.equals("hog", ignoreCase = true) &&
                            it.nameWithoutExtension.equals(descriptor.nameWithoutExtension, ignoreCase = true)
                    }
                FileSetMissionEntry(
                    displayName = content.displayName,
                    game = content.game,
                    descriptor = descriptor,
                    archive = archive,
                    versionName = content.versionName,
                    sourceUri = content.sourceUri,
                    ownedFiles = content.files,
                    contentId = content.id.takeIf { it in storedIds },
                )
            }.sortedWith(compareBy({ it.game }, { it.displayName.lowercase(Locale.US) }))
    }

    fun remove(
        setDir: File,
        entry: FileSetMissionEntry,
    ): Int {
        entry.contentId?.let { id ->
            val removedFiles = entry.files.count { it.isFile }
            return if (FileSetContentManager(setDir).deleteEntry(id)) removedFiles else 0
        }
        val root = setDir.canonicalFile
        val files = entry.files.map { it.canonicalFile }
        require(files.all { it.toPath().startsWith(root.toPath()) }) { "Mission files are outside the active set" }
        val manifest = AssetManifest(setDir)
        val safManifest = SafManifest.forDir(setDir)
        var removed = 0
        files.forEach { file ->
            if (file.isFile && file.delete()) removed++
            manifest.remove(file.name)
            safManifest.remove(file.name)
        }
        NativeTextureLookupCache.clear()
        return removed
    }
}
