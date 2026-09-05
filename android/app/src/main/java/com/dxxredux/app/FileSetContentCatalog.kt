package com.dxxredux.app

import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.util.Locale

internal enum class FileSetFileClass {
    BASE,
    LAUNCHER_STATE,
    PLAYER_STATE,
    MANAGED_CONTENT,
}

internal data class FileSetContentEntry(
    val id: String,
    val displayName: String,
    val game: String,
    val kind: String,
    val files: List<File>,
    val versionName: String?,
    val sourceUri: String?,
    val problem: String? = null,
    val enabled: Boolean = true,
    val order: Int = 0,
    val virtualPaths: List<String> = files.map { it.name },
) {
    val totalBytes: Long = files.sumOf { it.length() }
}

/**
 * Discovers user-owned content in a file set without treating launcher and
 * player bookkeeping as mods. Each managed file is assigned to exactly one
 * logical entry.
 */
internal object FileSetContentCatalog {
    const val KIND_LOOSE_MISSION = "loose_mission"
    const val KIND_MOD = "mod"
    const val KIND_MUSIC = "music"
    const val KIND_DEMO = "demo"
    const val KIND_OTHER = "other"

    private val launcherStateNames =
        setOf(
            "assets.json",
            ".saf_manifest.json",
            "content_state.json",
            "audio_sources.json",
            "audio_playlist.json",
        )
    private val launcherStateDirectories = setOf(".content", ".content_projection")
    private val atomicStateName =
        Regex("^\\..+\\.[0-9a-f]{8}-[0-9a-f-]{27}\\.(tmp|old|copy|migration|retired)$")
    private val playerExtensions =
        buildSet {
            addAll(setOf("plr", "plx", "eff", "ngp", "cfg"))
            for (index in 0..9) {
                add("sg$index")
                add("mg$index")
            }
        }

    fun classify(
        relativePath: String,
        isDirectory: Boolean = false,
    ): FileSetFileClass {
        val normalized = normalizePath(relativePath)
        val parts = normalized.split('/').filter { it.isNotEmpty() }
        val leaf = parts.lastOrNull()?.lowercase(Locale.US).orEmpty()
        if (parts.any { it.lowercase(Locale.US) in launcherStateDirectories }) {
            return FileSetFileClass.LAUNCHER_STATE
        }
        if (leaf in launcherStateNames || atomicStateName.matches(leaf) || leaf.endsWith(".migration.tmp")) {
            return FileSetFileClass.LAUNCHER_STATE
        }
        if (isDirectory) return FileSetFileClass.MANAGED_CONTENT
        if (parts.size == 1 && portableGameFilenameIdentity(leaf) in ALL_GAME_FILENAMES) {
            return FileSetFileClass.BASE
        }
        if (GameFileFormats.extensionOf(leaf) in playerExtensions ||
            parts.any { it.equals("Players", ignoreCase = true) }
        ) {
            return FileSetFileClass.PLAYER_STATE
        }
        return FileSetFileClass.MANAGED_CONTENT
    }

