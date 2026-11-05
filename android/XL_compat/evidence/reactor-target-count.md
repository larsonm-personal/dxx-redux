# XL reactor target count

Unchanged Boiling Point XL level 7, `control2.rl2`, SHA-256
`cfdfdd6d46eb2a4e933478f9ae37ea5cd7533ec2f7e53e649221fb6fc67c30ef`,
contains one 42-byte reactor record at offset 839380. The signed count is 26;
all ten serialized segment slots and all ten side slots are zero.

Pinned reference arbruijn/d2x-xl commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`:

- `objects/reactor.cpp:561`, ReadReactorTriggers, reads the signed count and calls
  CTriggerTargets::Read
- `main/trigger.cpp:129`, Read, reads exactly ten segment and ten side shorts,
  then invokes Check
- `main/trigger.cpp:97`, Check, caps positive counts at MAX_TRIGGER_TARGETS (10)
  before visiting targets
- `objects/reactor.cpp:225` traverses the effective links in order

The modern 21/23/25/27 inspector now retains `reactor_authored_count`, caps the
runtime target count at ten and validates each effective target. Duplicates and
order are retained. The private native translation changes only the count short
before invoking the classic loader. Original bytes are never changed. Legacy
15/17 admission and classic loaders are unchanged. Negative counts, invalid
geometry, multiple reactor records and truncation still fail closed. Reference
compaction of invalid targets is deliberately not admitted by this correction.

The source oracle executes the pinned Read/Check functions, with diagnostics
suppressed to avoid unrelated trigger-array pointer subtraction. Both source and
header hashes are pinned. Boundary controls cover 0, 1, 9, 10, 11, 26 and 32767;
inspection, stock translation, malformed records, ordered duplicate slots and
native disk-through-stock restoration have separate controls.

This does not qualify Boiling Point for play. Its independent model132.oof
SPCL/RANI requirements remain blocked. RANI contains a nonzero rotation, not
merely inert metadata. No readiness cohort, deadlines or assertions change.

After corrected inspection the original still reports five runtime requirements:
object trigger 5/type 16 and its owner lifecycle, lightning owner 32, and equipment
row 1 powerups 48/49. These remain failures alongside the model requirements.

Validation of the exact original layout: 3389 segments, 20334 sides, 263457
comparisons and 7230 vertex-brightness maxima agree with the pinned source.
The pre-fix oracle fails on the same original's reactor count. All 41 standalone
and reference CTests pass with the correction.

Native countdown controls pass for 23/40 and 27/40. The latter uses an explicitly
labeled derived count-26 record, verifies ten duplicate slots after load, and
checks count/order after disk restoration through a classic level. Original
Neptune fixture input is hash-checked and unchanged.

The unchanged Boiling Point unassisted attempt exits 1 at `level_load` with
`OOF runtime does not support chunk SPCL`; it does not reach 120 ticks. The
original HOG is byte-identical after the attempt.

Scoped quality and automation catalog validation pass. The catalog invocation
test is environment-blocked (exit 125): its Linux child supervisor cannot read
`/proc/self/task/<pid>/children`. No supervisor checks or deadlines were changed.

Both `test_upstream_compat` and `test_classic_trigger_demo` pass on the final
combined native build. No classic loader or test assertion was changed.

## Hosted modern-witness gate correction

The private smoke modern witness is a different original distribution from the
readiness witness above. Its unchanged `boilpnt.hog` SHA-256 is
`5cde12e3188fca12b655de3be424871f61bd464e9318b0da6bcdbc65b79551f3`;
its version-27 `control2.rl2` SHA-256 is
`28a21f68d71a7c76e2ed63da036e07f3d7e8522c9437ea49f01ad5b38ee2693c`.
It also has authored reactor count 26 and ten effective `(0, 0)` target slots.
The previous smoke gate incorrectly required this corrected original to fail
with `invalid reactor trigger count`. The unchanged original reproduces that
stale-gate failure.

The corrected gate requires inspection-only, non-playable status, version 27/40,
3389 segments, exact authored/effective reactor values and a successful pinned
source comparison. All seven original outcomes remain mandatory, including the
other two explicit rejections. The receipt still counts the three historical
source witnesses by name: canyon, canyon2 and watrwrks. The fourth source
comparison, control2, is a mandatory supplemental assertion, not a change to
that historical cohort. Actual original replay returns `(7, 3)`; no trusted
controller schema/count change is required. This is inspection evidence, not
unassisted readiness or successful runtime loading.

Negative controls reject the old control2 outcome, wrong raw count, missing or
changed effective targets, boolean/non-list targets, playable status and missing
or unsuccessful control2 source comparisons, alongside the existing membership,
format and rejection controls.
