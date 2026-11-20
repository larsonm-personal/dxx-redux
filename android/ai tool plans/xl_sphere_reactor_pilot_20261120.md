# Sphere reactor targeting and lethal-frame checks, 2026-11-20

The desktop pilot can aim at a live reactor by native object slot, reactor ID and signature. It uses the ship's ordinary steering and primary-fire controls. The existing center ray must be open and the target must be within 80 units. Tracking stops on identity or level change, loss of visibility, destruction or death. The ordinary robot case retains its hostile-robot checks.

Windows and Linux Debug each passed the registered reactor aiming control. The fixture positions a pilot once in segment 2956. After 90 simulation frames, native laser hits reduced reactor 433 from 300 shields to 246. Both owners rejected the wrong object, wrong signature and a distant reactor. They also reproduced ordinary robot combat and crossed the red door from 753 to 754 on a two-life save without a position assist. The original test-driver attempt selected the Pumo-specific entry point. Its failed logs were retained, and the registered control passed through the Sphere entry point.

A separate flight ended with −1.514343 shields before the death-sequence flag was set. The previous pilot emitted a passing checkpoint at that boundary. Both platforms now reject the identical flight and reject a new plan starting from that saved state. The failed controls preserve the native state and log digests. The deaths and positioning control do not qualify a campaign reactor kill.

The [receipt](../XL_compat/evidence/sphere-reactor-pilot-20261120.json) binds the private inputs, prior false pass, post-change controls and registered test results. The ordinary Android respawn and its cross-platform playback are recorded in the [respawn report](xl_sphere_ordinary_respawn_20261120.md).
