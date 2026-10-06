package com.dxxredux.app

import java.io.File
import java.io.IOException

/** The native chooser consumes this installation-local flag after its first Android UI draw */
internal object GraphicsFirstRunPreference {
    fun marker(noBackupFilesDir: File): File = File(noBackupFilesDir, "graphics-first-run-offered")

    fun isEnabled(noBackupFilesDir: File): Boolean = !marker(noBackupFilesDir).exists()

    fun setEnabled(
        noBackupFilesDir: File,
        enabled: Boolean,
    ) {
        val file = marker(noBackupFilesDir)
        if (enabled) {
            if (file.exists() && !file.delete()) throw IOException("Could not reset graphics chooser")
        } else {
            AtomicFilePublication.writeUtf8(file, "offered\n")
        }
    }
}
