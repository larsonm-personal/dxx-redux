# Descent Demo File Format and Regression Testing Analysis

## Demo File Extension
`.dem` -- stored in `demos/` directory via PHYSFS

## Core Architecture: State Recording, Not Input Recording

The demo system records **complete game state snapshots per frame**, not
player inputs. During playback:

- `newdemo_playback_one_frame()` reads events and **directly sets** object
  positions, velocities, orientations from the file
- `object_move_all()`, `do_ai_frame_all()`, and `do_controlcen_dead_frame()`
  are **skipped** (guarded by `Newdemo_state != ND_STATE_PLAYBACK`)
- Physics and AI do not run -- the engine is essentially a state-restoration
  movie player

This means **standard demo playback cannot test game logic**. A robot AI
change would produce identical demo playback because positions come from the
file, not from simulation.

## File Structure

### Header (ND_EVENT_START_DEMO = 1)
| Field | Type | Notes |
|-------|------|-------|
| event marker | byte | always 1 |
| version | byte | D1=13, D2=15 |
| game_type | byte | D1=2, D2=3 |
| GameTime | fix (4 bytes) | always 0 in redux |
| Game_mode | int | bitmask, player_num in high bits for MP |
| mission_filename | string | null-terminated, max 8 chars |
| player energy | byte | 0-255 |
| player shields | byte | 0-255 |
| player flags | int | PLAYER_FLAGS_* bitmask |
| primary_weapon | byte | equipped primary |
| secondary_weapon | byte | equipped secondary |
| primary_ammo[10] | short[10] | per-weapon ammo (MAX_PRIMARY_WEAPONS) |
| secondary_ammo[10] | short[10] | per-weapon ammo (MAX_SECONDARY_WEAPONS) |
| laser_level | byte | 0-5 |
| score | int | starting score |
| MP data | conditional | team info, player list, kill counts |

### Frame (ND_EVENT_START_FRAME = 2)
| Field | Type | Notes |
|-------|------|-------|
| event marker | byte | always 2 |
| prev_frame_bytes | short | byte count of previous frame (for reverse seek) |
| frame_number | int | sequential |
| frame_time | int (fix) | elapsed time this frame |
| events... | variable | until next START_FRAME or EOF |

Frames are only recorded if >= REC_DELAY (1/20 second) since last frame.
Objects are only recorded once per frame (tracked via nd_record_v_objs[]).

### Object Data (shortpos format)
| Field | Size | Notes |
|-------|------|-------|
| render_type | byte | RT_NONE skips rest (except cameras) |
| type | byte | OBJ_ROBOT, OBJ_PLAYER, OBJ_WEAPON, etc. |
| id | byte | robot type, weapon type, etc. |
| flags | byte | object flags |
| signature | short | unique object ID |
| orient | 9 bytes | rotation matrix, 1 byte per component |
| pos | 3 shorts | position relative to segment vertex |
| segnum | short | containing segment |
| velocity | 3 shorts | bit-shifted velocity |
| last_pos | 3 fix | previous position |
| movement data | conditional | velocity+thrust for physics, spin for spinning |
| control data | conditional | explosion timing, light intensity |
| render data | conditional | model/vclip specific |

### Event Types (50 in D2, 44 in D1)
State events: VIEWER_OBJECT (3), RENDER_OBJECT (4)
Player deltas: PLAYER_ENERGY (17), PLAYER_SHIELD (18), PLAYER_FLAGS (19),
  PLAYER_WEAPON (20), PLAYER_SCORE (38), PRIMARY_AMMO (39),
  SECONDARY_AMMO (40), LASER_LEVEL (42), PLAYER_AFTERBURNER (43)
Environment: WALL_HIT_PROCESS (8), TRIGGER (9), DOOR_OPENING (41),
  WALL_TOGGLE (13), WALL_SET_TMAP_NUM1/2 (26/27), CLOAKING_WALL (44)
Audio: SOUND (5), SOUND_ONCE (6), SOUND_3D (7/11),
  LINK_SOUND_TO_OBJ (49), KILL_SOUND_TO_OBJ (50)
Game: CONTROL_CENTER_DESTROYED (15), NEW_LEVEL (28),
  HOSTAGE_RESCUED (10), MORPH_FRAME (12)
Visual: PALETTE_EFFECT (16), HUD_MESSAGE (14), LETTERBOX (23),
  RESTORE_COCKPIT (24), REARVIEW (25), CHANGE_COCKPIT (45)
