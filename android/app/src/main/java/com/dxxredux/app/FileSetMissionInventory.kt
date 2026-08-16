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
) {
    val files: List<File> = listOfNotNull(descriptor, archive)
    val totalBytes: Long = files.sumOf { it.length() }
}

internal object FileSetMissionInventory {
    fun scan(setDir: File): List<FileSetMissionEntry> {
        val manifestEntries = AssetManifest(setDir).load().associateBy { portableGameFilenameIdentity(it.filename) }
        return setDir
            .walkTopDown()
            .maxDepth(2)
            .filter { it.isFile && GameFileFormats.isMissionDescriptor(it.name) }
            .mapNotNull { descriptor ->
                val mission =
                    runCatching { MissionZip.parseMissionDescriptor(descriptor.name, descriptor.readBytes()) }
                        .getOrNull() ?: return@mapNotNull null
                val archive =
                    descriptor.parentFile
                        ?.listFiles()
                        ?.firstOrNull {
                            it.isFile &&
                                it.extension.equals("hog", ignoreCase = true) &&
                                it.nameWithoutExtension.equals(descriptor.nameWithoutExtension, ignoreCase = true)
                        }
                val tracked =
                    listOfNotNull(descriptor, archive)
                        .mapNotNull { manifestEntries[portableGameFilenameIdentity(it.name)] }
                FileSetMissionEntry(
                    displayName = mission.displayName,
                    game = mission.game,
                    descriptor = descriptor,
                    archive = archive,
                    versionName = tracked.mapNotNull { it.versionName }.distinct().singleOrNull(),
                    sourceUri = tracked.mapNotNull { it.sourceUri }.distinct().singleOrNull(),
                )
            }.sortedWith(compareBy({ it.game }, { it.displayName.lowercase(Locale.US) }))
            .toList()
    }

    fun remove(
        setDir: File,
        entry: FileSetMissionEntry,
    ): Int {
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