    fun scan(setDir: File): List<FileSetContentEntry> {
        val root = setDir.canonicalFile
        val registeredCdAudioPaths = registeredCdAudioPaths(setDir)
        val managedFiles =
            setDir
                .walkTopDown()
                .onEnter { dir ->
                    dir == setDir || classify(dir.relativeTo(setDir).invariantSeparatorsPath, isDirectory = true) !=
                        FileSetFileClass.LAUNCHER_STATE
                }.filter { file ->
                    file.isFile &&
                        classify(file.relativeTo(setDir).invariantSeparatorsPath) == FileSetFileClass.MANAGED_CONTENT
                }.map { it.canonicalFile }
                .filter { it.toPath().startsWith(root.toPath()) }
                .filter { relativeIdentity(root, it) !in registeredCdAudioPaths }
                .sortedBy { relativeIdentity(root, it) }
                .toList()
        if (managedFiles.isEmpty()) return emptyList()

        val filesByDirectory =
            managedFiles.groupBy { it.parentFile?.canonicalFile }.mapValues { (_, files) ->
                files.associateBy { portableGameFilenameIdentity(it.name) }
            }
        val descriptors = managedFiles.filter { GameFileFormats.isMissionDescriptor(it.name) }
        val descriptorInfo = descriptors.associateWith(::parseDescriptor)
        val adjacency = managedFiles.associateWith { linkedSetOf<File>() }.toMutableMap()
        for (descriptor in descriptors) {
            val siblings = filesByDirectory[descriptor.parentFile?.canonicalFile].orEmpty()
            val parsed = descriptorInfo.getValue(descriptor)
            val referencedNames =
                buildSet {
                    add("${descriptor.nameWithoutExtension}.hog")
                    addAll(parsed?.levelNames.orEmpty())
                    addAll(parsed?.secretLevelNames.orEmpty())
                    addAll(parsed?.assetReferences?.values.orEmpty())
                }
            for (name in referencedNames) {
                val referenced = siblings[portableGameFilenameIdentity(GameFileFormats.leafName(name))] ?: continue
                adjacency.getValue(descriptor).add(referenced)
                adjacency.getValue(referenced).add(descriptor)
            }
        }
        for (cue in managedFiles.filter { GameFileFormats.extensionOf(it.name) == "cue" }) {
            val siblings = filesByDirectory[cue.parentFile?.canonicalFile].orEmpty()
            for (name in cueReferencedFiles(cue)) {
                val referenced = siblings[portableGameFilenameIdentity(GameFileFormats.leafName(name))] ?: continue
                adjacency.getValue(cue).add(referenced)
                adjacency.getValue(referenced).add(cue)
            }
        }
        for (demo in managedFiles.filter { it.name.endsWith(InputDemoManager.INPUT_DEMO_EXTENSION, true) }) {
            val siblings = filesByDirectory[demo.parentFile?.canonicalFile].orEmpty()
            val referencedNames =
                listOf(
                    demo.name + InputDemoManager.INPUT_DEMO_RNG_TRACE_SUFFIX,
                    demo.name.dropLast(InputDemoManager.INPUT_DEMO_EXTENSION.length) +
                        InputDemoManager.CLASSIC_DEMO_EXTENSION,
                )
            for (name in referencedNames) {
                val referenced = siblings[portableGameFilenameIdentity(name)] ?: continue
                adjacency.getValue(demo).add(referenced)
                adjacency.getValue(referenced).add(demo)
            }
        }

        val manifestEntries = AssetManifest(setDir).load().associateBy { portableGameFilenameIdentity(it.filename) }
        val visited = mutableSetOf<File>()
        val entries = mutableListOf<FileSetContentEntry>()
        for (file in managedFiles) {
            if (!visited.add(file)) continue
            val component = mutableListOf<File>()
            val pending = ArrayDeque<File>()
            pending.add(file)
            while (pending.isNotEmpty()) {
                val current = pending.removeFirst()
                component += current
                for (linked in adjacency.getValue(current)) {
                    if (visited.add(linked)) pending.add(linked)
                }
            }
            val componentDescriptors = component.filter { it in descriptorInfo }
            val primaryDescriptor = componentDescriptors.minByOrNull { relativeIdentity(root, it) }
            val parsed = primaryDescriptor?.let { descriptorInfo[it] }
            val kind = if (primaryDescriptor != null) KIND_LOOSE_MISSION else kindFor(component)
            val orderedFiles = component.sortedBy { relativeIdentity(root, it) }
            val relativePaths = orderedFiles.map { relativeIdentity(root, it) }
            val tracked =
                component.mapNotNull { item ->
                    if (item.parentFile?.canonicalFile == root) {
                        manifestEntries[portableGameFilenameIdentity(item.name)]
                    } else {
                        null
                    }
                }
            entries +=
                FileSetContentEntry(
                    id = stableId(kind, relativePaths),
                    displayName = parsed?.name?.takeIf { it.isNotBlank() } ?: displayName(component),
                    game = parsed?.game ?: GameFileFormats.gameHint(component.first().name),
                    kind = kind,
                    files = orderedFiles,
                    versionName = tracked.mapNotNull { it.versionName }.distinct().singleOrNull(),
                    sourceUri = tracked.mapNotNull { it.sourceUri }.distinct().singleOrNull(),
                    problem = parsed?.problem,
                    virtualPaths =
                        orderedFiles.map { item ->
                            val path = relativePath(root, item)
                            if (kind == KIND_LOOSE_MISSION && !path.startsWith("missions/", ignoreCase = true)) {
                                "missions/$path"
                            } else {
                                path
                            }
                        },
                )
        }
        return entries.sortedWith(compareBy({ it.kind }, { it.displayName.lowercase(Locale.US) }, { it.id }))
    }

