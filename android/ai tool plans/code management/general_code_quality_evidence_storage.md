# General code quality evidence storage

The full general_code_quality_evidence_ledger_20260811.md is a local working file, excluded from Git. Existing ledger paths and audit references remain valid locally.

Google Drive backup:

`G:\My Drive\descent android port\audit ledger\general_code_quality_evidence_ledger_20260811.md`

A sibling .sha256 file records the backup checksum. The first verified backup contains 280762805 bytes with SHA-256 6352d7b324779db25e0a562c2b871a5b6f75cec3431ba1f72b8d4d9974f5f349. This identifies the initial backup, not all future revisions.

Windows task DxxReduxEvidenceLedgerBackup runs android/helpers/backup-quality-evidence-ledger.ps1 every 15 minutes while the configured user is signed in. Google Drive for desktop uploads the resulting copy; successful local copying does not prove cloud upload completion. Check Drive sync status before relying on remote availability.

Run an immediate backup after a completed audit publication:

```powershell
pwsh -NoProfile -File android/helpers/backup-quality-evidence-ledger.ps1
```

The helper skips an identical verified backup. It verifies both the copied bytes and unchanged source before replacing the previous backup. If Drive is unavailable or the ledger changes during copying, it fails without replacing the previous backup. A stale checksum after an interrupted publication is repaired on the next run. Task Scheduler records failures; it does not display a notification.

On another machine, copy the Drive ledger to android/ai tool plans/code management/general_code_quality_evidence_ledger_20260811.md, then compare its SHA-256 with the Drive sidecar before resuming audit work. Run the helper with -DestinationDirectory for a different local Drive path. The Windows task is machine-local and must be registered separately on another machine.

Remove the task with:

```powershell
Unregister-ScheduledTask -TaskName DxxReduxEvidenceLedgerBackup -Confirm:$false
```

Historical Git copies are retained. The unpushed commit that introduced the LFS pointer must be amended to remove that pointer before pushing; a later deletion commit alone can still leave an LFS upload requirement in outgoing history.
