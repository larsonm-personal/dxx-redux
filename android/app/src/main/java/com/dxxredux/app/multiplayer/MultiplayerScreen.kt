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
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
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
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusProperties
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalConfiguration
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
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

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
    val connectFocus = remember { FocusRequester() }
    val cancelFocus = remember { FocusRequester() }
    val refreshFocus = remember { FocusRequester() }
    val lanFocus = remember { FocusRequester() }
    val serverUrlFocus = remember { FocusRequester() }
    val callsignFocus = remember { FocusRequester() }
    val newCallsignFocus = remember { FocusRequester() }
    val recentUrls = remember { mutableStateOf(RecentAddressPrefs.SERVER_URLS.load(context)) }
    var serverUrl by remember { mutableStateOf(recentUrls.value.firstOrNull() ?: state.serverUrl) }
    val pilotCallsigns =
        remember {
            val fsm = com.dxxredux.app.FileSetManager(context.filesDir)
            MultiplayerCallsigns.scan(context.filesDir, fsm.getSetDir(fsm.getActive()))
        }
    var pendingNewCallsign by remember { mutableStateOf<String?>(null) }
    val callsignOptions =
        remember(pilotCallsigns, pendingNewCallsign) {
            MultiplayerCallsigns.mergePendingCallsign(pilotCallsigns, pendingNewCallsign)
        }
    var callsign by remember {
        mutableStateOf(
            MultiplayerCallsigns.pickInitialCallsign(state.callsign, callsignOptions),
        )
    }
    var showCreateDialog by remember { mutableStateOf(false) }
    var textEntryActive by remember { mutableStateOf(false) }
    var dismissResumeOffer by remember { mutableStateOf(false) }
    var pendingOnlineResume by remember { mutableStateOf<MultiplayerResumeRecord?>(null) }
    var onlineResumeListRequested by remember { mutableStateOf(false) }
    var resumeStatus by remember { mutableStateOf<String?>(null) }
    val resumeRecord = remember { MultiplayerResumePrefs.load(context) }
    val activeGames = state.serverStatus?.activeGameList.orEmpty()
    val focusTarget = multiplayerBrowserInitialFocusTarget(state.status)
    val initialFocus =
        when (focusTarget) {
            MultiplayerBrowserInitialFocusTarget.CONNECT -> connectFocus
            MultiplayerBrowserInitialFocusTarget.CANCEL_CONNECT -> cancelFocus
            MultiplayerBrowserInitialFocusTarget.REFRESH_LOBBIES -> refreshFocus
        }

    RequestControllerInitialFocus(initialFocus, focusTarget, revealFocusOnRequest = false)
    ControllerTextEntryBackHandler(textEntryActive, initialFocus) { textEntryActive = it }

    // Auto-refresh lobby list every 5 seconds while connected
    if (state.status == ConnectionStatus.CONNECTED) {
        LaunchedEffect(Unit) {
            while (true) {
                delay(5000)
                MatchmakingService.requestLobbyList()
            }
        }
    }
    val isLandscape =
        LocalConfiguration.current.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE

    LaunchedEffect(state.callsign, callsignOptions) {
        if (callsign.isBlank() || callsignOptions.none { it.equals(callsign, ignoreCase = true) }) {
            callsign = MultiplayerCallsigns.pickInitialCallsign(state.callsign, callsignOptions)
        }
    }

    fun persistSelectedCallsign(): String? {
        val selected = MultiplayerCallsigns.sanitizeNewCallsign(callsign)
        if (selected.isBlank()) return null
        callsign = selected
        CallsignPrefs.save(context, selected)
        MatchmakingStateHolder.update { it.copy(callsign = selected) }
        return selected
    }

    fun rememberNewCallsign(newCallsign: String) {
        pendingNewCallsign = newCallsign
        callsign = newCallsign
        CallsignPrefs.save(context, newCallsign)
        MatchmakingStateHolder.update { it.copy(callsign = newCallsign) }
    }

    fun rememberExternalCallsign(externalCallsign: String) {
        if (pilotCallsigns.none { it.equals(externalCallsign, ignoreCase = true) }) {
            pendingNewCallsign = externalCallsign
        }
        callsign = externalCallsign
        CallsignPrefs.save(context, externalCallsign)
        MatchmakingStateHolder.update { it.copy(callsign = externalCallsign) }
    }

    fun connectWithSelectedCallsign() {
        val selected = persistSelectedCallsign() ?: return
        RecentAddressPrefs.SERVER_URLS.add(context, serverUrl)
        recentUrls.value = RecentAddressPrefs.SERVER_URLS.load(context)
        MatchmakingService.connect(serverUrl, selected)
    }

    fun openLanWithSelectedCallsign() {
        persistSelectedCallsign() ?: return
        MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.LAN) }
    }

    fun createOnlineResumeLobby(record: MultiplayerResumeRecord) {
        HostGameDefaults.save(context, record.toHostDefaults())
        writeCoopRestoreSlot(context.filesDir, record.game, record.coopRestoreSlot)
        MatchmakingService.createLobby(record.game, record.maxPlayers, record.toGameInfoJson())
    }

    fun beginOnlineResume(record: MultiplayerResumeRecord) {
        val savedServerUrl = record.serverUrl ?: return
        rememberExternalCallsign(record.localCallsign)
        serverUrl = savedServerUrl
        RecentAddressPrefs.SERVER_URLS.add(context, savedServerUrl)
        recentUrls.value = RecentAddressPrefs.SERVER_URLS.load(context)
        pendingOnlineResume = record
        onlineResumeListRequested = false
        resumeStatus = if (record.isHostOnlineCoop()) "Connecting to last server" else "Looking for last online host"
        if (state.status != ConnectionStatus.CONNECTED || state.serverUrl.trim() != savedServerUrl.trim()) {
            MatchmakingService.connect(savedServerUrl, record.localCallsign)
        }
    }

    LaunchedEffect(pendingOnlineResume, state.status, state.lobbies) {
        val record = pendingOnlineResume ?: return@LaunchedEffect
        if (state.status != ConnectionStatus.CONNECTED) return@LaunchedEffect
        if (record.isHostOnlineCoop()) {
            pendingOnlineResume = null
            resumeStatus = null
            createOnlineResumeLobby(record)
            return@LaunchedEffect
        }
        if (record.isClientOnlineCoop()) {
            if (!onlineResumeListRequested) {
                onlineResumeListRequested = true
                MatchmakingService.requestLobbyList()
            }
            val codedMatch = state.lobbies.firstOrNull { record.matchesOnlineLobby(it) && it.hasCode }
            val match = state.lobbies.firstOrNull { record.matchesOnlineLobby(it) && it.joinable && !it.hasCode }
            if (match != null) {
                pendingOnlineResume = null
                resumeStatus = null
                MatchmakingService.joinLobby(match.lobbyId)
            } else if (codedMatch != null) {
                resumeStatus = "Last lobby requires a code"
            } else {
                resumeStatus = "Looking for last online host"
            }
        }
    }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .showControllerFocusOnDpad(initialFocus, focusTarget)
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

        val offerRecord =
            resumeRecord?.takeIf {
                it.isHostLanCoop() || it.isClientLanCoop() || it.isHostOnlineCoop() || it.isClientOnlineCoop()
            }
        if (!dismissResumeOffer && offerRecord != null) {
            MultiplayerResumeOfferCard(
                record = offerRecord,
                primaryLabel =
                    when {
                        offerRecord.isHostLanCoop() -> "Open LAN Resume"
                        offerRecord.isClientLanCoop() -> "Open LAN Search"
                        offerRecord.isHostOnlineCoop() -> "Host Last Online"
                        else -> "Find Online Host"
                    },
                onPrimary = {
                    if (offerRecord.transport == "lan") {
                        if (offerRecord.isHostLanCoop()) {
                            HostGameDefaults.save(context, offerRecord.toHostDefaults())
                            writeCoopRestoreSlot(context.filesDir, offerRecord.game, offerRecord.coopRestoreSlot)
                        }
                        rememberExternalCallsign(offerRecord.localCallsign)
                        MatchmakingStateHolder.update {
                            it.copy(callsign = offerRecord.localCallsign, nav = MultiplayerNav.LAN)
                        }
                    } else {
                        beginOnlineResume(offerRecord)
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
            if (isLandscape) {
                OutlinedTextField(
                    value = serverUrl,
                    onValueChange = { serverUrl = it },
                    label = { Text("Matchmaking Server URL") },
                    singleLine = true,
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .focusRequester(serverUrlFocus)
                            .focusProperties { down = callsignFocus }
                            .controllerTextFieldDpadExit(down = callsignFocus)
                            .controllerTextEntryFocus { textEntryActive = it },
                )
                RecentSuggestions(recentUrls.value) { serverUrl = it }
                Spacer(Modifier.height(4.dp))
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    CallsignPickerField(
                        selectedCallsign = callsign,
                        callsigns = callsignOptions,
                        onSelect = { callsign = it },
                        modifier =
                            Modifier
                                .weight(1f)
                                .focusRequester(callsignFocus)
                                .focusProperties {
                                    up = serverUrlFocus
                                    right = newCallsignFocus
                                    down = connectFocus
                                },
                    )
                    NewCallsignButton(
                        existingCallsigns = pilotCallsigns,
                        onCreate = ::rememberNewCallsign,
                        enabled = !isConnecting,
                        modifier =
                            Modifier
                                .focusRequester(newCallsignFocus)
                                .focusProperties {
                                    up = serverUrlFocus
                                    left = callsignFocus
                                    down = connectFocus
                                },
                    )
                }
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(
                        onClick = ::connectWithSelectedCallsign,
                        enabled = !isConnecting && callsign.isNotBlank(),
                        modifier =
                            Modifier
                                .weight(1f)
                                .focusRequester(connectFocus)
                                .focusProperties {
                                    up = callsignFocus
                                    right = lanFocus
                                },
                    ) {
                        Text("Connect")
                    }
                    if (isConnecting) {
                        OutlinedButton(
                            onClick = { MatchmakingService.disconnect() },
                            modifier =
                                if (focusTarget == MultiplayerBrowserInitialFocusTarget.CANCEL_CONNECT) {
                                    Modifier
                                        .focusRequester(cancelFocus)
                                        .focusProperties { up = callsignFocus }
                                } else {
                                    Modifier
                                },
                        ) {
                            Text("Cancel")
                        }
                    }
                    Button(
                        onClick = ::openLanWithSelectedCallsign,
                        enabled = !isConnecting && callsign.isNotBlank(),
                        modifier =
                            Modifier
                                .focusRequester(lanFocus)
                                .focusProperties {
                                    up = callsignFocus
                                    left = connectFocus
                                },
                    ) {
                        Text("LAN")
                    }
                }
            } else {
                // Portrait: URL + Connect on one row, Callsign + LAN on the next
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    OutlinedTextField(
                        value = serverUrl,
                        onValueChange = { serverUrl = it },
                        label = { Text("Server URL") },
                        singleLine = true,
                        modifier =
                            Modifier
                                .weight(1f)
                                .focusRequester(serverUrlFocus)
                                .focusProperties { down = callsignFocus }
                                .controllerTextFieldDpadExit(down = callsignFocus)
                                .controllerTextEntryFocus { textEntryActive = it },
                    )
                    Button(
                        onClick = ::connectWithSelectedCallsign,
                        enabled = !isConnecting && callsign.isNotBlank(),
                        modifier =
                            Modifier
                                .focusRequester(connectFocus)
                                .focusProperties {
                                    up = callsignFocus
                                    right = lanFocus
                                },
                    ) {
                        Text("Connect")
                    }
                    if (isConnecting) {
                        OutlinedButton(
                            onClick = { MatchmakingService.disconnect() },
                            modifier =
                                if (focusTarget == MultiplayerBrowserInitialFocusTarget.CANCEL_CONNECT) {
                                    Modifier
                                        .focusRequester(cancelFocus)
                                        .focusProperties { up = callsignFocus }
                                } else {
                                    Modifier
                                },
                        ) {
                            Text("Cancel")
                        }
                    }
                }
                RecentSuggestions(recentUrls.value) { serverUrl = it }
                Spacer(Modifier.height(4.dp))
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    CallsignPickerField(
                        selectedCallsign = callsign,
                        callsigns = callsignOptions,
                        onSelect = { callsign = it },
                        modifier =
                            Modifier
                                .weight(1f)
                                .focusRequester(callsignFocus)
                                .focusProperties {
                                    up = serverUrlFocus
                                    right = newCallsignFocus
                                    down = connectFocus
                                },
                    )
                    NewCallsignButton(
                        existingCallsigns = pilotCallsigns,
                        onCreate = ::rememberNewCallsign,
                        enabled = !isConnecting,
                        modifier =
                            Modifier
                                .focusRequester(newCallsignFocus)
                                .focusProperties {
                                    up = serverUrlFocus
                                    left = callsignFocus
                                    right = lanFocus
                                    down = connectFocus
                                },
                    )
                    Button(
                        onClick = ::openLanWithSelectedCallsign,
                        enabled = !isConnecting && callsign.isNotBlank(),
                        modifier =
                            Modifier
                                .focusRequester(lanFocus)
                                .focusProperties {
                                    up = callsignFocus
                                    left = newCallsignFocus
                                },
                    ) {
                        Text("LAN")
                    }
                }
            }
        } else {
            // Connected state - three rows to avoid portrait crowding
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Button(
                    onClick = { MatchmakingService.requestLobbyList() },
                    modifier =
                        if (focusTarget == MultiplayerBrowserInitialFocusTarget.REFRESH_LOBBIES) {
                            Modifier.focusRequester(refreshFocus)
                        } else {
                            Modifier
                        },
                ) {
                    Text("Refresh Lobbies")
                }
                Button(onClick = { showCreateDialog = true }) {
                    Text("Create Lobby")
                }
            }
            Spacer(Modifier.height(4.dp))
            OutlinedButton(onClick = { MatchmakingService.disconnect() }) {
                Text("Disconnect")
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

        // -- Recent coop saves (scan both d1 and d2) --
        if (state.status == ConnectionStatus.CONNECTED) {
            RecentCoopGames(onCreateWithSave = { entry ->
                // Pre-fill CreateGameDialog via defaults and open it
                HostGameDefaults.save(
                    context,
                    HostGameDefaults.Defaults(
                        game = entry.game,
                        mission = entry.mission,
                        mode = "coop",
                        levelNum = entry.level,
                    ),
                )
                if (entry.slot >= 0) {
                    writeCoopRestoreSlot(context.filesDir, entry.game, entry.slot)
                }
                showCreateDialog = true
            })
        }

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
                    items(activeGames, key = { it.lobbyId.ifEmpty { "${it.hostCallsign}-${it.mission}" } }) { game ->
                        ActiveGameCard(game, onJoin = {
                            if (game.lobbyId.isNotEmpty()) {
                                MatchmakingService.joinLobby(game.lobbyId)
                            }
                        })
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
        CreateGameDialog(
            title = "Create Lobby",
            confirmLabel = "Create",
            onCreate = {
                game,
                mission,
                mode,
                maxPlayers,
                difficulty,
                levelNum,
                coopQol,
                fullDeathSpew,
                playerSpewNoExpire,
                clientsCanRequestRewind,
                restrictNonCoopFovToBase,
                ->
                showCreateDialog = false
                val gameInfo =
                    JsonObject(
                        mapOf(
                            "game" to JsonPrimitive(game),
                            "mission" to JsonPrimitive(mission ?: ""),
                            "mode" to JsonPrimitive(mode),
                            "difficulty" to JsonPrimitive(difficulty),
                            "level_num" to JsonPrimitive(levelNum),
                            "coop_qol" to JsonPrimitive(coopQol),
                            "full_death_spew" to JsonPrimitive(fullDeathSpew),
                            "player_spew_no_expire" to JsonPrimitive(playerSpewNoExpire),
                            "clients_can_request_rewind" to JsonPrimitive(clientsCanRequestRewind),
                            "restrict_noncoop_fov_to_base" to JsonPrimitive(restrictNonCoopFovToBase),
                        ),
                    )
                MatchmakingService.createLobby(game, maxPlayers, gameInfo)
            },
            onDismiss = { showCreateDialog = false },
        )
    }
}

