package com.dxxredux.app

import java.io.File
import java.util.Locale
import java.util.zip.ZipInputStream
import javax.xml.parsers.DocumentBuilderFactory

/** A document in its original archive location, independent of playable mission metadata */
data class PackageDocument(
    val path: String,
    val sizeBytes: Long,
) {
    val name: String get() = path.replace('\\', '/').substringAfterLast('/')
    val isReduxNotes: Boolean get() = name.equals("README.dxx-redux.md", true)
}

object PackageDocuments {
    // Keep supported names aligned with game_data/mods/d2x-xl/mod_documentation.py
    private val INLINE_README_EXTENSIONS = setOf("txt", "md", "docx")
    private val EXTERNAL_README_EXTENSIONS = setOf("pdf", "rtf", "doc")
    private const val MAX_DOCX_XML_BYTES = 4L * 1024L * 1024L
    private const val MAX_DOCX_TOTAL_BYTES = 16L * 1024L * 1024L

    private fun normalizePath(path: String) = path.replace('\\', '/').trim('/')

    private fun leafName(path: String) = normalizePath(path).substringAfterLast('/')

    fun order(
        documents: List<PackageDocument>,
        packageStem: String?,
    ): List<PackageDocument> {
        val candidates =
            documents
                .filter { isReadmeCandidate(it.path) }
                .sortedWith(compareBy<PackageDocument> { sortGroup(it.path) }.thenBy { it.path.lowercase(Locale.US) })
        val group =
            candidates.firstOrNull()?.let { first -> candidates.filter { sortGroup(it.path) == sortGroup(first.path) } }
                ?: return emptyList()
        val preferred =
            group.firstOrNull { it.name.substringBeforeLast('.').equals("README", true) }
                ?: packageStem?.takeIf { it.isNotBlank() }?.let { stem ->
                    group.firstOrNull { it.name.startsWith(stem, true) }
                }
                ?: group.sortedWith(compareByDescending<PackageDocument> { it.sizeBytes }.thenBy { it.path }).first()
        return listOf(preferred) + candidates.filterNot { it.path == preferred.path }
    }

    internal fun sortGroup(path: String): Int =
        when {
            isInlineReadmeCandidate(path) -> 0
            isExternalReadmeCandidate(path) -> 1
            else -> 2
        }

    fun readTextFile(
        file: File,
        path: String,
        maxBytes: Long = 1024L * 1024L,
    ): MissionZip.TextFileContent {
        if (!file.isFile) {
            return MissionZip.TextFileContent(
                "",
                truncated = false,
                problem = "Package archive is missing",
            )
        }
        return try {
            ArchiveFiles.open(file).use { archive ->
                val entry =
                    archive.findEntry(path) ?: return MissionZip.TextFileContent("", false, "Text file is missing")
                if (!isInlineReadmeCandidate(entry.path)) {
                    return MissionZip.TextFileContent("", false, "This document needs an external viewer")
                }
                val requestedLimit =
                    if (GameFileFormats.extensionOf(entry.path) == "docx") MAX_DOCX_TOTAL_BYTES else maxBytes
                val limit = requestedLimit.coerceAtLeast(1L).coerceAtMost((Int.MAX_VALUE - 1).toLong()).toInt()
                val bytes =
                    archive.openInputStream(entry).use { input ->
                        input.readPrefix(limit)
                    }
                decodeInlineDocument(entry.path, bytes, limit)
            }
        } catch (e: Exception) {
            MissionZip.TextFileContent("", truncated = false, problem = e.message ?: e.javaClass.simpleName)
        }
    }

    fun readExtractedDocument(
        file: File,
        path: String,
        maxBytes: Long = 1024L * 1024L,
    ): MissionZip.TextFileContent {
        if (!isInlineReadmeCandidate(path)) {
            return MissionZip.TextFileContent("", false, "This document needs an external viewer")
        }
        if (!file.isFile) return MissionZip.TextFileContent("", truncated = false, problem = "Document file is missing")
        return try {
            val requestedLimit = if (GameFileFormats.extensionOf(path) == "docx") MAX_DOCX_TOTAL_BYTES else maxBytes
            val limit = requestedLimit.coerceAtLeast(1L).coerceAtMost((Int.MAX_VALUE - 1).toLong()).toInt()
            file.inputStream().use { input ->
                val bytes = input.readPrefix(limit)
                decodeInlineDocument(path, bytes, limit)
            }
        } catch (e: Exception) {
            MissionZip.TextFileContent("", truncated = false, problem = e.message ?: e.javaClass.simpleName)
        }
    }

