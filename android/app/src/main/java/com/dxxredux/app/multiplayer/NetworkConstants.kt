package com.dxxredux.app.multiplayer

// Shared protocol constants -- keep in sync with server/src/protocol.rs
object NetworkConstants {
    const val PROTOCOL_VERSION: Int = 1
    const val CLIENT_VERSION: String = "android-0.1.0"
    const val PLATFORM: String = "android"

    // Default server URL for emulator (10.0.2.2 is the host loopback alias)
    const val DEFAULT_SERVER_URL: String = "ws://10.0.2.2:9000/ws"

    // Default WebSocket port and path (used when normalizing bare IP/hostname input)
    const val DEFAULT_WS_PORT: Int = 9000
    const val DEFAULT_WS_PATH: String = "/ws"

    // Reconnection
    const val RECONNECT_BASE_DELAY_MS: Long = 1000
    const val RECONNECT_MAX_DELAY_MS: Long = 30000

    // WebSocket close codes
    const val CLOSE_NORMAL: Int = 1000
    const val CLOSE_GOING_AWAY: Int = 1001

    // Game engine UDP ports (keep in sync with d1/d2 net_udp.h UDP_PORT_DEFAULT)
    const val ENGINE_PORT: Int = 42424
    const val PROXY_PORT_BASE: Int = 42430

    // LAN lobby discovery port (Kotlin-side only, not used by the game engine)
    const val LAN_LOBBY_PORT: Int = 42400
    const val LAN_ANNOUNCE_INTERVAL_MS: Long = 3000

    // Game mode string-to-int mapping (keep in sync with d1/d2 main/net_udp.h NETGAME_* defines)
    fun gameModeToInt(mode: String): Int =
        when (mode.lowercase()) {
            "anarchy" -> 0
            "team_anarchy" -> 1
            "robot_anarchy" -> 2
            "cooperative", "coop" -> 3
            "capture_flag", "ctf" -> 4
            "hoard" -> 5
            "team_hoard" -> 6
            "bounty" -> 7
            else -> 0
        }
}
