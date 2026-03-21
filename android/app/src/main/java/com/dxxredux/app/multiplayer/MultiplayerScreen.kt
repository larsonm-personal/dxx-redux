package com.dxxredux.app.multiplayer

import androidx.activity.compose.BackHandler
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
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dxxredux.app.lobby.LobbyService
import kotlinx.coroutines.delay
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive

@Composable
fun MultiplayerScreen(
    onBack: () -> Unit,
    onLaunchGame: (GameLaunchInfo) -> Unit,
) {
    val state by MatchmakingStateHolder.state.collectAsState()

    when (state.nav) {
        MultiplayerNav.LOBBY -> {
            BackHandler {
                MatchmakingService.leaveLobby()
                MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
            }
            val lobby = state.currentLobby
            if (lobby != null) {
                LobbyScreen(onLaunchGame)
            } else {
                // Stale nav state, reset
                MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
            }
        }
        MultiplayerNav.FRIENDS -> {
            BackHandler {
                MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
            }
            FriendsContent(state, onBack)
        }
        MultiplayerNav.LAN -> {
            BackHandler {
                if (LobbyService.joinedLobby.value != null) {
                    LobbyService.leaveLanLobby(state.callsign)
                } else {
                    MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
                }
            }
            LanContent(state, onBack, onLaunchGame)
        }
        MultiplayerNav.BROWSER -> {
            BackHandler(onBack = onBack)
            ServerBrowserContent(state, onBack)
        }
    }
}

@Composable
private fun ServerBrowserContent(
    state: MatchmakingState,
    onBack: () -> Unit,
) {
    val context = LocalContext.current
    var serverUrl by remember { mutableStateOf(state.serverUrl) }
    var callsign by remember { mutableStateOf(state.callsign) }
    var showCreateDialog by remember { mutableStateOf(false) }
    val recentUrls = remember { mutableStateOf(RecentAddressPrefs.SERVER_URLS.load(context)) }
    val activeGames = state.serverStatus?.activeGameList.orEmpty()

    // Auto-refresh lobby list every 5 seconds while connected
    if (state.status == ConnectionStatus.CONNECTED) {
        LaunchedEffect(Unit) {
            while (true) {
                delay(5000)
                MatchmakingService.requestLobbyList()
            }
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
        val isConnecting =
            state.status == ConnectionStatus.CONNECTING ||
                state.status == ConnectionStatus.AUTHENTICATING ||
                state.status == ConnectionStatus.RECONNECTING
        if (state.status == ConnectionStatus.DISCONNECTED || isConnecting) {
            OutlinedTextField(
                value = serverUrl,
                onValueChange = { serverUrl = it },
                label = { Text("Server URL") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            RecentSuggestions(recentUrls.value) { serverUrl = it }
            Spacer(Modifier.height(4.dp))
            OutlinedTextField(
                value = callsign,
                onValueChange = { callsign = it.take(CallsignPrefs.MAX_LEN) },
                label = { Text("Callsign") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = {
                        CallsignPrefs.save(context, callsign)
                        RecentAddressPrefs.SERVER_URLS.add(context, serverUrl)
                        recentUrls.value = RecentAddressPrefs.SERVER_URLS.load(context)
                        MatchmakingService.connect(serverUrl, callsign)
                    },
                    enabled = !isConnecting,
                    modifier = Modifier.weight(1f),
                ) {
                    Text("Connect")
                }
                if (isConnecting) {
                    OutlinedButton(onClick = { MatchmakingService.disconnect() }) {
                        Text("Cancel")
                    }
                }
                Button(
                    onClick = {
                        CallsignPrefs.save(context, callsign)
                        MatchmakingStateHolder.update { it.copy(callsign = callsign) }
                        MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.LAN) }
                    },
                    enabled = !isConnecting,
                ) {
                    Text("LAN")
                }
            }
        } else {
            // Connected state - two rows to avoid portrait crowding
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Button(onClick = { MatchmakingService.requestLobbyList() }) {
                    Text("Refresh Lobbies")
                }
                Button(onClick = { showCreateDialog = true }) {
                    Text("Create Lobby")
                }
                OutlinedButton(onClick = { MatchmakingService.disconnect() }) {
                    Text("Disconnect")
                }
            }
            Spacer(Modifier.height(4.dp))
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth(),
            ) {
                val pendingCount = state.pendingFriendRequests.size
                val friendLabel = if (pendingCount > 0) "Friends ($pendingCount)" else "Friends"
                Button(onClick = {
                    MatchmakingService.requestFriendList()
                    MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.FRIENDS) }
                }) {
                    Text(friendLabel)
                }
                Button(onClick = {
                    MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.LAN) }
                }) {
                    Text("LAN")
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

        // -- Maintenance warning --
        state.maintenanceMessage?.let { msg ->
            Spacer(Modifier.height(4.dp))
            Text(
                msg,
                color = MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.bodySmall,
            )
        }

        Spacer(Modifier.height(12.dp))
        HorizontalDivider()
        Spacer(Modifier.height(8.dp))

        // -- Lobby list + active games --
        if (state.lobbies.isNotEmpty() || activeGames.isNotEmpty()) {
            LazyColumn(modifier = Modifier.weight(1f)) {
                if (state.lobbies.isNotEmpty()) {
                    item {
                        Text(
                            "Lobbies (${state.lobbies.size})",
                            style = MaterialTheme.typography.titleSmall,
                        )
                        Spacer(Modifier.height(4.dp))
                    }
                    items(state.lobbies, key = { it.lobbyId }) { lobby ->
                        LobbyCard(lobby)
                        Spacer(Modifier.height(4.dp))
                    }
                }
                if (activeGames.isNotEmpty()) {
                    item {
                        Spacer(Modifier.height(8.dp))
                        Text(
                            "Active Games (${activeGames.size})",
                            style = MaterialTheme.typography.titleSmall,
                        )
                        Spacer(Modifier.height(4.dp))
                    }
                    items(activeGames, key = { "${it.hostCallsign}-${it.mission}" }) { game ->
                        ActiveGameCard(game)
                        Spacer(Modifier.height(4.dp))
                    }
                }
            }
        } else if (state.status == ConnectionStatus.CONNECTED) {
            Text("No lobbies found.", style = MaterialTheme.typography.bodyMedium)
            Spacer(Modifier.weight(1f))
        } else {
            Spacer(Modifier.weight(1f))
        }

        // -- Network events --
        NetworkEventsPanel(state)
        Spacer(Modifier.height(4.dp))

        // -- Status log --
        StatusLog(state.statusLog)
    }

    if (showCreateDialog) {
        CreateLobbyDialog(
            onCreate = { game, maxPlayers, gameInfo ->
                showCreateDialog = false
                MatchmakingService.createLobby(game, maxPlayers, gameInfo)
            },
            onDismiss = { showCreateDialog = false },
        )
    }
}

