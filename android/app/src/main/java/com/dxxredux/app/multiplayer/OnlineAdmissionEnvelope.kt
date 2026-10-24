package com.dxxredux.app.multiplayer

import java.nio.ByteBuffer
import java.util.UUID

/** Local-only native request metadata; mirrored in net/online_admission.h */
internal object OnlineAdmissionEnvelope {
    fun wrap(
        payload: ByteArray,
        admissionId: String?,
        slot: Int,
    ): ByteArray {
        // Both native engines use UPID_REQUEST=8; other network packets stay unchanged
        if (admissionId == null || payload.isEmpty() || payload[0].toInt() != 8) return payload
        require(slot in 1..7)
        val id = UUID.fromString(admissionId)
        require(id != UUID(0, 0))
        return ByteBuffer
            .allocate(21 + payload.size)
            .put(byteArrayOf(68, 88, 79, 65))
            .put(slot.toByte())
            .putLong(id.mostSignificantBits)
            .putLong(id.leastSignificantBits)
            .put(payload)
            .array()
    }
}
