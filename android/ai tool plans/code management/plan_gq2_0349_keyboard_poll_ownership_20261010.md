# GQ2 chunk0349: keyboard polling request ownership, 2026-10-10

Diagnosis and implementation plan only. Proposed GQF-0288/GQR-0274; canonical admission pending. No product/test/helper edits or runtime reproduction

## Evidence and scope

MainActivity.kt4725-4790 starts a fresh recursive height poll on every showKeyboard call and records only the most recently posted Runnable. Paired d1/main/newmenu.c1333-1348 and d2/main/newmenu.c1323-1338 call show on a tap of an active input field without testing whether a show is already requested. The draw paths also request show when native keyboard_requested is false.

With height unavailable, showA posts callback A. A later showB posts B and overwrites keyboardPollRunnable. Hide or suspend removes B and clears the field, leaving A queued. A tests only attemptsLeft; it can reschedule after hide/suspend, or publish an old positive IME height while hide animation still reports occlusion. No request generation, active-keyboard or suspended-state gate rejects A. The chain is bounded to20 attempts, so there is no unbounded-loop claim. Repeating requests can also leave more than one bounded chain.

Base3543-3608, frozen4588-4653 and current4725-4790 are line-equal: SHA256 2de843255cb9a76d151e46d6db831f7fc45b8ef809c3d8c2f49605565c0b6b70. This is an existing enclosing-source defect, not a regression introduced by assigned hunks164-202. Current source identity and exact ranges are in the0349 source and consumer checkpoints.

## Ownership and priority

P3, high source confidence; impact priority33 LOW (H/M/B/C/R=12/0/4/10/7). Bounded stale keyboard geometry and unnecessary retained UI work justify local cleanup, without attributing a crash, indefinite retention or reproduced visual failure.

This is distinct from BR0029's native geometry races, BR0044's JNI resources/exceptions, BR0078's engine/Activity admission, BR0465's composition semantics and BR0492's held-controller handoff. BR0340 describes independent overlay loops missing stop/detach handling; this path already tries to cancel on hide/suspend but loses older handles when a request is superseded. Its fix belongs in the actual keyboard request owner, not a generic network-overlay abstraction.

## Implementation

- [ ] Give the current keyboard show request an identity and one scheduled height-poll runnable owned by MainActivity
- [ ] Invalidate and cancel the previous request before starting another show; reject stale callbacks before sampling, publishing or reposting
- [ ] Cancel on hide, suspend and destruction, and gate delayed work against the current owner, active keyboard and Activity state
- [ ] Preserve current input type, text, focus and normal/TV geometry fallback behavior; coordinate native geometry publication with the existing BR0029 boundary
- [ ] Validate the actual Activity polling path and paired game callbacks, then run scoped quality, relevant Kotlin/native builds and meaningful D1/D2 integration during the implementation tranche

Avoid a general timer framework. One existing-owner cancel/start path plus request identity is sufficient; cancellation must still be correct if a runnable was already dequeued or reentrant show/hide changes ownership. Do not add launcher schema migrations or move native menu format knowledge to Kotlin.

## Required acceptance

Use controllable scheduling around the production callback path. Queue showA and showB while height is zero, hide or suspend while A is pending, then execute every queued callback. Assert no stale native height publication or repost, one live request at most, and no retained old Activity work. Repeat with height becoming positive during hide animation, rapid field changes, show after hide, detach/destroy/recreate, exhausted attempts and request replacement during sampling. Fresh showC must still publish correct geometry and stop when height is available.

Keep native front-menu selection and tap-triggered show as real D1/D2 integration triggers. Cover ordinary and TV keyboards, touch and controller activation, return from background and native closure. Pair with existing composing-text and held-input acceptance without claiming those roots fixed. The existing input-type unit tests and D2 manual navigation script do not exercise this callback schedule.


## Canonical admission

GQF-0288 OPEN/GQR-0274 TODO admitted by audited0349. Priority33 LOW(12/0/4/10/7), P3/high source confidence. Publication audit PASS2921 SHA9958eed949b9fabdce4bc4cfc05f677536ffce2ed5ba0832d192f41c813e2c19. Implementation and runtime acceptance remain pending; no code edited
