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
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun LobbyScreen(onLaunchGame: (GameLaunchInfo) -> Unit) {
    val state by MatchmakingStateHolder.state.collectAsState()
    val lobby = state.currentLobby ?: return
    val myId = state.playerId

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
            OutlinedButton(onClick = { MatchmakingService.leaveLobby() }) {
                Text("Leave")
            }
        }
        Spacer(Modifier.height(4.dp))
        Text(
            "Lobby ID: ${lobby.lobbyId.take(8)}...",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
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
        LobbyChatArea(state.chatMessages, modifier = Modifier.weight(0.3f))

        Spacer(Modifier.height(8.dp))

        // -- Network events --
        NetworkEventsPanel(state)
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

@Composable
private fun LobbyChatArea(
    messages: List<ChatMessage>,
    modifier: Modifier = Modifier,
) {
    var text by remember { mutableStateOf("") }

    Column(modifier = modifier) {
        Text("Chat", style = MaterialTheme.typography.titleSmall)
        val chatListState = rememberLazyListState()
        LaunchedEffect(messages.size) {
            if (messages.isNotEmpty()) {
                chatListState.animateScrollToItem(messages.size - 1)
            }
        }
        LazyColumn(
            state = chatListState,
            modifier = Modifier.weight(1f).fillMaxWidth(),
        ) {
            items(messages) { msg ->
                val prefix = if (msg.isMe) "You" else msg.fromCallsign
                Text(
                    "$prefix: ${msg.text}",
                    fontSize = 12.sp,
                    color =
                        if (msg.isMe) {
                            MaterialTheme.colorScheme.primary
                        } else {
                            MaterialTheme.colorScheme.onSurface
                        },
                )
            }
        }
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth(),
        ) {
            OutlinedTextField(
                value = text,
                onValueChange = { text = it },
                placeholder = { Text("Message...") },
                singleLine = true,
                modifier = Modifier.weight(1f),
            )
            Spacer(Modifier.width(4.dp))
            Button(
                onClick = {
                    if (text.isNotBlank()) {
                        MatchmakingService.sendLobbyChat(text.trim())
                        text = ""
                    }
                },
                enabled = text.isNotBlank(),
            ) {
                Text("Send")
            }
        }
    }
}
