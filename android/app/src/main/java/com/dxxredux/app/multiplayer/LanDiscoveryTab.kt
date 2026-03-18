package com.dxxredux.app.multiplayer

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.dxxredux.app.lobby.LobbyService

@Composable
fun LanDiscoveryTab(
    callsign: String,
    onLaunchGame: (GameLaunchInfo) -> Unit,
) {
    val context = LocalContext.current
    val discoveredLobbies by LobbyService.discoveredLobbies.collectAsState()
    val isHosting by LobbyService.isHosting.collectAsState()
    val isDiscovering by LobbyService.isDiscovering.collectAsState()
    val hostedPlayers by LobbyService.hostedLobbyPlayers.collectAsState()
    val lanLaunchEvent by LobbyService.lanLaunchEvent.collectAsState()

    var showHostDialog by remember { mutableStateOf(false) }
    var showStartGameDialog by remember { mutableStateOf(false) }
    var permissionGranted by remember {
        mutableStateOf(
            if (Build.VERSION.SDK_INT >= 33) {
                ContextCompat.checkSelfPermission(
                    context,
                    Manifest.permission.NEARBY_WIFI_DEVICES,
                ) == PackageManager.PERMISSION_GRANTED
            } else {
                true // Pre-33 devices don't need runtime permission for this
            },
        )
    }

    val permissionLauncher =
        rememberLauncherForActivityResult(
            ActivityResultContracts.RequestPermission(),
        ) { granted ->
            permissionGranted = granted
            if (granted && !isDiscovering) {
                LobbyService.startDiscovery(context, callsign)
            }
        }

    // Start/stop discovery with lifecycle
    DisposableEffect(permissionGranted) {
        if (permissionGranted && !isDiscovering) {
            LobbyService.startDiscovery(context, callsign)
        }
        onDispose {
            // Don't stop discovery on recomposition, only when leaving the screen
        }
    }

    // Consume LAN launch events (from host Start or joiner receiving START)
    LaunchedEffect(lanLaunchEvent) {
        val info = lanLaunchEvent ?: return@LaunchedEffect
        LobbyService.clearLaunchEvent()
        onLaunchGame(info)
    }

    Column(
        modifier = Modifier.fillMaxSize(),
    ) {
        if (!permissionGranted) {
            // Permission request card
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        "LAN discovery requires the Nearby Wi-Fi Devices permission.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    Spacer(Modifier.height(8.dp))
                    Button(
                        onClick = {
                            if (Build.VERSION.SDK_INT >= 33) {
                                permissionLauncher.launch(
                                    Manifest.permission.NEARBY_WIFI_DEVICES,
                                )
                            }
                        },
                    ) {
                        Text("Grant Permission")
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
        }

        // -- Action buttons --
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            if (!isDiscovering) {
                Button(
                    onClick = {
                        if (permissionGranted) {
                            LobbyService.startDiscovery(context, callsign)
                        } else if (Build.VERSION.SDK_INT >= 33) {
                            permissionLauncher.launch(
                                Manifest.permission.NEARBY_WIFI_DEVICES,
                            )
                        }
                    },
                ) {
                    Text("Start Scanning")
                }
            } else {
                OutlinedButton(onClick = { LobbyService.stopDiscovery() }) {
                    Text("Stop Scanning")
                }
            }

            if (!isHosting && isDiscovering) {
                Button(onClick = { showHostDialog = true }) {
                    Text("Host LAN Game")
                }
            } else if (isHosting) {
                OutlinedButton(onClick = { LobbyService.stopHosting() }) {
                    Text("Stop Hosting")
                }
            }
        }

        Spacer(Modifier.height(12.dp))

        // -- Hosted lobby info --
        if (isHosting) {
            Text("Your Hosted Lobby", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.height(4.dp))
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(12.dp)) {
                    Text(
                        "Players (${hostedPlayers.size}):",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    for (p in hostedPlayers) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.padding(vertical = 2.dp),
                        ) {
                            Text(
                                p.callsign,
                                style = MaterialTheme.typography.bodyMedium,
                                modifier = Modifier.weight(1f),
                            )
                            Text(
                                if (p.ready) "Ready" else "Not Ready",
                                style = MaterialTheme.typography.bodySmall,
                                color =
                                    if (p.ready) {
                                        MaterialTheme.colorScheme.primary
                                    } else {
                                        MaterialTheme.colorScheme.onSurfaceVariant
                                    },
                            )
                        }
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
            Button(
                onClick = { showStartGameDialog = true },
                modifier = Modifier.fillMaxWidth(),
                enabled = hostedPlayers.size >= 2,
            ) {
                Text("Start Game")
            }
            Spacer(Modifier.height(12.dp))
            HorizontalDivider()
            Spacer(Modifier.height(8.dp))
        }

        // -- Discovered lobbies --
        if (isDiscovering) {
            Text(
                "Local Network Games (${discoveredLobbies.size})",
                style = MaterialTheme.typography.titleSmall,
            )
            Spacer(Modifier.height(4.dp))

            if (discoveredLobbies.isEmpty()) {
                Text(
                    "Scanning for LAN games...",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            } else {
                LazyColumn(modifier = Modifier.weight(1f)) {
                    items(
                        discoveredLobbies,
                        key = { it.announce.lobbyId },
                    ) { lobby ->
                        LanLobbyCard(lobby, callsign)
                        Spacer(Modifier.height(4.dp))
                    }
                }
            }
        }
    }

    if (showHostDialog) {
        HostLanGameDialog(
            onHost = { game, mission, mode, maxPlayers ->
                showHostDialog = false
                LobbyService.hostLobby(callsign, game, mission, mode, maxPlayers)
            },
            onDismiss = { showHostDialog = false },
        )
    }

    if (showStartGameDialog) {
        StartLanGameDialog(
            onStart = { difficulty, levelNum ->
                showStartGameDialog = false
                LobbyService.startGame(difficulty, levelNum)
            },
            onDismiss = { showStartGameDialog = false },
        )
    }
}

@Composable
private fun LanLobbyCard(
    lobby: LobbyService.DiscoveredLobby,
    myCallsign: String,
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    lobby.announce.callsign,
                    style = MaterialTheme.typography.titleSmall,
                )
                Spacer(Modifier.weight(1f))
                Text(
                    "${lobby.announce.playerCount}/${lobby.announce.maxPlayers}",
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
            Text(
                "${lobby.announce.mission} -- ${lobby.announce.mode} (${lobby.announce.game})",
                style = MaterialTheme.typography.bodySmall,
            )
            Text(
                "Host: ${lobby.announce.hostAddress}",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(4.dp))
            Button(
                onClick = {
                    LobbyService.joinLobby(
                        lobby.announce.lobbyId,
                        lobby.announce.hostAddress,
                        myCallsign,
                    )
                },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("Join")
            }
        }
    }
}

