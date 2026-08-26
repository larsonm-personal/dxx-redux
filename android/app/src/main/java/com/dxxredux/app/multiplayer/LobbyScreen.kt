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
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.unit.dp
import com.dxxredux.app.VisualReplacementPolicy
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive

@Composable
fun LobbyScreen(
    onLaunchGame: (GameLaunchInfo) -> Unit,
    onLaunchRequested: (String) -> Unit,
) {
    val state by MatchmakingStateHolder.state.collectAsState()
    val lobby = state.currentLobby ?: return
    val myId = state.playerId
    val lobbyFocus = remember { FocusRequester() }
    RequestControllerInitialFocus(lobbyFocus, revealFocusOnRequest = false)

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
                .showControllerFocusOnDpad(lobbyFocus)
                .safeDrawingPadding()
                .padding(16.dp),
    ) {
        // -- Header --
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Lobby", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.weight(1f))
            OutlinedButton(
                onClick = { MatchmakingService.leaveLobby() },
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
        VisualReplacementPolicy.noticeText(gi)?.let {
            Text(
                it,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.primary,
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
                modifier = Modifier.weight(1f).focusRequester(lobbyFocus),
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
                CoopRestoreSelectionSummary(game, level ?: 1)
            }
            val allReady = lobby.players.all { it.ready }
            val enoughPlayers = lobby.players.size >= 2
            Button(
                onClick = {
                    onLaunchRequested(gi["game"]?.jsonPrimitive?.content ?: "d2")
                    MatchmakingService.startGame()
                },
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
            textEntryFallbackFocusRequester = lobbyFocus,
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
