# Render-backed XL Omega state

Continue #397 with the actual clock producer required by Omega. No pickup
admission, official readiness score, trusted cohort or original asset changes.

## Source and implementation

Pinned revision: 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`render/renderframe.cpp:735` increments after GameRenderFrame; line 348 adds an
increment for each end-level inner RenderFrame. Full automap increments at
`render/automap.cpp:504`. `gameio/loadgame.cpp:913` resets the counter on level
load; the weapon's last-fire marker is not reset there. Both exact LF and CRLF
hashes are verified by the supplemental runner; other representations/content
are rejected. No broad reference setup or pinning convention changed.

Redux now updates an isolated XL counter at those actual renderer boundaries.
It does not use simulation-frame or 50 ms tick counters. Auxiliary normal mono
views and simulation-only frames do not advance it. Classic/multiplayer gameplay
remains on existing behavior.

`weapons/omega.cpp:202-245` supplies the current/previous-render recharge gate,
accepted local-shot frame-time consumption and next-fire deadline of game time
plus one fixed-point unit. Rejected shots retain their state. Existing native
ownership/death/low-charge checks remain. The reference's normal Omega beam
geometry, gun-damage randomness and all visual parity are not claimed here.

XL trailer schema 32 stores the raw render counter and raw last-fire marker
alongside #397's relative missile timestamp. It preserves both identities and
validates all integer types, arity and 32-bit bounds before publication; no
counter-age normalization or classic save-layout change. As repository policy
allows for prerelease formats, this replaces schema 31 without a migration.
Active slow-clock speed/audio persistence is still outstanding.

## Actual-render qualification

`xl_render_clock.py` is an explicitly assisted developer probe, not readiness.
It requires a real GL context, native main/auxiliary drawing, a full automap draw
and an injected in-mine flythrough render state. It checks actual Omega firing,
rejection, recharge and a disk restore that reloads the level, then renders again.
The two configured cockpit subviews are disabled for this bounded mono profile.
This is not full cinematic, stereo, original mission completion or audio parity.
The dedicated command requires an isolated user directory; the runner creates
one and refuses an existing output directory.

The qualified event contributions are 0,1,0,1,2,1,0,1 for simulation, main,
auxiliary, automap, end-level main, end-level auxiliary, simulation and main again.
The disk restore returns counter identity to 1; the next main render advances to
2 and resumes recharge. The classic control keeps the XL counter at zero and
retains its legacy consumption, deadline and timestamp gate.

Use a stable Linux display, e.g. `xvfb-run -a -s '-screen 0 1024x768x24 -noreset'`
with the Python runner. Early classic attempts failed at SDL initialization with
`x11 not available`, before any probe event. Keeping Xvfb alive across display
client resets resolved this setup failure. No timeout or assertion was changed.

Derived controls use wetbots.hog produced by the existing water owner from
unchanged Neptune HOG SHA-256
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
Retail classic controls use Counterstrike level 1. No private assets are published.

## Remaining work

Wall/object effect timing, controls, authentic pickup presentation/audio and full
slow-clock persistence remain. Powerups 48/49 stay unsupported until their complete
behavior is qualified. No new original/readiness pass is asserted.

## Final validation

Both rendered controls passed on the exact desktop binary SHA-256
3240d3a4767ca2c889032794d2c772574d7f551861749c280d06db4c16011051,
using llvmpipe (LLVM 19.1.7, 256 bits). The same binary failed the no-OpenGL
negative control before producing a successful receipt. XL charge after a
1092-unit accepted shot was 31676 with deadline +1; classic retained 29492 and
+3276. Both rejected-shot controls retained firing state. The raw counter and
marker survived actual disk restoration followed by rendering and recharge.

Local receipts: /workspace/scratch/xl-render-clock/qualified-xl/result.json,
qualified-classic/result.json and qualified-no-gl/result.json. The derived
wetbots.hog identity is
6181c9f3ec72d4368b2646cb50b7d6cdcdd800b56a7e5da4353c94eeb3c8d044.
These are assisted component controls, not unchanged original campaign evidence.

Passed: all inherited source comparisons, including 3840 Omega increment/debit
cases, under source-oracle ASan/UBSan; water actors and 13 additional save-heavy
native owners; both classic CTests; 55 public contracts; 35 Python policy tests
with malformed event/receipt and exact LF/CRLF pin controls; scoped quality and
catalog validation. Native save tests reject 12 malformed weapon-state variants.
Catalog execution remains blocked by the known Linux child-supervisor
/proc/self/task/.../children limitation (child exit 125). Windows and hosted
qualification remain pending. No full-native sanitizer or complete visual parity
claim is made. Headless simulation intentionally does not synthesize render ticks
and cannot substitute for the rendered Omega qualification.
