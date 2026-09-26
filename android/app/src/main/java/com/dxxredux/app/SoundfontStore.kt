package com.dxxredux.app

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest

/** Global instrument assets shared by launcher previews and either game process. */
class SoundfontStore(
    filesDir: File,
    private val preferences: SharedPreferences,
) {
    constructor(
        context: Context,
    ) : this(context.filesDir, context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE))

    data class Font(
        val id: String,
        val name: String,
        val download: SoundfontDownload? = null,
    )

    data class State(
        val selected: String,
        val fonts: List<Font>,
        val renderer: String = DEFAULT_RENDERER,
        val reverb: Boolean = true,
        val chorus: Boolean = true,
        val eqProfiles: Map<String, String> = emptyMap(),
    ) {
        val eqProfile: String get() = MusicEq.profile(renderer, selected)
        val eq: String get() = eqFor(eqProfile)
        val soundfontEq: String get() = eqFor(MusicEq.profile("sf2", selected))

        fun eqFor(profile: String): String = eqProfiles[profile] ?: MusicEq.defaultPreset(profile)
    }

    private val directory = File(filesDir, "soundfonts")
    private val manifest = File(directory, "selection.json")

    fun read(): State =
        synchronized(lock) {
            val fonts = if (manifest.exists()) JSONObject(manifest.readText()).getJSONArray("fonts") else JSONArray()
            val catalog =
                (0 until fonts.length()).map {
                    val entry = fonts.getJSONObject(it)
                    Font(
                        entry.getString("id").also(::requireId),
                        entry.getString("name"),
                        entry.optJSONObject("download")?.let(SoundfontDownload::fromJson),
                    )
                }
            val selected = preferences.getString(PREF_SOUNDFONT, "") ?: ""
            State(
                selected.takeIf { id -> catalog.any { it.id == id } && File(path(id)).isFile } ?: "",
                catalog,
                reverb = preferences.getBoolean(PREF_REVERB, true),
                chorus = preferences.getBoolean(PREF_CHORUS, true),
                eqProfiles =
                    runCatching {
                        MusicEq.decodeProfiles(preferences.getString(PREF_EQ, "{}") ?: "{}")
                    }.getOrDefault(emptyMap()),
                renderer =
                    preferences
                        .getString(PREF_RENDERER, DEFAULT_RENDERER)
                        ?.takeIf { it in RENDERERS } ?: DEFAULT_RENDERER,
            )
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
        state.fonts.forEach {
            fonts.put(JSONObject().put("id", it.id).put("name", it.name).put("download", it.download?.toJson()))
        }
        val temporary = File.createTempFile("selection-", ".json", directory)
        try {
            temporary.outputStream().use { output ->
                output.write(
                    (
                        JSONObject()
                            .put("fonts", fonts)
                            .toString(2) +
                            "\n"
                    ).toByteArray(),
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
        download: SoundfontDownload? = null,
        validate: (File) -> Boolean,
    ): Font {
        val temporary =
            synchronized(lock) {
                check(directory.isDirectory || directory.mkdirs()) { "Cannot create soundfont storage" }
                File.createTempFile("import-", ".sf2", directory)
            }
        // Streaming providers can be slow; keep preference reads and selection responsive
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
            return synchronized(lock) {
                val state = read()
                state.fonts.firstOrNull { it.id == id }?.let { existing ->
                    val updated = existing.copy(download = download ?: existing.download)
                    if (updated !=
                        existing
                    ) {
                        write(state.copy(fonts = state.fonts.map { if (it.id == id) updated else it }))
                    }
                    return@synchronized updated
                }
                val font = Font(id, displayName.trim().take(160).ifEmpty { "Imported soundfont" }, download)
                val target = File(path(id))
                if (!temporary.renameTo(target)) AtomicFilePublication.publishFile(temporary, target)
                try {
                    write(state.copy(fonts = state.fonts + font))
                } catch (error: Exception) {
                    target.delete()
                    throw error
                }
                font
            }
        } finally {
            temporary.delete()
        }
    }

    /** User assets only; switch the active preview before removing its backing file. */
    fun delete(
        id: String,
        activate: (String) -> Boolean,
    ) = synchronized(lock) {
        val state = read()
        require(id.isNotEmpty() && state.fonts.any { it.id == id }) { "Unknown soundfont" }
        val file = File(path(id))
        if (file.exists() && !file.isFile) throw IOException("Could not delete soundfont file")
        val active = state.selected == id
        if (active) select("", activate)
        try {
            write(state.copy(fonts = state.fonts.filterNot { it.id == id }))
            if (file.exists() && !file.delete()) throw IOException("Could not delete soundfont file")
        } catch (error: Exception) {
            try {
                write(state)
            } catch (restore: Exception) {
                error.addSuppressed(restore)
            }
            if (active) {
                try {
                    select(id, activate)
                } catch (restore: Exception) {
                    error.addSuppressed(restore)
                }
            }
            throw error
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
            saveSelection(state.copy(selected = id))
        } catch (error: Exception) {
            activate(path(state.selected))
            throw error
        }
    }

    /** Keep instrument selection when changing renderer. Native activation stops old voices. */
    fun selectRenderer(
        renderer: String,
        activate: (String, Boolean) -> Boolean,
    ) = synchronized(lock) {
        require(renderer in RENDERERS) { "Unknown MIDI renderer" }
        val state = read()
        check(activate(path(state.selected), renderer == "ymfm")) { "Could not load sound profile" }
        try {
            saveSelection(state.copy(renderer = renderer))
        } catch (error: Exception) {
            activate(path(state.selected), state.renderer == "ymfm")
            throw error
        }
    }

    fun resetPreferences(activate: (String, Boolean) -> Boolean) =
        synchronized(lock) {
            val state = read()
            check(activate("", DEFAULT_RENDERER == "ymfm")) { "Could not load default MIDI profile" }
            try {
                saveSelection(
                    state.copy(
                        selected = "",
                        renderer = DEFAULT_RENDERER,
                        reverb = true,
                        chorus = true,
                        eqProfiles = emptyMap(),
                    ),
                )
            } catch (error: Exception) {
                activate(path(state.selected), state.renderer == "ymfm")
                throw error
            }
        }

    fun selectEffects(
        reverb: Boolean,
        chorus: Boolean,
        activate: (Boolean, Boolean) -> Boolean,
    ) = synchronized(lock) {
        val state = read()
        check(activate(reverb, chorus)) { "Could not change MIDI effects" }
        try {
            saveSelection(state.copy(reverb = reverb, chorus = chorus))
        } catch (error: Exception) {
            activate(state.reverb, state.chorus)
            throw error
        }
    }

    fun selectEq(
        preset: String,
        activate: (String) -> Boolean,
    ) = synchronized(lock) {
        require(preset in MusicEq.presets) { "Unknown music EQ preset" }
        val state = read()
        require(
            preset == MusicEq.FLAT || MusicEq.supportsProfile(state.eqProfile),
        ) { "No measured EQ for this profile" }
        check(activate(preset)) { "Could not change music EQ" }
        try {
            saveSelection(state.copy(eqProfiles = state.eqProfiles + (state.eqProfile to preset)))
        } catch (error: Exception) {
            activate(state.eq)
            throw error
        }
    }

    private fun saveSelection(state: State) {
        val previousRenderer = preferences.getString(PREF_RENDERER, null)
        val previousFont = preferences.getString(PREF_SOUNDFONT, null)
        val previousReverb = preferences.getBoolean(PREF_REVERB, true)
        val previousChorus = preferences.getBoolean(PREF_CHORUS, true)
        val previousEq = preferences.getString(PREF_EQ, "{}")
        if (!preferences
                .edit()
                .putString(PREF_RENDERER, state.renderer)
                .putString(PREF_SOUNDFONT, state.selected)
                .putBoolean(PREF_REVERB, state.reverb)
                .putBoolean(PREF_CHORUS, state.chorus)
                .putString(PREF_EQ, MusicEq.encodeProfiles(state.eqProfiles))
                .commit()
        ) {
            // SharedPreferences updates memory even when writing to disk fails
            preferences
                .edit()
                .putString(PREF_RENDERER, previousRenderer)
                .putString(PREF_SOUNDFONT, previousFont)
                .putBoolean(PREF_REVERB, previousReverb)
                .putBoolean(PREF_CHORUS, previousChorus)
                .putString(PREF_EQ, previousEq)
                .commit()
            throw IOException("Could not save MIDI preferences")
        }
    }

    companion object {
        const val PREF_RENDERER = "midi_renderer"
        const val PREF_SOUNDFONT = "midi_soundfont"
        const val PREF_REVERB = "midi_reverb"
        const val PREF_CHORUS = "midi_chorus"
        const val PREF_EQ = "midi_eq_profiles"
        const val DEFAULT_RENDERER = "ymfm"
        val RENDERERS = setOf("sf2", "ymfm")

        // Synchronized with MUSIC_SOUNDFONT_MAX_BYTES in music_soundfont.h
        const val MAX_BYTES = 64L * 1024 * 1024
        private val lock = Any()

        private fun requireId(id: String) = require(id.matches(Regex("[0-9a-f]{64}"))) { "Invalid soundfont identity" }
    }
}
