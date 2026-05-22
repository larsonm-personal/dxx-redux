package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class SafUriPermissionsTest {
    @Test
    fun derivesOwningTreeUriFromTreeDocumentUri() {
        val uri =
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss/document/6634-3535%3AInf%20abyss%2FDESCENT_II_ABYSS%20(Track%201).bin"

        assertEquals(
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss",
            deriveOwningTreeUriString(uri),
        )
    }

    @Test
    fun returnsNullForDocumentUriWithoutTreeSegment() {
        val uri = "content://com.android.externalstorage.documents/document/6634-3535%3Atrack.bin"

        assertNull(deriveOwningTreeUriString(uri))
    }

    @Test
    fun detectsPermissionCoveredByTrackedTreeDocumentUri() {
        val trackedUri =
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss/document/6634-3535%3AInf%20abyss%2FDESCENT_II_ABYSS%20(Track%201).bin"
        val permissionUri = "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss"

        assertTrue(isPersistedPermissionCoveredByTrackedUris(permissionUri, listOf(trackedUri)))
        assertFalse(isPersistedPermissionCoveredByTrackedUris(permissionUri, emptyList()))
    }

    @Test
    fun collectsOnlyPersistedPermissionsUnusedByRetainedUris() {
        val removedTrackedUri =
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss/document/6634-3535%3AInf%20abyss%2FDESCENT2.HOG"
        val retainedTrackedUri =
            "content://com.android.externalstorage.documents/tree/6634-3535%3ACustom%20Music/document/6634-3535%3ACustom%20Music%2Ftrack01.ogg"
        val persistedPermissions =
            listOf(
                "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss",
                "content://com.android.externalstorage.documents/tree/6634-3535%3ACustom%20Music",
            )

        assertEquals(
            setOf("content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss"),
            collectPersistedPermissionUrisToRelease(
                persistedPermissionUris = persistedPermissions,
                removedTrackedUris = listOf(removedTrackedUri),
                retainedTrackedUris = listOf(retainedTrackedUri),
            ),
        )
    }

    @Test
    fun keepsSharedPersistedPermissionWhenStillTrackedElsewhere() {
        val sharedTreePermission = "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss"
        val removedTrackedUri =
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss/document/6634-3535%3AInf%20abyss%2FDESCENT2.HOG"
        val retainedTrackedUri =
            "content://com.android.externalstorage.documents/tree/6634-3535%3AInf%20abyss/document/6634-3535%3AInf%20abyss%2FDESCENT_II_ABYSS%20(Track%201).bin"

        assertTrue(
            collectPersistedPermissionUrisToRelease(
                persistedPermissionUris = listOf(sharedTreePermission),
                removedTrackedUris = listOf(removedTrackedUri),
                retainedTrackedUris = listOf(retainedTrackedUri),
            ).isEmpty(),
        )
    }
}