/**
 * Show up to 5 recent coop saves across both d1 and d2.
 * Tapping an entry opens CreateGameDialog pre-filled with the save's settings.
 */
@Composable
private fun RecentCoopGames(onCreateWithSave: (CoopSaveEntry) -> Unit) {
    val context = LocalContext.current
    val filesDir = context.filesDir

    val recentSaves =
        remember {
            val allSaves = mutableListOf<CoopSaveEntry>()
            for (g in listOf("d1", "d2")) {
                val subdir = if (g == "d1") "d1x-redux" else "d2x-redux"
                val file = File(filesDir, "$subdir/coop_autosave_history.json")
                if (!file.exists()) continue
                try {
                    val myClientId = ClientIdentity.getInstallationId(context)
                    val arr = JSONArray(file.readText())
                    for (i in 0 until arr.length()) {
                        val obj = arr.getJSONObject(i)
                        val ids = obj.optJSONArray("client_ids")
                        var matched = false
                        if (ids != null) {
                            for (j in 0 until ids.length()) {
                                if (ids.optString(j) == myClientId) {
                                    matched = true
                                    break
                                }
                            }
                        }
                        if (!matched) continue
                        val callsigns = mutableListOf<String>()
                        val names = obj.optJSONArray("callsigns")
                        if (names != null) {
                            for (j in 0 until names.length()) callsigns.add(names.optString(j, "?"))
                        }
                        allSaves.add(
                            CoopSaveEntry(
                                slot = obj.optInt("slot", -1),
                                level = obj.optInt("level", 0),
                                timestamp = obj.optLong("timestamp", 0),
                                numPlayers = obj.optInt("num_players", 0),
                                callsigns = callsigns,
                                levelTimeSeconds = obj.optInt("level_time_seconds", 0),
                                type = obj.optString("type", "full_save"),
                                totalScore = obj.optInt("total_score", 0),
                                mission = obj.optString("mission", ""),
                                game = g,
                            ),
                        )
                    }
                } catch (_: Exception) {
                    // skip corrupt files
                }
            }
            allSaves.sortedByDescending { it.timestamp }.take(5)
        }

    if (recentSaves.isEmpty()) return

    Text("Recent Coop", style = MaterialTheme.typography.titleSmall)
    recentSaves.forEach { save ->
        val scoreStr = if (save.totalScore > 0) " ${save.totalScore}pts" else ""
        val label =
            "${save.game.uppercase()} ${save.mission} L${save.level}" +
                " - ${save.numPlayers}p - ${save.callsigns.joinToString()}" +
                "$scoreStr - ${formatTimeAgo(save.timestamp)}"
        OutlinedButton(
            onClick = { onCreateWithSave(save) },
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(label, fontSize = 11.sp, maxLines = 2)
        }
    }
    Spacer(Modifier.height(8.dp))
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
    var textEntryActive by remember { mutableStateOf(false) }
    val dismissFocus = remember { FocusRequester() }
    val dismissOrEndTextEntry =
        rememberControllerTextEntryDismiss(textEntryActive, dismissFocus, { textEntryActive = it }, onDismiss)
    RequestControllerInitialFocus(dismissFocus)
    AlertDialog(
        onDismissRequest = dismissOrEndTextEntry,
        title = { Text("Enter Lobby Code") },
        text = {
            OutlinedTextField(
                value = code,
                onValueChange = { code = it },
                label = { Text("Code") },
                singleLine = true,
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .controllerTextFieldDpadExit(up = dismissFocus)
                        .controllerTextEntryFocus { textEntryActive = it },
            )
        },
        confirmButton = {
            TextButton(onClick = { onJoin(code) }, enabled = code.isNotBlank()) {
                Text("Join")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(dismissFocus)) { Text("Cancel") }
        },
    )
}

