# Guidebot wall-edge stall research

## Goal

Determine why the D2 guidebot can remain stuck on a wall edge in a cooperative game, with emphasis on differences from base D2 behavior.

## Scope

- Read-only source and history analysis
- Guidebot AI path following, collision recovery, and multiplayer authority
- Recent route-planner and cooperative ownership changes
- No gameplay source edits unless a later task explicitly requests a fix

## Plan

- [x] Read repository instructions and preserve unrelated worktree changes
- [x] Trace guidebot movement, collision, and stuck-recovery paths
- [x] Trace cooperative guidebot ownership and simulation authority
- [x] Compare relevant code and history with the upstream/base behavior
- [x] Rank plausible causes and identify evidence needed to confirm the incident
- [x] Record conclusions and mark this research complete

## Findings

- Physics records `Ai_local_info[objnum].retry_count` after repeated collision iterations.
- The inherited AI recovery block only consumes retry counts when `GM_MULTI` is clear. In single-player, a stuck `AIM_GOTO_PLAYER` robot is moved toward its segment center and given a new path after more than three consecutive retries. In multiplayer, the same retry history is ignored and `consecutive_retries` is halved.
- The April 6, 2026 cooperative Guide-Bot change made one peer the authoritative owner and enabled owner-only escort behavior, but left the inherited `!(Game_mode & GM_MULTI)` recovery exclusion unchanged.
- Non-owner peers return before companion AI and act as pose replicas. This is intentional and makes the owner the only correct place to perform recovery.
- `Warp to me` was explicitly added as a recovery affordance for clipped, stuck, or unreachable geometry. It relocates the object to checked clear space, zeros motion, resets AI and path state, and returns it to `AIM_GOTO_PLAYER`.
- Upstream has the same multiplayer retry exclusion, but it does not have this branch's full cooperative Guide-Bot control feature. The common single-player experience benefits from the recovery block that cooperative play currently skips.

## Conclusion

The most likely cause of the persistent wall-edge stall is the inherited blanket multiplayer exclusion around AI collision-retry recovery. Wall-edge collision ambiguity is an acknowledged engine condition, but single-player normally has a recovery path while this branch's cooperative Guide-Bot does not. Owner packet starvation is a secondary possibility if the incident was observed by a non-owner, but Android sends pending companion position updates first, and a visually plausible wall-edge stall that persists until an owner-issued warp more strongly fits a genuinely stuck authoritative object.
