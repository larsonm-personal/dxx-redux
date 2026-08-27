package com.dxxredux.app

import java.io.File
import java.util.Locale

/** Canonical identity for portable ASCII game-data names, including repair of names folded under Turkish. */
internal fun portableGameFilenameIdentity(value: String): String = value.lowercase(Locale.ROOT).replace('\u0131', 'i')

/** Treat provider metadata as a label, never as a path. */
internal fun requireSafeProviderDisplayName(value: String): String {
    require(value.isNotBlank()) { "Provider returned an empty file name" }
    require(value.length <= 255) { "Provider file name is too long" }
    require(value != "." && value != "..") { "Provider returned a path component as a file name" }
    require(value.none { it == '/' || it == '\\' || it == ':' || it.code < 0x20 || it.code == 0x7f }) {
        "Provider file name contains a path separator or control character"
    }
    require(!value.endsWith('.') && !value.endsWith(' ')) { "Provider file name has a reserved suffix" }
    require(File(value).name == value && !File(value).isAbsolute) { "Provider returned a path instead of a file name" }
    val windowsStem = value.substringBefore('.').trimEnd().uppercase(Locale.ROOT)
    require(
        windowsStem !in setOf("CON", "PRN", "AUX", "NUL") &&
            !Regex("^(COM|LPT)[1-9]$").matches(windowsStem),
    ) { "Provider file name is reserved" }
    return value
}

object GameFileFormats {
    const val GAME_D1 = "d1"
    const val GAME_D2 = "d2"
    const val GAME_BOTH = "both"
    const val GAME_UNKNOWN = "unknown"

    const val MISSION_ZIP_DESCRIPTOR = "mission_descriptor"
    const val MISSION_ZIP_HOG = "mission_hog"
    const val MISSION_ZIP_MOD_ARCHIVE = "mod_archive"
    const val MISSION_ZIP_DOCUMENTATION = "documentation"
    const val MISSION_ZIP_OTHER = "other"

    data class MissionDescriptor(
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
        val modeFlags: Set<String> = emptySet(),
        val valid: Boolean = true,
        val problem: String? = null,
    ) {
        val displayName: String
            get() =
                name?.takeIf {
                    it.isNotBlank()
                } ?: path
                    .substringAfterLast('/')
                    .substringBeforeLast('.')
    }

    private data class FormatInfo(
        val label: String,
        val roleLabel: String = label,
        val gameHint: String? = null,
        val gameImport: Boolean = false,
        val discExtract: Boolean = false,
        val setGameData: Boolean = false,
        val gogAudio: Boolean = false,
        val baseReplacement: Boolean = false,
        val textureReplacement: Boolean = false,
        val soundReplacement: Boolean = false,
        val music: Boolean = false,
        val documentation: Boolean = false,
        val missionZipRole: String = MISSION_ZIP_OTHER,
    )

