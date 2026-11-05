# Disposable CI LAN topology

## Evidence

Parent inspection of run 41946467586, job 126997140406, artifact 13383186166 at 845722a8b9d59c7a9b167dfcac34b45093b60c21 found 12 bidirectional probes with actual 100% ping loss. Host route to 10.0.2.17 selected wlan0/table1016/source10.0.2.16. Client route to 10.0.2.16 selected eth0/table1015/source10.0.2.15; its eth0 neighbor entry was FAILED. Wi-Fi addresses existed and the host wlan0 neighbor was REACHABLE. This proves an incorrect client peer route, not every cause of the earlier #335 UDP failure.

## Plan

- Configure only the disposable android-dual CI topology, before test execution
- Validate every target as an online emulator with qemu property, root shell capability, distinct Wi-Fi IPv4 address, a verified eth0 interface, and either no Ethernet IPv4 address or the expected private Ethernet address before changing any target
- Bring eth0 down once using the existing emulator su convention, without changing adbd or application identity
- Keep bounded before/after receipts and require all unbound peer routes to select wlan0 with the local Wi-Fi source address
- Preserve existing connectivity preflight, timeouts and gameplay assertions; no retry-after-test-failure behavior
- Extend existing host-only tooling controls, run scoped quality and relevant catalogs, then publish a draft

## Limits

Local artifact downloads returned HTTP 403; detailed route evidence above was supplied by the parent. No emulator/SDK or /dev/kvm is available here. Host-only tests cannot establish Android routing convergence or gameplay success. The disposable VMs are terminated by the workflow cleanup; no physical-device or developer-emulator mutation is intended.

## Validation

- Nine host-only topology tests pass, covering two/three peers, all-target admission before mutation, root/command failures, persistent wrong routes/sources, link remaining up and transient route withdrawal without repeating the mutation
- Scoped mixed-language quality passes with pinned Ruff and Prettier; existing fixture-test formatting was normalized by the required formatter
- Automation catalog passes: 99 standalone JSON tests, 535 support scripts, 315 standalone PowerShell tests
- Full master-catalog test remains blocked by missing /proc/self/task/<pid>/children in this runtime
- The broader nightly Android fixture suite has an unchanged baseline failure: its libpulse0 assertion expects apt-get immediately followed by install, while the workflow already includes acquisition options; the same test fails when executed from the HEAD version
- Android convergence, ICMP, raw UDP and gameplay remain unverified until hosted CI executes this setup and the unchanged owners

## Hosted provisioning follow-up

Run 41957647848 exposed the observed Ethernet prefix 10.0.2.15/8; Ethernet now accepts /8 or /24 while Wi-Fi still requires /24. Run 41961380103 then returned successful empty IPv4 output for client eth0. The parent inspected artifact 13386936264 and confirmed all client commands returned zero with empty stderr. Admission now verifies the eth0 link independently and permits successful empty IPv4 output. Missing interfaces, command failures, unexpected addresses and invalid prefixes still fail before any mutation. Nine host-only controls cover addressed and unaddressed Ethernet, initially up or down, and strict post-mutation link and Wi-Fi route checks. Connectivity and gameplay checks remain unchanged.

Run 41964614444 failed before mutation because shell UID link inspection returned `request send failed: Permission denied`. Link inspection now uses the already-verified `su 0` shell, as does link mutation. Peer route inspection retains shell UID 2000 so the routing assertion is unchanged. The host fixture rejects unprivileged link inspection and covers privileged inspection failure both before mutation and during postcondition verification, preserving the command evidence and failed receipt.
