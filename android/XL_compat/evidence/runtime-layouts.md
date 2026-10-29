# Canonical modern runtime layouts

The normal singleplayer binary can translate canonical 25/40 and 27/40 levels
after the same independent feature/resource checks used for 23/40. This qualifies
the bounded layout path, not an original mission or campaign. Noncanonical side
geometry and deleted vertex slots remain rejected. Version-27 volumes selecting
authored fog also remain rejected until those colors/distances reach rendering.

## Translation and ownership

- Canonical explicit corners retain the classic topology, texture orientation,
  UV coordinates and compiled collision planes. Every game-section offset is
  rebased after removing the equipment descriptor and, for version 27, fog bytes
  from the intermediate native header
- Signed 16-bit producer/value fields remain in the source inventory. Native
  station links are rebuilt after XL initialization; inert wide values cannot
  alias a native producer. The same initialized assignments select equipment
  definitions and boss spawn resources
- Modern repair and equipment behavior applies to these audited layouts. The
  separate XL property, event, object and capacity checks remain mandatory
- Owned LGT values reach emitters. The complete embedded texture-color table
  takes precedence over CLR, matching the source. Vertex brightness follows
  explicit corner mappings, including inspected degenerate faces that still
  cannot enter the runtime
- New-layout identities retain explicit corners, wide extras, properties,
  damage and authored fog settings. Changing unused fog bytes invalidates a saved
  source identity. Generator-bearing sources also receive a corrected assignment
  identity so an older pending save cannot silently resume different contents

## Source evidence and correction

Pinned reference: arbruijn/d2x-xl
2576a98b5de310e3ac7c9777df2677d1627b8bae. The earlier
[layout evidence](modern-layouts.md) records the reader pins and adaptations.
The expanded comparison covers 60 layout/width/shape controls, including zero,
one, two and three active corners, and compares 480 vertex-brightness maxima.
All 14 original Pumo levels match the source readers: 19131 segments, 114786
sides, 1577494 field comparisons and 53253 vertex-brightness maxima. Their 3182
object boundaries/identities and 795 effect payloads also match. None of these
reader comparisons is gameplay acceptance.

The producer comparison previously began at AssignProducer and incorrectly
assumed serialized producer IDs survived initialization. The expanded extracted
CreateGenerator path failed before the correction. Source geometry loading first
renumbers each maker category; the combined source station list excludes reactors.
See [generator evidence](robot-generators.md#assignment-correction) for the 18
permutation cases, 24 mixed initialization cases and the corrected production
mapping. Raw original IDs and initialized row order now have separate fields in
the preserved candidate report.

## Native and platform controls

The derived fixture encodes the same Neptune geometry as 23/40, 25/40 and 27/40.
It adds a repair center, a protected room, deliberately wide inert fields,
conflicting owned lighting companions and unsupported geometry/fog controls.
Its four valid payloads match 32252 source-reader comparisons and 1148 vertex
brightness maxima. The original Neptune input remains unchanged:
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.

The real engine verifies all vertices, normals, topology, textures/UVLs, walls,
objects and trigger records; normal startup; actual repair/protection frames;
earned lighting; rejected-load preservation; disk restoration through stock;
and atomic rejection of a changed-fog identity. Separate 25/27 fixtures exercise
actual equipment and robot generation with pending disk restoration.

Eleven focused native/source tests pass, including the affected boss spawning,
transport and group controls. Five inventory/layout/initialization/source tests
also pass under MSVC and Linux GCC with -Wall -Wextra -Werror and undefined-behavior
checking. The normal Android app and instrumentation build successfully. The
five new runtime controls are required by the trusted original-asset runner;
all 12 runner-policy checks pass.

The normal Android APK passes all 87 registered steps: normal 23-to-25 and
25-to-27 transitions, exact geometry/resource checks, rejected admission,
earned light/energy changes, two detached captures and six real rewind restores,
disk saves through stock and resource cleanup. Protected app files have identical
before/after hashes. The controlled final pose draws textured geometry; this
activity witness is not a reference-image comparison or campaign playthrough.

Run ID 53d0a66c5cdd46f39e29ee6899a35235, elapsed 204046 ms, emulator-5554.
The initial test revisions omitted the multi-level starting-menu confirmation
and the worker drain needed during a paused capture. Both failed explicitly;
the corrected test uses the existing drain operation without advancing gameplay.

APK SHA256:
a17d223191f1879ec5b89602ae0a1be2918a53cc425b33f347b2fc388992e452

Screenshot SHA256:
ea66c0d3e6e118eccc696f0e81cfaba6b666350f2fe0967179f4777856cb572c

The same APK passes the corrected generator runner, 53/53 steps, run ID
b4d2f1e76a8e4479984b2318d97dc0e3, elapsed 127308 ms. It verifies actual
0/1/2 spawns, combat, pending disk state and six rewind restores. Equipment
passes 61/61 steps, run ID 271cd859ec9947c5917fefdbce262b56, elapsed 169103 ms,
including actual pickups/replenishment, mixed production, pending disk state and
nine rewind restores. Both restore protected files exactly. These reruns replace
the earlier generator-mapping expectations; original missions remain unqualified.

Both automation catalogs pass with 419 top-level entries and 475 support scripts.

Fixture HOG SHA256:
b17342693ffe4c3cd84485d32c682498b600254e13132d49a412e66c76631309

Descriptor SHA256:
b75b29439f4380affafecebcb9e7800837623750811c992d95f79014ca8fb49e

## Original requirements and next work

The refreshed [original inventory](modern-layouts.json) has 20 decoded levels,
three retained level failures and one rejected ambiguous container. Every
original still has independent blockers. Pumo loses its layout and incorrect
equipment-assignment gates; all 14 levels retain sound/effect requirements,
13 have particles, three lightning, 11 authored fog, and 12 unsupported segment
properties. Other remaining requirements include skybox/outdoor behavior,
effect activation, event actions, walls and custom resources.

Next implement authored visibility/environment behavior, then owned effect/audio
resources and their event consumers. Qualify their interactions, persistence and
transitions before admitting original levels. Full campaign playtesting remains
human work; no mission-name exception or autonomous traversal is involved.

Logs: temp/runtime-layout-*, temp/reference/modern-runtime-layouts,
temp/xl-runtime-layout-audit and temp/xl-runtime-layouts-android.
