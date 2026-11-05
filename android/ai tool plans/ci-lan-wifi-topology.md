# Disposable CI LAN topology

## Evidence

Parent inspection of run 41946467586, job 126997140406, artifact 13383186166 at 845722a8b9d59c7a9b167dfcac34b45093b60c21 found 12 bidirectional probes with actual 100% ping loss. Host route to 10.0.2.17 selected wlan0/table1016/source10.0.2.16. Client route to 10.0.2.16 selected eth0/table1015/source10.0.2.15; its eth0 neighbor entry was FAILED. Wi-Fi addresses existed and the host wlan0 neighbor was REACHABLE. This proves an incorrect client peer route, not every cause of the earlier #335 UDP failure.

## Plan

- Configure only the disposable android-dual CI topology, before test execution
- Validate every target as an online emulator with qemu property, root shell capability, distinct Wi-Fi IPv4 address and the expected private Ethernet address before changing any target
- Bring eth0 down once using the existing emulator su convention, without changing adbd or application identity
- Keep bounded before/after receipts and require all unbound peer routes to select wlan0 with the local Wi-Fi source address
- Preserve existing connectivity preflight, timeouts and gameplay assertions; no retry-after-test-failure behavior
- Extend existing host-only tooling controls, run scoped quality and relevant catalogs, then publish a draft

## Limits

Local artifact downloads returned HTTP 403; detailed route evidence above was supplied by the parent. No emulator/SDK or /dev/kvm is available here. Host-only tests cannot establish Android routing convergence or gameplay success. The disposable VMs are terminated by the workflow cleanup; no physical-device or developer-emulator mutation is intended.

## Validation

- Five host-only topology tests pass, covering two/three peers, all-target admission before mutation, root/command failures, persistent wrong routes/sources, link remaining up and transient route withdrawal without repeating the mutation
- Scoped mixed-language quality passes with pinned Ruff and Prettier; existing fixture-test formatting was normalized by the required formatter
- Automation catalog passes: 99 standalone JSON tests, 535 support scripts, 315 standalone PowerShell tests
- Full master-catalog test remains blocked by missing /proc/self/task/<pid>/children in this runtime
- The broader nightly Android fixture suite has an unchanged baseline failure: its libpulse0 assertion expects apt-get immediately followed by install, while the workflow already includes acquisition options; the same test fails when executed from the HEAD version
- Android convergence, ICMP, raw UDP and gameplay remain unverified until hosted CI executes this setup and the unchanged owners
