package com.dxxredux.app.multiplayer

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
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
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import com.dxxredux.app.BuildInfo
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.lobby.LobbyService
import java.net.Inet4Address
import java.net.NetworkInterface

// Address caching for LAN dialogs -- see RecentAddressPrefs.kt
private val LanIpsPrefs = RecentAddressPrefs.LAN_IPS

/** Once-per-process default IP prefix derived from the device's own IPv4 address. */
private var defaultIpPrefix: String? = null
private var defaultIpPrefixInitialized = false

private fun getDefaultIpPrefix(): String {
    if (!defaultIpPrefixInitialized) {
        defaultIpPrefixInitialized = true
        val ips = getLocalIpAddresses()
        if (ips.isNotEmpty()) {
            val parts = ips[0].split(".")
            if (parts.size == 4) {
                defaultIpPrefix = "${parts[0]}.${parts[1]}.${parts[2]}."
            }
        }
    }
    return defaultIpPrefix ?: ""
}

@Composable
fun LanDiscoveryTab(
    callsign: String,
    onLaunchGame: (GameLaunchInfo) -> Unit,
) {
    val joinedLobby by LobbyService.joinedLobby.collectAsState()

    if (joinedLobby != null) {
        LanJoinedLobbyView(callsign, onLaunchGame)
    } else {
        LanDiscoveryView(callsign, onLaunchGame)
    }
}

@Composable
private fun LanJoinedLobbyView(
    callsign: String,
    onLaunchGame: (GameLaunchInfo) -> Unit,
) {
    val joinedLobby by LobbyService.joinedLobby.collectAsState()
    val players by LobbyService.hostedLobbyPlayers.collectAsState()
    val lanLaunchEvent by LobbyService.lanLaunchEvent.collectAsState()
    val info = joinedLobby ?: return

    // Consume LAN launch events
    LaunchedEffect(lanLaunchEvent) {
        val launchInfo = lanLaunchEvent ?: return@LaunchedEffect
        LobbyService.clearLaunchEvent()
        onLaunchGame(launchInfo)
    }

    Column(modifier = Modifier.fillMaxSize()) {
        // Header
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("LAN Lobby", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.weight(1f))
            OutlinedButton(onClick = { LobbyService.leaveLanLobby(callsign) }) {
                Text("Leave")
            }
        }
        Spacer(Modifier.height(8.dp))

        // Lobby info
        Text(
            "${info.mission} -- ${info.mode} (${info.game})",
            style = MaterialTheme.typography.bodyMedium,
        )
        Text(
            "Host: ${info.hostAddr}",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (info.hostBuild.isNotEmpty() && info.hostBuild != BuildInfo.GIT_COMMIT_COUNT) {
            Text(
                "Warning: host build (${info.hostBuild}) differs from yours (${BuildInfo.GIT_COMMIT_COUNT})",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error,
            )
        }
        Spacer(Modifier.height(12.dp))

        // Player list
        Text(
            "Players (${players.size}/${info.maxPlayers})",
            style = MaterialTheme.typography.titleSmall,
        )
        Spacer(Modifier.height(4.dp))
        LazyColumn(modifier = Modifier.weight(1f)) {
            items(players, key = { it.callsign }) { player ->
                val displayName =
                    if (player.callsign == callsign) "${player.callsign} (self)" else player.callsign
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.padding(12.dp),
                    ) {
                        Text(
                            displayName,
                            style = MaterialTheme.typography.bodyMedium,
                            modifier = Modifier.weight(1f),
                        )
                        Text(
                            if (player.ready) "Ready" else "Not Ready",
                            style = MaterialTheme.typography.bodySmall,
                            color =
                                if (player.ready) {
                                    MaterialTheme.colorScheme.primary
                                } else {
                                    MaterialTheme.colorScheme.onSurfaceVariant
                                },
                        )
                    }
                }
                Spacer(Modifier.height(4.dp))
            }
        }
        Spacer(Modifier.height(8.dp))

        // Ready toggle
        val myReady = players.find { it.callsign == callsign }?.ready ?: false
        Button(
            onClick = {
                LobbyService.setReady(info.lobbyId, info.hostAddr, callsign, !myReady)
            },
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(if (myReady) "Unready" else "Ready")
        }
    }
}