    private fun parseDescriptor(file: File): GameFileFormats.MissionDescriptor? =
        runCatching { GameFileFormats.parseMissionDescriptor(file.name, file.readText()) }.getOrNull()

    private fun cueReferencedFiles(file: File): List<String> =
        runCatching {
            file.useLines { lines ->
                lines
                    .mapNotNull { line ->
                        CUE_FILE_PATTERN.matchEntire(line)?.let { match ->
                            match.groupValues[1].ifEmpty { match.groupValues[2] }
                        }
                    }.toList()
            }
        }.getOrDefault(emptyList())

    private fun kindFor(files: List<File>): String =
        when {
            files.any { GameFileFormats.isDxa(it.name) } -> KIND_MOD

            files.any {
                GameFileFormats.isMusicFile(it.name) || GameFileFormats.extensionOf(it.name) == "cue"
            } -> KIND_MUSIC

            files.any {
                GameFileFormats.extensionOf(it.name) == "dem" ||
                    it.name.endsWith(InputDemoManager.INPUT_DEMO_EXTENSION, ignoreCase = true)
            } -> KIND_DEMO

            else -> KIND_OTHER
        }

    private fun displayName(files: List<File>): String {
        val first = files.minBy { portableGameFilenameIdentity(it.name) }
        return first.nameWithoutExtension
            .replace(Regex("[_-]+"), " ")
            .trim()
            .ifBlank { first.name }
            .split(Regex("\\s+"))
            .joinToString(" ") { word -> word.replaceFirstChar { it.uppercase() } }
    }

    private fun stableId(
        kind: String,
        relativePaths: List<String>,
    ): String {
        val bytes =
            MessageDigest
                .getInstance(
                    "SHA-256",
                ).digest("$kind\n${relativePaths.joinToString("\n")}".toByteArray())
        return bytes.take(12).joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun relativeIdentity(
        root: File,
        file: File,
    ): String = normalizePath(file.relativeTo(root).invariantSeparatorsPath).lowercase(Locale.US)

    private fun relativePath(
        root: File,
        file: File,
    ): String = normalizePath(file.relativeTo(root).invariantSeparatorsPath)

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')

    internal fun registeredCdAudioPaths(setDir: File): Set<String> {
        val registry = File(setDir, ".content/audio/audio_sources.json")
        if (!registry.isFile) return emptySet()
        return runCatching {
            val sources = JSONObject(registry.readText()).getJSONArray("sources")
            val references = mutableListOf<String>()
            for (index in 0 until sources.length()) {
                val source = sources.getJSONObject(index)
                references += source.getString("cue")
                source.optJSONArray("bins")?.let { bins ->
                    for (binIndex in 0 until bins.length()) references += bins.getString(binIndex)
                }
                source.optJSONArray("bin_content_uris")?.let { bins ->
                    for (binIndex in 0 until bins.length()) references += bins.getString(binIndex)
                }
            }
            references
                .filter { it.isNotBlank() && "://" !in it }
                .map { GameFileFormats.leafName(it).lowercase(Locale.US) }
                .toSet()
        }.getOrDefault(emptySet())
    }

    private val CUE_FILE_PATTERN = Regex("(?i)^\\s*FILE\\s+(?:\"([^\"]+)\"|(\\S+)).*$")
}
