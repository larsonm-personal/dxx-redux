# Chunk 0504 audio and native-worker consumer checkpoint, 2026-10-16

Diagnosis and plans only, serially under plan_general_diagnosis_resume_20261009.md

Added 22 contexts and 2,017 overlapping lines. Cumulative coverage is 63 contexts / 4,640 overlapping lines / 12 whole file instances, six whole BR definitions and eight exact quality-definition rows. All thirteen assigned current sources and all five canonical ledgers still match the source checkpoint

## Audio conclusions

Read the complete spectral-report tool, preference helper, WAV decoder and shared music limits. The spectral fixture exercises alignment and comparison numerics with synthetic signals. The separate report path invokes the production host renderer, rejects an unexpected actual backend and binds reusable renders to renderer, font, source, PCM, revision and settings hashes. Stereo power averaging avoids antiphase cancellation; alignment uses rhythmic envelopes, local section checks and a drift gate. Only accepted pairs contribute to the aggregate. The tool permits individual failures and requires at least one accepted pair, so report success is not proof that every requested pair was measured

The reviewed render-comparison mode renders the synth tail to fill the observation window before writing and hashing PCM. Preserve that behavior. GQF-0268/GQR-0254 concerns a separate optional corpus reset/hash/seek path and remains OPEN/TODO

The installed-app EQ fixture reaches actual profile selection, native initialization and introspection through SetupActivity, MidiPreviewBridge and MusicEq. Its native EQ codes and preview state are useful selection/lifecycle assertions. They do not establish filter response, identical rendered PCM or device output. The preference helper treats a failed backup read as absent preferences, uses a fixed remote staging path and restores in sequential calls; existing state-restoration and observation owners need final reconciliation before any new finding

The WAV fixture calls the production SDL-based decoder. The decoder admits mono/stereo frames and rates, applies decoded/expansion limits, checks conversion allocation and publishes owned S16 PCM only after success. Production music loading uses the same WAV path after bounded reads and post-validates the result before accepting ownership. The tiny canonical WAV fixture is distinct from GQF-0165/GQR-0152's D2X-XL malformed-WAV parser regression. Preserve archived BR-0281 FIXED; no resource-pressure or malformed-media acceptance was executed here

## Worker conclusions

Read the complete host worker helper and line-oriented native main, plus the actual native runtime initialization, mount cleanup, failed-result and host adapter boundaries. Partial initialization poisons the process before engine state changes. A data-directory or content-game change requires retirement; successful initialization clears poison and permits reuse. Request mounts are cleaned on the explicit finish path, and failed cleanup poisons the worker

The PowerShell helper consumes restart-required results, retires the worker and retries one busy result in a fresh process. Its timeout is an idle watchdog: ordinary diagnostic output or matching changed checkpoint progress extends it. It is not a fixed total deadline. The helper does not validate result request_id/schema before returning the DXXMETA payload, and neither the native line reader nor this helper establishes a capped response size

Shutdown begins pipe draining before waiting, retains independently available stderr, records exit/drain state, disposes the process and reports failed retirement. These are source observations, not evidence that fresh processes terminated in this review. The native fixture is invoked by the sanitizer PublicRuntime path for both games and imported D1 profile switching. That invocation does not prove the Android service observer's generation, busy-retry deadline or terminal-publication contracts; whole current BR-0484 remains OPEN with its prior repairs preserved

## Ownership and discovery

Read whole BR-0536, BR-0588, BR-0484, BR-0276 and archived BR-0281 in addition to the prior whole BR-0662. Recovering the clipped BR-0484 trigger/impact completed its read. Preserve existing audio EOF-drain and observation acceptance; the EQ fixture has no spontaneous-EOF sentinel oracle

Read exact GQF/GQR definitions 0068/0055, 0158/0145, 0165/0152 and 0268/0254. Bounded-extractor registration already exists through the tooling workflow, explicit smoke list and its admitted-runtime child; no duplicate registration is proposed. The master runner discovers PowerShell and JSONC. The reviewed smoke and extraction CTest paths do not provide generic discovery of every Python test; the named source-contract and spectral coverage census is still pending

BR-0536's current evidence is about menu build policy. The observed Windows native build-versus-CTest configuration mismatch is related, but it still requires raw-quality and prior-plan reconciliation before selecting a root or continuation

## Evidence and continuation

Progress: temp/general_cleanup_20261006/gq2-audio-worker-progress-0504-20261016.json SHA 9ef49701ae90b243654c3e981834b0163844772cddc583b913243349fbbc9ce0

Independent audit: temp/general_cleanup_20261006/gq2-audio-worker-audit-0504-20261016.json PASS 245 checks, SHA d73275c5f6e9c92ac4234027e25a942d80a9868ad10d6cf40fcfd7019d1f1e82

Continue actual source-contract native/UI/build boundaries, route producer/projection/controlled physical effects, and complete suite provisioning/deadline coverage. Then finish whole-owner, raw-quality and prior-plan reconciliation before the direct unpooled disposition and publication

Only the source gate is closed. Four later gates remain open; chunk 0504 is TODO and the canonical queue is 503 DONE / 124 TODO / 627 total. No new finding, status/rating change, inherited saving, product/test/helper edit or execution, canonical evidence append or Drive copy occurred. Broader queue, sweeps, investigations, final-head supplement and closure remain required; goal active
