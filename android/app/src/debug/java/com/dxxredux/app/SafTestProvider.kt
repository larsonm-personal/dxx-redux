package com.dxxredux.app

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import java.io.File
import java.io.FileInputStream
import java.io.FileNotFoundException

class SafTestProvider : ContentProvider() {
    override fun onCreate(): Boolean = true

    override fun openFile(
        uri: Uri,
        mode: String,
    ): ParcelFileDescriptor {
        if (mode != "r") throw FileNotFoundException("Read-only test provider")
        val path = uri.pathSegments
        if (path.size != 2 || path[1].isEmpty() || path[1] != File(path[1]).name) {
            throw FileNotFoundException("Invalid test provider path")
        }
        val source = File(requireNotNull(context).filesDir, "saf_provider_source/${path[1]}")
        if (!source.isFile) throw FileNotFoundException(source.absolutePath)
        return when (path[0]) {
            "seekable" -> ParcelFileDescriptor.open(source, ParcelFileDescriptor.MODE_READ_ONLY)
            "pipe" -> openPipe(source)
            else -> throw FileNotFoundException("Unknown test provider mode")
        }
    }

    private fun openPipe(source: File): ParcelFileDescriptor {
        val pipe = ParcelFileDescriptor.createReliablePipe()
        Thread(
            {
                try {
                    FileInputStream(source).use { input ->
                        ParcelFileDescriptor.AutoCloseOutputStream(pipe[1]).use { output -> input.copyTo(output) }
                    }
                } catch (failure: Exception) {
                    runCatching { pipe[1].closeWithError(failure.message ?: "Provider pipe failed") }
                }
            },
            "saf-test-provider-pipe",
        ).start()
        return pipe[0]
    }

    override fun getType(uri: Uri): String = "application/octet-stream"

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?,
    ): Cursor? = null

    override fun insert(
        uri: Uri,
        values: ContentValues?,
    ): Uri? = null

    override fun delete(
        uri: Uri,
        selection: String?,
        selectionArgs: Array<out String>?,
    ): Int = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?,
    ): Int = 0
}
