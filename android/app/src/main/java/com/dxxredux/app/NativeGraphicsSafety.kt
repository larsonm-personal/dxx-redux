package com.dxxredux.app

import java.io.File
import java.io.IOException

/** File-only JNI service shared by launcher and game processes; it never initializes an engine */
internal object NativeGraphicsSafety {
    // Order must match graphics_safety_field in shared/graphics_safety_store.h
    val keys = GraphicsConfigSerialization.protectedKeys.toList()

    init {
        System.loadLibrary("dxx-graphics-safety")
    }

    private external fun nativeLock(root: String): Long

    private external fun nativeUnlock(lock: Long)

    private external fun nativeRecover(root: String): Int

    private external fun nativeRead(root: String): IntArray?

    private external fun nativeStage(
        root: String,
        keys: Array<String>,
        values: IntArray,
    ): Int

    fun install() {
        GraphicsConfigSerialization.nativeRead = ::read
        GraphicsConfigSerialization.nativeTransaction = { root, block ->
            val lock = nativeLock(root.absolutePath)
            if (lock == 0L) throw IOException("Could not lock graphics configuration")
            try {
                block()
            } finally {
                nativeUnlock(lock)
            }
        }
        GraphicsConfigSerialization.nativeStage = { root, updates ->
            val result =
                nativeStage(
                    root.absolutePath,
                    updates.map { it.first }.toTypedArray(),
                    updates.map { it.second.toInt() }.toIntArray(),
                )
            if (result == 0) throw IOException("Could not save graphics settings")
        }
    }

    fun recover(filesDir: File): Int = AtomicFilePublication.transaction { nativeRecover(filesDir.absolutePath) }

    fun read(filesDir: File): Map<String, Int> =
        AtomicFilePublication.transaction {
            val values = nativeRead(filesDir.absolutePath) ?: throw IOException("Could not read graphics settings")
            if (values.size != keys.size) throw IOException("Invalid graphics snapshot")
            keys.zip(values.toList()).toMap()
        }
}
