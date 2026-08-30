package com.dxxredux.app.multiplayer

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import com.dxxredux.app.BuildInfo
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.ModManager
import com.dxxredux.app.VisualReplacementPolicy
import com.dxxredux.app.formatBinarySize
import com.dxxredux.app.lobby.LobbyService
import com.dxxredux.app.tvFocusBorder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.net.Inet4Address
import java.net.NetworkInterface

// Address caching for LAN dialogs -- see RecentAddressPrefs.kt
private val LanIpsPrefs = RecentAddressPrefs.LAN_IPS

/** Once-per-process default IP prefix derived from the device's own IPv4 address. */
private var defaultIpPrefix: String? = null
private var defaultIpPrefixInitialized = false

private fun Context.findLifecycleOwner(): LifecycleOwner? =
    when (this) {
        is LifecycleOwner -> this
        is ContextWrapper -> baseContext.findLifecycleOwner()
        else -> null
    }

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
    onLaunchRequested: (String) -> Unit,
) {
    val joinedLobby by LobbyService.joinedLobby.collectAsState()
    val context = LocalContext.current
    val lifecycleOwner = remember(context) { context.findLifecycleOwner() }
    val currentCallsign by rememberUpdatedState(callsign)

    DisposableEffect(lifecycleOwner, context) {
        val owner = lifecycleOwner ?: return@DisposableEffect onDispose {}
        val observer =
            LifecycleEventObserver { _, event ->
                when (event) {
                    Lifecycle.Event.ON_PAUSE -> LobbyService.notifyAppBackgrounded()
                    Lifecycle.Event.ON_RESUME -> LobbyService.notifyAppResumed(context, currentCallsign)
                    else -> Unit
                }
            }
        owner.lifecycle.addObserver(observer)
        onDispose { owner.lifecycle.removeObserver(observer) }
    }

    if (joinedLobby != null) {
        LanJoinedLobbyView(callsign, onLaunchGame)
    } else {
        LanDiscoveryView(callsign, onLaunchGame, onLaunchRequested)
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
    val isHosting by LobbyService.isHosting.collectAsState()
    val chatMessages by LobbyService.chatMessages.collectAsState()
    val info = joinedLobby ?: return
    val readyFocus = remember { FocusRequester() }

    RequestControllerInitialFocus(readyFocus, revealFocusOnRequest = false)

    // Consume LAN launch events
    LaunchedEffect(lanLaunchEvent) {
        val launchInfo = lanLaunchEvent ?: return@LaunchedEffect
        LobbyService.clearLaunchEvent()
        onLaunchGame(launchInfo)
    }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .showControllerFocusOnDpad(readyFocus),
    ) {
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
        VisualReplacementPolicy
            .noticeText(
                info.stockVisualsEnforced,
                info.omittedVisualModCount,
                info.omittedVisualModNames,
            )?.let {
                Text(
                    it,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.primary,
                )
            }
        if (info.hostBuild.isNotEmpty() && info.hostBuild != BuildInfo.GIT_COMMIT_COUNT) {
            Text(
                "Warning: host build (${info.hostBuild}) differs from yours (${BuildInfo.GIT_COMMIT_COUNT})",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error,
            )
        }
        MissionStatusIndicator(info.missionStatus, Modifier.fillMaxWidth())
        val offeredMission = info.missionRequirement?.takeIf { it.isWrapper && it.offerAvailable }
        if (offeredMission != null && info.missionStatus?.status != MissionCompatibilityStatus.MATCH) {
            Text(
                "Host offer: ${offeredMission.wrapperFilename} - " +
                    "${formatBinarySize(offeredMission.sizeBytes ?: 0L)} - " +
                    "SHA-256 ${offeredMission.sha256?.take(12)}...",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        if (info.missionStatus?.status == MissionCompatibilityStatus.INSTALLED_DISABLED) {
            OutlinedButton(
                onClick = { LobbyService.enableMatchingMission() },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("Enable matching installed mission")
            }
        }
        val canDownload =
            info.missionRequirement?.offerAvailable == true &&
                info.missionStatus?.status in
                setOf(
                    MissionCompatibilityStatus.MISSING,
                    MissionCompatibilityStatus.SIZE_MISMATCH,
                    MissionCompatibilityStatus.HASH_MISMATCH,
                    MissionCompatibilityStatus.FAILED_RESUMABLE,
                )
        if (canDownload) {
            Button(
                onClick = { LobbyService.requestMissionDownload() },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(
                    if (info.missionStatus?.status == MissionCompatibilityStatus.FAILED_RESUMABLE) {
                        "Resume mission download"
                    } else {
                        "Download, enable, and use host mission"
                    },
                )
            }
        } else if (info.missionStatus?.status in
            setOf(
                MissionCompatibilityStatus.MISSING,
                MissionCompatibilityStatus.SIZE_MISMATCH,
                MissionCompatibilityStatus.HASH_MISMATCH,
            )
        ) {
            Text(
                "The host is not offering this mission for download",
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
        LazyColumn(modifier = Modifier.weight(0.5f)) {
            items(players, key = { it.callsign }) { player ->
                val isSelf = player.callsign == callsign
                val displayName =
                    if (isSelf) "${player.callsign} (self)" else player.callsign
                val displayedMissionStatus =
                    missionStatusForPlayerDisplay(isSelf, info.missionStatus, player.missionStatus)
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.padding(12.dp),
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(displayName, style = MaterialTheme.typography.bodyMedium)
                            MissionStatusIndicator(displayedMissionStatus)
                        }
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
                        if (isHosting && !isSelf) {
                            Spacer(Modifier.width(8.dp))
                            TextButton(
                                onClick = { LobbyService.kickPlayer(player.callsign) },
                            ) {
                                Text("Kick", fontSize = 12.sp)
                            }
                        }
                    }
                }
                Spacer(Modifier.height(4.dp))
            }
        }

        // Chat
        ChatArea(
            messages = chatMessages,
            onSend = { LobbyService.sendChat(callsign, it) },
            modifier = Modifier.weight(0.5f),
            textEntryFallbackFocusRequester = readyFocus,
        )

        Spacer(Modifier.height(8.dp))

        // Ready toggle
        val myReady = players.find { it.callsign == callsign }?.ready ?: false
        val myMissionMatches =
            info.missionStatus?.status == MissionCompatibilityStatus.MATCH ||
                players.find { it.callsign == callsign }?.missionStatus?.status == MissionCompatibilityStatus.MATCH
        Button(
            onClick = {
                LobbyService.setReady(info.lobbyId, info.hostAddr, callsign, !myReady)
            },
            modifier =
                Modifier
                    .fillMaxWidth()
                    .focusRequester(readyFocus),
            enabled = myReady || myMissionMatches,
        ) {
            Text(if (myReady) "Unready" else "Ready")
        }
    }
}

internal fun missionStatusForPlayerDisplay(
    isSelf: Boolean,
    localStatus: MissionStatusReport?,
    hostStatus: MissionStatusReport?,
): MissionStatusReport? = if (isSelf) localStatus ?: hostStatus else hostStatus

@Composable
private fun LanDiscoveryView(
    callsign: String,
    onLaunchGame: (GameLaunchInfo) -> Unit,
    onLaunchRequested: (String) -> Unit,
) {
    val context = LocalContext.current
    val isLandscape =
        LocalConfiguration.current.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
    val discoveredLobbies by LobbyService.discoveredLobbies.collectAsState()
    val isHosting by LobbyService.isHosting.collectAsState()
    val isDiscovering by LobbyService.isDiscovering.collectAsState()
    val hostedPlayers by LobbyService.hostedLobbyPlayers.collectAsState()
    val chatMessages by LobbyService.chatMessages.collectAsState()
    val lanLaunchEvent by LobbyService.lanLaunchEvent.collectAsState()
    val diagnostics by LobbyService.diagnostics.collectAsState()
    val broadcastFailing by LobbyService.broadcastFailing.collectAsState()
    val actionFocus = remember { FocusRequester() }
    val coroutineScope = rememberCoroutineScope()

    var showHostDialog by remember { mutableStateOf(false) }
    var showJoinByIpDialog by remember { mutableStateOf(false) }
    val hostDefaults = remember { HostGameDefaults.load(context) }
    var hostedGame by remember { mutableStateOf(hostDefaults.game) }
    var hostedMode by remember { mutableStateOf(hostDefaults.mode) }
    var hostedMission by remember { mutableStateOf(hostDefaults.mission) }
    var hostedDifficulty by remember { mutableStateOf(hostDefaults.difficulty) }
    var hostedLevelNum by remember { mutableStateOf(hostDefaults.levelNum) }
    var hostedCoopQol by remember { mutableStateOf(hostDefaults.coopQol) }
    var hostedDuplicateEnergyShields by remember { mutableStateOf(hostDefaults.duplicateEnergyShields) }
    var hostedFullDeathSpew by remember { mutableStateOf(hostDefaults.fullDeathSpew) }
    var hostedPlayerSpewNoExpire by remember { mutableStateOf(hostDefaults.playerSpewNoExpire) }
    var hostedClientsCanRequestRewind by remember { mutableStateOf(hostDefaults.clientsCanRequestRewind) }
    var hostedRestrictNonCoopFovToBase by remember { mutableStateOf(hostDefaults.restrictNonCoopFovToBase) }
    var hostedVisualSummary by remember { mutableStateOf(ModManager.VisualReplacementSummary()) }
    var dismissResumeOffer by remember { mutableStateOf(false) }
    var resumeStatus by remember { mutableStateOf<String?>(null) }
    val resumeRecord = remember { MultiplayerResumePrefs.load(context) }
    val resolvedResumeRecord = remember(resumeRecord) { resumeRecord?.let { resolveCoopHostResumeRecord(context, it) } }
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
    val localIpLabel = remember { getLocalIpLabel() }
    val focusTarget = lanDiscoveryInitialFocusTarget(permissionGranted)
    val permissionActionHasInitialFocus = focusTarget == LanDiscoveryInitialFocusTarget.PERMISSION_ACTION
    val lanActionHasInitialFocus = focusTarget == LanDiscoveryInitialFocusTarget.PRIMARY_ACTION

    RequestControllerInitialFocus(actionFocus, focusTarget, revealFocusOnRequest = false)

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

    fun hostResume(record: MultiplayerResumeRecord) {
        coroutineScope.launch {
            val hostCallsign = record.localCallsign
            HostGameDefaults.save(context, record.toHostDefaults())
            writeCoopRestoreChoice(context.filesDir, record.game, record.coopRestoreSelection())
            CallsignPrefs.save(context, hostCallsign)
            MatchmakingStateHolder.update { it.copy(callsign = hostCallsign) }
            hostedGame = record.game
            hostedMode = record.mode
            hostedMission = record.mission
            hostedDifficulty = record.difficulty
            hostedLevelNum = record.levelNum
            hostedCoopQol = record.coopQol
            hostedDuplicateEnergyShields = record.duplicateEnergyShields
            hostedFullDeathSpew = record.fullDeathSpew
            hostedPlayerSpewNoExpire = record.playerSpewNoExpire
            hostedClientsCanRequestRewind = record.clientsCanRequestRewind
            hostedRestrictNonCoopFovToBase = record.restrictNonCoopFovToBase
            val visualSummary = VisualReplacementPolicy.summaryForPvp(context, record.game, record.mode)
            hostedVisualSummary = visualSummary
            if (!LobbyService.isDiscovering.value) {
                LobbyService.startDiscovery(context, hostCallsign)
            }
            val fileSets = FileSetManager(context.filesDir)
            val setDir = fileSets.getSetDir(fileSets.getActive())
            val missionInfo =
                resolveMissionSelection(
                    MissionScanner.scan(context.filesDir, setDir, record.game, record.mode),
                    record.mission,
                ) ?: return@launch
            val requirement = MissionScanner.requirement(record.game, missionInfo, offerDownload = true)
            LobbyService.hostLobby(
                hostCallsign,
                record.game,
                record.mission,
                record.mode,
                record.maxPlayers,
                missionRequirement = requirement,
                restrictNonCoopFovToBase = record.restrictNonCoopFovToBase,
                stockVisualsEnforced = visualSummary.hasOmittedVisuals,
                omittedVisualModCount = visualSummary.omittedModCount,
                omittedVisualTextureCount = visualSummary.omittedTextureCount,
                omittedVisualModNames = visualSummary.omittedModNames,
            )
        }
    }

    fun clientResume(record: MultiplayerResumeRecord) {
        val hostAddr = record.lanHostAddr ?: return
        val resumeCallsign = record.localCallsign
        CallsignPrefs.save(context, resumeCallsign)
        MatchmakingStateHolder.update { it.copy(callsign = resumeCallsign) }
        if (!LobbyService.isDiscovering.value) {
            LobbyService.startDiscovery(context, resumeCallsign)
        }
        resumeStatus = "Looking for last host"
        coroutineScope.launch(Dispatchers.IO) {
            val foundLobby =
                LobbyService.tryJoinLobbyByIp(hostAddr, resumeCallsign, timeoutMs = 1500L) { announce ->
                    record.matchesLanResumeHost(announce)
                }
            withContext(Dispatchers.Main) {
                resumeStatus = if (foundLobby) null else "Last host not found"
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
    val lifecycleOwner = remember(context) { context.findLifecycleOwner() }
    DisposableEffect(lifecycleOwner, context) {
        val owner = lifecycleOwner ?: return@DisposableEffect onDispose {}
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
        owner.lifecycle.addObserver(observer)
        onDispose { owner.lifecycle.removeObserver(observer) }
    }

    // Consume LAN launch events (from host Start or joiner receiving START)
    LaunchedEffect(lanLaunchEvent) {
        val info = lanLaunchEvent ?: return@LaunchedEffect
        LobbyService.clearLaunchEvent()
        onLaunchGame(info)
    }

    LazyColumn(
        modifier =
            Modifier
                .fillMaxSize()
                .showControllerFocusOnDpad(actionFocus, focusTarget),
    ) {
        // -- Discovered lobbies --
        // Keep results first so the common single-game LAN path is immediately joinable
        if (isDiscovering) {
            item {
                Text(
                    "Scanning for LAN games...",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(4.dp))
                Text(
                    "Local Network Games (${discoveredLobbies.size})",
                    style = MaterialTheme.typography.titleSmall,
                )
                Spacer(Modifier.height(4.dp))
            }

            if (discoveredLobbies.isNotEmpty()) {
                items(
                    discoveredLobbies,
                    key = { it.announce.lobbyId },
                ) { lobby ->
                    LanLobbyCard(lobby, callsign, onJoinInGame = onLaunchGame)
                    Spacer(Modifier.height(4.dp))
                }
            }
        }

        if (!isLandscape && !isHosting) {
            item {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    if (localIpLabel != null) {
                        Text(
                            localIpLabel,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                            modifier = Modifier.weight(1f),
                        )
                    } else {
                        Spacer(Modifier.weight(1f))
                    }
                    OutlinedButton(
                        onClick = { showJoinByIpDialog = true },
                        contentPadding = PaddingValues(horizontal = 10.dp, vertical = 0.dp),
                        modifier = Modifier.height(32.dp),
                    ) {
                        Text("Join by IP", style = MaterialTheme.typography.bodySmall)
                    }
                }
                Spacer(Modifier.height(8.dp))
            }
        }

        if (!permissionGranted) {
            item {
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
                                modifier = Modifier.focusRequester(actionFocus),
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
                                modifier = Modifier.focusRequester(actionFocus),
                            ) {
                                Text("Grant Permission")
                            }
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
            }
        }

        // -- Action buttons --
        val offerRecord = resolvedResumeRecord?.takeIf { it.isHostLanCoop() || it.isClientLanCoop() }
        if (!dismissResumeOffer && offerRecord != null && !isHosting) {
            item {
                MultiplayerResumeOfferCard(
                    record = offerRecord,
                    primaryLabel = if (offerRecord.isHostLanCoop()) "Host Last Coop" else "Find Last Host",
                    onPrimary = {
                        if (permissionGranted || Build.VERSION.SDK_INT < 33) {
                            if (offerRecord.isHostLanCoop()) hostResume(offerRecord) else clientResume(offerRecord)
                        } else {
                            permissionLauncher.launch(Manifest.permission.NEARBY_WIFI_DEVICES)
                        }
                    },
                    onDismiss = { dismissResumeOffer = true },
                )
                resumeStatus?.let {
                    Spacer(Modifier.height(4.dp))
                    Text(
                        it,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                Spacer(Modifier.height(8.dp))
            }
        }

        item {
            if (isHosting) {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    OutlinedButton(
                        onClick = {
                            LobbyService.stopHosting()
                            hostedVisualSummary = ModManager.VisualReplacementSummary()
                        },
                        modifier =
                            Modifier
                                .weight(1f)
                                .then(if (lanActionHasInitialFocus) Modifier.focusRequester(actionFocus) else Modifier),
                    ) {
                        Text("Stop Hosting")
                    }
                }
            } else if (isLandscape) {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Button(
                        onClick = {
                            if (permissionGranted || Build.VERSION.SDK_INT < 33) {
                                showHostDialog = true
                            } else {
                                permissionLauncher.launch(
                                    Manifest.permission.NEARBY_WIFI_DEVICES,
                                )
                            }
                        },
                        modifier =
                            Modifier
                                .weight(1f)
                                .then(if (lanActionHasInitialFocus) Modifier.focusRequester(actionFocus) else Modifier),
                    ) {
                        Text("Host LAN Game")
                    }
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
                            modifier = Modifier.weight(1f),
                        ) {
                            Text("Start Scanning")
                        }
                    } else {
                        OutlinedButton(
                            onClick = { LobbyService.stopDiscovery() },
                            modifier = Modifier.weight(1f),
                        ) {
                            Text("Stop Scanning")
                        }
                    }
                }
                Spacer(Modifier.height(8.dp))
                Row(modifier = Modifier.fillMaxWidth()) {
                    OutlinedButton(
                        onClick = { showJoinByIpDialog = true },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Join by IP")
                    }
                }
            } else {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Button(
                        onClick = {
                            if (permissionGranted || Build.VERSION.SDK_INT < 33) {
                                showHostDialog = true
                            } else {
                                permissionLauncher.launch(
                                    Manifest.permission.NEARBY_WIFI_DEVICES,
                                )
                            }
                        },
                        modifier =
                            Modifier
                                .weight(1f)
                                .then(if (lanActionHasInitialFocus) Modifier.focusRequester(actionFocus) else Modifier),
                    ) {
                        Text("Host LAN Game")
                    }
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
                            modifier = Modifier.weight(1f),
                        ) {
                            Text("Start Scanning")
                        }
                    } else {
                        OutlinedButton(
                            onClick = { LobbyService.stopDiscovery() },
                            modifier = Modifier.weight(1f),
                        ) {
                            Text("Stop Scanning")
                        }
                    }
                }
            }

            Spacer(Modifier.height(12.dp))
        }

        // -- Hosted lobby info --
        if (isHosting) {
            item {
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
                                Column(modifier = Modifier.weight(1f)) {
                                    Text(displayName, style = MaterialTheme.typography.bodyMedium)
                                    MissionStatusIndicator(p.missionStatus)
                                }
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
                    VisualReplacementPolicy.noticeText(hostedVisualSummary)?.let {
                        Spacer(Modifier.height(4.dp))
                        Text(
                            it,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                }
                Spacer(Modifier.height(8.dp))
                if (hostedMode == "coop") {
                    CoopRestoreSelectionSummary(hostedGame, hostedLevelNum)
                }
                Button(
                    onClick = {
                        onLaunchRequested(hostedGame)
                        LobbyService.startGame(
                            hostedDifficulty,
                            hostedLevelNum,
                            coopQol = hostedCoopQol,
                            duplicateEnergyShields = hostedDuplicateEnergyShields,
                            fullDeathSpew = hostedFullDeathSpew,
                            playerSpewNoExpire = hostedPlayerSpewNoExpire,
                            clientsCanRequestRewind = hostedClientsCanRequestRewind,
                            restrictNonCoopFovToBase = hostedRestrictNonCoopFovToBase,
                        )
                    },
                    modifier = Modifier.fillMaxWidth(),
                    enabled =
                        hostedPlayers.size >= 2 &&
                            hostedPlayers.all {
                                it.ready && it.missionStatus?.status == MissionCompatibilityStatus.MATCH
                            },
                ) {
                    Text("Start Game")
                }
                Spacer(Modifier.height(8.dp))
                ChatArea(
                    messages = chatMessages,
                    onSend = { LobbyService.sendChat(callsign, it) },
                    modifier = Modifier.fillMaxWidth().height(220.dp),
                    textEntryFallbackFocusRequester = actionFocus,
                )
                Spacer(Modifier.height(12.dp))
                HorizontalDivider()
                Spacer(Modifier.height(8.dp))
            }
        }

        // Diagnostics status line
        if (diagnostics.isNotEmpty()) {
            item {
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
    }

    if (showHostDialog) {
        CreateGameDialog(
            title = "Host LAN Game",
            confirmLabel = "Host",
            missionDownloadSupported = true,
            onCreate = {
                game,
                mission,
                missionRequirement,
                mode,
                maxPlayers,
                difficulty,
                levelNum,
                coopQol,
                duplicateEnergyShields,
                fullDeathSpew,
                playerSpewNoExpire,
                clientsCanRequestRewind,
                restrictNonCoopFovToBase,
                ->
                showHostDialog = false
                hostedGame = game
                hostedMode = mode
                hostedMission = mission
                hostedDifficulty = difficulty
                hostedLevelNum = levelNum
                hostedCoopQol = coopQol
                hostedDuplicateEnergyShields = duplicateEnergyShields
                hostedFullDeathSpew = fullDeathSpew
                hostedPlayerSpewNoExpire = playerSpewNoExpire
                hostedClientsCanRequestRewind = clientsCanRequestRewind
                hostedRestrictNonCoopFovToBase = restrictNonCoopFovToBase
                coroutineScope.launch {
                    val visualSummary = VisualReplacementPolicy.summaryForPvp(context, game, mode)
                    hostedVisualSummary = visualSummary
                    LobbyService.hostLobby(
                        callsign,
                        game,
                        mission ?: "",
                        mode,
                        maxPlayers,
                        missionRequirement = missionRequirement,
                        restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                        stockVisualsEnforced = visualSummary.hasOmittedVisuals,
                        omittedVisualModCount = visualSummary.omittedModCount,
                        omittedVisualTextureCount = visualSummary.omittedTextureCount,
                        omittedVisualModNames = visualSummary.omittedModNames,
                    )
                }
            },
            onDismiss = { showHostDialog = false },
        )
    }

    if (showJoinByIpDialog) {
        JoinByIpDialog(
            recentIps = recentIps.value,
            onJoin = { hostAddr, game ->
                showJoinByIpDialog = false
                LanIpsPrefs.add(context, hostAddr)
                recentIps.value = LanIpsPrefs.load(context)
                // Try lobby join first (1s probe), fall back to direct game engine join
                coroutineScope.launch(Dispatchers.IO) {
                    val foundLobby = LobbyService.tryJoinLobbyByIp(hostAddr, callsign)
                    if (!foundLobby) {
                        withContext(Dispatchers.Main) {
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
                        }
                    }
                }
            },
            onDismiss = { showJoinByIpDialog = false },
        )
    }
}

@Composable
private fun LanLobbyCard(
    lobby: LobbyService.DiscoveredLobby,
    myCallsign: String,
    onJoinInGame: ((GameLaunchInfo) -> Unit)? = null,
) {
    val difficulties = listOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")
    val isInGame = lobby.announce.status == "in_game"
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
            VisualReplacementPolicy
                .noticeText(
                    lobby.announce.stockVisualsEnforced,
                    lobby.announce.omittedVisualModCount,
                    lobby.announce.omittedVisualModNames,
                )?.let {
                    Text(
                        it,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.primary,
                    )
                }
            if (isInGame) {
                val diffName = difficulties.getOrElse(lobby.announce.difficulty) { "?" }
                Text(
                    "In game: Level ${lobby.announce.levelNum}, $diffName",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.primary,
                )
            }
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
            if (isInGame && onJoinInGame != null) {
                Button(
                    onClick = {
                        onJoinInGame(
                            GameLaunchInfo(
                                game = lobby.announce.game,
                                mission = lobby.announce.mission,
                                mode = lobby.announce.mode,
                                difficulty = lobby.announce.difficulty,
                                levelNum = lobby.announce.levelNum,
                                maxPlayers = lobby.announce.maxPlayers,
                                yourSlot = 1,
                                isHost = false,
                                peers = emptyList(),
                                lanHostAddr = lobby.announce.hostAddress,
                                lanHostPort = lobby.announce.hostPort,
                                isLan = true,
                                hostCallsign = lobby.announce.callsign,
                                hostClientId = lobby.announce.hostClientId,
                                restrictNonCoopFovToBase = lobby.announce.restrictNonCoopFovToBase,
                            ),
                        )
                    },
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text("Join In-Game")
                }
            } else {
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
}

@Composable
private fun JoinByIpDialog(
    recentIps: List<String>,
    onJoin: (hostAddr: String, game: String) -> Unit,
    onDismiss: () -> Unit,
) {
    var hostIp by remember { mutableStateOf(getDefaultIpPrefix()) }
    var selectedGame by remember { mutableStateOf("d2") }
    var textEntryActive by remember { mutableStateOf(false) }
    val dismissFocus = remember { FocusRequester() }
    val hostIpFocus = remember { FocusRequester() }
    val gameFocus = remember { FocusRequester() }
    val isValidIp = isValidIpAddress(hostIp)
    val dismissOrEndTextEntry =
        rememberControllerTextEntryDismiss(textEntryActive, dismissFocus, { textEntryActive = it }, onDismiss)

    RequestControllerInitialFocus(dismissFocus)

    AlertDialog(
        onDismissRequest = dismissOrEndTextEntry,
        title = { Text("Join by IP") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    "Enter the host's IP address. Will try lobby join first, then direct connect.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                OutlinedTextField(
                    value = hostIp,
                    onValueChange = { hostIp = it.trim() },
                    label = { Text("Host IP Address") },
                    placeholder = { Text("192.168.1.100") },
                    singleLine = true,
                    isError = hostIp.isNotBlank() && !isValidIp,
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .focusRequester(hostIpFocus)
                            .controllerTextFieldDpadExit(up = dismissFocus, down = gameFocus)
                            .controllerTextEntryFocus { textEntryActive = it },
                )
                RecentSuggestions(recentIps) { hostIp = it }
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Game:", style = MaterialTheme.typography.bodyMedium)
                    listOf("d1" to "Descent 1", "d2" to "Descent 2").forEachIndexed { index, (key, label) ->
                        FilterChip(
                            selected = selectedGame == key,
                            onClick = { selectedGame = key },
                            label = { Text(label) },
                            modifier =
                                Modifier
                                    .then(if (index == 0) Modifier.focusRequester(gameFocus) else Modifier)
                                    .tvFocusBorder(),
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
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(dismissFocus)) { Text("Cancel") }
        },
    )
}

// RecentIpSuggestions replaced by shared RecentSuggestions in RecentAddressPrefs.kt

private val ipPattern = Regex("""^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$""")

private fun isValidIpAddress(ip: String): Boolean =
    ip.matches(ipPattern) && ip.split(".").all { it.toIntOrNull() in 0..255 }

internal fun getLocalIpLabel(): String? {
    val localIps = getLocalIpAddresses()
    if (localIps.isEmpty()) return null
    return if (localIps.size == 1) {
        "Your IP: ${localIps[0]}"
    } else {
        "Your IPs: ${localIps.joinToString(", ")}"
    }
}

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
