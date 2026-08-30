package com.dxxredux.app

import java.io.File
import java.io.FileInputStream
import java.security.MessageDigest

internal data class MissionContentIdentity(
    val sizeBytes: Long,
    val sha256: String,
    val chunkSizeBytes: Int,
    val chunkSha256: List<String>,
) {
    init {
        require(sizeBytes >= 0L)
        require(SHA256_PATTERN.matches(sha256))
        require(chunkSizeBytes > 0)
        require(chunkSha256.all(SHA256_PATTERN::matches))
        require(chunkSha256.size.toLong() == chunkCount(sizeBytes, chunkSizeBytes))
    }

    val contentId: String get() = "$sha256:$sizeBytes"

    companion object {
        const val DEFAULT_CHUNK_SIZE_BYTES = 1024 * 1024
        private val SHA256_PATTERN = Regex("[0-9a-f]{64}")

        fun compute(
            file: File,
            chunkSizeBytes: Int = DEFAULT_CHUNK_SIZE_BYTES,
            onProgress: (Long) -> Unit = {},
        ): MissionContentIdentity {
            require(file.isFile) { "Mission wrapper is missing: ${file.absolutePath}" }
            require(chunkSizeBytes > 0)
            val whole = MessageDigest.getInstance("SHA-256")
            val chunks = mutableListOf<String>()
            val buffer = ByteArray(chunkSizeBytes)
            var total = 0L
            FileInputStream(file).use { input ->
                while (true) {
                    var used = 0
                    while (used < buffer.size) {
                        val count = input.read(buffer, used, buffer.size - used)
                        if (count < 0) break
                        if (count == 0) continue
                        used += count
                    }
                    if (used == 0) break
                    whole.update(buffer, 0, used)
                    val chunk = MessageDigest.getInstance("SHA-256")
                    chunk.update(buffer, 0, used)
                    chunks += chunk.digest().toHex()
                    total += used
                    onProgress(total)
                    if (used < buffer.size) break
                }
            }
            check(total == file.length()) { "Mission wrapper changed while hashing: ${file.name}" }
            return MissionContentIdentity(total, whole.digest().toHex(), chunkSizeBytes, chunks)
        }

        fun isValidSha256(value: String): Boolean = SHA256_PATTERN.matches(value)

        private fun chunkCount(
            sizeBytes: Long,
            chunkSizeBytes: Int,
        ): Long = if (sizeBytes == 0L) 0L else (sizeBytes - 1L) / chunkSizeBytes + 1L

        private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it) }
    }
}
