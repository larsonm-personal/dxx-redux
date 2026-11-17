# Expanded readiness build margin

The repaired owner checkpoint's trusted run 43991081941 exhausted its probe
budget after 61 of 64 cases. Preserve that failure: neither retrying until green
nor changing case membership, frame counts or watchdogs repairs timing margin.
The next boss checkpoint's measurement job completed, showing that host timing
varies; its private behavioral owner is still running.

Evaluate a private C++ precompiled nlohmann/json.hpp header on the existing D2
headless route target only. This is a narrow native build integration change;
it changes no XL policy, compiler optimization level or measurement contract.
Do not generalize it to other targets without evidence.

First time a clean ordinary developer-OFF build without PCH, then a separate
clean build with PCH using the same MSVC toolchain, dependencies and two-worker
limit. Retain configure/build logs and elapsed times separately. Inspect include
ordering and warnings, compare original-case runtime output, and require Linux
CI compilation and unchanged readiness before accepting the change. Reject the
optimization if the benefit does not justify it or semantics differ. Windows
timings alone do not establish hosted Linux savings.

Keep scratch builds on E: because the prescribed cleanup found no eligible
workspace artifacts. Protect authentic inputs and prior failed receipts.

## Local experiment

Separate clean builds at parent 8f3a1241fd35d26642c924d7ee267eca728b0aed used
MSVC x86, Ninja, RelWithDebInfo, ordinary developer-OFF, the same retained
dependencies, and two workers. Baseline completed in 174.7511182 seconds; the
candidate with only the target-private PCH completed in 130.1118447 seconds.
Both exited zero. This single ordered pair saved 44.6392735 seconds (25.54%);
it is not a repeated statistical benchmark or proof of Linux hosted savings.
Compiler warning lines matched. CMake generated a C++-guarded include of only
nlohmann/json.hpp; the target's C translation units do not consume it.

Private logs, wrappers and timing receipts:
E:/dxx-redux-builds/xl-recovery-20261116/pch-benchmark-01.
Candidate executable SHA256:
fe6cbf01f0e23c761ee5b0111cf546059ba5d6e1c1bb24d5799950da6b7bfcc2.
The unchanged full survey in pch-64-01 exhausted its 360-second budget after
322.766 seconds: 51 passes, eight load failures, five untested, original 8/8.
The runner reserves the full 45-second case watchdog before starting each case.
This is an incomplete local measurement, not a new 56/64 result. The receipt's
parent SHA identifies the starting point, not an unmodified parent binary:
this executable includes the two-line CMake PCH patch. A separate baseline/PCH
comparison of the five untested cases is running in pch-tail-comparison-01 to
investigate runtime effects. It does not replace the exhausted bounded receipt.
The separate five-case diagnostic passed on both binaries: baseline 53.203
seconds, candidate 51.797 seconds, with identical load/tick JSON proofs for all
five. This found no runtime regression in those cases but does not complete the
bounded survey. Ninja duration sums also support a compilation benefit: target
C object compilation totaled 49.178 versus 50.085 seconds, while .cpp object
compilation totaled 212.219 versus 130.114 seconds (parallel task sums, not wall
time). Hosted verification remains pending.

The isolated CMake source patch SHA256 is
97a64e0b9b78fb02f61e1a7ffe904b6ef8a2fcebfaf9347c22a85e166e0dcd37.
