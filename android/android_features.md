# Launcher

- Helps importing game files (.hog etc.)
- Launches either D1 or D2 engines (from the same app)
- Chooses some game options (like rendering details)
- Joystick control mapping: test page showing controller details and allows mapping. edited configs are saved to player files so the in-game control mapping still works but is pre-filled

# Game file management

- archive support sufficient to directly extract the windows gog installers for d1+d2 so users can install and play without needing a PC to mess with files, plus download recommended hires/sound fixes
- Archive support for a few other types of file like dos installer files and some mac ones, but the most important are the windows offline installers from gog, or CD images
- Allows importing game files (.hog,.pig, etc.) and either copying them or leaving them in place
- Allows importing game files from bin+cue disc images (scans the data tracks, finds assets, finds .sow and extracts as needed)
  - Extra stuff like the extra missions included in the anniversary edition CD will be imported to an entry in the mods/levels list so it can be optionally used
- Allows keeping multiple base file sets (mostly for testing, most users will just import their best files and be done)
- On import, attempts to label the files using a list of known file hashes so users know which descent version’s assets they’re playing with
- Redbook audio is recognized on disc image import. Redbook tracks are recognized by file hash as well
- Option to choose the drive for game data, this helps with storage-limited devices such as nvidia shield that need to use the sd card for expanded storage

# Single player QoL

- D2: allow spawning the guidebot at the player’s position if it hasn’t been released yet, or the map maker didn’t include it. If it’s blown up it can be respawned, as well
- Guidebot now routes to switches that control locked doors/walls, and fly-through triggers, in addition to keys->reactor->exit. The routing is pretty good, for example obsidian level 3’s notorious 20-step sequence is correctly decoded, as well as obsidian level 4’s keys-after-reactor sequence. Castaway level 2 is another tough one that it handles. Switches and “shoot from” points are shown on the automap if that is toggled on (default is off since it’s like a cheat). My intent is to de-fang some of the more egregious routefinding-for-routefinding’s sake that’s in early level packs (which, at its worst, is virtually impossible to get on the first try without save scumming). At the same time, you don’t have to use guidebot if you don’t want to - some people like these levels in their original form, and many people like manual routefinding/map reading too
- Some extra guidebot goals such as “find the largest unexplored mine section”
- Allow switching difficulty mid-level (this will only change ongoing effects such as robot speed and damage, not initial conditions like health of existing robots or initial ammo loadout)
- Automatically scans levels for secret areas and gives a text message when one is discovered (flown into). When game progress tracking is turned on, the secrets found/total counts appear next to hostage and robot counts
- A new special cheat that highlights and labels secret areas on the automap. When the cheat is active, guidebot can navigate to a secret as a task. Secret area classification isn’t perfect because the actual areas aren’t labeled by the map maker (like they are in doom), so the game is using some hints based on map geometry
- Boss health bar (optional)
- FoV slider for 90-100-110-120 degrees
- D1 missions played within D2 have support for their textures etc. so they look right, plus an attempt to get weapon and robot behavior to match (with a sort of emulation layer). This gives the advantages of D2 (basically: hud cameras, and can spawn guidebot if desired). I recommend playing D1 this way if you own both games (it needs both sets of files). Long term there should be a unification of the game engines like d2xxl has done and rebirth has partially done

# LAN play quality-of-life

- Optional guidebot for coop games. The player to release the guidebot becomes the owner. If they leave the game the guidebot attaches to another player. Ownership can be abdicated, giving the guidebot to another random player
- Coop: allow briefings (optional setting. Base game behavior is to omit them). The host can force a mine start instead of waiting for players to finish watching them
- Coop: allow secret areas (optional setting. The base game behavior is to disable them). If a player enters a secret warp door, the remaining players are brought to the secret area after a short countdown. Secret doors are blocked if a normal exit door is used by another player (all players are still in a “beat the reactor countdown to the exit” race). Secret doors taken during reactor countdown work normally: they bring the whole party to the secret area, then the next level after the secret
- Coop: track a player’s loot so if they leave and rejoin they get it back (they still generate spew on exit, and any uncollected spew is given to them on return)
- Coop: remove absorption time from player spew (optional)
- Special case for two player local coop games: if the host leaves, the client can take over as host (allowing the host to rejoin what is now the server)
- Warp to other player button is shown when a player is some distance away (optional). Helps prevent softlocks, makes the game more cooperative
- breadcrumb-like path to nearest other player, and to guidebot (optional)
- QoL changes can be enabled/disabled when launching a game
- The game tries to automatically save cooperative game progress and show available saves to resume from the launcher. Resuming from the launcher (starting a game with a save game selected) is possible, it starts the level then loads the save when all players have joined
- Allow more coop starts than maps have configured by generating offsets for existing start points
- While in lobby, check mission zip size and hash vs. host (a warning only, it doesn’t prevent playing, but the intent is to flag incompatible versions)
- Allow downloading missions from the host if the client needs them to play (excluding licensed base game data)

