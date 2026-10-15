# Proposed GQR-0304: assert native boss health across live difficulty and restore, 2026-10-14

Diagnosis-only proposed GQF-0318OPEN/GQR-0304TODO; not canonically admitted or implemented. Direct PRIMARY47MEDIUM H/M/B/C/R23/0/4/10/10. Scoped test-oracle gap, not an observed production health regression.

The named test promises untouched boss rescaling and full-health save/restore. Its frozen/current steps check difficultyhistory and inactive/undrawnHUD but never inspect actual boss shields. HUDactive tracks engagement identity, reset at fresh level load and restored only through actual boss/player collision or network notification. HiddenHUD therefore stays consistent with incorrect unengaged health. Earlier Obsidian diagnosis explicitly required numerical health preservation and hiddenHUD; current health-policy implementation is retained.

Implement in test owner with the existing native observation boundary. level_scene.robots already contains every live robot's object slot/id/position/shields without selecting a HUD boss. Add the minimum native read-only boss identity/signature/boss flag/rawshields/current maximum needed for exact comparisons and stable post-restore matching if existing float fields/fixture identity are insufficient. Reuse shared boss maximum policy; do not reproduce health formulas in Kotlin/PowerShell and do not activate HUD just to read it.

Require identified boss full-health on initialRookie, full-health at recalculatedTrainee maximum afterlivechange, validunchangedserialized/restoredTraineehealth, and retained difficultyhistory. Independently retain HUDinactive/undrawn throughout. Reject no-boss/ambiguous/wronglevel/wrongengine/wrongidentity observations rather than vacuous zero/empty-list success.

Future acceptance must exercise actual selected Obsidian level13 save/restore and deliberate controls: disable rescale, preserveoldRookiehealthonTrainee, forcehalfdefaultrestore, alterrestorehealthwhilestillhidden, wrongboss/no-boss/stalegeneration, and successfulsame-difficulty/full/damaged policy controls. Require every wrong-health case to fail even when HUD remains hidden, one correlated terminalresult, and native serialization/engagement behavior unchanged. Reuse shared native unit tests for formula/rounding; those do not replace integration checks. No controls or runtime tests executed in this diagnosis.

Distinct from BR0283wrongmissionfallback, BR0285typedcomparison, BR0233publication, GQF0297boss-previewdependencies/GQF0307delivered-audiofinitevalidator and GQF0317MetadataFLYOUTverdict. Existing JNI/thread/session/schema/staging/output owners retain separate acceptance. No inherited1996saving. H23 integrationverdictintegrity,M0 originalpayoff,B4 onefixture,C10 exactfrozen/currentproducer-intentchain,R10 localizedtest/read-onlyobservationrepair. No other severity pooled.


## Canonical admission handoff, 2026-10-14

GQF-0318OPEN/GQR-0304TODO canonically admitted under0443 exactnineoperationpublication andindependentproposed/publishedreplayPASS4545. This terminal handoff followsimmutable snapshot/Driveproof;theearlierproposedwording remains historical prepublicationtext. Implementation remainsTODO; no actualwronghealthrun/mutationexecuted. Direct PRIMARY47MEDIUM23/0/4/10/10 unchanged. See primary0443plan for report/snapshot/mutation/audit/mountedDrivereceipt and fullcompletionlimits.
