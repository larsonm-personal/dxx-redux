# D1 content and behavior in the D2 engine

This directory owns D1 compatibility policy and data. The parent `CMakeLists.txt`
lists these sources for the ordinary, headless, test and Android D2 targets

| Files | Responsibility |
| --- | --- |
| `d1_in_d2.c/.h` | Session identity, preparation/publication and profile transitions |
| `d1_in_d2_assets.*`, `d1_in_d2_bitmaps.*`, `d1_custom.*`, `d1_pig_validation.*` | Original D1 definitions, pixels, sounds, custom content and validation |
| `d1_in_d2_guidebot.c` | Private optional-resource importer and generation collaborator |
| `d1_in_d2_ai*`, `d1_in_d2_weapons.*`, `d1_in_d2_semantics.*` | Native robot/weapon operations and small gameplay rules |
| `d1_in_d2_levels.*` | Source references, triggers and campaign progression |
| `d1_in_d2_presentation.*`, `d1_in_d2_briefing.c`, `d1_in_d2_cockpit.*` | Original presentation and camera layout |
| `d1_save_translate.*`, `d1_in_d2_input_demo.*` | Native checkpoint and replay adapters |

Engine callers include the domain entry point explicitly, for example
`d1_in_d2/d1_in_d2_levels.h`. Files within this directory use local sibling
includes. Asset staging and AI internal headers stay private to their owners
and focused integration fixtures

Prefer complete source data, then small policy calculations, then a complete D1
operation where its algorithm differs. Original engine files retain dispatch
and neutral rendering/audio/geometry/object services. Do not move D1 decisions
back into their callers or create a compatibility counterpart for every engine
file. The session facade coordinates lifetime, not every frame operation

The [consolidation plan](../../../android/ai%20tool%20plans/asset%20management/d1-in-d2-consolidation-plan.md)
defines remaining work and acceptance. The [implementation ledger](../../../android/ai%20tool%20plans/asset%20management/d1-in-d2-implementation-ledger.md)
records evidence and its limits. Folder placement alone does not establish
fidelity or retire the remaining legacy overlay implementation
