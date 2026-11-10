package com.dxxredux.app

import android.content.Context
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.os.SystemClock
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.apache.commons.compress.archivers.sevenz.SevenZFile
import java.io.File
import java.io.FileInputStream
import java.util.zip.ZipFile

/** Resource packs share normal mod storage and activation, not retail game-file slots */
internal suspend fun importResourceMod(
    context: Context,
    uri: Uri,
    name: String,
    manager: ModManager = ModManager.forActiveSet(context.filesDir, context),
    onProgress: suspend (String, Long, Long) -> Unit = { _, _, _ -> },
): ModManager.ModInfo? =
    withContext(Dispatchers.IO) {
        val catalog = XL_RESOURCE_MOD
        val extension = name.substringAfterLast('.', "").lowercase()
        if (extension !in setOf("zip", "7z")) {
            if (catalog.target(name) == null) return@withContext null
            val size = ImportStorageGuard.queryUriSizeBytes(context.contentResolver, uri) ?: -1L
            onProgress("Importing ${catalog.displayName}: $name", 0, size.coerceAtLeast(0))
            return@withContext manager.importResourceMod(catalog) { destination ->
                ResourceModStore.update(
                    catalog,
                    destination,
                    listOf(
                        ResourceModStore.Source(name, size) {
                            context.contentResolver.openInputStream(uri) ?: error("Cannot open $name")
                        },
                    ),
                )
            }
        }

        var continueRetailImport = false

        fun accepts(paths: List<String>): Boolean {
            // Mission-local resources must stay attached to their mission package
            val names = paths.map(::archiveGameFilename)
            if (names.any { it.endsWith(".mn2") || it.endsWith(".msn") }) return false
            val retail =
                (D1_FILES + D2_FILES + D2_DEMO_FILES + D2_PARTIAL_FILES)
                    .filter { it.required }
                    .flatMap { listOf(it.filename) + it.alternatives }
                    .toSet()
            continueRetailImport = names.any { it in retail }
            return catalog.select(paths).isNotEmpty()
        }

        // Reading a solid 7z catalog can take time before extraction starts
        onProgress("Opening $name", 0, 0)
        val scratch = OwnedCacheDirectories.create(context.cacheDir)
        var direct: ParcelFileDescriptor.AutoCloseInputStream? = null
        try {
            if (extension == "7z") {
                direct =
                    try {
                        context.contentResolver.openFileDescriptor(uri, "r")?.let { descriptor ->
                            val stream = ParcelFileDescriptor.AutoCloseInputStream(descriptor)
                            try {
                                check(stream.channel.size() > 0)
                                stream.channel.position(0)
                                stream
                            } catch (_: Exception) {
                                stream.close()
                                null
                            }
                        }
                    } catch (_: Exception) {
                        null
                    }
            }
            val staged = File(scratch, "source.$extension")
            if (direct == null) {
                copyUriToFileWithProgress(context, uri, staged) { done, total ->
                    onProgress("Reading $name", done, total)
                }
            }
            if (extension == "7z") {
                val builder = SevenZFile.builder().setMaxMemoryLimitKiB(ExtractionLimits.MAX_SEVEN_Z_DECODER_MEMORY_KIB)
                val archive =
                    direct?.let { builder.setSeekableByteChannel(it.channel).get() } ?: builder.setFile(staged).get()
                archive.use {
                    val entries =
                        it.entries
                            .asSequence()
                            .take(65537)
                            .toList()
                    require(entries.size <= 65536) { "Resource distribution catalog is too large" }
                    if (!accepts(
                            entries
                                .filterNot { entry ->
                                    entry.isDirectory
                                }.map { entry -> entry.name },
                        )
                    ) {
                        return@withContext null
                    }
                    onProgress("Importing ${catalog.displayName}", 0, 0)
                    val started = SystemClock.uptimeMillis()
                    val files = entries.filterNot { entry -> entry.isDirectory }
                    val selected = catalog.select(files.map { entry -> entry.name })
                    val selectedEntries =
                        selected.map { selection ->
                            files.single { entry ->
                                entry.name ==
                                    selection.source
                            }
                        }
                    val decoded = File(scratch, "selected-resources")
                    val native =
                        direct?.let { source ->
                            extractSelectedSevenZResources(source.channel, entries, selectedEntries, decoded)
                        } ?: FileInputStream(staged).use { source ->
                            extractSelectedSevenZResources(source.channel, entries, selectedEntries, decoded)
                        }
                    Log.i(
                        "DXX-ResourceImport",
                        "selected_decode native=$native elapsed_ms=${SystemClock.uptimeMillis() - started}",
                    )
                    manager
                        .importResourceMod(
                            catalog,
                        ) { destination ->
                            if (native) {
                                ResourceModStore.update(
                                    catalog,
                                    destination,
                                    selectedEntries.map { entry ->
                                        ResourceModStore.Source(entry.name, entry.size) {
                                            File(
                                                decoded,
                                                checkNotNull(canonicalArchiveRelativePath(entry.name)),
                                            ).inputStream()
                                        }
                                    },
                                )
                            } else {
                                ResourceModStore.importSevenZ(catalog, destination, it)
                            }
                        }.takeUnless { continueRetailImport }
                }
            } else {
                ZipFile(staged).use { archive ->
                    val entries =
                        archive
                            .entries()
                            .asSequence()
                            .take(65537)
                            .toList()
                    require(entries.size <= 65536) { "Resource distribution catalog is too large" }
                    val files = entries.filterNot { it.isDirectory }
                    if (!accepts(files.map { it.name })) return@withContext null
                    val selected = catalog.select(files.map { it.name })
                    onProgress("Importing ${catalog.displayName}", 0, 0)
                    manager
                        .importResourceMod(catalog) { destination ->
                            ResourceModStore.update(
                                catalog,
                                destination,
                                selected.map { selection ->
                                    val entry = files.single { it.name == selection.source }
                                    ResourceModStore.Source(entry.name, entry.size) { archive.getInputStream(entry) }
                                },
                            )
                        }.takeUnless { continueRetailImport }
                }
            }
        } finally {
            direct?.close()
            scratch.deleteRecursively()
        }
    }
