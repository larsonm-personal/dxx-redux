# Chunk0434 mouse-edge and extreme-stick acceptance continuation, 2026-10-14

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. No product/test/helper changes or runtime acceptance.

## Assigned source disposition

TouchMouseEdgeMovementTest is a new285-line helper/serialization suite. It specifies linear edge ramp, screen standoff versus touch-region width, origin travel gate, capped strength, editor direction labels and shared domain/settings round-trip. TouchStickExtremeActionTest's assigned three hunks only expand expressions for formatting. Preserve both suites and current behavior; no weakened assertion or semantic change is warranted from formatting alone.

Current extreme tests cover raw/human JSON, configured hysteresis, D2 afterburner display and upward-touch polarity. They do not exercise real pointer activation, delayed pulse ownership, mode change, native button counts or lifecycle release.

## Current helper and runtime boundaries

The shared edge helper derives edge width from the touch-region span, full-rate standoff from full-screen span, clips full-rate boundaries to the region and gates each direction on origin travel of half the edge width. Current drain runs on the main handler while mouse pointers are active, consumes bounded pending drag, adds edge contribution and only then applies inversion and final normalized clamp. Retain this spatial definition and its common editor/runtime parameters.

Editor visual bands clip their inner boundaries for drawing, while runtime computes ramp strength with the original edge width. Do not simplistically substitute clipped visual-band width into runtime during deduplication. Any small shared-geometry factoring must retain unclipped width, standoff and origin gating, with clipped drawing output distinct; do not add a generic control framework or claim inherited payoff for movement within new Android code.

Mouse begin records separate drag/edge origins and clears pending/history. Ordinary reset releases held extreme actions, clears pending/history/origins, emits zero axes and stops drain once no mouse pointers remain. Preserve those repairs. Real deactivation/layout replacement, stale callback retirement and native consumption remain to trace; one ordinary reset body is not complete lifecycle acceptance.

Extreme travel uses unbounded raw radius-normalized displacement and upward-positive Y before the ordinary stick circle clamp, deadzone/curve/sensitivity. Actual update uses that travel with configured inversion for threshold/release hysteresis. HOLD dispatches state changes; PULSE_ON_ENTER uses delayed release. Actual source tags distinguish stick/action indices, but complete generation and pointer ownership are not established by pure tests. Do not merge physical travel thresholds with clamped gameplay output.

## Existing cadence finding

BR-0464 remains exact: updateMouseAccelerationHistory normalizes dt to16ms but clamps windowCount to at least1, applying a full decay/grace increment for every1..16ms sample. Actual overlay samples SystemClock.uptimeMillis per delivered update and scales that event's delta immediately. The new edge tests neither change that code nor compare equal trajectories across event cadences. Preserve default/max/history policy while correcting time normalization in the existing owner; no duplicate finding or fresh probe.

Future acceptance should drive equal trajectories split across1/4/8/16/32/64ms, jitter, equal times, pauses and MotionEvent history, and compare integrated output/history with acceleration enabled/disabled. Separately exercise screen/touch-region clipping, edge-origin gating, left/right/up/down inversion, overlapping visual ramps and ordinary drag+edge clamping through actual view/drain.

## Remaining review

Read actual stored/human schema/domain and editor consumers, full pointer/lifecycle/mode/arbiter/pulse paths, controller/native handoff and relevant paired engine consumers. Reconcile whole existing owners and prior scope before direct rating/disposition. No new root, status change, inherited saving, build/test or device/provider/media/network execution. Chunk0434 remains TODO; canonical counts unchanged.
