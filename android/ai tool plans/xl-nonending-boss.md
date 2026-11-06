# Sphere non-ending boss qualification

Qualify only the original Sphere HXM signed boss byte -1, against pinned
objects/robot.cpp and ai/aiframe.cpp. Preserve the authored signed byte, source
identity and classic behavior. Decode its effective profile as 1 while retaining
that it does not end the level. Existing positive profiles 21..28 remain intact;
all other currently unsupported profiles remain rejected.

Audit AI, collision, spawning, death, objective and save consumers. Run source
oracle controls and actual HXM load/combat/death-roll/disk-restore controls,
including a non-ending boss with no other remaining objectives. Reproduce the
unchanged Sphere level with genuine WAVs and original Vertigo dependencies.
No admission until those controls pass; report downstream blockers honestly.

## Evidence and scope

Pinned reference revision: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
`objects/robot.cpp` reads the signed boss byte, records `bEndsLevel = raw > 0`,
and negates negative roles. `ai/aiframe.cpp` profile 1 in the D2 property table
teleports, does not spawn on teleport and has no special weapon immunity.
`ai/aiboss.cpp::DoBossDyingFrame` invokes reactor completion only for
`bEndsLevel`. The new source oracle executes those original reader statements
and the original death-completion function, and checks all 256 signed inputs.
Only -1 and the existing 21..28 roles are admitted. Pins follow the repository's
existing CRLF reference setup and match its existing camera/disarm pins.

Keep raw -1 in the owned HXM definition and save identity. The existing generic
XL boss AI already handles this nonzero role; collision deliberately avoids the
D2-only immunity path for effective profile 1. Spawn selection maps -1 to 1.
Required objectives continue to count only positive authored roles. Optional
boss death retires its owner and plays the existing death effects, without
calling reactor completion. Dynamic/contained bosses and other signed profiles
remain unsupported. Classic loading and behavior are unchanged.

Runtime controls use the pinned ZIP release of Apocalyptic Factor XL, HOG
SHA-256 `f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db`.
Its distinct 7z release is rejected by the existing identity gate. Derived HXM
controls change only the boss role and retain all existing introduction/removal
controls. They verify ordinary laser and Vulcan damage, cloak activation,
lethal combat, death-roll save/restore through a stock level, signed identity,
and no countdown even when no other required objective remains. The same
non-ending fixture fails before this fix and completes 120 unassisted ticks
afterward. Existing 50 spawning/RNG/timing/collision comparisons, both reactor
orders, malformed save controls and classic isolation also pass.

## Unchanged Sphere reproduction

Private assets revision: `b5f68b8f433e0fc170306a168534304ae64f7720`.
All 22 XL resources match the manifest. Add the existing qualified original
Vertigo archive, SHA-256
`97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6`, to
supply its HXM robot slots. Original `sp-02.rl2` remains SHA-256
`a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b`;
`sp-02.hxm` remains
`78686e51eaf9849e8cd9bb798920ce7d6eec81249a92c4f5ca40560383d128d9`.
Object 239 uses robot 17; its HXM boss byte at robot-record offset 280 is 255,
interpreted as signed -1. No mission or asset bytes were changed.

The standalone headless engine now clears boss admission and stops honestly at
`XL fog requires an active OpenGL renderer`. A separate integration build used
only the diagnostic desktop entrypoint from draft #384 at
`f792436c710f3ee2cd7a17a5ef9a0ea06b2ea213`; that instrumentation is not in this
patch. With Xvfb/llvmpipe it clears the renderer requirement and stops at
`object 243: robot joint references missing submodel`. This is robot 18, whose
HXM record uses model 29 and nonempty joint ranges 360..379. This downstream
animation/dependency issue is not bypassed. An initial Xvfb startup attempt
failed before admission; the explicit 640x480x24 no-reset display initialized
correctly. Neither original run is a load/120-tick pass, and official readiness
was not changed.

54 public XL contracts/source comparisons and scoped quality pass. Catalog
validation passes; catalog execution is blocked by the environment's unavailable
`/proc/self/task/.../children` interface (exit 125). Hosted validation and Windows
runtime qualification remain pending.
