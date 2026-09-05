# Investigate co-op restore exit after SDK update

- [x] Inspect supplied device log and reconstruct restore/exit sequence
- [x] Trace relevant code and identify supported cause or missing evidence
- [x] Record findings and next diagnostic or corrective action

## Findings

- Actual supplied file is Downloads/debuglog_20260904_153156.txt, 81 lines, host only
- User confirms both host and client returned to the launcher
- Internal build 21710, revision label 1be4aa25, arm64-v8a, built 2026-09-04 15:26
- Header enables only Guide-Bot; forced texture/load profiling records are also present
- Castaway D2 level 4 (u1bunker.rl2) finishes initial level initialization at 15:32:47.256 and runs for several seconds
- Second load starts at 15:32:53.107 and initialization finishes at 15:32:54.406
- At 15:32:54.411 the bot moves from its initial segment 716 to segment 287 with a restored path and goal, consistent with restored save state being used
- Log ends at 15:32:54.425 after automatic slowdown capture history; the final recorded frame took 1,381,247 microseconds
- android_profile.c uses reason=severe for slow-frame detection, not a fatal exception
- No save version, restore success/failure, disconnect reason, shutdown reason, or crash signature appears in this export
- state.c logs restore versions/completion under Game Logs; COOPLOG routes player remapping and transfer/sync diagnostics to Coop Desync, neither enabled here
- Code inspection also finds multi_restore_game ignores state_restore_all_sub's return value before marking restoration complete; this is a diagnostic lead, not an established cause of this incident

## Next evidence

- Reproduce with Game Logs, Coop Desync, Network, Launcher, and Dormancy enabled on both peers and export both logs
- Obtain the original failing co-op save and its originating build/version to reproduce prior-version restoration without modifying the user's original
- SDK causality and crash versus orderly exit cannot be established from this log alone
- No runtime code changed based on the incomplete evidence

## Follow-up paired logs

- debuglog_20260904_153156 (1).txt is host touch; debuglog_20260904_153155.txt is client Player68
- Host synchronizes the save transfer, then loads Players/save_sets/coop/castaway/coopsave.mg5, base save version 29
- Host at 15:39:12.168 and client at 15:39:12.576 both report unsupported or missing coop metadata after the base-save completion message
- Both player mappings fall back to spawning fresh because the metadata reader fails, even though Player68 appears in the saved player table
- Client explicitly reports Host save failed and transfer apply status=0; this is not an incomplete save transfer
- Both report jni startup main returned, followed by launcher onResume, establishing native main returned rather than merely losing the foreground activity
- Current coop metadata reader requires footer CFP6 and metadata version 6; commit 591d5dbb on September 1 changed version 5/CFP5 to version 6/CFP6 and expanded player records
- Old co-op metadata is therefore a strong explanation independent of the SDK change, but the original save is needed to distinguish old version, missing trailer, or malformed trailer
- The base save complete message is premature relative to co-op validation, and the host ignores restoration failure before reporting done
- Next step is to inspect the original coopsave.mg5 trailer, then address restoration failure handling and any explicitly requested old-save recovery

## Connected phone retrieval attempt

- Connected Samsung SM-S931U is authorized for ADB, but run-as rejects com.dxxredux.app as not debuggable
- Save Explorer in the installed build has load/delete actions but no save export action
- Pulled installed base APK only into android/temp/coop-restore-phone to inspect its public signing certificate
- Installed certificate SHA256 starts 60e8ad9b; local internal bundle signing certificate starts c4edcb36, so a locally signed debug update cannot replace this installation in place
- No save was pulled, no app update/uninstall or data reset was performed
- Retrieval requires a save-export feature delivered through the same Play signing channel (or another existing authorized private-data access mechanism)