# Mod/level management

- (todo) recommends high res, sound fix etc. mods to improve the base game and automatically downloads them
- allows using dxa mod files and choosing which ones are active at game start (mod manager functionality). Multiple mods can be active at once with a priority order in case of masking. Mission sets are also managed this way. Multiple missions can be active at once and their changes won’t mask to other missions when they aren’t being played (for example, ewithin changes some robot sounds. Those sounds only change while ewithin is being played, even if it’s active in the mod list)
- Extensive mod/mission metadata inspection - tap on files to dive into their embedded assets, with lists of changes. The viewer includes:
  - a readme file viewer
  - robot viewer showing 3d model, attacks, sounds, damage per second
  - individual level metadata: number of robots, matcens, hostages, normalized volume and par time metrics, etc., along with route analysis
  - 3d map preview
- allows importing custom missions (level packs) and choosing which ones are active at game start
- Can import .zip/.rar/.7z level packages from [sectorgame.com](http://sectorgame.com) without extracting. Allows picking music from the zip (midi/hmp or mp3/ogg/etc.; level makers are doing both now) or from one of the other audio sources

# Save file management

- auto save on minimize (best effort: android minimize can be done in different ways) (can be disabled) and auto save on quit to launcher (can be disabled). Three slots dedicated to this
- Auto save every 5 minutes. Two slots dedicated to this
- Quick save/load which can be mapped to controller buttons. One slot dedicated to this
- Pre-fill manual save names like “level N” to save typing
- show the most recent save game in the launcher and offer to directly load (can be disabled)
- Rewind button: automatically saves every 5 seconds and allows binding a rewind button to go back to 5/10/20 seconds earlier (can be disabled)
- Saves are de-duplicated between mission sets and cooperative play so players can switch between missions and not overwrite/lose saves on the previous mission. The in-game “load” menu will show the saves for the mission with the most recent save. The launcher’s “save explorer” allows browsing all of them

# Music

- MIDI(hmp/hmq) tracks work using an included MIDI library. Soundfonts are selectable. Some enhancements to midi playback accuracy based on a/b tests vs. dosbox that found some limitations in redux. Optional adlib/opl3-style FM playback as an alternative, this was also tuned using a/b testing. Both of these sound good to my ear and match youtube recordings pretty well (even though they’re synthesized on the fly)
- Parses and plays redbook audio from the gog CD image if available, or other bin+cue images
  - allows loading multiple disc images and combining them into one playlist. This is a neat feature for multi-cd releases such as the definitive edition
- Track name (if known) is shown when a new song starts. Can come from mp3/midi metadata or a chromaprint match
- Track skip/music player interface in overlay
- redbook tracks from most discs are labeled with song titles based on a built-in chromaprint database
- mp3/ogg/flac music file sets are supported (for example, the midi->mp3 rips that are floating around to showcase various synthesizers). These are also recognized with chromaprint in most cases
- Chromaprint web lookups (off by default) for unrecognized tracks using acoust-id
- Preference for CD audio vs. midi vs. discrete files is saved in save games, so it can be preserved when playing different level packs (these sometimes come with midi audio, sometimes discrete, sometimes none)

# Touch interfaces

- Optional touch controls overlay
- Customizable button locations, joysticks, mouse mode joysticks, sensitivity, exponential throw. Wheel menus for weapons and guidebot. vertical/horizontal slide menu for guidebot. Optional double tap to shoot, latching shoot buttons, etc.
- After a lot of testing my preferred touch look control is a “mouse-mode” region (drag distance->look distance), with stick-type continuous movement regions at the edges
- Joystick touch controls have a flexible center point - the stick starts at 0 throw when you first touch (this is a common strategy in other games)
- Touch joysticks can have button actions mapped to the end of their travel. The main use for this is to set up a throttle stick that controls normal forward/reverse but triggers afterburner at 130%+ forward throw, for example
- A couple menu buttons. One contains all important controls that are left unbound (so players aren’t locked out from using, for example, energy->shield because they forgot to bind it). Another for game menus etc.
- Optional gyro axis bindings for movement etc.. this is not my preference but I think some will like it for slide up/down and roll

# Gamepad interfaces

- Most (all?) menus are reachable with a gamepad in order to be usable on android TV.  Some menus might still lack support as the base game (rebirth/redux) doesn’t have this and it was a lot of work, please submit bug reports for any not working
- Some touch menus get turned into gamepad-accessible menus when the APK detects no touch interface. Otherwise they’re unified interfaces

# menus

- Enlarge all menus to .85x of screen height to help with touch navigation. Some menus are enlarged to .85x of width and then made scrollable (such as the controls editing pages). When in the main game menus, they can be further enlarged by two-finger zoom in an inactive region
- Add “ok” to some menus to be able to touch through them
- Allow swiping to scroll through menus. Other touch interface improvements like moving the text entry area into view when the android keyboard opens
- Controller menu support: d-pad and analog sticks move up/down menus, a selects, b cancels
- Some special case things like long-press on the weapon autoselect menu in-game to drag (although the launcher’s menu for this setting is preferred)
- Please submit bug reports for touch limitations, the intent is to have this be complete

# Graphics

- Ported the high res texture packs from d2x-xl. Used etc2 compression for arm gles size/performance. Texture packs are available @ 128x128 (downscaled from 512), 256x256, 512x512 (base game is 64x64). these imported textures aren't perfect. most players should stick to the base textures which look great with anisotropic filtering, texture filtering and msaa
- ported/updted the xfing uud1tp/uud2tp/uud2sp to .dxa and set them up as patches rather than as redistributions of hog/pig/etc.
- The game runs a gles 3.0 shim so it has access to etc2 and newer effects but the desktop code can stay the same
- Texture filtering, anisotropic filtering, anti aliasing options (these existed in the base redux game, but are cleaned up and have more options)
- Graphics debugging overlay with texture name labels
- Option to move score and other corner text away from rounded android screen corners based on the android API for corner dimensions
- Automap drawing optimized (example: very large level “uneasy 4” needing ~8k lines rendered ran at 1 frame/second on my flagship phone, now ~60 fps)

# Network play

- Currently, LAN and direct (fill-in-IP, with port forwarding) connections work
  - Auto find LAN games with broadcast packets
  - In-game network overlays for connection info (connection type, ping, packet loss)
- I’m planning a matchmaking server in future work, there is a partially working one already but support is hidden from the app
  - (planned) Matchmaking server with game list, friends list, etc.
  - (planned) “ICE”-style stun/turn/direct connection manager/helper for hole punching
  - (planned) Relay (“turn”-type) server built into the matchmaking server for network connections that fail hole punching/direct connect

# Demo System

- A new demo format which is done in the style of doom’s demos: only player inputs are recorded, then they’re replayed for the demo, with RNG maintained as repeatable.  These “input-based” demos can be started mid-level, in which case they first capture an embedded save game and then per-frame inputs
- The advantage of this demo format is that they’re typically smaller than descent’s original .DEM demos (which encoded visible object positions for each frame), and they can be used to build up a body of game engine regression tests which re-run demo files and compare to a known-good final state
- Having this demo system lets us ensure the PC vs. android port behavior is the same (which is tricky because there are some floating point operations that can vary between architectures)
- Cleaned up the RNG paths such that the game engine’s RNG is separated from rendering side effects: there is now a headless demo runner that can verify demo files very quickly, and game engine determinism is slightly improved in terms of being fully independent of rendering
- With a large enough body of test files, we can then make large refactorings to the game engine code without worrying about changing behavior.  The biggest changes needed are to de-duplicate the d1/ and d2/ source

# Technical improvements

- Android build scripts and dependency fetching. These are slop, I didn’t care about style/taste, but they work
- An introspection and game driving interface. This supports regression tests that open the game up, start a level, open the minimap, etc. and check that the introspection API shows the right things
- Direct gog installer support (for the gog offline PC installer files), using libarchive, innoextract, etc., which gets game assets out of the installer plus redbook audio for d2
- Support for installing from a bin+cue CD image, getting game assets from the data track and using the redbook audio tracks for music. Mac CD images are supported as well
- Regression tests that install from one of the installable discs or gog files, open the game, start a level, and ensure the game runs in the first level
- Integrated MIDI library
- Some abstractions around the game’s existing file API so it can map to android and mod manager needs

# Technical todo

- Swap sha code to a cmake fetch dependency of some kind (?)
- Swap bin/cue parser to a cmake fetch dependency also (?)
- Versioning in vcpkg dependencies? Auto update scripts once pinned?
