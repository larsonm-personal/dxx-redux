package com.dxxredux.app.lobby

import org.json.JSONObject

// LAN lobby discovery protocol. JSON-over-UDP on port 42400.
// Host broadcasts ANNOUNCE; clients send JOIN/LEAVE; host sends PLAYER_LIST/START.
// All messages carry a lobby_id (UUID string) so multiple hosts on one network
// are disambiguated. The host is authoritative for the player list.

// Outbound message types (sent by host or client)

const val MSG_ANNOUNCE = "ANNOUNCE"
const val MSG_JOIN = "JOIN"
const val MSG_LEAVE = "LEAVE"
const val MSG_PLAYER_LIST = "PLAYER_LIST"
const val MSG_READY = "READY"
const val MSG_START = "START"
const val MSG_PING = "PING"
const val MSG_PONG = "PONG"

/** A single player in a LAN lobby. */
data class LanPlayer(
    val callsign: String,
    val address: String,
    val ready: Boolean = false,
)

/** Parsed LAN lobby announcement (received from a host broadcast). */
data class LanLobbyAnnounce(
    val lobbyId: String,
    val callsign: String,
    val game: String,
    val mission: String,
    val mode: String,
    val playerCount: Int,
    val maxPlayers: Int,
    val hostAddress: String = "",
)

/** Build a JSON ANNOUNCE packet for broadcasting. */
fun buildAnnounce(
    lobbyId: String,
    callsign: String,
    game: String,
    mission: String,
    mode: String,
    playerCount: Int,
    maxPlayers: Int,
): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_ANNOUNCE)
    json.put("lobby_id", lobbyId)
    json.put("callsign", callsign)
    json.put("game", game)
    json.put("mission", mission)
    json.put("mode", mode)
    json.put("player_count", playerCount)
    json.put("max_players", maxPlayers)
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Build a JSON JOIN packet for sending to the host. */
fun buildJoin(
    lobbyId: String,
    callsign: String,
): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_JOIN)
    json.put("lobby_id", lobbyId)
    json.put("callsign", callsign)
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Build a JSON LEAVE packet. */
fun buildLeave(
    lobbyId: String,
    callsign: String,
): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_LEAVE)
    json.put("lobby_id", lobbyId)
    json.put("callsign", callsign)
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Build a JSON READY packet. */
fun buildReady(
    lobbyId: String,
    callsign: String,
    ready: Boolean,
): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_READY)
    json.put("lobby_id", lobbyId)
    json.put("callsign", callsign)
    json.put("ready", ready)
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Build a PLAYER_LIST packet (sent by host to all players). */
fun buildPlayerList(
    lobbyId: String,
    players: List<LanPlayer>,
): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_PLAYER_LIST)
    json.put("lobby_id", lobbyId)
    val arr = org.json.JSONArray()
    for (p in players) {
        val pj = JSONObject()
        pj.put("callsign", p.callsign)
        pj.put("address", p.address)
        pj.put("ready", p.ready)
        arr.put(pj)
    }
    json.put("players", arr)
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Build a START packet (sent by host to all players). */
fun buildStart(
    lobbyId: String,
    hostAddress: String,
    hostPort: Int,
    game: String,
    mission: String,
    mode: String,
    difficulty: Int,
): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_START)
    json.put("lobby_id", lobbyId)
    json.put("host_address", hostAddress)
    json.put("host_port", hostPort)
    json.put("game", game)
    json.put("mission", mission)
    json.put("mode", mode)
    json.put("difficulty", difficulty)
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Build a PING packet. */
fun buildPing(lobbyId: String): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_PING)
    json.put("lobby_id", lobbyId)
    json.put("ts", System.currentTimeMillis())
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Build a PONG packet. */
fun buildPong(
    lobbyId: String,
    origTs: Long,
): ByteArray {
    val json = JSONObject()
    json.put("type", MSG_PONG)
    json.put("lobby_id", lobbyId)
    json.put("ts", origTs)
    return json.toString().toByteArray(Charsets.UTF_8)
}

/** Parse a raw UDP packet into a JSONObject, or null if invalid. */
fun parsePacket(
    data: ByteArray,
    length: Int,
): JSONObject? =
    try {
        JSONObject(String(data, 0, length, Charsets.UTF_8))
    } catch (_: Exception) {
        null
    }
