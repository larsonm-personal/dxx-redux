# Public replay headed and true-headless qualification

## Goal

Reuse the existing input-demo regression launchers for the compact public corpus.
Run native D1 and D2 ordinary level starts without an SDL video initialization,
window, OpenGL context or X server as the primary CI check. Keep the real visual
runner as the secondary fidelity/debug path, using Xvfb for cloud screenshots.
Both paths compare to the same unchanged live-capture oracles; neither replay
produces replacement expectations. Preserve bounded plain-text input/oracle
pairs, no saves or warps, and the existing imported D1 qualification limits.

## Steps

1. Extend the shared dedicated console main to native D1 and ordinary level
   starts through the existing replay load/start/frame functions. Build both
   engines and investigate any oracle divergence rather than masking it.
2. Add explicit console executable selection to the existing host replay runner.
   Route public corpus selection through the established headless/graphics demo
   regression entry points. Make headless selection fail closed rather than
   silently creating a no-present window. Keep imported D1 parity separate.
3. Run every compact scene twice per backend against unchanged exact frame,
   diagnostic, terminal and SIM-RNG hashes. Run the existing scoped imported
   comparison on the headed pass and preserve all unqualified differences.
4. Add real-engine negative controls for changed inputs and incorrect expected
   output through each backend and the full wrapper chain. Require runtime
   evidence and correct failure causes; admission/setup failure is not success.
   Derive coverage counts from the admitted corpus inventory.
5. Integrate parallel primary no-display and secondary Xvfb CI jobs, sharing
   hashed binaries and pinned assets from one build. Native-only negative controls
   explicitly report imported parity not run; retain the positive paired gate
   once. Register new tests and
   run catalog checks, focused tests, scoped formatting and independent review.
6. Publish a stacked draft above PR19 and monitor exact-head CI to completion.

## Boundaries

No new routes or oracle changes are part of this segment. Imported D1 flight
qualifies exact gameplay-state/SIM-RNG only. Imported pickup/door are diagnostic
probes; diagnostic/object/world and full simulation parity remain unqualified.
