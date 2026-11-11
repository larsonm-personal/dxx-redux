# Usable XL time equipment

The single-player OpenGL XL runtime supports placed and generated slow-motion
and bullet-time pickups, normal controls, energy use, pickup rendering and active
save restoration. Classic missions and multiplayer retain their existing paths.
The reference is `arbruijn/d2x-xl` at
`2576a98b5de310e3ac7c9777df2677d1627b8bae`, using the default speed setting 6

## Ordinary setup and controls

Import the user's original XL distribution through the launcher's existing file
picker. Its `XL resources` entry must be enabled in the same active file set as
the retail D2 files and mission. The importer selects addon resources; it does
not run the installer. Original `textures/d2/slowmotion#0.png` and
`textures/d2/bullettime#0.png` are retained as sixteen vertical square frames.
The standard resource sound bank and camera HOG use that same owned resource
mount. Retail, mission and any required genuine Vertigo resources remain separate
user-supplied inputs

On Windows, keep the selected resources at their original relative paths inside
the game-data directory: `d2x-xl.hog`, `textures/d2/` and `sounds/d2x-xl/` are
examples. Place the original mission HOG/MN2 files under `missions/` and its
original addon files under `mods/<mission>/`. Start `d2x-redux.exe -hogdir
"<game-data-directory>"` and select the mission through New game. Use a writable
executable directory for the Windows pilot/configuration files

Collect the original items normally. Desktop uses **Alt+S** for Slow Motion and
**Alt+B** for Bullet Time. Android exposes **Slow Motion** and **Bullet Time** in
the Extra-button action picker; both dispatch through the existing native action
infrastructure. Activation follows the reference's equipment masks and requires
more than ten energy. Repeated presses change the active mode or return it to
normal according to the source toggle rules

Slow motion slows the world and console-player clocks toward 3. Bullet time
retains the slowed world while returning the console player toward 1. Normal
physics, AI, weapons and lifetime consumers use these distinct clocks; global
`FrameTime` is not scaled. Transitions advance once per frame strictly after
25 milliseconds, without catching up a stalled frame. Long native frames retain
the source's energy expression within its checked integer bound

## Shared ownership and lifecycle

- IDs 48/49 use one initializer for placed objects, equipment makers and ordinary
  contained/death drops. Ownership uses the native player's flags. Duplicate
  pickups retain the source behavior
- Newly spawned time items whose retail metadata has zero radius use four world
  units, matching all four authored Warp Shock 6 time pickups. This is an explicit
  portable collision policy: the retail HAM omits usable addon size metadata.
  Existing authored positive radii are preserved
- The production frame adapter consumes requests once, applies energy changes
  through the player owner and delivers sound/message events synchronously
- Genuine `slowdown.wav` and `speedup.wav` are prepared with the same owned bank.
  The shared XL effects sample clock changes between normal and threefold slower
  playback. SDL device and music ownership remain with their existing backend;
  this does not claim a music tempo change
- The XL save block preserves both clocks, transition state, audio sample phase
  and pickup animation ages. Validation precedes application; wall-clock ages
  are rebased on restore. The native player record supplies carried-equipment
  resource requirements before level preparation, including levels without a
  placed time item. Kotlin does not interpret the player/save binary format
- New games clear ownership. Native new-ship/death handling retains time flags
  as the reference does; single-player death also drops the owned devices.
  Committed level loading clears transient clocks/audio. Pause stops normal
  simulation stepping. Exhausted energy and reactor countdown use the source
  transition rules

Input-demo recording/replay still rejects these controls because its real-time
render-clock timeline is not implemented. The Omega replay limitation remains
explicit; no rejection guard was removed

## Current demonstrations and limits

The existing protected Android addon-import runner has a `TimeEquipment` mode
and one shared non-standalone gameplay scenario. It imported the genuine
1.6 GB distribution, restarted the launcher, toggled the resource entry, launched
unchanged Warp Shock 6, collected both devices through ordinary collision,
used normal touch actions, spent energy, saved and continued in a fresh game
process. All 62 imported native resource hashes match the importer inventory and
desktop staging. Profile/configuration hashes match after restoration

That Android demonstration uses a native-written pilot with level 6 unlocked,
five disclosed position assists including a real fuel-center refill, and the
supported 960x540 rendering setting. Live AI remains enabled. It establishes the
feature path, not connected travel or a campaign completion. The software
emulator's slow rendering needs longer transition observations because the
reference advances at most one step per rendered gameplay frame

The existing desktop rendered probe also acquires the original items, uses normal
Alt bindings, saves active bullet time and continues after a fresh-process load.
Identical 24-frame forward thrust from the same original pose produces greater
player travel in bullet time than ordinary slow motion; both use live native
simulation and rendering. These are assisted focused demonstrations. Actual
endpoint audio recording is a separate verification from resource lookup and
mixer callback counters

Normal window pause/resume during a transition preserves clocks and energy.
A separate controlled cycle carries the equipment into original Warp Shock's
secret level, which has no authored time items, saves active bullet time, starts
a new game to clear ownership and prepared time images, then restores that save.
The resources and active clocks are restored before live rendering continues.
Restoring the unequipped save afterward clears both ownership and prepared time
images again. Its four explicit level-load assists do not establish campaign or
secret-entry travel

Actual Windows rendering-endpoint loopback confirms genuine transition playback
and the threefold slower effects clock with the real SDL `waveout` backend.
Music was muted in that isolated diagnostic profile to identify the effects;
live ambient and gameplay sounds remained enabled. No human listening claim is
made. Default DirectSound output in the background native probe remains
unverified; callbacks and mixer PCM alone did not establish device output

Complete-resource original Sphere 2 renders 120 simulation/render frames.
Boiling Point 7 currently passes engine admission but stops at an authored
`lava-fall.wav` absent from both checked original package variants and the genuine
XL resource distribution. No substitute sound or mission-specific bypass was
added. Original Warp Shock 5 separately rejects its `volcano.HXM` definitions as
malformed or requiring unsupported normalization. This was retained as a real
blocker rather than bypassed for the carry demonstration. Full Dinter particles
and Omega input-demo timing remain follow-on work
