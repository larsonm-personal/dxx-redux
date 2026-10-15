# Chunk 0410 Android documentation continuation, 2026-10-13

Diagnosis and future implementation only. Retain all four authored paths/32assigned hunks745review580added165deleted,current727lines/three post-frozen hunks. All four paths are absent from the1996reference;no engine-line removal,applied saving or quantified removal estimate.

## Preserve handwritten issue tracking and curate upstream scope

Do not edit outstanding_bugs.md or normalize its authored statuses,wording,paths or chronology. Its explicit no-edit instruction remains controlling. Keep this working issue history separate from executable acceptance and maintained release documentation. BR-0002 owns the upstream allowlist:decide inclusion of local development history and migrate any required design contract to maintained subsystem documentation before omitting historical plans. Source references to future work and old device observations do not prove implementation,regression coverage or release readiness.

Do not duplicate the bug list into another status system or create a new documentation framework. Existing review owners and their exact current status remain authoritative for this diagnosis. Updating source or future plan prose must not silently close a handwritten report or an older acceptance gate.

## Keep a small release guide linked to executable entry points

Retain branding versus package/namespace distinctions,the distribution/SDK table,the Play AAB plus direct APK workflow,the GitHub current/legacy pair,and the separation between build-output reuse and clean pushed source. Keep complete commands for the two principal workflows and link release-github.ps1 help for advanced asset recovery,custom notes and tag replacement behavior rather than copying that helper's full policy into README.

Retain current BuildMode selection before destination work,BuildOnly local behavior,UploadOnly verification without a build,isolated current/legacy outputs and task history,shared dependency caches,and packaged identity verification. Do not simplify away source-commit stability checks,signature/package/version/SDK checks,all-ABI/both-engine identity,matching pair identity or hash-checked retirement of the older release. Do not imply that choosing Clean can repair dirty source or that an old upload can be accepted from filenames alone.

Treat distribution_versions.conf andtool_versions.conf as source owners for their distinct settings;avoid another release constants inventory. Any future README reduction must keep enough package/signing/install coexistence information to choose the right artifact and protect existing app data. Source inspection here is not a completed build/signing/install/release run.

## Finish the existing privacy data-flow reconciliation

BR-0642 remains OPEN. Preserve the policy's repaired distinction among local files,manual log export,server-held multiplayer records,Google SDK activity,downloads and opt-in AcoustID. Do not restore the superseded categorical nothing-uploaded claim or location-permission description.

Make the sign-in flow explicit:the client obtains a short-lived Google authorization code and sends it with the callsign to the selected matchmaking operator;the server may exchange that code with Google and persist a stable player identity. Naming only the eventual player ID omits the authorization-code recipient. Keep Play versus SDK-free direct-install variants explicit,including provider startup and update-check behavior rather than treating unused app UI as proof that an SDK is idle.

Keep one reviewed source inventory tying destinations,field categories,purpose,release configuration and operator responsibility together. Identify how an operator states retention and provides access/deletion handling;do not promise global deletion from uninstalling or a client disconnect. Match store disclosures and first-use text to the actual selected release,with future request/database/log inspection before acceptance. Current source ranges do not prove every network request,SDK internal behavior or deployed operator policy.

Retain manual export warnings about device details,paths,callsigns and IP addresses. Explicit ACTION_SEND/ClipData read grants andowned export snapshots make sharing user initiated;another application's handling is outside local cleanup. QR source uses local decoding with barcode-image output disabled and can save the resulting LAN address in recents;preserve that distinction. Backup rules exclude client_identity.xml;do not broaden that statement to all local settings or transferred game data.

## Refresh sound-trace coverage without weakening its limits

BR-0006 owns documentation currency. Replace SOUND_TRACE.md's blanket claim that Mac/custom paths lack individual load snapshots with the actual covered paths:retail Mac resource-bank loading,legacy RAW registration andmod PCM/DXA overlay refresh now call the load-snapshot hook. Retain an explicit remaining limitation for compatibility paths such as already-prepared D1-in-D2 custom generation samples that bypass the overlay-refresh loop. A new concise coverage table or short paragraph is enough;do not assert complete provenance across every replacement path.

Keep load/current-input/conversion-input/conversion-output comparisons separate. Preserve FNV-1a plus length as bounded non-cryptographic evidence,-1 as unknown,read_ok=0 as unknown load baseline,and the warning that matching bytes do not prove final mixed speaker audio. Mapping candidates are not object attribution. Keep the exact-bank stock reference qualified to its fingerprint rather than every D2 release.

State the current report budget precisely from the implementation and preserve logging/reset boundaries. The native fingerprint fixture and registered host runner cover utility semantics;the owned stock-D2 emulator smoke script resets mods/default state and checks trace/gain patterns. Keep manual reproduction collection distinct from that destructive test setup and retain the instruction not to run it on the reproduction phone. No tests were run in this diagnosis.

## Ownership, validation and disposition

Primary prior GQ1-0631 BR-0642 REFERENCE56 MEDIUM-HIGH/components32/0/7/10/7 is retained unpooled. BR-0002 andBR-0006 support their existing upstream-scope and operational-currency work;do not add their scores. No new root,remediation,numbered investigation,status,rating or acceptance mutation.

Future validation should follow the documented commands and inspect actual artifacts for both release editions,trace configured/unused/opted-in/failure network flows and operator database/log outcomes,and exercise covered and unknown sound baselines while preserving the native fixture and stock-smoke boundaries. These are future checks,not fresh acceptance.

NO_INHERITED_EFFECT/allfour original-absent/zero applied saving. Parent409 through the explicit405recovery lineage remains qualified. Missing older recursive ancestry/global fingerprint TSV and broader queue/preflights/sweeps/investigations/finalHEAD/dirty-untracked/closure remain pending. Canonical publication and independent byte replay are required before0410 terminal credit is applied to this chunk.
