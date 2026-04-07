# Study: Enabling the Guidebot in Multiplayer Coop

## Current State

The guidebot is completely disabled in multiplayer via `if (Game_mode & GM_MULTI) return` guards in three places in `escort.c`, plus a message "No Guide-Bot in Multiplayer!" when you try the menu. The AI code in `ai.c` still runs the companion frame (`do_escort_frame`) regardless of game mode, but the escort code's own guards prevent messaging, goal-setting, and the menu from functioning.

## Answers to Specific Questions

### 1. Does the guidebot collide with the player? With robots?

**Player**: NO. `collide_robot_and_player()` in `collide.c:985` explicitly returns early for companions: `if (Robot_info[robot->id].companion) return;`. The guidebot passes completely through the player with no bump, no push, no damage.

**Robots**: YES. `collide_robot_and_robot()` calls `bump_two_objects(robot1, robot2, 1)` with no companion exception. The guidebot physically bumps into enemy robots and vice versa. However, the companion does NOT take collision damage from other robots (`bump_this_object()` at `collide.c:252` skips damage for companions).

**Walls/Doors**: NO. `collide_robot_and_wall()` at `collide.c:107` returns early for companions (same as Brain robot). The guidebot clips through walls and doors in the physics system.

This is good news for multiplayer -- the guidebot won't interfere with player movement.

### 2. Guidebot flare damage

**Single-player mechanism**: `bash_buddy_weapon_info()` in `escort.c:893` overrides the flare's parent to be `ConsoleObject` (the local player) with `parent_type = OBJ_PLAYER`. This makes `laser_are_related()` in `fvi.c:828` block the collision between the flare and its parent player during intersection tests. So the flare physically cannot reach the player who owns the guidebot. However, this is only a parent-skip, not an immunity -- if the flare's parent were set differently, it COULD damage the player.

**Were players ever damaged by buddy flares?** In DOS D2, yes -- but only via splash damage from the buddy's mega/smart missiles fired with the `GABBAGABBAHEY` cheat (`cheats.buddyangry`). Normal flares can't hit the parent player due to `laser_are_related()`. The comment at `collide.c:1588` ("Put in at request of Jasen (and Adam) because the Buddy-Bot gets in their way") refers to preventing player weapons from damaging the companion, not the other way around. Rebirth/Redux didn't change this -- the mechanism was always parent-skip in FVI, not an explicit immunity check in `collide_player_and_weapon()`.

**Multiplayer concern**: In multiplayer, the flare's parent is set to the owner's ConsoleObject. On the owner's machine, `laser_are_related()` prevents self-hits. But on OTHER players' machines, when the flare is recreated via `multi_do_robot_fire()` -> `Laser_create_new_easy()`, the parent is set to the robot (the companion), not the player. So the flare COULD hit non-owner players. Fix: add a companion-weapon check in `collide_player_and_weapon()`:
```c
// Skip damage from companion robot flares
if (weapon->ctype.laser_info.parent_type == OBJ_ROBOT &&
    weapon->ctype.laser_info.parent_num >= 0 &&
    Robot_info[Objects[weapon->ctype.laser_info.parent_num].id].companion)
    return;
```

**Flares vs robots**: CAN hit and damage. The `collide_robot_and_weapon()` early-return check at `collide.c:1588` explicitly excludes `parent_type == OBJ_ROBOT` hits, so companion flares do normal damage to enemy robots.

**Flares vs walls/doors**: The guidebot's flares can open doors and trigger destructible wall effects (`collide.c:721`, `check_effect_blowup()` at `collide.c:520`). The flare is treated as a player weapon for trigger purposes.

For multiplayer, flare damage to robots is actually harmless because the guidebot's flare DPS is tiny. The door-opening behavior is the important function. If one player owns the guidebot and fires its flares, the door-open events are already wall state changes that get synchronized via existing multi wall packets.

### 3. Can the guidebot be killed?

**By player weapons**: NO. The `collide_robot_and_weapon()` guard at `collide.c:1588` prevents all non-robot weapons from damaging the companion.

**By enemy robot weapons**: YES. Robot-fired weapons do hit and damage the companion normally through `apply_damage_to_robot()`.

