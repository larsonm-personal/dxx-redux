package com.dxxredux.app

import java.io.File

private const val MAX_PILOT_PREF_FILE_BYTES = 16L * 1024L * 1024L
private const val MAX_PILOT_PREF_TOTAL_BYTES = 64L * 1024L * 1024L

internal fun writePilotPreferencesToAll(
    filesDir: File,
    writeD1: () -> Int,
    writeD2: () -> Int,
): Int =
    AtomicFilePublication.transaction {
        val originals =
            listOf("d1x-redux", "d2x-redux")
                .flatMap { gameDir ->
                    listOf(File(filesDir, gameDir), File(filesDir, "$gameDir/Players")).flatMap { directory ->
                        directory.listFiles().orEmpty().filter { file ->
                            file.isFile &&
                                (
                                    file.extension.equals("plr", ignoreCase = true) ||
                                        file.extension.equals("plx", ignoreCase = true)
                                )
                        }
                    }
                }.distinctBy { it.absolutePath }
        var totalBytes = 0L
        val snapshots =
            try {
                originals.map { file ->
                    val length = file.length()
                    check(length in 0..MAX_PILOT_PREF_FILE_BYTES)
                    totalBytes += length
                    check(totalBytes <= MAX_PILOT_PREF_TOTAL_BYTES)
                    file to file.readBytes()
                }
            } catch (_: Throwable) {
                return@transaction -1
            }

        val d1Result = writeD1()
        val d2Result = if (d1Result >= 0) writeD2() else -1
        if (d1Result >= 0 && d2Result >= 0) return@transaction d1Result + d2Result

        try {
            AtomicFilePublication.writeBytesBatch(snapshots)
            -1
        } catch (_: Throwable) {
            -2
        }
    }