/** A single entry from coop_autosave_history.json, filtered to match the lobby's mission. */
internal data class CoopSaveEntry(
    val slot: Int,
    val level: Int,
    val timestamp: Long,
    val numPlayers: Int,
    val callsigns: List<String>,
    val clientIds: List<String> = emptyList(),
    val levelTimeSeconds: Int = 0,
    val type: String = "full_save", // "full_save" or "checkpoint"
    val totalScore: Int = 0,
    val mission: String = "",
    val game: String = "", // "d1" or "d2", used for recent games display
)

/** Format a unix timestamp as a relative time string like "5 min ago". */
internal fun formatTimeAgo(timestampSec: Long): String {
    val now = System.currentTimeMillis() / 1000
    val delta = now - timestampSec
    return when {
        delta < 60 -> "just now"
        delta < 3600 -> "${delta / 60} min ago"
        delta < 86400 -> "${delta / 3600}h ago"
        else -> "${delta / 86400}d ago"
    }
}

/**
 * Read coop_autosave_history.json and return entries matching the given mission
 * where the current player's client_id is among the save's participants.
 * Returns newest first (sorted by timestamp descending).
 */
internal fun readCoopAutosaveHistory(
    filesDir: File,
    game: String,
    mission: String?,
    context: android.content.Context,
): List<CoopSaveEntry> {
    if (mission == null) return emptyList()
    val subdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    val file = File(filesDir, "$subdir/coop_autosave_history.json")
    if (!file.exists()) return emptyList()
    val myClientId = ClientIdentity.getInstallationId(context)
    return try {
        val arr = JSONArray(file.readText())
        val entries = mutableListOf<CoopSaveEntry>()
        for (i in 0 until arr.length()) {
            val obj = arr.getJSONObject(i)
            val fileMission = obj.optString("mission", "")
            if (!fileMission.equals(mission, ignoreCase = true)) continue
            // Check if our client_id is in this save's participants
            val ids = obj.optJSONArray("client_ids")
            var matched = false
            val clientIdList = mutableListOf<String>()
            if (ids != null) {
                for (j in 0 until ids.length()) {
                    val cid = ids.optString(j, "")
                    clientIdList.add(cid)
                    if (cid == myClientId) matched = true
                }
            }
            if (!matched) continue
            val callsigns = mutableListOf<String>()
            val names = obj.optJSONArray("callsigns")
            if (names != null) {
                for (j in 0 until names.length()) callsigns.add(names.optString(j, "?"))
            }
            entries.add(
                CoopSaveEntry(
                    slot = obj.optInt("slot", -1),
                    level = obj.optInt("level", 0),
                    timestamp = obj.optLong("timestamp", 0),
                    numPlayers = obj.optInt("num_players", 0),
                    callsigns = callsigns,
                    clientIds = clientIdList,
                    levelTimeSeconds = obj.optInt("level_time_seconds", 0),
                    type = obj.optString("type", "full_save"),
                    totalScore = obj.optInt("total_score", 0),
                    mission = fileMission,
                    game = game,
                ),
            )
        }
        entries.sortedByDescending { it.timestamp }
    } catch (_: Exception) {
        emptyList()
    }
}

