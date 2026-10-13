# Compact public shoot-switch replay

## Goal

Capture a D2 demo level-two replay from its normal spawn using only ordinary controls. Prove an initially locked passage is unlocked by a projectile hitting the real shoot switch, both linked wall records change, and the player traverses that passage

## Plan

1. Verify the public level-two switch candidate and its route using the production live recorder
2. Capture twice independently, pack each replay and oracle into at most 100000 combined bytes, and retain immutable own-engine frame, terminal, diagnostic and simulation RNG expectations
3. Extend action admission with switch-specific evidence and adversarial tests without weakening imported D1 parity boundaries
4. Run relevant native and complete compact corpus tests, obtain independent review, and publish a focused draft PR stacked after the pickup/combat work

## Constraints

No position or object injection, warps, cheats, embedded saves, compressed fixtures or sidecar evasion. D2-specific coverage does not require a fabricated D1 counterpart

## Verified implementation

- Ordinary controls reach the real switch at frame 175 and traverse segment 85 to 235 at frame 217 (218 frames total)
- The original forward wall is locked; its reverse side is originally unlocked. Both start closed and both enter opening state after activation
- The strict oracle initially detected an AI-local sound timestamp mismatch. The fixture omitted the desktop bitmap initialization step, so it calculated ambience without transparency flags. Calling the existing production startup helper corrected it without changing simulation or weakening evidence
- Full frame/state/diagnostic hashes, terminal result and 3191 SIM RNG events now match native playback
- Successful trigger activation is read-only trace evidence, coupled to exact switch overlay, lock, paired door and traversal outcomes

## Validation

- Final parent: `c8fb6ccda1fe107f39a32965434a70d70e344efa`, after the pickup/autoselect, combat and Linux hidden-DEX cleanup segments
- Two independent captures from the final stacked source produce identical raw recording, RNG, observations, capture report and compact pair; the admitted pair totals 38093 bytes
- Recapturing the old six scenes preserves every existing frame, terminal and SIM oracle
- Both Debug engines and capture fixtures build; relevant D1/D2 CTest cases pass
- All seven native scenarios pass twice with strict frame/diagnostic/terminal/SIM and outcome checks
- Mutation/admission tests, scoped mixed-language quality and automation catalog validation pass; independent review findings are fixed
- The local run-all catalog self-test is blocked by the sandbox's missing `/proc/self/task/<pid>/children` path; it is not bypassed
