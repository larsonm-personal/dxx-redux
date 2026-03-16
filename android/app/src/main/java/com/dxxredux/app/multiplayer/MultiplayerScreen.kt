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
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
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
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun MultiplayerScreen(onBack: () -> Unit) {
    val state by MatchmakingStateHolder.state.collectAsState()
    var serverUrl by remember { mutableStateOf(state.serverUrl) }
    var callsign by remember { mutableStateOf(state.callsign) }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .padding(16.dp),
    ) {
        // -- Header --
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Multiplayer", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.weight(1f))
            OutlinedButton(onClick = onBack) { Text("Back") }
        }
        Spacer(Modifier.height(8.dp))

        // -- Connection bar --
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(
                text = "Status: ${state.status.name}",
                style = MaterialTheme.typography.bodyMedium,
                color =
                    when (state.status) {
                        ConnectionStatus.CONNECTED -> MaterialTheme.colorScheme.primary
                        ConnectionStatus.DISCONNECTED -> MaterialTheme.colorScheme.error
                        else -> MaterialTheme.colorScheme.onSurfaceVariant
                    },
            )
            state.serverStatus?.let { ss ->
                Spacer(Modifier.width(16.dp))
                Text(
                    "${ss.onlinePlayers} online, ${ss.activeGamesCount} games",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
        Spacer(Modifier.height(8.dp))

        // -- Connect controls --
        if (state.status == ConnectionStatus.DISCONNECTED ||
            state.status == ConnectionStatus.RECONNECTING
        ) {
            OutlinedTextField(
                value = serverUrl,
                onValueChange = { serverUrl = it },
                label = { Text("Server URL") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(4.dp))
            OutlinedTextField(
                value = callsign,
                onValueChange = { callsign = it },
                label = { Text("Callsign") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(8.dp))
            Button(
                onClick = { MatchmakingService.connect(serverUrl, callsign) },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("Connect")
            }
        } else {
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Button(onClick = { MatchmakingService.requestLobbyList() }) {
                    Text("Refresh Lobbies")
                }
                OutlinedButton(onClick = { MatchmakingService.disconnect() }) {
                    Text("Disconnect")
                }
            }
        }

        // -- Error display --
        state.errorMessage?.let { err ->
            Spacer(Modifier.height(4.dp))
            Text(err, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
        }

        // -- MOTD --
        state.motd?.let { motd ->
            Spacer(Modifier.height(4.dp))
            Text("MOTD: $motd", style = MaterialTheme.typography.bodySmall)
        }

        Spacer(Modifier.height(12.dp))
        HorizontalDivider()
        Spacer(Modifier.height(8.dp))

        // -- Lobby list --
        if (state.lobbies.isNotEmpty()) {
            Text("Lobbies (${state.lobbies.size})", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.height(4.dp))
            LazyColumn(modifier = Modifier.weight(1f)) {
                items(state.lobbies, key = { it.lobbyId }) { lobby ->
                    LobbyCard(lobby)
                    Spacer(Modifier.height(4.dp))
                }
            }
        } else if (state.status == ConnectionStatus.CONNECTED) {
            Text("No lobbies found.", style = MaterialTheme.typography.bodyMedium)
            Spacer(Modifier.weight(1f))
        } else {
            Spacer(Modifier.weight(1f))
        }

        // -- Status log --
        HorizontalDivider()
        Spacer(Modifier.height(4.dp))
        Text("Log", style = MaterialTheme.typography.titleSmall)
        val logListState = rememberLazyListState()
        LaunchedEffect(state.statusLog.size) {
            if (state.statusLog.isNotEmpty()) {
                logListState.animateScrollToItem(state.statusLog.size - 1)
            }
        }
        LazyColumn(
            state = logListState,
            modifier =
                Modifier
                    .height(120.dp)
                    .fillMaxWidth(),
        ) {
            items(state.statusLog) { line ->
                Text(line, fontSize = 11.sp, fontFamily = FontFamily.Monospace, maxLines = 2)
            }
        }
    }
}

@Composable
private fun LobbyCard(lobby: LobbyInfo) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(lobby.hostCallsign, style = MaterialTheme.typography.titleSmall)
                Spacer(Modifier.weight(1f))
                Text(
                    "${lobby.playerCount}/${lobby.maxPlayers}",
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
            Text(
                "${lobby.mission} -- ${lobby.mode}",
                style = MaterialTheme.typography.bodySmall,
            )
            if (lobby.hasCode) {
                Text("(code required)", style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}
