# Android publishing menu

- Add Play Store (default), GitHub release and both destinations to the existing
  `0_upload_to_test.ps1` wrapper
- Prompt for a valid release version before either workflow starts, preserving
  existing Play revision calculation and exact AAB deployment
- Keep existing parameter-only Play calls and BuildOnly behavior; add Action
  and ReleaseVersion for explicit unattended runs
- Exercise the real wrapper with mocked builders and remote services, covering
  default selection, release-only isolation, both ordering and failure handling
- Register the integration runner, run catalog checks and scoped code quality

Completed: all 17 isolated publishing-menu scenarios passed on PowerShell 5.1
and 7, preserving Play revision calculation and exact AAB selection. Existing
deployment contracts, both automation/catalog checks and scoped quality passed.
Tests used mocked builds and services; no real artifacts were published.
