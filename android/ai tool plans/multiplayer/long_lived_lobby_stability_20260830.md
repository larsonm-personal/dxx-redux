# Long-lived lobby stability

## Goal

Keep hosted and joined LAN lobbies functional indefinitely while both apps remain active, including ready changes, chat, mission status, and player membership.

## Plan

- [x] Trace UDP socket, receive loop, heartbeat, and pruning lifecycles
- [x] Add durable diagnostics for unexpected receive-loop or socket failure
- [x] Fix the identified long-lived lobby failure mode
- [x] Add focused lifecycle and liveness regression coverage
- [x] Run scoped formatting, focused tests, and an Android debug build

## Invariants

- A transient receive or send error must not permanently stop lobby traffic
- Heartbeats must keep both host membership and joiner host-liveness current
- Timeout cleanup must remain bounded for truly disconnected peers
- Preserve unrelated mission transfer and lobby changes in the dirty worktree

## Findings

- The launcher lobby initially lacked a screen-off keepalive path; the follow-up screen-off liveness work replaced the temporary screen-awake mitigation with a foreground-service and partial-wake-lock lease
- Lobby receive, announce, heartbeat, and prune jobs shared a plain parent job, allowing one unexpected child failure to cancel all future lobby traffic
- The transport had no active-lobby watchdog or automatic socket recovery

## Validation

- Scoped code quality passed
- Lobby diagnostics, mission refresh, mission display, and protocol unit tests passed
- `:app:assembleDebug` passed
- Two-emulator LAN integration passed with a 70-second idle window, post-idle unready, ready, and chat traffic, plus a 20-second background/resume cycle
- `git diff --check` passed

Status: complete
