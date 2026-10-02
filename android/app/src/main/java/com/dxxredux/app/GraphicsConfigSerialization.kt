package com.dxxredux.app

import java.io.File
import java.io.RandomAccessFile

/** Application installs the native lock/stager; host JVM fixtures use the file-only backend */
internal object GraphicsConfigSerialization {
    var nativeTransaction: ((File, () -> Unit) -> Unit)? = null
    var nativeStage: ((File, List<Pair<String, String>>) -> Unit)? = null
    var nativeRead: ((File) -> Map<String, Int>)? = null

    // Keep this ordered set synchronized with graphics_safety_keys in shared/graphics_safety_store.cpp
    val protectedKeys =
        setOf(
            "TexFilt",
            "AnisoLevel",
            "MsaaLevel",
            "MenuTexFilt",
            "HudTexFilt",
            "ResolutionX",
            "ResolutionY",
            "AspectX",
            "AspectY",
            "ColorDepth",
        )

    fun transaction(
        root: File,
        block: () -> Unit,
    ) = AtomicFilePublication.transaction {
        root.mkdirs()
        val native = nativeTransaction
        if (native != null) {
            native(root, block)
        } else {
            RandomAccessFile(File(root, ".graphics_safety.lock"), "rw").use { file ->
                file.channel.lock().use { block() }
            }
        }
    }
}
