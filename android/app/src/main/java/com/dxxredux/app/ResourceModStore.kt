package com.dxxredux.app

import org.apache.commons.compress.archivers.sevenz.SevenZFile
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.security.MessageDigest

/** Private extracted resources and inventory only; never creates a distributable archive */
internal object ResourceModStore {
    private const val INVENTORY = ".resource_inventory.json"

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(65536)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                digest.update(buffer, 0, count)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it.toInt() and 255) }
    }

    data class Source(
        val path: String,
        val size: Long,
        val open: () -> InputStream,
    )

    data class Installed(
        val path: String,
        val source: String,
        val size: Long,
        val sha256: String,
    )

    fun inventory(
        root: File,
        verifyHashes: Boolean = true,
    ): List<Installed> {
        val file = File(root, INVENTORY)
        val document =
            file.inputStream().use {
                JSONObject(String(it.readBytesBounded(256 * 1024L, INVENTORY), Charsets.UTF_8))
            }
        require(document.getString("resource_group") == XL_RESOURCE_MOD.id) { "Unknown imported resource group" }
        val entries = document.getJSONArray("files")
        require(entries.length() <= ExtractionLimits.MAX_ENTRIES) { "Too many imported resources" }
        val result =
            (0 until entries.length()).map { index ->
                val entry = entries.getJSONObject(index)
                val path = entry.getString("path")
                require(XL_RESOURCE_MOD.target(path) == path) { "Unexpected imported resource path" }
                val content = File(root, path)
                require(
                    content.canonicalPath.startsWith(root.canonicalPath + File.separator),
                ) { "Resource escaped its directory" }
                require(
                    content.isFile && content.length() == entry.getLong("size"),
                ) { "Imported resource missing or changed: $path" }
                val hash = entry.getString("sha256")
                if (verifyHashes) {
                    require(
                        sha256(content) == hash,
                    ) { "Imported resource changed: $path" }
                }
                Installed(path, entry.getString("source"), content.length(), hash)
            }
        require(
            result
                .map {
                    XL_RESOURCE_MOD.identity(it.path)
                }.distinct()
                .size == result.size,
        ) { "Duplicate imported resources" }
        return result
    }

    fun valid(root: File): Boolean = runCatching { inventory(root).isNotEmpty() }.getOrDefault(false)

    fun revision(root: File): String {
        inventory(root)
        return sha256(File(root, INVENTORY))
    }

    fun importSevenZ(
        catalog: ResourceModCatalog,
        destination: File,
        archive: SevenZFile,
    ): List<String> {
        val entries =
            archive.entries
                .asSequence()
                .take(65537)
                .toList()
        require(entries.size <= 65536) { "Resource distribution catalog is too large" }
        val files = entries.filterNot { it.isDirectory }
        return update(
            catalog,
            destination,
            catalog.select(files.map { it.name }).map { selection ->
                val entry = files.single { it.name == selection.source }
                Source(entry.name, entry.size) { archive.getInputStream(entry) }
            },
        )
    }

    fun update(
        catalog: ResourceModCatalog,
        destination: File,
        sources: List<Source>,
    ): List<String> =
        AtomicFilePublication.transaction {
            require(sources.isNotEmpty()) { "No resource files selected" }
            val selected = catalog.select(sources.map { it.path })
            require(selected.size == sources.size) { "Unsupported resource selected" }
            val replaced = selected.map { catalog.identity(it.target) }.toSet()
            val previous = if (destination.exists()) inventory(destination) else emptyList()
            val temporary = AtomicFilePublication.uniqueSibling(destination, "resource-import")
            require(temporary.mkdirs()) { "Cannot stage imported resources" }
            val budget = ExtractionBudget()
            val installed = mutableListOf<Installed>()
            try {
                fun copy(
                    path: String,
                    source: String,
                    size: Long,
                    input: () -> InputStream,
                ) {
                    budget.registerEntry(size, -1, path)
                    val target = File(temporary, path)
                    target.parentFile!!.mkdirs()
                    val digest = MessageDigest.getInstance("SHA-256")
                    val count =
                        FileOutputStream(target).use { output ->
                            val copied =
                                input().use { stream ->
                                    java.security
                                        .DigestInputStream(
                                            stream,
                                            digest,
                                        ).copyToBounded(output, budget, -1, path)
                                }
                            output.fd.sync()
                            copied
                        }
                    require(size < 0 || count == size) { "Resource size changed during import: $path" }
                    installed +=
                        Installed(
                            path,
                            source,
                            count,
                            digest.digest().joinToString("") {
                                "%02x".format(it.toInt() and 255)
                            },
                        )
                }
                for (old in previous) {
                    if (catalog.identity(old.path) !in replaced) {
                        copy(old.path, old.source, old.size) { File(destination, old.path).inputStream() }
                    }
                }
                // Keep archive order to avoid decoding solid 7z blocks repeatedly
                for (selection in selected) {
                    val source = sources.single { it.path == selection.source }
                    copy(selection.target, source.path, source.size, source.open)
                }
                val records = JSONArray()
                installed.sortedBy { it.path }.forEach {
                    records.put(
                        JSONObject()
                            .put(
                                "path",
                                it.path,
                            ).put("source", it.source)
                            .put("size", it.size)
                            .put("sha256", it.sha256),
                    )
                }
                AtomicFilePublication.writeUtf8(
                    File(temporary, INVENTORY),
                    JSONObject()
                        .put("resource_group", catalog.id)
                        .put("files", records)
                        .toString(2),
                )
                inventory(temporary)
                AtomicFilePublication.publishDirectory(temporary, destination)
                installed.map { it.path }.sorted()
            } finally {
                temporary.deleteRecursively()
            }
        }
}
