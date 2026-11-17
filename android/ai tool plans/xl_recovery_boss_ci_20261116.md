# Boss group CI investigation

Trusted run 43992568832 at candidate 8f3a1241fd35d26642c924d7ee267eca728b0aed
failed xl_boss_groups. Its sanitized host receipt confirms test execution but
does not expose the private CTest diagnostic. The public linked Linux boss
consumer passed; it does not execute this native group fixture and source
sample comparison. The successful Windows Debug group run emitted both required
witnesses and matched all 350 source samples in 33.271 seconds.

Investigate the actual build differences before changing behavior or limits:

1. Rebuild the native developer owner with RelWithDebInfo, matching the hosted
   owner's optimization mode, and run the unchanged boss_groups.cmake wrapper.
2. Run the source reference optimized and with a second compiler against the
   retained 350 Windows samples. Keep source, native and reference failures
   distinct; a Windows result cannot establish Linux behavior.
3. If no local reproduction is found, obtain a narrow, safe diagnostic or
   equivalent Linux execution. Do not guess that the 80-second child timeout
   caused the failure, raise that limit, remove scenarios or reinterpret a
   missing result as a pass.
4. Repair the demonstrated cause, rerun the full native group owner and source
   comparison, and publish a coherent checkpoint for fresh exact-head CI.

Private evidence root:
E:/dxx-redux-builds/xl-recovery-20261116/boss-linux-repro-01.
The optimized MSVC x64 source reference matched the retained 350 samples.
An initial Clang configure used mixed compiler families and was rejected by
CMake; both C and C++ are now explicitly selected as Clang, with the failed
configure logs retained. No engine behavior changed during this investigation.

## Reproduction and repair

The optimized native owner passed the full wrapper in 6.460 seconds with the
78-definition provisioned runtime. The same binary and wrapper reproduced the
CI failure with game_data/retail-ci/d2: the native controls passed, but the
reference exhausted its RNG transcript. That runtime has 66 definitions.
The fixture mutates loaded bosses to role -28 after admission; its natural
spawn table includes Vertigo IDs 72 and 73. These deliberately absent inputs
are rejected by the native definition guard after the selection RNG draw.

The adapter incorrectly treated its entire 200-slot storage capacity as loaded
definitions. Do not call the extracted Release lookup on an unloaded slot:
include/descent.h has an unchecked pointer lookup in Release, while the Debug
lookup in main/gamedata.cpp checks the loaded count. Their audited SHA256 values
are f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4 and
782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12.
No equivalence to an uninitialized source definition is claimed.

The repair bounds adapter storage by loaded definitions and explicitly checks
absent default selections as rejection controls: no spawn, unchanged cooldown,
unchanged robot counters and exactly the selection RNG draw. All 350 scenarios
remain. Retail-only results now report 345 source comparisons plus five
missing-definition rejection controls; the provisioned bank still reports 350
source comparisons. No production admission or runtime behavior changed.

Validation: the complete RelWithDebInfo native wrapper passed with the actual
retail-only bank after repair. Optimized MSVC x64 and Clang 22 references passed
the retail-only samples; MSVC also passed all 350 provisioned samples. Separate
mutations of a missing-definition sample's cooldown and robot counter both
failed with the expected guard diagnostic. Scoped formatting passed. Clang 18
was rejected by the installed MSVC STL version requirement; Clang 22 passed
without disabling that requirement. Hosted repair qualification is pending.
