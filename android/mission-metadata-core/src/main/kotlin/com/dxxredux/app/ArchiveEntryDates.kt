package com.dxxredux.app

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry
import org.apache.commons.compress.archivers.sevenz.SevenZFile
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone
import java.util.zip.ZipEntry
import java.util.zip.ZipFile

/** Original archive evidence shared by Android import and the host CLI; no date inference */
object ArchiveEntryDates {
    fun zip(entry: ZipEntry): String? =
        entry.time.takeIf { it >= 0 }?.let {
            SimpleDateFormat("yyyy-MM-dd", Locale.ROOT).format(Date(it))
        }

    fun sevenZ(entry: SevenZArchiveEntry): String? =
        if (entry.hasLastModifiedDate) utc(entry.lastModifiedDate) else null

    fun utc(date: Date): String =
        SimpleDateFormat("yyyy-MM-dd", Locale.ROOT)
            .apply {
                timeZone = TimeZone.getTimeZone("UTC")
            }.format(date)

    fun read(file: File): JsonObject =
        buildJsonObject {
            fun add(
                path: String,
                modified: String?,
                kind: String,
            ) {
                if (modified == null) return
                val normalized = path.replace('\\', '/')
                put(
                    normalized,
                    buildJsonObject {
                        put("file", normalized)
                        put("modified", modified)
                        put("kind", kind)
                    },
                )
            }
            when (file.extension.lowercase(Locale.ROOT)) {
                "zip" -> {
                    ZipFile(file).use { archive ->
                        archive.entries().asSequence().filterNot { it.isDirectory }.forEach {
                            add(it.name, zip(it), "zip_entry_mtime")
                        }
                    }
                }

                "7z" -> {
                    SevenZFile.builder().setFile(file).setMaxMemoryLimitKiB(256 * 1024).get().use { archive ->
                        archive.entries.filterNot { it.isDirectory }.forEach {
                            add(it.name.orEmpty(), sevenZ(it), "7z_entry_mtime")
                        }
                    }
                }
            }
        }
}
