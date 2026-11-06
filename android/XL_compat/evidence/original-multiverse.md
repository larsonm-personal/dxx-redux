# Original Multiverse entry and Crauphel escape, November 4 2026

The protected Pumo owner now tests the other authored hub branch. Select
`HubActivation` with `HubDestination Multiverse` and full original resource
arguments. `MultiverseEscape` extends it through originallevel2 (mvlev1.rl2,
Crauphel Scilab), a fresh-process pre-escape checkpoint and normal entry/save-load
in originallevel3 (mvlev2.rl2, Majha Mining Station). Default PMC behavior remains.
These modes reject missing full Pumo resources, Vertigo substitution and mixing
with missing-camera/briefing modes before device access. Existing owned-profile
backup, restoration and serial-device protections remain.

## Exact retained engine and original resources

Existing universal APK29471 only, package com.dxxredux.app,
source ec7715ffa5cd34865b701aa2152ee42f2a51b018, SHA256
`da070ebaacf95ea9be2f95fbd630ef5db1ccea0e24cf9108bbb6822c96a0661d`.
All six D1/D2 ARM64/ARMv7/x86_64 IDs:
`dxx-build:ec7715ffa5cd34865b701aa2152ee42f2a51b018:29471`.
No APK build or install. The original mission HOG SHA256 is
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`;
MN2 `45b4138e0a0d49ab8ee09334b7cfa97c0ec75ac6c6b6406b21eba28ddaa19cfd`;
VHAM `a623c7a909e6a7b546816f5082e57d33a19fc4b59f84bb3f8493df3ac86b6b05`.
Full original HAM/HXM/VHAM/ASE/OOF and supporting resources were staged. Actual
CameraHog SHA256:
`df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a`.
No original content was changed or distributed.

## Earned actions and receipts

Warm PASS78/78, run8518426c7d714f188b88595d9821f3da,94985ms.
Fresh-process PASS22/22, rundd910caf502a4d87887892b862b41648,29638ms.
Game PID13302 changed to14050. Protected before/after manifest SHA256:
`160d7bc5c74593e3db73d3ac46baed653d55096c34dd403cb87cafe3af5ef253`, identical.

The run covers:

- Normal hub44/side2 laser/thrust through boost1 and exit0 to level2, hub save
  restore and repeated entry; held/released normal firing after arrival with
  actual firing timestamps in the supported bounded range
- Closed reactor-access gate control before reactor destruction
- Red key absent at the exterior approach, normal thrust pickup, keyed-door
  flight and warm save/load with the earned key
- Normal laser damage to original reactor48 (300 initial shields), objective1->0
  and the Trainee90-second default countdown
- Pre-escape disk checkpoint, normal flight into boost0, continuous authored
  boost corridor, all17boost triggers and master1, fire controls19/20 and the
  explicit type25/time99 action; countdown becomes99seconds
- Ordinary exit18 to Level2Complete, fresh-process repeat, normal completion-menu
  advance/briefing to level3 and destination save/load

The warm endpoint is level2/segment418 at(-7.0534,218.4000,364.7830),
71.2188shields/96.0801energy, red present, objective0/countdown99. The cold endpoint
is level3/segment72 at(0,-31.2183,-532),100shields/100energy, default keys cleared
and countdown inactive. Both rendered endpoints were inspected.

Exact output `temp/xl-original-multiverse-escape` includes both results, state and
step traces/logs, multiverse-warm.jsonc, before/after manifests, cold-proof.json,
escape-complete.png and cold-render.png. Earlier admission-only40steps passed
as run2d6ce96ae2b346fe8d2e1aedb57df953; its firing check initially exercised only
the lower timestamp bound. The78-step route validates the corrected full range.

## Failed controls and source interpretation

Rune09133e8d869404696be2afe47a02459 failed48/78: wall collision pushed a fixed
point into the key before thrust. That is not earned-pickup proof. The test keeps
its absent-key control and now approaches from neighboring segment18/side4,
distance8, about18units from the key with10units corridor half-width.

Run22079249aa754f2ca5b83c00b87aba69 failed66/78 after actual key pickup, keyed-door
flight/save and reactor destruction. It incorrectly expected the map's base30
rather than the Trainee90-second default. `d2/main/cntrlcen.c` selects the default
reactor-time table by difficulty. Separately, type25 uses positive `time` as whole
seconds; `value` is inert, so this escape's `time99` means99seconds. Both strict
90/99 expectations passed after correcting the scenario. No engine patch,
mission edit, grant, trigger/exit dispatch, guard relaxation or timeout increase.
All failed controls restored their protected profiles exactly and remain failed.

## Boundaries

This qualifies bounded ordinary-player activation, combat, continuous local boost
flight and saves on the owned software emulator. Fixed local approaches remain
explicit. It does not qualify connected human traversal, all encounters, current-
head native behavior, phone controls/performance or the remaining original maps.
The mission has14levels. Continue level3 onward and both level4 exit branches,
including level14. Original mvend.rl2 has an automatic message with no authored
exit/reactor; verify its actual terminal behavior without inventing an exit.
UI import remains separate from engine staging. Retained full-original GuideBot
568-frame hub result is unsupported companion activation, not a campaign pass.