@Composable
private fun LobbyCard(lobby: LobbyInfo) {
    var showCodeDialog by remember { mutableStateOf(false) }
    val isConnected =
        MatchmakingStateHolder.state
            .collectAsState()
            .value.status == ConnectionStatus.CONNECTED

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(lobby.hostCallsign, style = MaterialTheme.typography.titleSmall)
                if (lobby.lobbyState == "in_progress") {
                    Spacer(Modifier.width(6.dp))
                    Text(
                        "IN GAME",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary,
                    )
                }
                Spacer(Modifier.weight(1f))
                lobby.hostPingMs?.let { ping ->
                    Text(
                        "${ping}ms",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(Modifier.width(8.dp))
                }
                Text(
                    "${lobby.playerCount}/${lobby.maxPlayers}",
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
            val diffNames = listOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")
            val diff = lobby.gameInfo["difficulty"]?.jsonPrimitive?.intOrNull
            val level = lobby.gameInfo["level_num"]?.jsonPrimitive?.intOrNull
            val configLine =
                buildString {
                    val mission = lobby.gameInfo["mission"]?.jsonPrimitive?.content ?: ""
                    val mode = lobby.gameInfo["mode"]?.jsonPrimitive?.content ?: ""
                    append(mission)
                    if (mode.isNotEmpty()) append(" -- $mode")
                    if (diff != null) append(" / ${diffNames.getOrElse(diff) { "Diff $diff" }}")
                    // For in-progress games, show current level from server
                    val displayLevel = if (lobby.lobbyState == "in_progress") lobby.currentLevel else level
                    if (displayLevel != null) append(" / Lv $displayLevel")
                }
            Text(configLine, style = MaterialTheme.typography.bodySmall)
            if (lobby.hasCode) {
                Text("(code required)", style = MaterialTheme.typography.bodySmall)
            }
            if (lobby.joinable && isConnected) {
                Spacer(Modifier.height(4.dp))
                Button(
                    onClick = {
                        if (lobby.hasCode) {
                            showCodeDialog = true
                        } else {
                            MatchmakingService.joinLobby(lobby.lobbyId)
                        }
                    },
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text("Join")
                }
            }
        }
    }

    if (showCodeDialog) {
        LobbyCodeDialog(
            onJoin = { code ->
                showCodeDialog = false
                MatchmakingService.joinLobby(lobby.lobbyId, code)
            },
            onDismiss = { showCodeDialog = false },
        )
    }
}