    fun isInlineReadmeCandidate(path: String): Boolean =
        GameFileFormats.extensionOf(path) in INLINE_README_EXTENSIONS || leafName(path).equals("README", true)

    fun isExternalReadmeCandidate(path: String): Boolean =
        GameFileFormats.extensionOf(path) in EXTERNAL_README_EXTENSIONS

    fun isReadmeCandidate(path: String): Boolean = isInlineReadmeCandidate(path) || isExternalReadmeCandidate(path)

    fun externalViewMimeType(path: String): String =
        when (GameFileFormats.extensionOf(path)) {
            "pdf" -> "application/pdf"
            "rtf" -> "application/rtf"
            "doc" -> "application/msword"
            "docx" -> "application/vnd.openxmlformats-officedocument.wordprocessingml.document"
            "txt", "md", "" -> "text/plain"
            else -> "application/octet-stream"
        }

    private fun java.io.InputStream.readPrefix(limit: Int): ByteArray {
        val buffer = ByteArray(limit + 1)
        var total = 0
        while (total < buffer.size) {
            val read = read(buffer, total, buffer.size - total)
            if (read < 0) break
            if (read == 0) {
                val byte = read()
                if (byte < 0) break
                buffer[total++] = byte.toByte()
            } else {
                total += read
            }
        }
        return buffer.copyOf(total)
    }

    private fun readDocxText(bytes: ByteArray): MissionZip.TextFileContent {
        val budget =
            ExtractionBudget(
                maxEntryBytes = MAX_DOCX_XML_BYTES,
                maxTotalBytes = MAX_DOCX_TOTAL_BYTES,
                maxEntries = 256,
            )
        ZipInputStream(bytes.inputStream().buffered()).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                val path = normalizePath(entry.name)
                budget.registerEntry(entry.size, entry.compressedSize, path)
                if (!entry.isDirectory) {
                    val content =
                        zip.readBytesBounded(
                            MAX_DOCX_XML_BYTES,
                            path,
                            budget,
                            entry.compressedSize,
                            entry.size,
                        )
                    if (path.equals("word/document.xml", ignoreCase = true)) {
                        return MissionZip.TextFileContent(parseDocxDocumentXml(content), truncated = false)
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return MissionZip.TextFileContent("", truncated = false, problem = "Word document content is missing")
    }

    private fun decodeInlineDocument(
        path: String,
        bytes: ByteArray,
        limit: Int,
    ): MissionZip.TextFileContent =
        if (GameFileFormats.extensionOf(path) == "docx") {
            if (bytes.size > limit) {
                MissionZip.TextFileContent("", truncated = true, problem = "Word document exceeds $limit bytes")
            } else {
                readDocxText(bytes)
            }
        } else {
            val truncated = bytes.size > limit
            MissionZip.TextFileContent(
                MissionZip.decodeLegacyText(bytes.copyOf(minOf(bytes.size, limit)), truncated),
                truncated,
            )
        }

    private fun parseDocxDocumentXml(bytes: ByteArray): String {
        val factory = DocumentBuilderFactory.newInstance()
        factory.isNamespaceAware = true
        factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true)
        factory.setFeature("http://xml.org/sax/features/external-general-entities", false)
        factory.setFeature("http://xml.org/sax/features/external-parameter-entities", false)
        factory.setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd", false)
        val document = factory.newDocumentBuilder().parse(bytes.inputStream())
        val paragraphs = document.getElementsByTagNameNS("*", "p")
        return buildString {
            for (paragraphIndex in 0 until paragraphs.length) {
                val children = paragraphs.item(paragraphIndex).childNodes
                appendDocxText(children)
                if (isNotEmpty() && last() != '\n') append('\n')
            }
        }.trimEnd()
    }

    private fun StringBuilder.appendDocxText(nodes: org.w3c.dom.NodeList) {
        for (index in 0 until nodes.length) {
            val node = nodes.item(index)
            when (node.localName) {
                "t" -> append(node.textContent)
                "tab" -> append('\t')
                "br", "cr" -> append('\n')
                else -> appendDocxText(node.childNodes)
            }
        }
    }
}
