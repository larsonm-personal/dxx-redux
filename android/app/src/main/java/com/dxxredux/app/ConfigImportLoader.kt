package com.dxxredux.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runInterruptible
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.charset.CodingErrorAction

internal const val MAX_CONFIG_IMPORT_BYTES = 1024 * 1024
internal const val MAX_CONFIG_IMPORT_DEPTH = 32
internal const val MAX_CONFIG_STRING_CHARS = 64 * 1024
internal const val MAX_CONFIG_CONTAINER_ENTRIES = 1024
internal const val MAX_CONFIG_SLOTS = 128
internal const val MAX_CONFIG_VALUES = 20_000

internal class ConfigImportLimitException(
    message: String,
) : IllegalArgumentException(message)

internal data class PreparedConfigImport(
    val json: JSONObject,
    val type: String,
)

internal sealed interface ConfigImportPreparation {
    data class Ready(
        val config: PreparedConfigImport,
    ) : ConfigImportPreparation

    data class Error(
        val message: String,
    ) : ConfigImportPreparation
}

internal fun readBoundedConfigText(
    input: InputStream,
    maxBytes: Int = MAX_CONFIG_IMPORT_BYTES,
): String {
    val output = ByteArrayOutputStream(minOf(maxBytes, 16 * 1024))
    val buffer = ByteArray(16 * 1024)
    var total = 0
    while (true) {
        val read = input.read(buffer)
        if (read < 0) break
        if (read == 0) {
            val byte = input.read()
            if (byte < 0) break
            if (total == maxBytes) throw ConfigImportLimitException("configuration file exceeds $maxBytes bytes")
            output.write(byte)
            total++
            continue
        }
        if (read > maxBytes - total) {
            throw ConfigImportLimitException("configuration file exceeds $maxBytes bytes")
        }
        output.write(buffer, 0, read)
        total += read
    }
    return Charsets.UTF_8
        .newDecoder()
        .onMalformedInput(CodingErrorAction.REPORT)
        .onUnmappableCharacter(CodingErrorAction.REPORT)
        .decode(ByteBuffer.wrap(output.toByteArray()))
        .toString()
}

private fun validateLexicalLimits(text: String) {
    var depth = 0
    var stringLength = 0
    var inString = false
    var escaped = false
    for (character in text) {
        if (inString) {
            if (escaped) {
                escaped = false
            } else if (character == '\\') {
                escaped = true
            } else if (character == '"') {
                inString = false
                stringLength = 0
            } else {
                stringLength++
                if (stringLength > MAX_CONFIG_STRING_CHARS) {
                    throw ConfigImportLimitException("configuration string exceeds $MAX_CONFIG_STRING_CHARS characters")
                }
            }
        } else {
            when (character) {
                '"' -> {
                    inString = true
                }

                '{', '[' -> {
                    depth++
                    if (depth > MAX_CONFIG_IMPORT_DEPTH) {
                        throw ConfigImportLimitException(
                            "configuration nesting exceeds $MAX_CONFIG_IMPORT_DEPTH levels",
                        )
                    }
                }

                '}', ']' -> {
                    depth--
                }
            }
        }
    }
}

private fun validateTreeLimits(root: JSONObject) {
    val pending = ArrayDeque<Any?>()
    pending.add(root)
    var values = 0
    while (pending.isNotEmpty()) {
        val value = pending.removeLast()
        values++
        if (values > MAX_CONFIG_VALUES) {
            throw ConfigImportLimitException("configuration exceeds $MAX_CONFIG_VALUES values")
        }
        when (value) {
            is JSONObject -> {
                if (value.length() > MAX_CONFIG_CONTAINER_ENTRIES) {
                    throw ConfigImportLimitException(
                        "configuration object exceeds $MAX_CONFIG_CONTAINER_ENTRIES entries",
                    )
                }
                val keys = value.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    if (key.length > MAX_CONFIG_STRING_CHARS) {
                        throw ConfigImportLimitException(
                            "configuration key exceeds $MAX_CONFIG_STRING_CHARS characters",
                        )
                    }
                    pending.add(value.get(key))
                }
            }

            is JSONArray -> {
                if (value.length() > MAX_CONFIG_CONTAINER_ENTRIES) {
                    throw ConfigImportLimitException(
                        "configuration array exceeds $MAX_CONFIG_CONTAINER_ENTRIES entries",
                    )
                }
                for (index in 0 until value.length()) pending.add(value.get(index))
            }

            is String -> {
                if (value.length > MAX_CONFIG_STRING_CHARS) {
                    throw ConfigImportLimitException(
                        "configuration string exceeds $MAX_CONFIG_STRING_CHARS characters",
                    )
                }
            }
        }
    }
    for (key in listOf("touch_layout_slots", "controller_config_slots")) {
        val slots = root.optJSONArray(key)
        if (slots != null && slots.length() > MAX_CONFIG_SLOTS) {
            throw ConfigImportLimitException("$key exceeds $MAX_CONFIG_SLOTS slots")
        }
    }
}

internal fun prepareConfigImport(input: InputStream): ConfigImportPreparation =
    try {
        val text = readBoundedConfigText(input)
        validateLexicalLimits(text)
        val json = JSONObject(text)
        validateTreeLimits(json)
        val type = HumanReadableConfig.detectConfigType(json)
        if (type == "unknown") {
            ConfigImportPreparation.Error("Error: unrecognized config type")
        } else {
            ConfigImportPreparation.Ready(PreparedConfigImport(json, type))
        }
    } catch (error: ConfigImportLimitException) {
        ConfigImportPreparation.Error("Error: ${error.message}")
    } catch (error: Exception) {
        ConfigImportPreparation.Error("Error: invalid configuration - ${error.message}")
    }

internal suspend fun <T> onConfigImportWorker(block: () -> T): T = runInterruptible(Dispatchers.IO, block)
