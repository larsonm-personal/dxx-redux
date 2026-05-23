# Fix proxy socket timeout crash + C-side net logs + overlay auto-show

## Problem 1: LocalhostProxy crash (SocketTimeoutException)
The shared candidate socket has soTimeout set by ConnectivityChecker (200ms)
and StunClient. When handed to LocalhostProxy.sharedReceiveLoop(), the
receive() call throws SocketTimeoutException which isn't caught.
This kills the receive coroutine, preventing ALL incoming packet delivery.
This is the root cause of the stalled game connection.

### Fix
- Reset socket.soTimeout = 0 before handing to proxy (in GameStarting handler)
- Also catch SocketTimeoutException in sharedReceiveLoop as a safety net

## Problem 2: No C-side MPDIAG in network logs
net_udp_auto_join() and net_udp_auto_host() use con_printf and
net_log_comment instead of MPDIAG. net_log_comment requires
GameArg.LogNetTraffic and writes to a file, not the JNI bridge.
Same issue in net_udp_sync_poll and related functions.

### Fix
- Convert con_printf/net_log_comment calls in auto_join, auto_host,
  sync_poll, send_sync, start_game to MPDIAG
- Apply to both d1 and d2

## Problem 3: Network overlay not showing
The overlay auto-show logic in the polling loop's catch block doesn't
show the overlay. If nativeIsInGame() throws during startup, the
overlay never gets shown.

### Fix
- Add netEventsOverlay show/hide logic to the catch block too
- Also show overlay in the else branch if gameLaunchInfo is set
