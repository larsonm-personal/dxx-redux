package com.dxxredux.app.multiplayer

import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile
import java.security.KeyFactory
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.spec.ECGenParameterSpec
import java.security.spec.PKCS8EncodedKeySpec
import java.security.spec.X509EncodedKeySpec

/** Private, non-backed-up installation identity and durable replay counter */
internal class UdpReconnectStore(
    private val directory: File,
    private val replaceFile: (File, File) -> Unit = { source, target ->
        // Same-directory rename replaces atomically on Android, including API 23
        check(source.renameTo(target)) { "Could not commit reconnect identity" }
    },
) {
    private data class State(
        val keyPair: KeyPair,
        val counter: Long,
    )

    fun keyPair(): KeyPair = locked { readOrCreate().keyPair }

    // Commit before signing so a killed process never reuses an accepted counter
    fun nextCounter(): Long =
        locked {
            val state = readOrCreate()
            check(state.counter < Long.MAX_VALUE) { "Reconnect counter exhausted" }
            val next = state.copy(counter = state.counter + 1)
            write(next)
            next.counter
        }

    private fun <T> locked(action: () -> T): T =
        synchronized(lock) {
            check(directory.isDirectory || directory.mkdirs())
            RandomAccessFile(File(directory, "identity.lock"), "rw").use { file ->
                file.channel.lock().use { action() }
            }
        }

    private fun readOrCreate(): State {
        val file = File(directory, "identity.bin")
        if (!file.exists()) {
            val pair =
                KeyPairGenerator
                    .getInstance("EC")
                    .apply {
                        initialize(ECGenParameterSpec("secp256r1"))
                    }.generateKeyPair()
            return State(pair, 0).also(::write)
        }
        // Fail closed on damaged storage instead of silently changing identity
        return DataInputStream(file.inputStream()).use { input ->
            check(input.readInt() == 1) { "Invalid reconnect identity version" }
            val counter = input.readLong()
            check(counter >= 0)

            fun readKey(): ByteArray {
                val size = input.readInt()
                check(size in 1..4096)
                return ByteArray(size).also(input::readFully)
            }
            val factory = KeyFactory.getInstance("EC")
            val publicKey = factory.generatePublic(X509EncodedKeySpec(readKey()))
            val privateKey = factory.generatePrivate(PKCS8EncodedKeySpec(readKey()))
            State(KeyPair(publicKey, privateKey), counter)
        }
    }

    private fun write(state: State) {
        val pending = File(directory, "identity.pending")
        FileOutputStream(pending).use { file ->
            val output = DataOutputStream(file)
            output.writeInt(1)
            output.writeLong(state.counter)
            for (key in listOf(state.keyPair.public, state.keyPair.private)) {
                val bytes = key.encoded
                output.writeInt(bytes.size)
                output.write(bytes)
            }
            output.flush()
            file.fd.sync()
        }
        replaceFile(pending, File(directory, "identity.bin"))
    }

    companion object {
        private val lock = Any()
    }
}
