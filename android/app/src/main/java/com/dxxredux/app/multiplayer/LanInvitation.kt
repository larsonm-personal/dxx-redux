package com.dxxredux.app.multiplayer

/** The QR names a host; lobby discovery supplies the game and engine port */
internal object LanInvitation {
    private val link = Regex("descent://([0-9.]{7,15})/?")

    fun parse(
        value: String,
        broadcastAddresses: Set<String> = emptySet(),
    ): String? {
        if (value.length !in 17..26) return null
        val address = link.matchEntire(value)?.groupValues?.get(1) ?: return null
        return validAddress(address)?.takeUnless { it in broadcastAddresses }
    }

    fun validAddress(value: String): String? {
        if (value.length !in 7..15) return null
        val parts = value.split('.')
        if (parts.size != 4) return null
        val octets =
            parts.map { part ->
                if (part.isEmpty() || part.length > 3 || part.any { it !in '0'..'9' } ||
                    (part.length > 1 && part[0] == '0')
                ) {
                    return null
                }
                part.toIntOrNull()?.takeIf { it in 0..255 } ?: return null
            }
        if (octets[0] == 0 || octets[0] == 127 || octets[0] >= 224) return null
        return octets.joinToString(".")
    }

    fun encode(address: String): String {
        require(validAddress(address) == address)
        return "descent://$address"
    }
}

/** Reveal is transient UI state, never a saved preference */
internal class LanQrRevealState {
    var address: String? = null
        private set
    var revealed = false
        private set

    fun setAddress(value: String?) {
        if (address != value) conceal()
        address = value
    }

    fun reveal(): Boolean {
        if (address == null) return false
        revealed = true
        return true
    }

    fun conceal() {
        revealed = false
    }
}