@Composable
private fun LobbyCodeDialog(
    onJoin: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var code by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Enter Lobby Code") },
        text = {
            OutlinedTextField(
                value = code,
                onValueChange = { code = it },
                label = { Text("Code") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
        },
        confirmButton = {
            TextButton(onClick = { onJoin(code) }, enabled = code.isNotBlank()) {
                Text("Join")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun CreateLobbyDialog(
    onCreate: (game: String, maxPlayers: Int, gameInfo: JsonObject) -> Unit,
    onDismiss: () -> Unit,
) {
    var game by remember { mutableStateOf("d2") }
    var mission by remember { mutableStateOf<String?>(null) }
    var mode by remember { mutableStateOf("anarchy") }
    var maxPlayersText by remember { mutableStateOf("4") }
    var difficulty by remember { mutableStateOf(1) }
    var levelNumText by remember { mutableStateOf("1") }

    val difficultyNames = listOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Create Lobby") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                // Game selector: simple toggle between d1/d2
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf("d1", "d2").forEach { g ->
                        if (g == game) {
                            Button(onClick = {}) { Text(g.uppercase()) }
                        } else {
                            OutlinedButton(onClick = {
                                game = g
                                mission = null
                            }) { Text(g.uppercase()) }
                        }
                    }
                }
                MissionPickerField(
                    selectedFilename = mission,
                    game = game,
                    onSelect = { mission = it },
                )
                // Mode selector
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf("anarchy", "coop").forEach { m ->
                        if (m == mode) {
                            Button(onClick = {}) { Text(m.replaceFirstChar { it.uppercase() }) }
                        } else {
                            OutlinedButton(onClick = { mode = m }) {
                                Text(m.replaceFirstChar { it.uppercase() })
                            }
                        }
                    }
                }
                // Difficulty selector
                Text("Difficulty", style = MaterialTheme.typography.labelMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    difficultyNames.forEachIndexed { idx, name ->
                        if (idx == difficulty) {
                            Button(onClick = {}, modifier = Modifier.weight(1f)) {
                                Text(name, maxLines = 1, fontSize = 10.sp)
                            }
                        } else {
                            OutlinedButton(
                                onClick = { difficulty = idx },
                                modifier = Modifier.weight(1f),
                            ) {
                                Text(name, maxLines = 1, fontSize = 10.sp)
                            }
                        }
                    }
                }
                // Level number + Max players on same row
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = levelNumText,
                        onValueChange = { levelNumText = it.filter { c -> c.isDigit() } },
                        label = { Text("Level") },
                        singleLine = true,
                        modifier = Modifier.weight(1f),
                    )
                    OutlinedTextField(
                        value = maxPlayersText,
                        onValueChange = { maxPlayersText = it.filter { c -> c.isDigit() } },
                        label = { Text("Max Players") },
                        singleLine = true,
                        modifier = Modifier.weight(1f),
                    )
                }
            }
        },
        confirmButton = {
            val maxPlayers = maxPlayersText.toIntOrNull() ?: 0
            val levelNum = levelNumText.toIntOrNull() ?: 1
            TextButton(
                onClick = {
                    val gameInfo =
                        JsonObject(
                            mapOf(
                                "mission" to JsonPrimitive(mission ?: ""),
                                "mode" to JsonPrimitive(mode),
                                "difficulty" to JsonPrimitive(difficulty),
                                "level_num" to JsonPrimitive(levelNum),
                            ),
                        )
                    onCreate(game, maxPlayers, gameInfo)
                },
                enabled = mission != null && maxPlayers in 2..8 && levelNum >= 1,
            ) {
                Text("Create")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun LanContent(
    state: MatchmakingState,
    onBack: () -> Unit,
    onLaunchGame: (GameLaunchInfo) -> Unit,
) {
    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .padding(16.dp),
    ) {
        Text("LAN Games", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(4.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = {
                MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
            }) { Text("Back to Lobbies") }
            OutlinedButton(onClick = onBack) { Text("Back") }
        }
        Spacer(Modifier.height(8.dp))

        LanDiscoveryTab(callsign = state.callsign, onLaunchGame = onLaunchGame)
    }
}

@Composable
private fun FriendsContent(
    state: MatchmakingState,
    onBack: () -> Unit,
) {
    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .padding(16.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Friends", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.weight(1f))
            OutlinedButton(onClick = {
                MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
            }) { Text("Back to Lobbies") }
            Spacer(Modifier.width(8.dp))
            OutlinedButton(onClick = onBack) { Text("Back") }
        }
        Spacer(Modifier.height(8.dp))

        FriendsTab(
            friends = state.friends,
            pendingRequests = state.pendingFriendRequests,
        )
    }
}

@Composable
internal fun StatusLog(statusLog: List<String>) {
    HorizontalDivider()
    Spacer(Modifier.height(4.dp))
    Text("Log", style = MaterialTheme.typography.titleSmall)
    val logListState = rememberLazyListState()
    LaunchedEffect(statusLog.size) {
        if (statusLog.isNotEmpty()) {
            logListState.animateScrollToItem(statusLog.size - 1)
        }
    }
    LazyColumn(
        state = logListState,
        modifier =
            Modifier
                .height(120.dp)
                .fillMaxWidth(),
    ) {
        items(statusLog) { line ->
            Text(line, fontSize = 11.sp, fontFamily = FontFamily.Monospace, maxLines = 2)
        }
    }
}
