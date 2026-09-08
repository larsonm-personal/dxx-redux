package com.dxxredux.app

import java.util.Locale
import java.nio.ByteBuffer
import java.nio.charset.CharacterCodingException
import java.nio.charset.CodingErrorAction
import java.nio.charset.Charset

data class ParsedMissionDescriptor(
    val path: String,
    val name: String?,
    val type: String?,
    val author: String?,
    val editor: String?,
    val levelNames: List<String>,
    val secretLevelNames: List<String>,
    val secretLevelOrigins: List<Int>,
    val declaredLevelCount: Int?,
    val declaredSecretLevelCount: Int?,
    val assetReferences: Map<String, String>,
    val game: String,
    val modeFlags: Set<String>,
    val valid: Boolean,
    val problem: String?,
) {
    val displayName: String
        get() = name?.takeIf { it.isNotBlank() } ?: path.substringAfterLast('/').substringBeforeLast('.')
}

object MissionDescriptorPolicy {
    fun decode(bytes: ByteArray): String =
        try {
            Charsets.UTF_8.newDecoder()
                .onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT)
                .decode(ByteBuffer.wrap(bytes)).toString()
        } catch (_: CharacterCodingException) {
            bytes.toString(Charset.forName("windows-1252"))
        }

    fun parse(path: String, text: String): ParsedMissionDescriptor {
        val values = linkedMapOf<String, String>()
        val assetReferences = linkedMapOf<String, String>()
        val levels = mutableListOf<String>()
        val secrets = mutableListOf<String>()
        val secretOrigins = mutableListOf<Int>()
        var remainingLevels = 0
        var remainingSecrets = 0
        var declaredLevelCount: Int? = null
        var declaredSecretLevelCount: Int? = null
        var problem: String? = null
        for (rawLine in text.lineSequence()) {
            val line = rawLine.trim()
            if (remainingLevels > 0) {
                remainingLevels--
                val level = cleanListLine(line).substringBefore(',').trim()
                if (line.isBlank() || line.startsWith(";") || line.startsWith("#") ||
                    '=' in line || level.isBlank() || level.length > 12
                ) problem = problem ?: "Invalid ordinary level list" else levels += level
                continue
            }
            if (remainingSecrets > 0) {
                remainingSecrets--
                val secretLine = cleanListLine(line)
                val secret = secretLine.substringBefore(',').trim()
                val origins = secretLine.substringAfter(',', "").split(',').mapNotNull { it.trim().toIntOrNull() }
                if (line.isBlank() || line.startsWith(";") || line.startsWith("#") ||
                    '=' in line || ',' !in secretLine || secret.isBlank() || secret.length > 12 ||
                    origins.isEmpty() || origins.any { it !in 1..levels.size }
                ) {
                    problem = problem ?: "Invalid secret level list"
                } else {
                    secrets += secret
                    secretOrigins += origins.first()
                }
                continue
            }
            if (line.isBlank() || line.startsWith(";") || line.startsWith("#")) continue
            val eq = line.indexOf('=')
            if (eq < 0) continue
            val key = line.substring(0, eq).trim().lowercase(Locale.US)
            val value = cleanValue(line.substring(eq + 1))
            values[key] = value
            when (key) {
                "num_levels" -> {
                    declaredLevelCount = value.toIntOrNull()
                    if (declaredLevelCount == null || declaredLevelCount !in 1..127) problem = problem ?: "Invalid ordinary level count"
                    remainingLevels = declaredLevelCount ?: 0
                }
                "num_secrets" -> {
                    declaredSecretLevelCount = value.toIntOrNull()
                    if (declaredSecretLevelCount == null || declaredSecretLevelCount !in 0..127) problem = problem ?: "Invalid secret level count"
                    remainingSecrets = declaredSecretLevelCount ?: 0
                }
                "briefing", "ending", "!ham", "ham", "hxm", "pig", "hog" ->
                    if (value.isNotBlank()) assetReferences[assetReferenceLabel(key)] = value
            }
        }
        val name = preferredName(path, values)
        if (name.isNullOrBlank()) problem = problem ?: "Mission name is missing"
        if (declaredLevelCount == null || remainingLevels != 0 || levels.size != declaredLevelCount) {
            problem = problem ?: "Ordinary level list is incomplete"
        }
        val expectedSecrets = declaredSecretLevelCount ?: 0
        if (remainingSecrets != 0 || secrets.size != expectedSecrets || secretOrigins.size != expectedSecrets) {
            problem = problem ?: "Secret level list is incomplete"
        }
        return ParsedMissionDescriptor(
            path.replace('\\', '/').trim('/'), name, values["type"], values["author"], values["editor"],
            levels, secrets, secretOrigins, declaredLevelCount, declaredSecretLevelCount, assetReferences,
            detectGame(path, levels),
            setOf("normal", "coop", "anarchy", "robo_anarchy", "capture_flag", "hoard")
                .filterTo(linkedSetOf()) { values[it]?.lowercase(Locale.US) in setOf("yes", "true", "1") },
            problem == null, problem,
        )
    }

    private fun preferredName(path: String, values: Map<String, String>): String? {
        val name = values["name"]?.takeIf { it.isNotBlank() }
        val enhanced = listOf("xname", "zname", "!name").firstNotNullOfOrNull { values[it]?.takeIf(String::isNotBlank) }
        val stem = path.replace('\\', '/').substringAfterLast('/').substringBeforeLast('.')
        return if (enhanced != null && name.equals(stem, ignoreCase = true)) enhanced else name ?: enhanced
    }

    private fun detectGame(path: String, levels: List<String>): String {
        when (extension(path)) { "mn2" -> return "d2"; "msn" -> return "d1" }
        val hints = levels.mapNotNull { when (extension(it)) { "rl2", "sl2" -> "d2"; "rdl", "sdl" -> "d1"; else -> null } }.distinct()
        return hints.singleOrNull() ?: "both"
    }

    private fun extension(path: String) = path.substringAfterLast('/').substringAfterLast('\\').substringAfterLast('.', "").lowercase(Locale.US)
    private fun cleanValue(value: String) = value.substringBefore(';').trim()
    private fun cleanListLine(value: String) = value.substringBefore(';').trim()
    private fun assetReferenceLabel(key: String) = when (key) {
        "briefing" -> "Briefing"; "ending" -> "Ending"; "!ham", "ham" -> "HAM"
        "hxm" -> "HXM"; "pig" -> "PIG"; "hog" -> "HOG"; else -> key.uppercase(Locale.US)
    }
}