@Composable
private fun HostLanGameDialog(
    onHost: (game: String, mission: String, mode: String, maxPlayers: Int) -> Unit,
    onDismiss: () -> Unit,
) {
    var game by remember { mutableStateOf("d2") }
    var mission by remember { mutableStateOf("") }
    var missionSelected by remember { mutableStateOf(false) }
    var mode by remember { mutableStateOf("coop") }
    var maxPlayersText by remember { mutableStateOf("4") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Host LAN Game") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf("d1", "d2").forEach { g ->
                        if (g == game) {
                            Button(onClick = {}) { Text(g.uppercase()) }
                        } else {
                            OutlinedButton(onClick = {
                                game = g
                                mission = ""
                                missionSelected = false
                            }) { Text(g.uppercase()) }
                        }
                    }
                }
                MissionPickerField(
                    selectedFilename = mission,
                    game = game,
                    onSelect = {
                        mission = it
                        missionSelected = true
                    },
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf("anarchy", "coop").forEach { m ->
                        if (m == mode) {
                            Button(onClick = {}) {
                                Text(m.replaceFirstChar { it.uppercase() })
                            }
                        } else {
                            OutlinedButton(onClick = { mode = m }) {
                                Text(m.replaceFirstChar { it.uppercase() })
                            }
                        }
                    }
                }
                OutlinedTextField(
                    value = maxPlayersText,
                    onValueChange = { maxPlayersText = it.filter { c -> c.isDigit() } },
                    label = { Text("Max Players") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        },
        confirmButton = {
            val maxPlayers = maxPlayersText.toIntOrNull() ?: 0
            TextButton(
                onClick = { onHost(game, mission, mode, maxPlayers) },
                enabled = missionSelected && maxPlayers in 2..8,
            ) {
                Text("Host")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun StartLanGameDialog(
    onStart: (difficulty: Int, levelNum: Int) -> Unit,
    onDismiss: () -> Unit,
) {
    val difficulties = listOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")
    var difficulty by remember { mutableStateOf(1) }
    var levelText by remember { mutableStateOf("1") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Start Game") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Difficulty", style = MaterialTheme.typography.bodyMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    difficulties.forEachIndexed { idx, name ->
                        if (idx == difficulty) {
                            Button(onClick = {}, modifier = Modifier.weight(1f)) {
                                Text(name, maxLines = 1, fontSize = 11.sp)
                            }
                        } else {
                            OutlinedButton(
                                onClick = { difficulty = idx },
                                modifier = Modifier.weight(1f),
                            ) {
                                Text(name, maxLines = 1, fontSize = 11.sp)
                            }
                        }
                    }
                }
                OutlinedTextField(
                    value = levelText,
                    onValueChange = { levelText = it.filter { c -> c.isDigit() } },
                    label = { Text("Starting Level") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        },
        confirmButton = {
            val level = levelText.toIntOrNull() ?: 0
            TextButton(
                onClick = { onStart(difficulty, level) },
                enabled = level in 1..30,
            ) {
                Text("Start")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}