internal data class CoopRestoreSelection(
    val slot: Int?,
)

private const val COOP_RESTORE_START_FRESH_SENTINEL = -1

/** Write the selected restore choice for the C engine to pick up on launch. */
internal fun writeCoopRestoreSlot(
    filesDir: File,
    game: String,
    slot: Int?,
) {
    val subdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    val file = File(filesDir, "$subdir/coop_restore_slot.txt")
    file.parentFile?.mkdirs()
    if (slot != null) {
        file.writeText(slot.toString())
    } else {
        file.writeText(COOP_RESTORE_START_FRESH_SENTINEL.toString())
    }
}

/** Read the currently written restore slot, or null if none. */
internal fun readCoopRestoreSlot(
    filesDir: File,
    game: String,
): Int? = readCoopRestoreSelection(filesDir, game)?.slot

/** Read the currently written restore choice, or null when no choice was made. */
internal fun readCoopRestoreSelection(
    filesDir: File,
    game: String,
): CoopRestoreSelection? {
    val subdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    val file = File(filesDir, "$subdir/coop_restore_slot.txt")
    return try {
        val slot = if (file.exists()) file.readText().trim().toIntOrNull() else return null
        when (slot) {
            in 0..9 -> CoopRestoreSelection(slot)
            COOP_RESTORE_START_FRESH_SENTINEL -> CoopRestoreSelection(null)
            else -> null
        }
    } catch (_: Exception) {
        null
    }
}