MP: MULTI_CLOAK/DECLOAK (29/30), MULTI_DEATH/KILL (32/33),
  MULTI_CONNECT/RECONNECT/DISCONNECT (34-36), MULTI_SCORE (37)
D2-only: START_GUIDED (46), END_GUIDED (47), SECRET_THINGY (48),
  EFFECT_BLOWUP (21), HOMING_DISTANCE (22)

### Footer (ND_EVENT_EOF = 0)
| Field | Type | Notes |
|-------|------|-------|
| event marker | byte | always 0 |
| prev_frame_bytes | short | for backward seeking |
| MP cloaked mask | byte | only in multiplayer |
| padding | short + short + int | EOF markers |
| energy | byte | final value, 0-255 |
| shields | byte | final value, 0-255 |
| flags | int | final player flags |
| primary_weapon | byte | equipped |
| secondary_weapon | byte | equipped |
| primary_ammo[10] | short[10] | final ammo counts |
| secondary_ammo[10] | short[10] | final ammo counts |
| laser_level | byte | final laser level |
| score | int | final score (SP), or MP kill data |
| byte_count | short | byte count of footer data |
| current_level | byte | level at end |
| EOF marker | byte | always 0 |

## RNG System

- Algorithm: LCG in d2/maths/rand.c -- `(seed * 0x41c64e6d + 0x3039) >> 16 & 0x7fff`
- Global seed: `static unsigned int d_rand_seed`
- Seeded from wall clock: `d_srand((fix)timer_query())` at net game start
- NO seeding during demo playback
- For deterministic replay via input recording, must seed with fixed value

## Playback Speed Control

- NORMAL_PLAYBACK: real-time at recorded speed
- SKIP_PLAYBACK: drops frames if rendering falls behind
- INTERPOLATE_PLAYBACK: lerps object positions between frames
- FASTFORWARD: reads 10 frames per render frame
- Single-step forward/backward supported
- VCR-style controls (pause, rewind, fast-forward)

## Existing Command-Line Support

- `-autodemo` flag: plays random demo from demos/ directory at main menu
- Uses `newdemo_start_playback(NULL)` for random selection
- No headless mode exists in the codebase

## D1 vs D2 Compatibility

- Version check enforced: D2 rejects D1 demos (version < 15)
- D1 has 44 event types, D2 has 50
- D1 uses game_type=2, D2 uses game_type=3
- Endian handling built in (byte swapping for cross-architecture)

## Key Source Files

| File | Role |
|------|------|
| d2/main/newdemo.c | Recording, playback, all event handlers (~3800 lines) |
| d2/main/newdemo.h | Event constants, state machine declarations |
| d2/main/game.c | Game loop -- physics/AI skip during playback |
| d2/maths/rand.c | RNG (LCG, global seed) |
| d2/main/kconfig.h | control_info struct (player input) |
| d2/main/player.h | player struct (full game state) |
| d1/main/newdemo.c | D1 variant |

## Key Functions

| Function | File | Purpose |
|----------|------|---------|
| newdemo_record_start_demo() | newdemo.c | Write header |
| newdemo_record_start_frame() | newdemo.c | Frame marker + timing |
| newdemo_record_render_object() | newdemo.c | Write object state |
| nd_write_object() / nd_read_object() | newdemo.c | Object serialization |
| newdemo_write_end() | newdemo.c | Write footer with final state |
| newdemo_read_demo_start() | newdemo.c | Parse header, load mission |
| newdemo_read_frame_information() | newdemo.c | Parse all events in a frame |
| newdemo_playback_one_frame() | newdemo.c | Main playback loop |
| newdemo_start_playback() | newdemo.c | Open file, validate, start |
| newdemo_stop_playback() | newdemo.c | Cleanup, set GM_GAME_OVER |

## Implications for Regression Testing

### What works
- Demo files already store mission filename, so we know what game data to load
- The footer stores final player state (score, energy, shields, weapons, ammo)
- Playback infrastructure exists (file I/O, level loading, VCR controls)
- Fixed-point math means cross-platform determinism is feasible

### What doesn't work
- State-based recording means game logic changes are invisible to playback
- No input recording exists -- must be added
- No headless mode -- must be added for fast CI testing
- RNG not seeded deterministically -- must be controlled
- Frame timing from wall clock -- must be captured and replayed exactly

### Required new work for input-driven regression testing
1. Input recording: capture control_info per frame alongside frame timing
2. Deterministic RNG: seed at level start from file, not wall clock
3. Input playback mode: feed recorded inputs, run full simulation
4. State capture at completion: serialize game state to comparison file
5. Headless mode: skip rendering for fast test execution
