package com.dxxredux.app

import android.content.Context
import android.util.Log
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Patches .plr (pilot) files in-place with controller KeySettings.
 *
 * Binary layout (version 24):
 *   0: SAVE_FILE_ID  (4 bytes LE, 0x44504C52 = "DPLR")
 *   4: version       (2 bytes LE)
 *   6: window w/h    (4 bytes)
 *  10: 9 single-byte fields (version >= 19 includes automap_hires)
 *  19: NHighestLevels (2 bytes LE)
 *  21: HighestLevels  (N * 10 bytes)
 *  21+N*10: messageMacros (4 * 35 = 140 bytes)
 *  21+N*10+140: KeySettings[0] keyboard (60 bytes)
 *  21+N*10+200: KeySettings[1] joystick (60 bytes)
 *  21+N*10+260: obsolete maps           (180 bytes)
 *  21+N*10+440: KeySettings[2] mouse    (60 bytes)
 *  21+N*10+500: obsolete maps           (120 bytes)
 *  21+N*10+620: control_type_dos        (1 byte)
 */
object PilotPatcher {

    private const val TAG = "PilotPatcher"
    private const val SAVE_FILE_ID = 0x44504C52  // 'D','P','L','R' as LE uint32
    private const val COMPATIBLE_VERSION = 17
    private const val MAX_CONTROLS = 60
    private const val HLI_SIZE = 10              // sizeof(hli) = char[9] + ubyte
    private const val MESSAGE_BLOCK = 4 * 35     // 4 macros * 35 chars
    private const val FIXED_HEADER = 19          // bytes before NHighestLevels
    private const val KS_KEYBOARD_OFF = 0        // relative to ksBase
    private const val KS_JOYSTICK_OFF = 60       // relative to ksBase
    private const val CONTROL_TYPE_OFF = 480     // relative to ksBase

    /**
     * Patch all .plr files in the app's files directory (and Players/ subdirectory)
     * with the given joystick and keyboard KeySettings arrays.
     *
     * @return number of files successfully patched
     */
    fun patchAll(
        context: Context,
        joystickSettings: ByteArray,
        keyboardSettings: ByteArray,
        controlType: Int
    ): Int {
        val filesDir = context.filesDir
        var patched = 0

        // Search both root and Players/ subdirectory
        val dirs = listOfNotNull(filesDir, File(filesDir, "Players").takeIf { it.isDirectory })
        for (dir in dirs) {
            val plrFiles = dir.listFiles { f -> f.extension.equals("plr", ignoreCase = true) }
                ?: continue
            for (plr in plrFiles) {
                if (patchFile(plr, joystickSettings, keyboardSettings, controlType)) {
                    patched++
                }
            }
        }

        Log.i(TAG, "Patched $patched pilot file(s)")
        return patched
    }

    /**
     * Patch a single .plr file in-place.
     */
    private fun patchFile(
        file: File,
        joystickSettings: ByteArray,
        keyboardSettings: ByteArray,
        controlType: Int
    ): Boolean {
        if (file.length() < FIXED_HEADER + 2) {
            Log.w(TAG, "File too small: ${file.name}")
            return false
        }

        try {
            RandomAccessFile(file, "rw").use { raf ->
                val buf4 = ByteArray(4)
                val buf2 = ByteArray(2)

                // Read and verify SAVE_FILE_ID
                raf.readFully(buf4)
                val id = ByteBuffer.wrap(buf4).order(ByteOrder.LITTLE_ENDIAN).int
                if (id != SAVE_FILE_ID) {
                    Log.w(TAG, "Bad file ID in ${file.name}: 0x${id.toString(16)}")
                    return false
                }

                // Read version
                raf.readFully(buf2)
                val version = ByteBuffer.wrap(buf2).order(ByteOrder.LITTLE_ENDIAN).short.toInt()
                if (version < COMPATIBLE_VERSION) {
                    Log.w(TAG, "Incompatible version ${version} in ${file.name}")
                    return false
                }

                // Read NHighestLevels at offset 19
                raf.seek(FIXED_HEADER.toLong())
                raf.readFully(buf2)
                val nHighest = ByteBuffer.wrap(buf2).order(ByteOrder.LITTLE_ENDIAN).short.toInt() and 0xFFFF

                // Calculate KeySettings base offset
                val ksBase = FIXED_HEADER + 2 + (nHighest * HLI_SIZE) + MESSAGE_BLOCK

                // Sanity check file length
                val minLen = ksBase + CONTROL_TYPE_OFF + 1
                if (file.length() < minLen) {
                    Log.w(TAG, "File ${file.name} too short for nHighest=$nHighest")
                    return false
                }

                // Patch KeySettings[0] (keyboard)
                raf.seek((ksBase + KS_KEYBOARD_OFF).toLong())
                raf.write(keyboardSettings, 0, minOf(keyboardSettings.size, MAX_CONTROLS))

                // Patch KeySettings[1] (joystick)
                raf.seek((ksBase + KS_JOYSTICK_OFF).toLong())
                raf.write(joystickSettings, 0, minOf(joystickSettings.size, MAX_CONTROLS))

                // Patch control_type_dos
                raf.seek((ksBase + CONTROL_TYPE_OFF).toLong())
                raf.write(controlType)

                Log.i(TAG, "Patched ${file.name} (nHighest=$nHighest, ksBase=$ksBase)")
                return true
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to patch ${file.name}", e)
            return false
        }
    }
}