internal fun initialCoopRestoreEnabled(selection: CoopRestoreSelection?): Boolean = selection?.slot != null

internal fun shouldAutoEnableCoopRestore(selection: CoopRestoreSelection?): Boolean = selection == null

internal fun initialCoopSaveSelection(
    coopSaves: List<CoopSaveEntry>,
    selection: CoopRestoreSelection?,
): CoopSaveEntry? =
    if (selection != null) {
        selection.slot?.let { slot -> coopSaves.firstOrNull { it.slot == slot } }
    } else {
        coopSaves.firstOrNull { it.type == "full_save" }
    }

internal fun restoreSaveForHostedLevel(
    selectedSave: CoopSaveEntry?,
    levelNum: Int,
): CoopSaveEntry? = selectedSave?.takeIf { it.slot >= 0 && it.level == levelNum }

/**
 * Read coop_progress.json and return a CoopSaveEntry of type "checkpoint".
 * Returns null if no progress file exists, mission doesn't match, or level <= 0.
 */
internal fun readCoopProgressAsEntry(
    filesDir: File,
    game: String,
    mission: String?,
    context: android.content.Context,
): CoopSaveEntry? {
    if (mission == null) return null
    val subdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    val file = File(filesDir, "$subdir/coop_progress.json")
    if (!file.exists()) return null
    val myClientId = ClientIdentity.getInstallationId(context)
    return try {
        val json = JSONObject(file.readText())
        val fileMission = json.optString("mission", "")
        if (!fileMission.equals(mission, ignoreCase = true)) return null
        val level = json.optInt("last_completed_level", 0)
        if (level <= 0) return null
        // Check if our client_id is in the progress participants
        val ids = json.optJSONArray("client_ids")
        var matched = ids == null // if no client_ids field, accept (old format)
        val clientIdList = mutableListOf<String>()
        if (ids != null) {
            for (j in 0 until ids.length()) {
                val cid = ids.optString(j, "")
                clientIdList.add(cid)
                if (cid == myClientId) matched = true
            }
        }
        if (!matched) return null
        val callsigns = mutableListOf<String>()
        val names = json.optJSONArray("players")
        if (names != null) {
            for (j in 0 until names.length()) callsigns.add(names.optString(j, "?"))
        }
        CoopSaveEntry(
            slot = -1,
            level = level,
            timestamp = json.optLong("timestamp", 0),
            numPlayers = json.optInt("num_players", 0),
            callsigns = callsigns,
            clientIds = clientIdList,
            type = "checkpoint",
            mission = fileMission,
            game = game,
        )
    } catch (_: Exception) {
        null
    }
}

