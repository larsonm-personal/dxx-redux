package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest

/** Global instrument assets shared by launcher previews and either game process. */
class SoundfontStore(
    filesDir: File,
) {
    data class Font(
        val id: String,
        val name: String,
    )

    data class State(
        val selected: String,
        val fonts: List<Font>,
    )

    private val directory = File(filesDir, "soundfonts")
    private val manifest = File(directory, "selection.json")

    fun read(): State =
        synchronized(lock) {
            if (!manifest.exists()) return@synchronized State("", emptyList())
            val json = JSONObject(manifest.readText())
            val fonts = json.getJSONArray("fonts")
            State(
                json.getString("selected"),
                (0 until fonts.length()).map {
                    val entry = fonts.getJSONObject(it)
                    Font(entry.getString("id").also(::requireId), entry.getString("name"))
                },
            ).also { state ->
                require(state.selected.isEmpty() || state.fonts.any { it.id == state.selected })
            }
        }

    fun selectedPath(): String = path(read().selected)

    private fun path(id: String): String {
        if (id.isEmpty()) return ""
        requireId(id)
        return File(directory, "$id.sf2").absolutePath
    }

    private fun write(state: State) {
        check(directory.isDirectory || directory.mkdirs()) { "Cannot create soundfont storage" }
        val fonts = JSONArray()
        state.fonts.forEach { fonts.put(JSONObject().put("id", it.id).put("name", it.name)) }
        val temporary = File.createTempFile("selection-", ".json", directory)
        try {
            temporary.outputStream().use { output ->
                output.write(
                    (JSONObject().put("selected", state.selected).put("fonts", fonts).toString(2) + "\n").toByteArray(),
                )
                output.fd.sync()
            }
            // Android's same-directory rename replaces atomically, including on API 24
            if (!temporary.renameTo(manifest)) AtomicFilePublication.publishFile(temporary, manifest)
        } finally {
            temporary.delete()
        }
    }

    /** Input remains caller-owned; only a validated, complete copy is published. */
    fun import(
        input: InputStream,
        displayName: String,
        validate: (File) -> Boolean,
    ): Font =
        synchronized(lock) {
            check(directory.isDirectory || directory.mkdirs()) { "Cannot create soundfont storage" }
            val temporary = File.createTempFile("import-", ".sf2", directory)
            try {
                val digest = MessageDigest.getInstance("SHA-256")
                var total = 0L
                temporary.outputStream().use { output ->
                    val buffer = ByteArray(64 * 1024)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        if (count == 0) continue
                        total += count
                        if (total > MAX_BYTES) throw IOException("Soundfonts must be 64 MiB or smaller")
                        output.write(buffer, 0, count)
                        digest.update(buffer, 0, count)
                    }
                    output.fd.sync()
                }
                if (total == 0L || !validate(temporary)) throw IOException("This file is not a supported SF2 soundfont")
                val id = digest.digest().joinToString("") { "%02x".format(it) }
                val state = read()
                state.fonts.firstOrNull { it.id == id }?.let { return@synchronized it }
                val font = Font(id, displayName.trim().take(160).ifEmpty { "Imported soundfont" })
                val target = File(path(id))
                if (!temporary.renameTo(target)) AtomicFilePublication.publishFile(temporary, target)
                try {
                    write(state.copy(fonts = state.fonts + font))
                } catch (error: Exception) {
                    target.delete()
                    throw error
                }
                font
            } finally {
                temporary.delete()
            }
        }

    /** Activate first, then persist; restore the old synth if persistence fails. */
    fun select(
        id: String,
        activate: (String) -> Boolean,
    ) = synchronized(lock) {
        val state = read()
        require(id.isEmpty() || state.fonts.any { it.id == id }) { "Unknown soundfont" }
        if (!activate(
                path(id),
            )
        ) {
            throw IOException("Could not load this soundfont; the previous selection is still active")
        }
        try {
            write(state.copy(selected = id))
        } catch (error: Exception) {
            activate(path(state.selected))
            throw error
        }
    }

    companion object {
        // Synchronized with MUSIC_SOUNDFONT_MAX_BYTES in music_soundfont.h
        const val MAX_BYTES = 64L * 1024 * 1024
        private val lock = Any()

        private fun requireId(id: String) = require(id.matches(Regex("[0-9a-f]{64}"))) { "Invalid soundfont identity" }
    }
}