**By splash/blast damage**: YES. `object_create_explosion_sub()` in `fireball.c:167` applies splash damage to all robots in radius including companions. No companion exemption exists in the splash damage loop.

**By collision damage from robots**: NO. The `bump_this_object()` function skips damage application for companions (only applies push forces).

**After death**: The guidebot does NOT respawn. `create_buddy_bot()` is only called at level init. `Buddy_objnum` is never reset to -1 after death, leaving a dangling reference. Death is permanent for the level.

**Level 24 special**: On the final level of the built-in mission (network builds only), the companion is fully invulnerable via an early return in `apply_damage_to_robot()`.

For multiplayer, the simplest approach: make the companion invulnerable (extend the level-24 protection to all coop levels). Since it can't respawn and losing it mid-level would be confusing in a multi-player context, invulnerability is the cleanest solution. If killability is desired later, a respawn mechanism would need to be added.

---

## Architecture for Multiplayer Guidebot

### Proposed Model: Owner-Controlled Companion

**Concept**: The player who first frees the guidebot (destroys its cage) becomes the "escort owner." On that player's machine, the full escort AI runs normally. The owner gets the guidebot menu (Shift+F4), can set goals, and sees buddy messages. To all other players, the guidebot appears as just another robot whose position is replicated via the existing `MULTI_ROBOT_POSITION` system.

### Why This Works

The existing multiplayer robot ownership system (`REMOTE_OWNER`, `robot_controlled[]`, `MULTI_ROBOT_POSITION`, `MULTI_ROBOT_FIRE`) already does everything needed:

1. **Position sync**: Owner sends `MULTI_ROBOT_POSITION` packets (~27 bytes, shortpos format). Non-owners receive and update the robot's position/orientation. This already includes interpolation/smoothing.

2. **Fire sync**: Owner sends `MULTI_ROBOT_FIRE` packets (18 bytes) when the guidebot fires a flare at a door. Non-owners create the flare projectile locally.

3. **Ownership persistence**: Normal robots can be stolen by other players (via `multi_robot_request_change()` when they shoot/interact with the robot). For the companion, ownership should be **locked** to the releasing player -- no contention, no priority resolution needed.

4. **AI distribution**: All players already run `do_ai_frame()` for all robots. For the companion, only the owner would run `do_escort_frame()`. Non-owners would run a minimal "follow the replicated position" mode (the same thing they already do for any owned-by-another-player robot).

### Key Changes Required

#### 1. New Global: Escort Owner (`escort.c`)

```c
int Escort_owner_player = -1;  // -1 = no owner yet, 0-N = player who freed it
```

Set when a player destroys the blastable wall(s) containing the guidebot. The `Buddy_allowed_to_talk` flag transitions from 0 to 1 in `ok_for_buddy_to_talk()` when the cage walls are gone -- this is the trigger point.

#### 2. Guard Clause Changes (3 places in `escort.c`)

Replace `if (Game_mode & GM_MULTI) return` with:
```c
if ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP)) return;  // Only coop
if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num) return;  // Only owner
```

This enables the guidebot **only in coop** and **only for the owner**.

#### 3. Lock Robot Ownership (`multibot.c`)

In `multi_can_move_robot()` and `multi_do_claim_robot()`, add:
```c
if (Robot_info[Objects[objnum].id].companion && REMOTE_OWNER != -1)
    return 0;  // Companion ownership is permanent
```

This prevents other players from stealing control of the guidebot through the normal robot contention system.

#### 4. New Packet: Set Escort Owner (`multi.c`)

A small new packet type `MULTI_ESCORT_OWNER` (3 bytes: type, player_num, buddy_objnum) to announce who owns the guidebot. Sent once when the cage is broken. All players store `Escort_owner_player` locally.

#### 5. Invulnerability in Coop

Extend the level-24 protection to all coop levels:
```c
if (Robot_info[robot->id].companion) {
    if (Game_mode & GM_MULTI_COOP)
        return 0;  // Companion is invulnerable in all coop levels
}
```

#### 6. `ConsoleObject` References (17 places in `escort.c`)

