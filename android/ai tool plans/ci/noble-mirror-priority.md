# Noble hosted CI mirror priority

- Confirm the hosted runner mirror list and affected host/Android provisioning paths
- Swap only the known Azure/archive priorities on disposable GitHub-hosted Noble x64 runners
- Preserve sources, signing configuration, pinned packages, strict errors and timeouts
- Test idempotence, unexpected configuration rejection and trusted-controller integration
- Validate locally, open a draft PR on a new branch and inspect CI for its exact head
