package com.dxxredux.app.multiplayer

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive

@Composable
fun LobbyScreen(onLaunchGame: (GameLaunchInfo) -> Unit) {
    val state by MatchmakingStateHolder.state.collectAsState()
    val lobby = state.currentLobby ?: return
    val myId = state.playerId
    val lobbyFocus = remember { FocusRequester() }
    LaunchedEffect(Unit) { lobbyFocus.requestFocus() }

    // Launch the game when gameLaunchInfo becomes available
    val launchInfo = state.gameLaunchInfo
    LaunchedEffect(launchInfo) {
        if (launchInfo != null) {
            onLaunchGame(launchInfo)
        }
    }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .padding(16.dp),
    ) {
        // -- Header --
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Lobby", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.weight(1f))
            OutlinedButton(
                onClick = { MatchmakingService.leaveLobby() },
                modifier = Modifier.focusRequester(lobbyFocus),
            ) {
                Text("Leave")
            }
        }
        Spacer(Modifier.height(4.dp))
        Text(
            "Lobby ID: ${lobby.lobbyId.take(8)}...",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        // Game config summary from game_info
        val gi = lobby.gameInfo
        val mission = gi["mission"]?.jsonPrimitive?.content ?: ""
        val mode = gi["mode"]?.jsonPrimitive?.content ?: ""
        val diff = gi["difficulty"]?.jsonPrimitive?.intOrNull
        val level = gi["level_num"]?.jsonPrimitive?.intOrNull
        val diffNames = listOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")
        val configParts =
            buildList {
                if (mission.isNotEmpty()) add(mission)
                if (mode.isNotEmpty()) add(mode)
                if (diff != null) add(diffNames.getOrElse(diff) { "Diff $diff" })
                if (level != null) add("Level $level")
            }
        if (configParts.isNotEmpty()) {
            Text(
                configParts.joinToString(" / "),
                style = MaterialTheme.typography.bodySmall,
            )
        }
        Spacer(Modifier.height(12.dp))

        // -- Player list --
        Text(
            "Players (${lobby.players.size})",
            style = MaterialTheme.typography.titleSmall,
        )
        Spacer(Modifier.height(4.dp))
        LazyColumn(modifier = Modifier.weight(0.4f)) {
            items(lobby.players, key = { it.playerId }) { player ->
                PlayerCard(
                    player = player,
                    isHost = player.playerId == lobby.hostPlayerId,
                    isMe = player.playerId == myId,
                    canKick = lobby.isHost && player.playerId != myId,
                )
                Spacer(Modifier.height(4.dp))
            }
        }
        Spacer(Modifier.height(8.dp))

        // -- Action buttons --
        val myPlayer = lobby.players.find { it.playerId == myId }
        val myReady = myPlayer?.ready ?: false

        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Button(
                onClick = { MatchmakingService.setReady(!myReady) },
                modifier = Modifier.weight(1f),
                colors =
                    if (myReady) {
                        ButtonDefaults.buttonColors(
                            containerColor = MaterialTheme.colorScheme.secondary,
                        )
                    } else {
                        ButtonDefaults.buttonColors()
                    },
            ) {
                Text(if (myReady) "Unready" else "Ready")
            }
        }

        if (lobby.isHost) {
            Spacer(Modifier.height(8.dp))
            if (mode == "coop") {
                val game = gi["game"]?.jsonPrimitive?.content ?: "d2"
                CoopSaveOffer(game = game, mission = mission, lobby = lobby)
            }
            val allReady = lobby.players.all { it.ready }
            val enoughPlayers = lobby.players.size >= 2
            Button(
                onClick = { MatchmakingService.startGame() },
                enabled = allReady && enoughPlayers,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("Start Game")
            }
            if (!allReady) {
                Text(
                    "Waiting for all players to ready up",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Spacer(Modifier.height(8.dp))

        // -- Chat --
        ChatArea(
            messages = state.chatMessages,
            onSend = { MatchmakingService.sendLobbyChat(it) },
            modifier = Modifier.weight(0.3f),
        )

        Spacer(Modifier.height(8.dp))

        // -- ICE progress --
        IceProgressPanel(state)
        Spacer(Modifier.height(4.dp))

        // -- Status log --
        StatusLog(state.statusLog)
    }
}

@Composable
private fun PlayerCard(
    player: LobbyPlayerInfo,
    isHost: Boolean,
    isMe: Boolean,
    canKick: Boolean,
) {
    val cardColors =
        if (isMe) {
            CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer,
            )
        } else {
            CardDefaults.cardColors()
        }

    Card(modifier = Modifier.fillMaxWidth(), colors = cardColors) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(12.dp),
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(player.callsign, style = MaterialTheme.typography.titleSmall)
                    if (isHost) {
                        Spacer(Modifier.width(6.dp))
                        Text(
                            "(Host)",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                    if (isMe) {
                        Spacer(Modifier.width(6.dp))
                        Text(
                            "(You)",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                        )
                    }
                }
                Row {
                    Text(
                        if (player.ready) "Ready" else "Not ready",
                        style = MaterialTheme.typography.bodySmall,
                        color =
                            if (player.ready) {
                                MaterialTheme.colorScheme.primary
                            } else {
                                MaterialTheme.colorScheme.onSurfaceVariant
                            },
                    )
                    player.pingMs?.let { ping ->
                        Spacer(Modifier.width(12.dp))
                        Text(
                            "${ping}ms",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
            if (canKick) {
                OutlinedButton(
                    onClick = { MatchmakingService.kickPlayer(player.playerId) },
                    colors =
                        ButtonDefaults.outlinedButtonColors(
                            contentColor = MaterialTheme.colorScheme.error,
                        ),
                ) {
                    Text("Kick")
                }
            }
        }
    }
}

/**
 * Auto-offer matching coop saves to the host based on lobby player callsigns.
 * Matches saves from coop_autosave_history.json where the most current lobby
 * players appear in the save's callsign list. Auto-selects the best match;
 * the host can dismiss to "start fresh".
 */
@Composable
private fun CoopSaveOffer(
    game: String,
    mission: String,
    lobby: CurrentLobbyState,
) {
    val context = LocalContext.current
    val filesDir = context.filesDir

    val saves =
        remember(game, mission) {
            readCoopAutosaveHistory(filesDir, game, mission, context)
        }
    if (saves.isEmpty()) return

    val lobbyCallsigns =
        remember(lobby.players) {
            lobby.players.map { it.callsign.lowercase() }.toSet()
        }

    // Score each save: prefer more matching callsigns, then newest
    val bestMatch =
        remember(saves, lobbyCallsigns) {
            saves
                .mapNotNull { save ->
                    val matchCount =
                        save.callsigns.count {
                            it.lowercase() in lobbyCallsigns
                        }
                    if (matchCount > 0) Triple(save, matchCount, save.timestamp) else null
                }.sortedWith(
                    compareByDescending<Triple<CoopSaveEntry, Int, Long>> {
                        it.second
                    }.thenByDescending { it.third },
                ).firstOrNull()
                ?.first
        } ?: return

    var useRestore by remember { mutableStateOf(true) }

    // When the best match changes (players join/leave), re-select it
    LaunchedEffect(bestMatch) { useRestore = true }

    // Write/delete coop_restore_slot.txt based on current selection
    LaunchedEffect(useRestore, bestMatch) {
        writeCoopRestoreSlot(filesDir, game, if (useRestore) bestMatch.slot else null)
    }

    val label =
        "L${bestMatch.level} - ${bestMatch.numPlayers}p" +
            " - ${bestMatch.callsigns.joinToString()}" +
            " - ${formatTimeAgo(bestMatch.timestamp)}"

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(8.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Text("Save found: $label", style = MaterialTheme.typography.bodySmall)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (useRestore) {
                    Button(onClick = {}, modifier = Modifier.weight(1f)) {
                        Text("Restore", fontSize = 12.sp)
                    }
                    OutlinedButton(
                        onClick = { useRestore = false },
                        modifier = Modifier.weight(1f),
                    ) { Text("Start fresh", fontSize = 12.sp) }
                } else {
                    OutlinedButton(
                        onClick = { useRestore = true },
                        modifier = Modifier.weight(1f),
                    ) { Text("Restore", fontSize = 12.sp) }
                    Button(onClick = {}, modifier = Modifier.weight(1f)) {
                        Text("Start fresh", fontSize = 12.sp)
                    }
                }
            }
        }
    }
    Spacer(Modifier.height(4.dp))
}
