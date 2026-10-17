package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.InputStream
import java.util.ArrayDeque
import java.util.zip.ZipEntry
import java.util.zip.ZipFile

private const val JSON_TOKEN_DELIMITERS = ",:{}[] \t\r\n"

internal data class JsonComplexityLimits(
    val nodes: Int,
    val keys: Int,
    val depth: Int,
    val stringBytes: Int,
)

internal fun InputStream.readUtf8TextBounded(
    maxBytes: Int,
    description: String,
): String {
    require(maxBytes >= 0)
    val output = ByteArrayOutputStream(minOf(maxBytes, DEFAULT_BUFFER_SIZE))
    val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
    var total = 0
    while (true) {
        val count = read(buffer)
        if (count < 0) break
        if (count == 0) {
            val byte = read()
            if (byte < 0) break
            total++
            if (total > maxBytes) throw IllegalArgumentException("$description exceeds $maxBytes bytes")
            output.write(byte)
            continue
        }
        total += count
        if (total > maxBytes) throw IllegalArgumentException("$description exceeds $maxBytes bytes")
        output.write(buffer, 0, count)
    }
    return output.toString(Charsets.UTF_8.name())
}

internal fun File.readUtf8TextBounded(
    maxBytes: Int,
    description: String,
): String {
    if (length() > maxBytes) throw IllegalArgumentException("$description exceeds $maxBytes bytes")
    return inputStream().use { it.readUtf8TextBounded(maxBytes, description) }
}

internal fun ZipFile.readUtf8TextBounded(
    entry: ZipEntry,
    maxBytes: Int,
    description: String,
): String {
    if (entry.size > maxBytes) throw IllegalArgumentException("$description exceeds $maxBytes bytes")
    return getInputStream(entry).use { it.readUtf8TextBounded(maxBytes, description) }
}

internal fun preflightJsonComplexity(
    text: String,
    limits: JsonComplexityLimits,
    description: String,
) {
    var depth = 0
    var nodes = 0
    var keys = 0
    var index = 0

    fun countNode() {
        nodes++
        if (nodes > limits.nodes) throw IllegalArgumentException("$description exceeds ${limits.nodes} JSON nodes")
    }

    while (index < text.length) {
        when (text[index]) {
            '{', '[' -> {
                depth++
                if (depth > limits.depth) {
                    throw IllegalArgumentException("$description exceeds JSON nesting depth ${limits.depth}")
                }
                countNode()
                index++
            }

            '}', ']' -> {
                depth--
                index++
            }

            '"' -> {
                index++
                var escaped = false
                while (index < text.length) {
                    val stringCharacter = text[index++]
                    if (escaped) {
                        escaped = false
                    } else if (stringCharacter == '\\') {
                        escaped = true
                    } else if (stringCharacter == '"') {
                        break
                    }
                }
                var next = index
                while (next < text.length && text[next].isWhitespace()) next++
                if (next < text.length && text[next] == ':') {
                    keys++
                    if (keys > limits.keys) {
                        throw IllegalArgumentException("$description exceeds ${limits.keys} JSON keys")
                    }
                } else {
                    countNode()
                }
            }

            ',', ':', ' ', '\t', '\r', '\n' -> {
                index++
            }

            else -> {
                countNode()
                index++
                while (index < text.length && text[index] !in JSON_TOKEN_DELIMITERS) {
                    index++
                }
            }
        }
    }
}

internal fun validateJsonComplexity(
    root: Any,
    limits: JsonComplexityLimits,
    description: String,
) {
    data class Pending(
        val value: Any?,
        val depth: Int,
    )

    var nodeCount = 1
    var keyCount = 0
    var stringBytes = 0
    val pending = ArrayDeque<Pending>()
    pending.add(Pending(root, 1))

    fun countString(value: String) {
        stringBytes += value.toByteArray(Charsets.UTF_8).size
        if (stringBytes > limits.stringBytes) {
            throw IllegalArgumentException("$description exceeds ${limits.stringBytes} JSON string bytes")
        }
    }

    fun add(
        value: Any?,
        depth: Int,
    ) {
        nodeCount++
        if (nodeCount > limits.nodes) {
            throw IllegalArgumentException("$description exceeds ${limits.nodes} JSON nodes")
        }
        pending.add(Pending(value, depth))
    }

    while (pending.isNotEmpty()) {
        val current = pending.removeLast()
        if (current.depth > limits.depth) {
            throw IllegalArgumentException("$description exceeds JSON nesting depth ${limits.depth}")
        }
        when (val value = current.value) {
            is JSONObject -> {
                val keys = value.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    keyCount++
                    if (keyCount > limits.keys) {
                        throw IllegalArgumentException("$description exceeds ${limits.keys} JSON keys")
                    }
                    countString(key)
                    add(value.opt(key), current.depth + 1)
                }
            }

            is JSONArray -> {
                for (index in 0 until value.length()) add(value.opt(index), current.depth + 1)
            }

            is String -> {
                countString(value)
            }
        }
    }
}