/**
 * Read coop_progress.json from the game's data directory.
 * Returns the last completed level if the file exists and matches the given mission,
 * or null otherwise. The game writes this file at the end of each coop level.
 */
internal fun readCoopProgress(
    filesDir: File,
    game: String,
    mission: String?,
): Int? {
    if (mission == null) return null
    val subdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    val file = File(filesDir, "$subdir/coop_progress.json")
    if (!file.exists()) return null
    return try {
        val json = JSONObject(file.readText())
        val fileMission = json.optString("mission", "")
        if (fileMission.equals(mission, ignoreCase = true)) {
            json.optInt("last_completed_level", 0).takeIf { it > 0 }
        } else {
            null
        }
    } catch (_: Exception) {
        null
    }
}

@Composable
private fun LanContent(
    state: MatchmakingState,
    onBack: () -> Unit,
    onLaunchGame: (GameLaunchInfo) -> Unit,
) {
    val isLandscape =
        LocalConfiguration.current.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
    val localIpLabel = remember { getLocalIpLabel() }
    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .padding(16.dp),
    ) {
        if (isLandscape) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "LAN Games",
                    style = MaterialTheme.typography.headlineMedium,
                    modifier = Modifier.weight(1f),
                )
                OutlinedButton(
                    onClick = {
                        MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
                    },
                ) { Text("Back") }
            }
            localIpLabel?.let {
                Spacer(Modifier.height(4.dp))
                Text(
                    it,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        } else {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("LAN Games", style = MaterialTheme.typography.headlineMedium)
                Spacer(Modifier.weight(1f))
                OutlinedButton(
                    onClick = {
                        MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
                    },
                ) { Text("Back") }
            }
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
    val friendsFocus = remember { FocusRequester() }
    RequestControllerInitialFocus(friendsFocus, revealFocusOnRequest = false)
    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .showControllerFocusOnDpad(friendsFocus)
                .safeDrawingPadding()
                .padding(16.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Friends", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.weight(1f))
            OutlinedButton(
                onClick = {
                    MatchmakingStateHolder.update { it.copy(nav = MultiplayerNav.BROWSER) }
                },
            ) { Text("Back to Lobbies") }
            Spacer(Modifier.width(8.dp))
            OutlinedButton(onClick = onBack) { Text("Back") }
        }
        Spacer(Modifier.height(8.dp))

        FriendsTab(
            friends = state.friends,
            pendingRequests = state.pendingFriendRequests,
            initialFocusRequester = friendsFocus,
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
