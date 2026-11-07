# Original Sophodo input readiness, November 7, 2026

This is a bounded native original-resource result for the level-6 Sophodo
fixture. It does not qualify later level-7 gameplay or a connected human route.

The first optimized run on `55e67309066cead2f73a9c91541ae43bab8a7500`
passed the unchanged Majha, Metillis and Iquaxel fixtures. Sophodo stopped at
main step 126: the player had three kills, below the existing four-kill
requirement for guard 137. The same cold-earned level-6 entry and original
plan failed at the same step on parent #398
(`c0263f8eaead6f76f801f8f35906836acf21d3f7`), with the same recorded guard
health and path. This was inherited probe behavior, not evidence of a #407
gameplay regression. The initial `-O0` Majha timeout is retained separately;
the same-source optimized engine passed Majha under its unchanged budget.

The source plan had waited for door 43 to start opening (`state > 0`) before
repositioning to face guard 137. `d2/main/wall.h` defines `WALL_DOOR_OPEN=4`;
state 1 means the door is still opening. At the old step 125, the door remained
in state 1 and primary fire had 13,107 raw time units, about 0.2 seconds,
before it could fire again. Guard 137 fell from 210 to 5 shields, then moved
beyond the probe's nearby-robot snapshot while the kill count remained three.
Absence from that nearby list did not prove a kill.

A single controlled input-only run changed Sophodo JSON step 123 to wait for
door 43 state 4. It kept all 144 actions, the 3-second door wait, the 15-second
guard wait, the four-kill requirement, all other assertions and the existing
phase and CTest budgets. The plan-source SHA-256 was
`0dada4dd92cf2f5bd94144aa5da28eff64a0aab2d795a0e551c4ab9af0be6d2b`.
The run used the unchanged optimized 55e engine, SHA-256
`ab75f0a437b4f27253752b771f18f1f8a0c93e4226a96def6b0676bee18c38d9`.
Its input was the _cold-earned_ Iquaxel level-6 save, SHA-256
`8d2604c364b1b28c14ce137c8ef4b7b6fb5c9d3eff0a87f999fe9962da22f57e`,
bound to cold producer receipt SHA-256
`7b16c756b10da806e5269022049ba6082925b77e3a5cb6def13ab7b1c534cf5a`.
The separately preserved warm Iquaxel entry and its main receipt are different
inputs and were not used for this comparison.

At the revised approach, door 43 was fully open and primary fire was ready.
The ordinary guard fight increased the kill count to four. The complete CMake
owner passed: 144 main actions, the normal level-6 to level-7 transition,
the existing trigger and door checks, the authored 99-second countdown, and
the 11-action fresh-process cold escape. The main run recorded 49 scripted
view-aim adjustments; the cold escape recorded none. Neither run granted
health, ammunition or keys, disabled enemies, or forced a trigger or exit.
The warm level-7 destination save SHA-256 is
`1a47026ec2062342be083c1e50bdfdd285e244efe90fb29aa745febb5cd31a90`,
with main receipt SHA-256
`83212b7f59757faa5d6c8aa9550e2ce8add92b67fb6ab2ec1df41012613e5025`.
The separately produced cold destination save SHA-256 is
`69183152b08380b37266d3342296b8f668e38add183af71876ce122c6cc0d497`,
with cold receipt SHA-256
`9822c1461ba8e2b19368c20c3a9364c49c527a625836d24ed716248a7492f339`.

The read-only original-resource input contained 2,670 files, including the
original Pumo mission and owned models and the user-supplied XL camera addon.
Its complete private manifest SHA-256 is
`5a971e04170af58c001d393e1b9a5b50d0eac8a67b4f60170c25fb5f3e8545bd`.
The camera HOG was the documented `e7351d66...` original distribution variant,
not the `df69c62a...` whole-file variant. Retail D2 data was separately
verified. Raw resources, saves, plans and traces remain private. No APK,
device or user pilot was changed.

This establishes input readiness for the original Sophodo fixture at the
tested 55e engine. The combined child candidate, including later observation
changes, still requires its own exact-build rerun. It does not establish
unaided combat, rendered gameplay or earned progression beyond level 7.
