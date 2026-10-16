# Chunk 0489 helper transport and process-pool checkpoint, 2026-10-15

Diagnosis only; serial review, no product/test/helper edits or execution. All five primary gates remain open

Receipt temp/general_cleanup_20261006/gq2-helper-transport-progress-0489-20261015.json SHA256 7f3650a17509e8bdadce603192468c32e3087274ebca4dccf468d537617705c7 captures five explicit contexts / 1212 lines. Read current test_helpers.ps1 L1-L980 continuously, whole125-line headless_process_pool.ps1, whole55-line process_lifetime.ps1, and test_target.ps1 L64-L91/L123-L146. Current helper L981-L3164 and needed frozen enclosing control flow remain unread, with no whole-helper credit

ADB adapters select serial/package and asynchronously consume both output streams. Their return type is text or null, without explicit native process status. Adb-Timeout tries tree termination, a five-second wait and stream closure on timeout; Adb-Dev-Timeout only tries direct Kill and returns null. Neither normal pipe GetResult nor outer polling deadlines prove a complete bounded lifetime. PowerShell argument assembly also differs from the headless pool's ArgumentList/quoted fallback

Health checks distinguish online/boot/package/shared-storage and app-private marker readiness. Software-emulator budgets apply to infrastructure defaults; nested queries can extend outer wall-clock polling limits. Physical target initialization defaults to the separate diagnostic package. The selected power-session helper verifies restoration of the original setting; its wider caller lifetime is not reviewed here

Wait-ProcessDead treats missing/unparseable PID output as absent. Stop-AppAndWait discards its Boolean. A force-stop request therefore does not prove acknowledged retirement, particularly when transport is unavailable. Preserve per-device targeting while reconciling this with existing liveness and cleanup owners

Reset preserves pilots, removes named test state, and marks ordinary graphics onboarding complete. Default active-set publication uses each game's same-directory temporary rename followed by exact path readback, then cleans local/device temporary files. This is separate per-game publication, not a multi-target atomic transaction. Standard-data presence checks require positive HOG sizes, not full content hashes

The headless pool bounds concurrent workers, reports start exceptions to completion with ExitCode-1, and gathers exit/timeout/output before disposing and removing the item. Thus both assigned runners reject ordinary start failure through their exit predicate. Their accepted exit0or2 and result-file presence do not by themselves validate native JSON content or sanitizer assertions

Process lifetime setup enrolls the Windows parent before children and routes Windows workers through a local supervisor; Linux uses a Python supervisor. The wrapper's stop helper waits after tree/direct fallback. The C#/Windows/Unix supervisor bodies are not yet reviewed, so do not infer complete descendant, inherited-pipe or forced-exit coverage from the wrapper alone

One combined output clipped helper L812-L980 due an unrelated ledger search. Recovered that range in isolation before recording complete L1-L980 credit; no ledger-search body credit claimed

Next continue current helper L981-L3164, then needed frozen enclosing logic; inspect actual supervisor implementation, LAN caller/staging/current-result identity and native config/route contracts. Reconcile whole existing findings, quality rows and prior plans before deriving direct priority. No new finding/remediation/status/rating/runtime acceptance, original saving, canonical append, queue completion or fresh Drive backup
