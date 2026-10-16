# Chunk 0504 native and recovery consumer checkpoint, 2026-10-16

Diagnosis and plans only, serially under plan_general_diagnosis_resume_20261009.md

Reviewed 41 explicit contexts covering 2,623 overlapping lines, including three whole files: the shared transfer policy, shared initial-sync retry policy and RecoveryInstrumentation. Read the whole current BR-0662 assertion-coverage owner. These are source review boundaries, not executed test results

## Findings from the consumer trace

Both policy tests are built and registered in the shared CTest list. WAV decoding has a separate SDL-dependent registration. Paired host roots include the tests only when BUILD_TESTING is enabled and the target is not Android. There is no missing-registration claim for these targets; the matcen registration issue admitted in chunk 0503 remains separate

The production Android transfer pump uses the policy functions exercised by the fixture. It combines readiness, reliable-queue capacity, rollback recipient copies and wall-clock tokens before sending chunks. Receive application is deferred to a game-frame boundary and checks transfer phase and checksum. The reviewed ranges do not cover the entire packet parser or native restore path, so the synthetic policy fixture cannot establish those behaviors

Both games reset, arm and confirm the shared initial-sync retry state. Authenticated reconnect proofs can resend SYNC within the retry window. The reviewed PDATA paths validate shape, sender and world admission before confirmation. These observations preserve the paired implementation and Android guards without claiming a live network test

The retry test uses standard assert, unlike the always-enabled transfer CHECK harness and WAV's local NDEBUG removal. BR-0662 already owns optimized-build assertion loss. Its whole current definition and acceptance matrix were read; no duplicate finding is admitted here. Current compiler commands and the remaining owner reconciliation are still needed before a final disposition

The Windows native-suite runner sends Configuration to CTest, but its shared build helper accepts no Configuration argument and invokes the Windows build script's RelWithDebInfo default. The Linux branch instead passes its selected configuration directly and defaults to Debug. This distinction needs existing-owner/prior-plan reconciliation; it is not evidence that the current Linux default has disabled assertions

RecoveryInstrumentation invokes nine checks in its ordinary suite. Transfer cancellation uses real loopback TCP and files: replacement resumes at the verified offset, old sockets close, verified bytes survive, and obsolete callbacks stop. Phase cancellation waits for the actual writer mutex to retire. FINALIZING cancellation occurs at the phase-entry callback before archive import, so it is not evidence of cancellation midway through import

The obsolete WebSocket checks use a fake socket and injected callbacks against the actual listener. Production state-changing callbacks are guarded by current socket identity and manual-disconnect state. The probe handoff uses an injected blocking reader to check cancellation plus actual join before socket transfer, with production generation, connection and job checks. Proxy tests use real occupied UDP ports and a closed shared socket. Launch tests use reflected LAN state and a real SetupActivity with an intentionally missing target; they do not prove successful game startup

The suite policy names recovery instrumentation as an owner and network scenario. Native metadata worker tests are listed among caller-supplied fixtures. Full scheduling, provisioning and timeout behavior remain unreviewed. An absent literal reference to the named Python tests is not proof of missing registration because dynamic discovery has not been traced

## Evidence

Progress: temp/general_cleanup_20261006/gq2-consumer-ingress-0504-20261016.json SHA 09a2f08b3c38a3faacb11aa7850575abfb5e01e0fc67fcb2027debd8a719dcd3

Independent audit: temp/general_cleanup_20261006/gq2-consumer-ingress-audit-0504-20261016.json PASS 155 checks, SHA 332e313326b5a5406caae6bc9fd9d613bbf6ef82b7fa4273558e71af35a79318

The evidence artifact retains exact context bytes and hashes, the whole BR-0662 definition, detailed observations and read limits. Initial requests beyond the actual CMake/test_env EOF were rejected and corrected before publication. An inline audit wrapper interpolation error occurred before execution and was corrected; no product or canonical state changed

## Resume

Continue with music numerical/EQ/WAV consumers, native metadata worker lifecycle, the source-contract engine/build/discovery boundaries and the physical-route producer/projection/native effects. Then reconcile whole owners, raw quality rows and prior plans before deriving the direct unpooled disposition

Only the source gate is closed. Four later gates remain open and chunk 0504 is still TODO. Canonical queue stays 503 DONE / 124 TODO / 627 total. All thirteen assigned current sources and five canonical ledgers match the source checkpoint. No product/test/helper edits or execution, new finding, evidence-ledger append or Drive copy occurred. Broad queue, sweeps, investigations, final-head supplement and closure remain required; goal active
