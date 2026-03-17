package com.dxxredux.app.multiplayer

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun FriendsTab(
    friends: List<FriendInfo>,
    pendingRequests: List<FriendRequestReceivedMsg>,
) {
    var showAddDialog by remember { mutableStateOf(false) }

    Column {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Friends", style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.weight(1f))
            Button(onClick = { showAddDialog = true }) { Text("Add Friend") }
        }
        Spacer(Modifier.height(8.dp))

        LazyColumn(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            // Pending incoming requests
            if (pendingRequests.isNotEmpty()) {
                item {
                    Text(
                        "Pending Requests",
                        style = MaterialTheme.typography.titleSmall,
                        color = MaterialTheme.colorScheme.primary,
                    )
                }
                items(pendingRequests, key = { it.fromPlayerId }) { req ->
                    PendingRequestCard(req)
                }
                item { Spacer(Modifier.height(4.dp)) }
            }

            // Pending outgoing (status == "pending" in friend list)
            val pendingOut = friends.filter { it.status == "pending" }
            if (pendingOut.isNotEmpty()) {
                item {
                    Text("Sent Requests", style = MaterialTheme.typography.titleSmall)
                }
                items(pendingOut, key = { it.playerId }) { friend ->
                    FriendCard(friend)
                }
                item { Spacer(Modifier.height(4.dp)) }
            }

            // Accepted friends, online first
            val accepted =
                friends
                    .filter { it.status == "accepted" }
                    .sortedBy { if (it.presence == "offline") 1 else 0 }
            if (accepted.isNotEmpty()) {
                item {
                    HorizontalDivider()
                    Spacer(Modifier.height(4.dp))
                }
                items(accepted, key = { it.playerId }) { friend ->
                    FriendCard(friend)
                }
            }

            if (friends.isEmpty() && pendingRequests.isEmpty()) {
                item {
                    Text(
                        "No friends yet. Add a friend by callsign.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            }
        }
    }

    if (showAddDialog) {
        AddFriendDialog(
            onAdd = { callsign ->
                showAddDialog = false
                MatchmakingService.sendFriendRequest(callsign)
            },
            onDismiss = { showAddDialog = false },
        )
    }
}

@Composable
private fun PendingRequestCard(req: FriendRequestReceivedMsg) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors =
            CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer,
            ),
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(12.dp),
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(req.fromCallsign, style = MaterialTheme.typography.titleSmall)
                Text("wants to be friends", style = MaterialTheme.typography.bodySmall)
            }
            Button(onClick = { MatchmakingService.acceptFriend(req.fromPlayerId) }) {
                Text("Accept")
            }
        }
    }
}

@Composable
private fun FriendCard(friend: FriendInfo) {
    var showActions by remember { mutableStateOf(false) }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                // Presence indicator
                Text(
                    text = presenceIndicator(friend.presence),
                    color = presenceColor(friend.presence),
                    style = MaterialTheme.typography.bodyMedium,
                )
                Spacer(Modifier.width(8.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(friend.callsign, style = MaterialTheme.typography.titleSmall)
                    when {
                        friend.status == "pending" -> {
                            Text(
                                "Request sent",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        friend.presence == "in_game" && friend.inGameDetails != null -> {
                            val d = friend.inGameDetails
                            Text(
                                "${d.mission} (${d.playerCount}/${d.maxPlayers})",
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }
                        friend.presence == "online" -> {
                            Text(
                                "Online",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.primary,
                            )
                        }
                        else -> {
                            Text(
                                "Offline",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }

                // Action buttons
                if (friend.status == "accepted") {
                    if (friend.inGameDetails?.joinable == true) {
                        Button(
                            onClick = { MatchmakingService.joinFriendGame(friend.playerId) },
                        ) {
                            Text("Join")
                        }
                        Spacer(Modifier.width(4.dp))
                    }
                    OutlinedButton(onClick = { showActions = !showActions }) {
                        Text("...")
                    }
                }
            }

            if (showActions && friend.status == "accepted") {
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = {
                            showActions = false
                            MatchmakingService.removeFriend(friend.playerId)
                        },
                    ) {
                        Text("Remove")
                    }
                    OutlinedButton(
                        onClick = {
                            showActions = false
                            MatchmakingService.blockPlayer(friend.playerId)
                        },
                    ) {
                        Text("Block")
                    }
                }
            }
        }
    }
}

@Composable
private fun AddFriendDialog(
    onAdd: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var callsign by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Add Friend") },
        text = {
            OutlinedTextField(
                value = callsign,
                onValueChange = { callsign = it },
                label = { Text("Callsign") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
        },
        confirmButton = {
            TextButton(
                onClick = { onAdd(callsign.trim()) },
                enabled = callsign.isNotBlank(),
            ) {
                Text("Send Request")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun presenceColor(presence: String) =
    when (presence) {
        "online" -> MaterialTheme.colorScheme.primary
        "in_game" -> MaterialTheme.colorScheme.tertiary
        else -> MaterialTheme.colorScheme.onSurfaceVariant
    }

private fun presenceIndicator(presence: String) =
    when (presence) {
        "online" -> "[*]"
        "in_game" -> "[>]"
        else -> "[ ]"
    }