    private val formats =
        mapOf(
            "hog" to
                FormatInfo(
                    "Mission or game archive",
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                    missionZipRole = MISSION_ZIP_HOG,
                ),
            "pig" to
                FormatInfo(
                    "Texture and sound data",
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "ham" to
                FormatInfo(
                    "Robot and weapon data",
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "vham" to FormatInfo("Variant robot and weapon data", baseReplacement = true),
            "s11" to
                FormatInfo(
                    "11 kHz sound data",
                    roleLabel = "Sound effects",
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "s22" to
                FormatInfo(
                    "22 kHz sound data",
                    roleLabel = "Sound effects",
                    gameHint = GAME_D2,
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "dem" to FormatInfo("Demo recording", gameImport = true, discExtract = true),
            "mvl" to
                FormatInfo(
                    "Movie library archive",
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "mn2" to
                FormatInfo(
                    "Descent 2 mission descriptor",
                    roleLabel = "Mission descriptor",
                    gameHint = GAME_D2,
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                    missionZipRole = MISSION_ZIP_DESCRIPTOR,
                ),
            "msn" to
                FormatInfo(
                    "Descent 1 mission descriptor",
                    roleLabel = "Mission descriptor",
                    gameHint = GAME_D1,
                    gameImport = true,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                    missionZipRole = MISSION_ZIP_DESCRIPTOR,
                ),
            "rdl" to
                FormatInfo(
                    "Descent 1 level",
                    roleLabel = "D1 level",
                    gameHint = GAME_D1,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "rl2" to
                FormatInfo(
                    "Descent 2 level",
                    roleLabel = "D2 level",
                    gameHint = GAME_D2,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "sdl" to
                FormatInfo(
                    "Descent 1 secret level",
                    roleLabel = "Secret level",
                    gameHint = GAME_D1,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "sl2" to
                FormatInfo(
                    "Descent 2 secret level",
                    roleLabel = "Secret level",
                    gameHint = GAME_D2,
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "dxa" to
                FormatInfo(
                    "Game mod",
                    roleLabel = "Bundled mod archive",
                    discExtract = true,
                    setGameData = true,
                    missionZipRole = MISSION_ZIP_MOD_ARCHIVE,
                ),
            "pog" to
                FormatInfo(
                    "Texture override pack",
                    roleLabel = "Texture override",
                    discExtract = true,
                    setGameData = true,
                    baseReplacement = true,
                ),
            "dtx" to
                FormatInfo("D2X-XL texture pack", discExtract = true, setGameData = true, textureReplacement = true),
            "hxm" to FormatInfo("Robot data patch", discExtract = true),
            "sow" to FormatInfo("SOW archive", discExtract = true),
            "gog" to FormatInfo("GOG CD audio image", gameImport = true, gogAudio = true),
            "inst" to FormatInfo("GOG CD audio cue sheet", gameImport = true, gogAudio = true),
            "ied" to FormatInfo("Inferno editor file"),
            "txb" to FormatInfo("Encoded briefing or text", roleLabel = "Briefing text"),
            "ctb" to FormatInfo("Cockpit data", roleLabel = "Cockpit data"),
            "sng" to FormatInfo("Song list"),
            "pcx" to FormatInfo("Briefing or cutscene image"),
            "hmp" to FormatInfo("HMI MIDI music", music = true),
            "hmq" to FormatInfo("HMI MIDI music", music = true),
            "mid" to FormatInfo("MIDI music", music = true),
            "raw" to FormatInfo("Raw PCM audio", soundReplacement = true),
            "voc" to FormatInfo("VOC audio", soundReplacement = true),
            "wav" to FormatInfo("WAV audio file", roleLabel = "Audio", soundReplacement = true, music = true),
            "mp3" to FormatInfo("MP3 music file", roleLabel = "Audio", music = true),
            "ogg" to FormatInfo("Ogg Vorbis music file", roleLabel = "Audio", music = true),
            "flac" to FormatInfo("FLAC music file", roleLabel = "Audio", music = true),
            "m3u" to FormatInfo("Music playlist", music = true),
            "png" to FormatInfo("PNG image", textureReplacement = true),
            "jpg" to FormatInfo("JPEG image", textureReplacement = true),
            "tga" to FormatInfo("TGA image", textureReplacement = true),
            "ktx2" to FormatInfo("KTX2 texture", textureReplacement = true),
            "plr" to FormatInfo("Pilot file"),
            "plx" to FormatInfo("Extended pilot settings"),
            "eff" to FormatInfo("Effects settings"),
            "ngp" to FormatInfo("Network game profile"),
            "cfg" to FormatInfo("Game configuration"),
            "json" to FormatInfo("Launcher settings"),
            "jsonl" to FormatInfo("Launcher event log"),
            "txt" to FormatInfo("Text file", documentation = true, missionZipRole = MISSION_ZIP_DOCUMENTATION),
            "md" to FormatInfo("Markdown file", documentation = true, missionZipRole = MISSION_ZIP_DOCUMENTATION),
            "pdf" to FormatInfo("PDF document", documentation = true, missionZipRole = MISSION_ZIP_DOCUMENTATION),
            "rtf" to FormatInfo("Rich text file", documentation = true, missionZipRole = MISSION_ZIP_DOCUMENTATION),
            "doc" to FormatInfo("Word document", documentation = true, missionZipRole = MISSION_ZIP_DOCUMENTATION),
            "docx" to FormatInfo("Word document", documentation = true, missionZipRole = MISSION_ZIP_DOCUMENTATION),
            "log" to FormatInfo("Log file"),
            "exe" to FormatInfo("Windows installer"),
            "pkg" to FormatInfo("Mac installer"),
            "zip" to FormatInfo("ZIP archive"),
            "7z" to FormatInfo("7z archive"),
            "rar" to FormatInfo("RAR archive"),
            "sit" to FormatInfo("StuffIt archive"),
            "hqx" to FormatInfo("BinHex archive"),
            "bin" to FormatInfo("CD disc image"),
            "img" to FormatInfo("CD disc image"),
            "iso" to FormatInfo("CD disc image"),
            "cue" to FormatInfo("CD cue sheet"),
        )

    val gameImportExtensions: Set<String> = formats.filterValues { it.gameImport }.keys
    val discExtractExtensions: Set<String> = formats.filterValues { it.discExtract }.keys
    val setGameDataExtensions: Set<String> = formats.filterValues { it.setGameData }.keys
    val gogAudioExtensions: Set<String> = formats.filterValues { it.gogAudio }.keys

    /*
     * Mac HFS/STI media needs auxiliary config, text, and palette files and
     * intentionally does not use the generic ISO data-track allowlist.
     */
    val macDiscExtractExtensions: Set<String> =
        setOf("hog", "ham", "pig", "s11", "s22", "mn2", "mvl", "sow", "dxa", "cfg", "txt", "256", "msn", "dem")

    private val savedGameExtensions =
        buildSet {
            for (i in 0..9) {
                add("sg$i")
                add("mg$i")
            }
        }

    fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

    fun extensionOf(filename: String): String {
        val leaf = leafName(filename).lowercase(Locale.US)
        val ext = leaf.substringAfterLast('.', "")
        if (ext == leaf) return ""
        if (ext.length >= 3 && ext.take(3) == "dxa" && (ext.length == 3 || !ext[3].isLetterOrDigit())) {
            return "dxa"
        }
        return ext
    }

    fun isDxa(filename: String): Boolean = extensionOf(filename) == "dxa"

    fun stripDxaSuffix(filename: String): String {
        val dotIndex = filename.lastIndexOf('.')
        return if (dotIndex >= 0 && isDxa(filename)) filename.substring(0, dotIndex) else filename
    }

    fun typeLabel(filename: String): String {
        val leaf = leafName(filename).lowercase(Locale.US)
        val ext = extensionOf(leaf)
        return when {
            leaf == "assets.json" -> "Game file manifest"
            leaf == ".saf_manifest.json" -> "SAF game-file link manifest"
            leaf == "audio_sources.json" -> "CD audio source settings"
            leaf == "audio_playlist.json" -> "Generated CD audio playlist"
            leaf == "file_sets.json" -> "Imported file set settings"
            leaf == "import_location.txt" -> "Imported files location setting"
            ext in savedGameExtensions -> "Saved game"
            ext.isEmpty() -> "File"
            else -> formats[ext]?.label ?: ".$ext file"
        }
    }

    fun extensionDescription(filename: String): String {
        val ext = extensionOf(filename)
        val label = typeLabel(filename)
        return if (ext.isEmpty()) label else ".$ext - ${lowerFirst(label)}"
    }

    fun storagePurpose(
        file: File,
        relativePath: String,
        importedRootFile: Boolean,
    ): String {
        val name = file.name.lowercase(Locale.US)
        val path = relativePath.replace('\\', '/').lowercase(Locale.US)
        val importedPath = importedRootFile || path.startsWith("imported/") || path.startsWith("sets/")
        return when {
            isGeneratedMergedStorageArtifact(file) -> "Imported (merged) CD audio"
            "/route-cache/" in "/$path" && ".samples-" in name -> "Route analysis checkpoint"
            "/route-cache/" in "/$path" && name.endsWith(".bin") -> "Route analysis cache"
            "/route-cache/" in "/$path" && ".tmp-" in name -> "Temporary route analysis file"
            importedPath -> typeLabel(name)
            path.startsWith("d1x-redux/") || path.startsWith("d2x-redux/") -> typeLabel(name)
            path.startsWith("mods/") -> typeLabel(name)
            path.startsWith("custom_music/") -> typeLabel(name)
            path.startsWith("debuglogs/") -> "Debug log"
            path.startsWith("tombstones/") || path.startsWith("crashlogs/") -> "Crash report"
            path.startsWith("tmp/") -> "Temporary import file"
            else -> typeLabel(name)
        }
    }

    fun roleLabel(filename: String): String = formats[extensionOf(filename)]?.roleLabel ?: typeLabel(filename)

    fun missionZipRoleForFile(filename: String): String =
        formats[extensionOf(filename)]?.missionZipRole ?: MISSION_ZIP_OTHER

    fun missionZipRoleLabel(role: String): String =
        when (role) {
            MISSION_ZIP_DESCRIPTOR -> "Mission descriptor"
            MISSION_ZIP_HOG -> "Mission assets"
            MISSION_ZIP_MOD_ARCHIVE -> "Bundled mod archive"
            MISSION_ZIP_DOCUMENTATION -> "Documentation"
            else -> "Other file"
        }

    fun hasGameImportExtension(filename: String): Boolean = extensionOf(filename) in gameImportExtensions

    fun hasDiscExtractExtension(filename: String): Boolean = extensionOf(filename) in discExtractExtensions

    fun isSetGameData(filename: String): Boolean = extensionOf(filename) in setGameDataExtensions

    fun isGogAudioFile(filename: String): Boolean = formats[extensionOf(filename)]?.gogAudio == true

    fun isMissionDescriptor(filename: String): Boolean = extensionOf(filename) in setOf("mn2", "msn")

    fun isMetadataInspectable(filename: String): Boolean = extensionOf(filename) in setOf("hog", "dxa", "pig", "pog")

    fun isLevelFile(filename: String): Boolean = gameForLevel(filename) != null

    fun isTextureReplacement(filename: String): Boolean = formats[extensionOf(filename)]?.textureReplacement == true

    fun isSoundReplacement(filename: String): Boolean = formats[extensionOf(filename)]?.soundReplacement == true

    fun isMusicFile(filename: String): Boolean = formats[extensionOf(filename)]?.music == true

    fun isDocumentation(filename: String): Boolean = formats[extensionOf(filename)]?.documentation == true

    fun isBaseReplacement(filename: String): Boolean = formats[extensionOf(filename)]?.baseReplacement == true

    fun modCategoryLabel(filename: String): String? =
        when {
            isBaseReplacement(filename) -> "Base game file replacements"
            isTextureReplacement(filename) -> "Texture replacements"
            isSoundReplacement(filename) -> "Individual sound file replacements"
            isMusicFile(filename) -> "Music files"
            isDocumentation(filename) -> "Documentation"
            else -> null
        }

    fun gameForDescriptor(filename: String): String? =
        when (extensionOf(filename)) {
            "mn2" -> GAME_D2
            "msn" -> GAME_D1
            else -> null
        }

    fun gameForLevel(filename: String): String? =
        when (extensionOf(filename)) {
            "rl2", "sl2" -> GAME_D2
            "rdl", "sdl" -> GAME_D1
            else -> null
        }

    fun gameHint(
        filename: String,
        children: List<String> = emptyList(),
        fallback: String = GAME_BOTH,
    ): String {
        val hints =
            (listOf(filename) + children)
                .mapNotNull { path -> formats[extensionOf(path)]?.gameHint ?: gameForLevel(path) }
                .distinct()
        return when {
            hints.size == 1 -> {
                hints.single()
            }

            hints.size > 1 -> {
                GAME_BOTH
            }

            else -> {
                val lower = filename.lowercase(Locale.US)
                when {
                    lower.contains("d1") && !lower.contains("d2") -> GAME_D1
                    lower.contains("d2") && !lower.contains("d1") -> GAME_D2
                    else -> fallback
                }
            }
        }
    }

    fun parseMissionDescriptor(
        path: String,
        text: String,
    ): MissionDescriptor {
        val maxMissionLevels = 127
        val maxMissionFilenameLength = 12
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
                val level = cleanMissionListLine(line).substringBefore(',').trim()
                if (line.isBlank() || line.startsWith(";") || line.startsWith("#") ||
                    '=' in line || level.isBlank() || level.length > maxMissionFilenameLength
                ) {
                    problem = problem ?: "Invalid ordinary level list"
                } else {
                    levels += level
                }
                continue
            }
            if (remainingSecrets > 0) {
                remainingSecrets--
                val secretLine = cleanMissionListLine(line)
                val secret = secretLine.substringBefore(',').trim()
                val origins =
                    secretLine
                        .substringAfter(',', "")
                        .split(',')
                        .mapNotNull { it.trim().toIntOrNull() }
                if (line.isBlank() || line.startsWith(";") || line.startsWith("#") ||
                    '=' in line || ',' !in secretLine || secret.isBlank() ||
                    secret.length > maxMissionFilenameLength || origins.isEmpty() ||
                    origins.any { it !in 1..levels.size }
                ) {
                    problem = problem ?: "Invalid secret level list"
                } else {
                    secrets += secret
                    // D2 accepts several ordinary-level entry points for one
                    // secret level. Keep the first as the canonical origin used
                    // by launcher ordering while validating every listed entry.
                    secretOrigins += origins.first()
                }
                continue
            }
            if (line.isBlank() || line.startsWith(";") || line.startsWith("#")) continue
            val eq = line.indexOf('=')
            if (eq < 0) continue
            val key = line.substring(0, eq).trim().lowercase(Locale.US)
            val value = cleanMissionValue(line.substring(eq + 1))
            values[key] = value
            when (key) {
                "num_levels" -> {
                    declaredLevelCount = value.toIntOrNull()
                    if (declaredLevelCount == null || declaredLevelCount !in 1..maxMissionLevels) {
                        problem = problem ?: "Invalid ordinary level count"
                    }
                    remainingLevels = declaredLevelCount ?: 0
                }

                "num_secrets" -> {
                    declaredSecretLevelCount = value.toIntOrNull()
                    if (declaredSecretLevelCount == null || declaredSecretLevelCount !in 0..maxMissionLevels) {
                        problem = problem ?: "Invalid secret level count"
                    }
                    remainingSecrets = declaredSecretLevelCount ?: 0
                }

                "briefing", "ending", "!ham", "ham", "hxm", "pig", "hog" -> {
                    if (value.isNotBlank()) assetReferences[assetReferenceLabel(key)] = value
                }
            }
        }
        val name = preferredMissionName(path, values)
        if (name.isNullOrBlank()) problem = problem ?: "Mission name is missing"
        if (declaredLevelCount == null || remainingLevels != 0 || levels.size != declaredLevelCount) {
            problem = problem ?: "Ordinary level list is incomplete"
        }
        val expectedSecrets = declaredSecretLevelCount ?: 0
        if (remainingSecrets != 0 || secrets.size != expectedSecrets || secretOrigins.size != expectedSecrets) {
            problem = problem ?: "Secret level list is incomplete"
        }
        return MissionDescriptor(
            path = normalizePath(path),
            name = name,
            type = values["type"],
            author = values["author"],
            editor = values["editor"],
            levelNames = levels,
            secretLevelNames = secrets,
            secretLevelOrigins = secretOrigins,
            declaredLevelCount = declaredLevelCount,
            declaredSecretLevelCount = declaredSecretLevelCount,
            assetReferences = assetReferences,
            game = detectMissionGame(path, levels),
            modeFlags =
                setOf("normal", "coop", "anarchy", "robo_anarchy", "capture_flag", "hoard")
                    .filterTo(linkedSetOf()) { key ->
                        values[key]?.lowercase(Locale.US) in setOf("yes", "true", "1")
                    },
            valid = problem == null,
            problem = problem,
        )
    }

    private fun detectMissionGame(
        path: String,
        levelNames: List<String>,
    ): String {
        gameForDescriptor(path)?.let { return it }
        val levelHints = levelNames.mapNotNull { gameForLevel(it) }.distinct()
        return when {
            levelHints.size == 1 -> levelHints.single()
            levelHints.size > 1 -> GAME_BOTH
            else -> GAME_BOTH
        }
    }

    private fun firstMissionValue(
        values: Map<String, String>,
        vararg keys: String,
    ): String? = keys.firstNotNullOfOrNull { values[it]?.takeIf { value -> value.isNotBlank() } }

    private fun preferredMissionName(
        path: String,
        values: Map<String, String>,
    ): String? {
        val name = firstMissionValue(values, "name")
        val enhancedName = firstMissionValue(values, "xname", "zname", "!name")
        val descriptorStem = leafName(path).substringBeforeLast('.')
        return if (enhancedName != null && name.equals(descriptorStem, ignoreCase = true)) {
            enhancedName
        } else {
            name ?: enhancedName
        }
    }

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')

    private fun cleanMissionValue(value: String): String = value.substringBefore(';').trim()

    private fun cleanMissionListLine(value: String): String = value.substringBefore(';').trim()

    private fun assetReferenceLabel(key: String): String =
        when (key) {
            "briefing" -> "Briefing"
            "ending" -> "Ending"
            "!ham", "ham" -> "HAM"
            "hxm" -> "HXM"
            "pig" -> "PIG"
            "hog" -> "HOG"
            else -> key.uppercase(Locale.US)
        }

    private fun lowerFirst(text: String): String =
        if (text.isEmpty()) text else text.substring(0, 1).lowercase(Locale.US) + text.substring(1)
}
