package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log

private const val TAG = "DXX-SafPerm"

internal fun persistReadPermissionForUri(
    context: Context,
    uri: Uri,
): Boolean {
    val resolver = context.contentResolver
    try {
        resolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        return true
    } catch (directError: SecurityException) {
        val treeUri = deriveOwningTreeUri(uri)
        if (treeUri == null) {
            Log.w(TAG, "Could not persist read permission for $uri", directError)
            return false
        }
        return try {
            resolver.takePersistableUriPermission(treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            Log.i(TAG, "Persisted tree read permission for $treeUri")
            true
        } catch (treeError: SecurityException) {
            Log.w(TAG, "Could not persist read permission for $uri or $treeUri", treeError)
            false
        }
    }
}

internal fun releaseReadPermissionForUri(
    context: Context,
    uri: Uri,
) {
    val resolver = context.contentResolver
    val seen = mutableSetOf<String>()

    fun releaseOne(target: Uri) {
        val key = target.toString()
        if (!seen.add(key)) return
        try {
            resolver.releasePersistableUriPermission(target, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (_: SecurityException) {
        }
    }

    releaseOne(uri)
    deriveOwningTreeUri(uri)?.let(::releaseOne)
}

internal fun canAccessSafUri(
    context: Context,
    uri: Uri,
    useFileDescriptor: Boolean = false,
): Boolean =
    try {
        if (useFileDescriptor) {
            context.contentResolver.openFileDescriptor(uri, "r")?.use { true } ?: false
        } else {
            context.contentResolver.openInputStream(uri)?.use { true } ?: false
        }
    } catch (_: Exception) {
        false
    }

internal fun deriveOwningTreeUri(uri: Uri): Uri? = deriveOwningTreeUriString(uri.toString())?.let(Uri::parse)

internal fun deriveOwningTreeUriString(uri: String): String? {
    val treeMarker = "/tree/"
    val documentMarker = "/document/"
    val treeStart = uri.indexOf(treeMarker)
    if (treeStart < 0) return null
    val documentStart = uri.indexOf(documentMarker, treeStart + treeMarker.length)
    return if (documentStart >= 0) uri.substring(0, documentStart) else uri
}

internal fun isPersistedPermissionCoveredByTrackedUris(
    permissionUri: String,
    trackedUris: Collection<String>,
): Boolean =
    trackedUris.any { trackedUri ->
        trackedUri == permissionUri || deriveOwningTreeUriString(trackedUri) == permissionUri
    }

internal fun collectPersistedPermissionUrisToRelease(
    persistedPermissionUris: Collection<String>,
    removedTrackedUris: Collection<String>,
    retainedTrackedUris: Collection<String> = emptyList(),
): Set<String> =
    persistedPermissionUris
        .filter { permissionUri ->
            isPersistedPermissionCoveredByTrackedUris(permissionUri, removedTrackedUris) &&
                !isPersistedPermissionCoveredByTrackedUris(permissionUri, retainedTrackedUris)
        }.toCollection(linkedSetOf())

internal fun revokeUnusedPersistedReadPermissions(
    context: Context,
    removedTrackedUris: Collection<String>,
    retainedTrackedUris: Collection<String> = emptyList(),
) {
    if (removedTrackedUris.isEmpty()) return

    val resolver = context.contentResolver
    val permissionUrisToRelease =
        collectPersistedPermissionUrisToRelease(
            persistedPermissionUris = resolver.persistedUriPermissions.map { it.uri.toString() },
            removedTrackedUris = removedTrackedUris,
            retainedTrackedUris = retainedTrackedUris,
        )

    permissionUrisToRelease.forEach { permissionUri ->
        try {
            resolver.releasePersistableUriPermission(
                Uri.parse(permissionUri),
                Intent.FLAG_GRANT_READ_URI_PERMISSION,
            )
        } catch (_: SecurityException) {
        }
    }
}
