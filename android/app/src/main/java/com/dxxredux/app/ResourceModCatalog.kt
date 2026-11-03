package com.dxxredux.app

import java.util.Locale

/** Maps user-supplied resource files into one private resource directory, independently of retail file tracking */
internal data class ResourceModCatalog(
    val id: String,
    val displayName: String,
    val game: String,
    val files: Map<String, String>,
    val directories: List<Directory>,
) {
    data class Directory(
        val path: String,
        val extensions: Set<String>,
        val stems: Set<String>? = null,
        val rawNames: Set<String> = emptySet(),
    )

    data class Selection(
        val source: String,
        val target: String,
    )

    fun target(path: String): String? {
        val normalized = path.replace('\\', '/').lowercase(Locale.ROOT)
        val parts = normalized.split('/')
        require(parts.none { it == ".." || it == "." || it.contains(':') || it.any { c -> c.code < 32 } }) {
            "Unsafe resource path: $path"
        }
        require(!normalized.startsWith('/') && parts.none { it.isEmpty() }) { "Unsafe resource path: $path" }
        files[parts.last()]?.let { return it }
        for (directory in directories) {
            val name = parts.last()
            if (name.substringAfterLast('.', "") !in directory.extensions) continue
            if (directory.stems != null && name.substringBeforeLast('.') !in directory.stems) continue
            if (parts.size == 1 && (directory.stems != null || name in directory.rawNames)) {
                return "${directory.path}/$name"
            }
            if (normalized.endsWith("/${directory.path}/$name") || normalized == "${directory.path}/$name") {
                return "${directory.path}/$name"
            }
        }
        return null
    }

    fun select(paths: Iterable<String>): List<Selection> {
        val selected = paths.mapNotNull { source -> target(source)?.let { Selection(source, it) } }
        require(
            selected.map { identity(it.target) }.distinct().size == selected.size,
        ) { "Archive has colliding resource outputs" }
        // Keep archive order so solid 7z blocks can be decoded forward once
        return selected
    }

    // Alternative encodings represent the same resource, not two independently mounted assets
    fun identity(target: String): String =
        if (directories.any { it.extensions.size > 1 && target.startsWith("${it.path}/") }) {
            target.substringBeforeLast('.')
        } else {
            target
        }
}

internal val XL_RESOURCE_MOD =
    ResourceModCatalog(
        id = "xl-resources",
        displayName = "XL resources",
        game = "d2",
        files = mapOf("d2x-xl.hog" to "d2x-xl.hog", "d2x.ham" to "d2x.ham"),
        directories =
            listOf(
                ResourceModCatalog.Directory(
                    "textures/d2x-xl",
                    setOf("png", "tga"),
                    setOf(
                        "smoke",
                        "smokingfire",
                        "bubble",
                        "corona",
                        "glare",
                        "forcefield",
                        "forcefield2",
                        "forcefield3",
                    ),
                ),
                ResourceModCatalog.Directory(
                    "sounds/d2x-xl",
                    setOf("wav"),
                    // Raw import names mirror the standard addon bank in XL_compat/named_sound.cpp
                    rawNames =
                        setOf(
                            "missileflight-small.wav",
                            "missileflight-big.wav",
                            "vulcan-firing.wav",
                            "gauss-firing.wav",
                            "gatling-speedup.wav",
                            "flareburning.wav",
                            "lowping.wav",
                            "highping.wav",
                            "lightning.wav",
                            "slowdown.wav",
                            "speedup.wav",
                            "airbubbles.wav",
                            "zoom1.wav",
                            "zoom2.wav",
                            "headlight.wav",
                            "boiling-lava.wav",
                            "busy-machine.wav",
                            "computer.wav",
                            "deep-hum.wav",
                            "dripping-water.wav",
                            "dripping-water-2.wav",
                            "dripping-water-3.wav",
                            "earthquake.wav",
                            "energy.wav",
                            "falling-rocks.wav",
                            "falling-rocks-2.wav",
                            "fast-fan.wav",
                            "flowing-lava.wav",
                            "flowing-water.wav",
                            "insects.wav",
                            "machine-gear.wav",
                            "mighty-machine.wav",
                            "static-buzz.wav",
                            "steam.wav",
                            "teleporter.wav",
                            "waterfall.wav",
                            "fire.wav",
                            "jet-engine.wav",
                            "nuke-explosion-short.wav",
                            "scrape.wav",
                            "scrape2.wav",
                            "scrape3.wav",
                            "scrape4.wav",
                        ),
                ),
            ),
    )
