package com.dxxredux.app

import java.io.File

/** Keep the newest matching files, leaving other directory contents alone */
internal fun pruneOldLogFiles(
    directory: File,
    maxFiles: Int,
    matches: (File) -> Boolean,
    delete: (File) -> Unit = { it.delete() },
) {
    require(maxFiles >= 0)
    val files =
        directory
            .listFiles()
            ?.filter { it.isFile && matches(it) }
            ?.sortedWith(compareByDescending<File> { it.lastModified() }.thenByDescending { it.name })
            ?: return
    files.drop(maxFiles).forEach(delete)
}
