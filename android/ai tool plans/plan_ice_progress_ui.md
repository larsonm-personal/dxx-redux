# Plan: ICE Progress UI + C-Side Network Visibility

## Context

The ICE process (STUN discovery, candidate exchange, connectivity probing) happens
while in the lobby. The C engine's connection phase (pilot select, connecting) also
has detailed logs via MPDIAG macro. Both need UI/log visibility.

## Current Flow

1. Lobby gets 2+ players -> STUN discovery starts (StunClient + UPnP in parallel)
2. sendStunResult() -> server broadcasts PEER_CANDIDATES to all
3. Server sends CONNECTIVITY_CHECK_GO with sorted CandidatePair list
4. ConnectivityChecker.probe() sends DXPC probes, first response wins
5. sendConnectivityOk() -> server records result
6. Server sends GAME_STARTING with per-peer assignments
7. Game engine launches, C code does UDP connect (game_connect/auto_join)

## Changes

### A. ICE Phase Tracking in MatchmakingState

Add `icePhase` enum and per-peer ICE results to MatchmakingState:

```kotlin
enum class IcePhase {
    IDLE,           // Not started
    STUN_DISCOVERY, // Running StunClient.discover() + UPnP
    STUN_COMPLETE,  // Got STUN results, waiting for peer candidates
    PROBING,        // ConnectivityChecker.probe() running
    COMPLETE,       // Got result -- either direct or relay
    FAILED,         // Error during any phase
}

data class IceStatus(
    val phase: IcePhase = IcePhase.IDLE,
    val stunNatType: String? = null,
    val stunCandidateCount: Int = 0,
    val upnpMapped: Boolean = false,
    val probeResult: String? = null,   // "direct_lan (14ms)" or "relay" or null
    val errorMessage: String? = null,
)
```

Add `iceStatus: IceStatus` to MatchmakingState.

### B. IceProgressPanel Composable (LobbyScreen)

Replace the current flat NetworkEventsPanel listing with a step-by-step view:

In the LobbyScreen, add a new `IceProgressPanel(state)` composable that shows:
1. [check/spinner/x] STUN Discovery: "full_cone, 3 candidates" / "running..." / "failed"
2. [check/spinner/x] UPnP: "mapped 203.0.113.5:42424" / "not available"
3. [check/spinner/x] Peer Exchange: "received from 2 peers" / "waiting..."
4. [check/spinner/x] Connectivity Probe: "direct_lan (14ms)" / "probing..." / "relay fallback"
5. Overall: "ICE Ready: direct connection" or "ICE Ready: relay" or "Checking..."

This replaces NetworkEventsPanel. Keep NetworkEventsPanel for the browser screen.

### C. Update MatchmakingService to Track ICE Phases

In launchStunDiscovery():
- Set phase = STUN_DISCOVERY at start
- Set phase = STUN_COMPLETE with natType and candidateCount on success
- Set upnpMapped if UPnP succeeded
- Set phase = FAILED on error

In launchConnectivityCheck():
- Set phase = PROBING at start
- Set phase = COMPLETE with probeResult on success
- Set phase = COMPLETE with probeResult = "relay" on failure/timeout

### D. Forward C MPDIAG to MatchmakingState.statusLog + overlay

The MPDIAG macro already calls android_net_log() -> netLogFromNative() -> NetLog.log().
Add: netLogFromNative() also calls MatchmakingStateHolder.appendLog() so the lines
appear in the StatusLog and are visible in the overlay.

The C logs during pilot-select/connecting will then show in:
1. The in-game NetworkEventsOverlay (reads statusLog via stateProvider)
2. The NetLog export file (already working via existing bridge)

### E. Show C-Side Progress in NetworkEventsOverlay

Update NetworkEventsOverlay to show the statusLog (last N lines) in addition to
the connection info panel. This gives real-time visibility into what the C engine
is doing during the connecting phase.

### F. NetLog Init Verification

NetLog.init() IS called in SetupActivity.onCreate(). The bridge already works.
The user's previous complaint about "zero in-game logs" was the missing NetLog.log()
calls in MatchmakingService.kt, which was fixed in the previous session.

Verify: if the user had NetLog enabled, MPDIAG lines from C WILL appear in the
exported log. No changes needed to the C->Kotlin bridge.

## File Changes

- MatchmakingState.kt: Add IcePhase, IceStatus, iceStatus field
- MatchmakingService.kt: Track ICE phases via iceStatus updates
- IceProgressPanel.kt (new): Step-by-step ICE progress composable
- LobbyScreen.kt: Replace NetworkEventsPanel with IceProgressPanel
- NetworkEventsOverlay.kt: Add statusLog display section
- MainActivity.kt: Forward netLogFromNative to MatchmakingStateHolder.appendLog

## Execution Order

1. Add IcePhase/IceStatus to MatchmakingState
2. Create IceProgressPanel composable
3. Track ICE phases in MatchmakingService
4. Wire IceProgressPanel into LobbyScreen
5. Forward C logs to MatchmakingStateHolder
6. Add statusLog rendering to NetworkEventsOverlay
7. Build + lint
