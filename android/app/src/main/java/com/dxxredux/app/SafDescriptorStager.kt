package com.dxxredux.app

import android.os.ParcelFileDescriptor
import android.system.Os
import android.system.OsConstants
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

internal object SafDescriptorStager {
    private const val TAG = "DXX-SAF-Stage"
    private const val STAGE_DIRECTORY = "saf_descriptor_stage"
    private const val MAX_STAGE_BYTES = 2L * 1024L * 1024L * 1024L
    private const val REQUIRED_FREE_BYTES = 64L * 1024L * 1024L
    private const val MAX_ZERO_READS = 1024
    private val lock = Any()

    fun detachSeekable(
        source: ParcelFileDescriptor,
        cacheDir: File,
        label: String,
    ): Int = openSeekable(source, cacheDir, label).detachFd()

    fun openSeekable(
        source: ParcelFileDescriptor,
        cacheDir: File,
        label: String,
    ): ParcelFileDescriptor {
        if (!needsStaging(source)) return source
        return synchronized(lock) {
            stageAndOpen(source, cacheDir, label)
        }
    }

    fun needsStaging(source: ParcelFileDescriptor): Boolean = !isSeekableRegular(source)

    private fun isSeekableRegular(source: ParcelFileDescriptor): Boolean =
        try {
            val stat = Os.fstat(source.fileDescriptor)
            OsConstants.S_ISREG(stat.st_mode).also { regular ->
                if (regular) {
                    Os.lseek(source.fileDescriptor, 0L, OsConstants.SEEK_CUR)
                    val probe = ByteArray(1)
                    if (stat.st_size > 0L && Os.pread(source.fileDescriptor, probe, 0, 1, stat.st_size - 1) != 1) {
                        return false
                    }
                    if (Os.pread(source.fileDescriptor, probe, 0, 1, stat.st_size) != 0) return false
                }
            }
        } catch (_: Exception) {
            false
        }

    private fun stageAndOpen(
        source: ParcelFileDescriptor,
        cacheDir: File,
        label: String,
    ): ParcelFileDescriptor {
        val root = File(cacheDir, STAGE_DIRECTORY)
        check(root.mkdirs() || root.isDirectory) { "Could not create SAF staging directory" }
        root.listFiles().orEmpty().forEach(File::delete)
        val stageLimit = minOf(MAX_STAGE_BYTES, (root.usableSpace - REQUIRED_FREE_BYTES).coerceAtLeast(0L))
        if (stageLimit == 0L) throw IOException("Insufficient storage to stage SAF source")
        val temporary = File.createTempFile(".source-", ".tmp", root)
        try {
            val copied =
                ParcelFileDescriptor.AutoCloseInputStream(source).use { input ->
                    FileOutputStream(temporary).use { output ->
                        val buffer = ByteArray(64 * 1024)
                        var total = 0L
                        var zeroReads = 0
                        while (true) {
                            val count = input.read(buffer)
                            if (count < 0) break
                            if (count == 0) {
                                zeroReads++
                                if (zeroReads > MAX_ZERO_READS) throw IOException("SAF source stopped making progress")
                                continue
                            }
                            zeroReads = 0
                            if (total > stageLimit - count) {
                                throw IOException("SAF source exceeds the $stageLimit byte staging budget")
                            }
                            output.write(buffer, 0, count)
                            total += count
                        }
                        output.flush()
                        output.fd.sync()
                        total
                    }
                }
            val staged = ParcelFileDescriptor.open(temporary, ParcelFileDescriptor.MODE_READ_ONLY)
            try {
                check(temporary.delete()) { "Could not unlink SAF staging file" }
                Log.i(TAG, "Staged nonseekable SAF source $label ($copied bytes)")
                return staged
            } catch (failure: Exception) {
                runCatching { staged.close() }
                throw failure
            }
        } finally {
            runCatching { source.close() }
            temporary.delete()
        }
    }
}