The escort code uses `ConsoleObject` extensively (the local player's ship). In multiplayer, this is always the local player, which is fine if only the owner runs the escort AI. However, for pathfinding like "create path away from player segment," the code references `ConsoleObject->segnum` -- this works correctly because the owner's ConsoleObject IS the player the guidebot should follow.

No changes needed here as long as only the owner runs `do_escort_frame()`.

#### 7. Guidebot Menu for Non-Owners

Non-owners trying to press Shift+F4 would see "Guide-Bot is controlled by [callsign]" instead of the menu. Only the owner gets the menu.

### State That Needs Synchronization

| State | Sync Method | Notes |
|-------|-------------|-------|
| Position/orientation | Existing `MULTI_ROBOT_POSITION` | Already works |
| Flare firing | Existing `MULTI_ROBOT_FIRE` | Already works |
| Door opens | Existing wall state packets | Already works |
| Escort owner assignment | New `MULTI_ESCORT_OWNER` packet | One-time, at cage break |
| Buddy messages | NOT synced | Owner-only display |
| Escort goal | NOT synced | Owner-only state |
| Guidebot death (if enabled) | Existing `MULTI_ROBOT_EXPLODE` | Already works |

### What Does NOT Need Sync

- `Escort_special_goal`, `Escort_goal_object`, `Escort_goal_index` -- owner-only state
- `Buddy_last_seen_player`, `Buddy_allowed_to_talk` -- owner-only state
- `Buddy_messages_suppressed` -- owner-only preference
- `Looking_for_marker` -- owner-only interaction

### Non-Owner Rendering

Non-owners see the guidebot as a normal robot. It renders with the same model/textures (the thief/guidebot model is in the robot data). Position updates arrive via `MULTI_ROBOT_POSITION` at the normal robot update rate. The existing robot interpolation/smoothing that applies to all remote-owned robots applies here too.

### Edge Cases

1. **Owner disconnects**: `multi_strip_robots()` already releases all robots when a player drops (sets `REMOTE_OWNER = -1`). For the guidebot, we should go further: **auto-adopt to a random remaining player**. Flow:
   - `multi_strip_robots(playernum)` fires for the disconnected player
   - If `playernum == Escort_owner_player`, pick a new owner from remaining connected players (random or lowest player number)
   - The new host (or whoever detects the drop) sends a `MULTI_ESCORT_OWNER` packet with the new owner
   - The new owner's machine starts running `do_escort_frame()` and gains the guidebot menu
   - The guidebot does `AIM_GOTO_PLAYER` toward the new owner's ConsoleObject
   - This is ~10 lines of code in the disconnect handler

2. **Owner dies**: Player respawns and the guidebot returns to them (it already does `AIM_GOTO_PLAYER`). No special handling needed -- the owner doesn't change on death.

3. **Cage broken by non-owner's weapon**: The player whose weapon destroyed the blastable wall becomes the owner. This is detected locally by whoever fires the destroying shot. A `MULTI_ESCORT_OWNER` packet announces it.

4. **Multiple guidebots in custom levels**: `find_escort()` only returns the first one. Multiple companions are not well-supported even in single player. For multiplayer, probably best to keep the same limitation.

5. **Guidebot opens a key-locked door**: The escort code checks `ConsoleObject->flags & PLAYER_FLAGS_*_KEY`. In multiplayer, this means the owner's keys determine what the guidebot can open. This seems reasonable -- the guidebot serves the owner.

### Complexity Assessment

**Low complexity changes** (just removing guards + adding owner check):
- `buddy_message()` guard (line 399)
- `init_thief_for_level()` stolen items guard (line 1601)
- `do_escort_menu()` guard (line 1724)

**Medium complexity** (new but small):
- `Escort_owner_player` global + `MULTI_ESCORT_OWNER` packet
- Lock companion ownership in `multibot.c`
- Extend invulnerability to all coop levels

**Already working** (no changes needed):
- Position/fire/explosion replication
- Robot rendering on non-owner machines
- Door opening wall state sync
- Collision model (no player collision, yes robot collision)
- AI frame execution

### Estimated Scope

- ~50-80 lines of new code in `escort.c`, `multibot.c`, `multi.c`, `collide.c`
- 1 new packet type (3 bytes, sent once per level)
- No new files needed
- No changes needed in d1/ (d1 has no guidebot)
- All changes are `#ifdef NETWORK` or `GM_MULTI_COOP` guarded

### Risks

1. **`d_rand()` divergence**: The escort AI uses `d_rand()` for path lengths. Since only the owner runs `do_escort_frame()`, and other players receive position updates, this isn't a problem -- there's no expectation of deterministic sync.

2. **Point_segs exhaustion**: The guidebot creates paths frequently (every 5 seconds). In multiplayer with more robots needing paths, `Point_segs` could fill up faster. This is an existing multiplayer issue not specific to the guidebot.

3. **Frame rate dependency**: `do_escort_frame` runs every AI frame. On slower machines, the owner might send position updates less frequently. This is the same as any robot ownership situation.

4. **Flare door sync**: When the guidebot shoots a flare at a door and the door opens, the wall state change is synchronized via existing wall packets. However, the flare projectile itself might not render on non-owner machines if `MULTI_ROBOT_FIRE` is missed. Door state is the important thing and that's reliable.

5. **Companion flares hitting non-owner players**: As detailed in section 2 above, when a companion flare is replicated to another player's machine via `multi_do_robot_fire()`, the parent is set to the robot, not the player. This means the flare CAN collide with and damage non-owner players. Needs an explicit companion-weapon guard in `collide_player_and_weapon()`.

---

## Android-Specific Integration

### Coop Overlay: Guidebot Owner Indicator

The existing `CoopStatsOverlay.kt` shows robot kill stats and teammate status in the top-left. A guidebot owner indicator fits naturally here.

**Proposed design**: A small guidebot icon (from the robot model sprite sheet, or a simple custom drawable) displayed in the coop overlay area. Next to it, show the owner's callsign (or "You" if local player is owner). Only visible when `Game_mode & GM_MULTI_COOP` and a guidebot exists on the level.

**Implementation**:
- New JNI function: `nativeGetEscortOwnerStatus()` returning owner player index (-1 if no guidebot or not yet freed)
- C side: expose `Escort_owner_player` via JNI (trivial -- same pattern as existing `nativeGetCoopRobotStats()`)
- Kotlin side: add a small section to `CoopStatsOverlay.kt` that polls this value and renders the icon + callsign
- Icon: could reuse the existing guidebot model thumbnail or a simple vector drawable. A tiny robot silhouette would be enough

**Polling**: Same 100ms polling loop already used by CoopStatsOverlay. No additional timer needed.

### Touch Controls: Guidebot Menu Visibility

The guide-bot radial menu trigger already exists in `TouchOverlayView.kt` (control id `"Guide"`, skipped for D1 at line 1485). In multiplayer coop, this control should be:
- **Visible and active** for the escort owner
- **Hidden** for non-owners (they can't use the guidebot menu anyway)

**Implementation**:
- New JNI function: `nativeIsEscortOwner()` returning boolean
- In the radial menu trigger loop (line 1485), extend the skip condition:
  ```kotlin
  if (rm.control.id == "Guide" && gameVariant == "d1") continue
  if (rm.control.id == "Guide" && isMultiCoop && !nativeIsEscortOwner()) continue
  ```
- The `isMultiCoop` flag is already available from `nativeGetGameMode()` checks used elsewhere
- Non-owners see no guide-bot radial, so they can't accidentally open a menu that won't work

**Alternative**: Instead of hiding completely, show the guide-bot radial in a dimmed/disabled state so non-owners know the feature exists. Tapping it shows "Guide-Bot controlled by [callsign]" as a HUD message. This is more discoverable but slightly more work.

### Ownership Transfer UX

When ownership transfers (original owner disconnected), the new owner should see:
- A brief HUD message: "Guide-Bot is now following you"
- The guide-bot radial menu control appears (was previously hidden)
- The guidebot icon in the coop overlay updates to show "You"

The departing owner's machine doesn't need to do anything (they're gone). The guidebot pathfinds to the new owner automatically via `AIM_GOTO_PLAYER`.