@Composable
private fun LanDiscoveryView(
    callsign: String,
    onLaunchGame: (GameLaunchInfo) -> Unit,
) {
    val context = LocalContext.current
    val discoveredLobbies by LobbyService.discoveredLobbies.collectAsState()
    val isHosting by LobbyService.isHosting.collectAsState()
    val isDiscovering by LobbyService.isDiscovering.collectAsState()
    val hostedPlayers by LobbyService.hostedLobbyPlayers.collectAsState()
    val lanLaunchEvent by LobbyService.lanLaunchEvent.collectAsState()
    val diagnostics by LobbyService.diagnostics.collectAsState()
    val broadcastFailing by LobbyService.broadcastFailing.collectAsState()

    var showHostDialog by remember { mutableStateOf(false) }
    var showStartGameDialog by remember { mutableStateOf(false) }
    var showJoinByIpDialog by remember { mutableStateOf(false) }
    var showJoinLobbyByIpDialog by remember { mutableStateOf(false) }
    var hostedLevelCount by remember { mutableStateOf(30) }
    val recentIps = remember { mutableStateOf(LanIpsPrefs.load(context)) }
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
    var permissionPermanentlyDenied by remember { mutableStateOf(false) }

    val permissionLauncher =
        rememberLauncherForActivityResult(
            ActivityResultContracts.RequestPermission(),
        ) { granted ->
            permissionGranted = granted
            if (granted) {
                permissionPermanentlyDenied = false
                if (!isDiscovering) {
                    LobbyService.startDiscovery(context, callsign)
                }
            } else if (Build.VERSION.SDK_INT >= 33) {
                val activity = context as? Activity
                if (activity != null &&
                    !activity.shouldShowRequestPermissionRationale(
                        Manifest.permission.NEARBY_WIFI_DEVICES,
                    )
                ) {
                    permissionPermanentlyDenied = true
                }
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

    // Re-check permission when returning from Settings
    @Suppress("DEPRECATION")
    val lifecycleOwner = LocalLifecycleOwner.current
    DisposableEffect(lifecycleOwner) {
        val observer =
            LifecycleEventObserver { _, event ->
                if (event == Lifecycle.Event.ON_RESUME && Build.VERSION.SDK_INT >= 33) {
                    val granted =
                        ContextCompat.checkSelfPermission(
                            context,
                            Manifest.permission.NEARBY_WIFI_DEVICES,
                        ) == PackageManager.PERMISSION_GRANTED
                    permissionGranted = granted
                    if (granted) permissionPermanentlyDenied = false
                }
            }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
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
                    if (permissionPermanentlyDenied) {
                        Text(
                            "Permission was denied. Please enable it in app settings.",
                            style = MaterialTheme.typography.bodySmall,
                        )
                        Spacer(Modifier.height(8.dp))
                        Button(
                            onClick = {
                                val intent =
                                    Intent(
                                        Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                                        Uri.fromParts("package", context.packageName, null),
                                    )
                                context.startActivity(intent)
                            },
                        ) {
                            Text("Open Settings")
                        }
                    } else {
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

        if (!isHosting && isDiscovering) {
            Spacer(Modifier.height(4.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { showJoinLobbyByIpDialog = true }) {
                    Text("Join Lobby by IP")
                }
                OutlinedButton(onClick = { showJoinByIpDialog = true }) {
                    Text("Join Game by IP")
                }
            }
        }

        // Show local IP address for direct-connect reference
        val localIps = remember { getLocalIpAddresses() }
        if (localIps.isNotEmpty()) {
            Spacer(Modifier.height(4.dp))
            Text(
                if (localIps.size == 1) {
                    "Your IP: ${localIps[0]}"
                } else {
                    "Your IPs: ${localIps.joinToString(", ")}"
                },
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
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
                        val displayName =
                            if (p.callsign == callsign) "${p.callsign} (self)" else p.callsign
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.padding(vertical = 2.dp),
                        ) {
                            Text(
                                displayName,
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

        // Diagnostics status line
        if (diagnostics.isNotEmpty()) {
            Spacer(Modifier.height(8.dp))
            Text(
                diagnostics,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error,
            )
            if (broadcastFailing) {
                Spacer(Modifier.height(4.dp))
                OutlinedButton(
                    onClick = {
                        val intent =
                            Intent(
                                Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                                Uri.fromParts("package", context.packageName, null),
                            )
                        context.startActivity(intent)
                    },
                ) {
                    Text("Open App Settings")
                }
            }
        }
    }

    if (showHostDialog) {
        HostLanGameDialog(
            onHost = { game, mission, mode, maxPlayers, levelCount ->
                showHostDialog = false
                hostedLevelCount = if (levelCount > 0) levelCount else 30
                LobbyService.hostLobby(callsign, game, mission, mode, maxPlayers)
            },
            onDismiss = { showHostDialog = false },
        )
    }

    if (showStartGameDialog) {
        StartLanGameDialog(
            levelCount = hostedLevelCount,
            onStart = { difficulty, levelNum ->
                showStartGameDialog = false
                LobbyService.startGame(difficulty, levelNum)
            },
            onDismiss = { showStartGameDialog = false },
        )
    }

    if (showJoinByIpDialog) {
        JoinByIpDialog(
            recentIps = recentIps.value,
            onJoin = { hostAddr, game ->
                showJoinByIpDialog = false
                LanIpsPrefs.add(context, hostAddr)
                recentIps.value = LanIpsPrefs.load(context)
                // Direct LAN join: launch game as joiner pointed at the given IP
                onLaunchGame(
                    GameLaunchInfo(
                        game = game,
                        mission = "",
                        mode = "coop",
                        difficulty = 1,
                        levelNum = 1,
                        maxPlayers = 4,
                        yourSlot = 1,
                        isHost = false,
                        peers = emptyList(),
                        lanHostAddr = hostAddr,
                        isLan = true,
                    ),
                )
            },
            onDismiss = { showJoinByIpDialog = false },
        )
    }

    if (showJoinLobbyByIpDialog) {
        JoinLobbyByIpDialog(
            recentIps = recentIps.value,
            onJoin = { hostAddr ->
                showJoinLobbyByIpDialog = false
                LanIpsPrefs.add(context, hostAddr)
                recentIps.value = LanIpsPrefs.load(context)
                // Send a lobby-protocol JOIN to the given IP (use empty lobbyId; host will respond)
                LobbyService.joinLobbyByIp(hostAddr, callsign)
            },
            onDismiss = { showJoinLobbyByIpDialog = false },
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
            if (lobby.announce.build.isNotEmpty() && lobby.announce.build != BuildInfo.GIT_COMMIT_COUNT) {
                Text(
                    "Version mismatch (host: ${lobby.announce.build}, you: ${BuildInfo.GIT_COMMIT_COUNT})",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error,
                )
            }
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
    onHost: (game: String, mission: String, mode: String, maxPlayers: Int, levelCount: Int) -> Unit,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val fsm = remember { FileSetManager(context.filesDir) }
    val setDir = remember(fsm) { fsm.getSetDir(fsm.getActive()) }
    var game by remember { mutableStateOf("d2") }
    var mission by remember { mutableStateOf<String?>(null) }
    var mode by remember { mutableStateOf("coop") }
    var maxPlayersText by remember { mutableStateOf("4") }
    val missions = remember(game, setDir) { MissionScanner.scan(setDir, game) }
    val selectedLevelCount =
        remember(mission, missions) {
            missions.find { it.filename == mission }?.levelCount ?: 0
        }

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
                onClick = { onHost(game, mission ?: "", mode, maxPlayers, selectedLevelCount) },
                enabled = mission != null && maxPlayers in 2..8,
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
    levelCount: Int,
    onStart: (difficulty: Int, levelNum: Int) -> Unit,
    onDismiss: () -> Unit,
) {
    val maxLevel = if (levelCount > 0) levelCount else 30
    val difficulties = listOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")
    var difficulty by remember { mutableStateOf(1) }
    var selectedLevel by remember { mutableStateOf(1) }

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
                Text("Starting Level", style = MaterialTheme.typography.bodyMedium)
                LazyColumn(modifier = Modifier.heightIn(max = 200.dp)) {
                    items(maxLevel) { idx ->
                        val level = idx + 1
                        Text(
                            "Level $level",
                            style =
                                if (level == selectedLevel) {
                                    MaterialTheme.typography.bodyLarge
                                } else {
                                    MaterialTheme.typography.bodyMedium
                                },
                            color =
                                if (level == selectedLevel) {
                                    MaterialTheme.colorScheme.primary
                                } else {
                                    MaterialTheme.colorScheme.onSurface
                                },
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { selectedLevel = level }
                                    .padding(vertical = 6.dp, horizontal = 4.dp),
                        )
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = { onStart(difficulty, selectedLevel) },
                enabled = selectedLevel in 1..maxLevel,
            ) {
                Text("Start")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun JoinByIpDialog(
    recentIps: List<String>,
    onJoin: (hostAddr: String, game: String) -> Unit,
    onDismiss: () -> Unit,
) {
    var hostIp by remember { mutableStateOf(getDefaultIpPrefix()) }
    var selectedGame by remember { mutableStateOf("d2") }
    val isValidIp = isValidIpAddress(hostIp)

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Join Game by IP") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    "Enter the host's IP address to connect directly.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                OutlinedTextField(
                    value = hostIp,
                    onValueChange = { hostIp = it.trim() },
                    label = { Text("Host IP Address") },
                    placeholder = { Text("192.168.1.100") },
                    singleLine = true,
                    isError = hostIp.isNotBlank() && !isValidIp,
                    modifier = Modifier.fillMaxWidth(),
                )
                RecentSuggestions(recentIps) { hostIp = it }
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Game:", style = MaterialTheme.typography.bodyMedium)
                    listOf("d1" to "Descent 1", "d2" to "Descent 2").forEach { (key, label) ->
                        FilterChip(
                            selected = selectedGame == key,
                            onClick = { selectedGame = key },
                            label = { Text(label) },
                        )
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = { onJoin(hostIp, selectedGame) },
                enabled = isValidIp,
            ) {
                Text("Join")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun JoinLobbyByIpDialog(
    recentIps: List<String>,
    onJoin: (hostAddr: String) -> Unit,
    onDismiss: () -> Unit,
) {
    var hostIp by remember { mutableStateOf(getDefaultIpPrefix()) }
    val isValidIp = isValidIpAddress(hostIp)

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Join Lobby by IP") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    "Enter the host's IP to join their lobby directly (bypasses broadcast discovery).",
                    style = MaterialTheme.typography.bodyMedium,
                )
                OutlinedTextField(
                    value = hostIp,
                    onValueChange = { hostIp = it.trim() },
                    label = { Text("Host IP Address") },
                    placeholder = { Text("192.168.1.100") },
                    singleLine = true,
                    isError = hostIp.isNotBlank() && !isValidIp,
                    modifier = Modifier.fillMaxWidth(),
                )
                RecentSuggestions(recentIps) { hostIp = it }
            }
        },
        confirmButton = {
            TextButton(
                onClick = { onJoin(hostIp) },
                enabled = isValidIp,
            ) {
                Text("Join")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

// RecentIpSuggestions replaced by shared RecentSuggestions in RecentAddressPrefs.kt

private val ipPattern = Regex("""^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$""")

private fun isValidIpAddress(ip: String): Boolean =
    ip.matches(ipPattern) && ip.split(".").all { it.toIntOrNull() in 0..255 }

private fun getLocalIpAddresses(): List<String> =
    try {
        val interfaces = NetworkInterface.getNetworkInterfaces()?.toList() ?: emptyList()
        interfaces
            .filter { it.isUp && !it.isLoopback }
            .flatMap { it.inetAddresses.toList() }
            .filterIsInstance<Inet4Address>()
            .mapNotNull { it.hostAddress }
    } catch (_: Exception) {
        emptyList()
    }
