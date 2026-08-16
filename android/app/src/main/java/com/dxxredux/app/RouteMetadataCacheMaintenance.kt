package com.dxxredux.app

import java.io.File

internal data class RouteMetadataCacheCleanupResult(
    val removedFiles: Int = 0,
    val removedDirectories: Int = 0,
)

internal object RouteMetadataCacheMaintenance {
    private val generationDirectory = Regex("(?:g)?[0-9]+")
    private val completedCheckpoint = Regex("(.+\\.bin)\\.samples-[0-9]{6}")
    private val temporaryFile = Regex(".+\\.tmp-[0-9]+")

    fun prune(filesDir: File): RouteMetadataCacheCleanupResult {
        var removedFiles = 0
        var removedDirectories = 0
        cacheRoots(filesDir).forEach { root ->
            root.listFiles()?.forEach { child ->
                if (child.isDirectory &&
                    generationDirectory.matches(child.name) &&
                    child.name != "g$ROUTE_METADATA_CACHE_GENERATION"
                ) {
                    if (child.deleteRecursively()) removedDirectories++
                }
            }
            val current = File(root, "g$ROUTE_METADATA_CACHE_GENERATION")
            current.listFiles()?.forEach { file ->
                if (!file.isFile) return@forEach
                val checkpointBase = completedCheckpoint.matchEntire(file.name)?.groupValues?.get(1)
                if ((checkpointBase != null && File(current, checkpointBase).isFile) ||
                    temporaryFile.matches(file.name)
                ) {
                    if (file.delete()) removedFiles++
                }
            }
        }
        return RouteMetadataCacheCleanupResult(removedFiles, removedDirectories)
    }

    fun clear(filesDir: File): RouteMetadataCacheCleanupResult {
        var removedFiles = 0
        var removedDirectories = 0
        cacheRoots(filesDir).forEach { root ->
            if (root.exists()) {
                removedFiles += root.walkTopDown().count { it.isFile }
                if (root.deleteRecursively()) removedDirectories++
            }
        }
        listOf(
            "route_metadata_precompute.json",
            "route_metadata_precompute.lock",
        ).forEach { name ->
            if (File(filesDir, name).delete()) removedFiles++
        }
        return RouteMetadataCacheCleanupResult(removedFiles, removedDirectories)
    }

    private fun cacheRoots(filesDir: File): List<File> =
        listOf("d1x-redux", "d2x-redux").map { File(filesDir, "$it/route-cache") }
}
