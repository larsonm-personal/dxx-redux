package com.dxxredux.app.multiplayer

import java.security.AlgorithmParameters
import java.security.KeyFactory
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.SecureRandom
import java.security.Signature
import java.security.interfaces.ECPublicKey
import java.security.spec.ECGenParameterSpec
import java.security.spec.ECParameterSpec
import java.security.spec.ECPoint
import java.security.spec.ECPublicKeySpec

/**
 * Process-lifetime signing identity used by the native UDP reconnect protocol
 *
 * The public key can be replicated to every peer for host migration. The private
 * key never leaves the game process
 */
object UdpReconnectIdentity {
    private val secureRandom = SecureRandom()
    private val keyPair: KeyPair by lazy {
        KeyPairGenerator
            .getInstance("EC")
            .apply { initialize(ECGenParameterSpec("secp256r1"), secureRandom) }
            .generateKeyPair()
    }
    private val encodedPublicKey: ByteArray by lazy {
        val publicKey = keyPair.public as ECPublicKey
        byteArrayOf(UNCOMPRESSED_POINT) +
            publicKey.w.affineX.toUnsignedFixed(COORDINATE_SIZE) +
            publicKey.w.affineY.toUnsignedFixed(COORDINATE_SIZE)
    }

    @JvmStatic
    fun publicKey(): ByteArray = encodedPublicKey.copyOf()

    @JvmStatic
    fun sign(message: ByteArray): ByteArray =
        Signature
            .getInstance("SHA256withECDSA")
            .apply {
                initSign(keyPair.private, secureRandom)
                update(message)
            }.sign()

    @JvmStatic
    fun verify(
        publicKey: ByteArray,
        message: ByteArray,
        signature: ByteArray,
    ): Boolean =
        runCatching {
            require(publicKey.size == PUBLIC_KEY_SIZE && publicKey[0] == UNCOMPRESSED_POINT)
            val parameters =
                AlgorithmParameters
                    .getInstance("EC")
                    .apply { init(ECGenParameterSpec("secp256r1")) }
                    .getParameterSpec(ECParameterSpec::class.java)
            val key =
                KeyFactory
                    .getInstance("EC")
                    .generatePublic(
                        ECPublicKeySpec(
                            ECPoint(
                                publicKey.copyOfRange(1, 1 + COORDINATE_SIZE).toPositiveBigInteger(),
                                publicKey.copyOfRange(1 + COORDINATE_SIZE, PUBLIC_KEY_SIZE).toPositiveBigInteger(),
                            ),
                            parameters,
                        ),
                    )
            Signature
                .getInstance("SHA256withECDSA")
                .apply {
                    initVerify(key)
                    update(message)
                }.verify(signature)
        }.getOrDefault(false)

    @JvmStatic
    fun randomBytes(size: Int): ByteArray = ByteArray(size.coerceAtLeast(0)).also(secureRandom::nextBytes)

    private fun java.math.BigInteger.toUnsignedFixed(size: Int): ByteArray {
        val encoded = toByteArray()
        val first = (encoded.size - size).coerceAtLeast(0)
        return encoded.copyOfRange(first, encoded.size).let {
            ByteArray(size - it.size) + it
        }
    }

    private fun ByteArray.toPositiveBigInteger() = java.math.BigInteger(1, this)

    private const val COORDINATE_SIZE = 32
    private const val PUBLIC_KEY_SIZE = 1 + COORDINATE_SIZE * 2
    private const val UNCOMPRESSED_POINT: Byte = 0x04
}
