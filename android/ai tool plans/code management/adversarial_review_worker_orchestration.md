# Adversarial Review Worker Orchestration

## Purpose

Keep the root conversation as the campaign orchestrator while assigning expensive
review reasoning to a reusable `gpt-5.6-sol` worker at medium effort. Reusing one
worker for two or three consecutive chunks preserves nearby context without
copying the growing campaign history into every new worker.

This document supplements
`branch_adversarial_review_process.md`. The process remains authoritative,
including its one-queue-item-per-review-call rule and single-ledger-writer rule.

## Plan

- [x] Define root and worker responsibilities
- [x] Define the two-to-three-chunk worker lifecycle
- [x] Define a minimal delegation packet and result contract
- [x] Complete the first delegated chunk using this protocol
- [x] Complete the 30-chunk orchestrated batch from `R1-CHUNK-0075`
  through `R1-CHUNK-0104`
- [x] Reuse each worker for two or three chunks, then rotate it
- [x] Audit all 30 completion records and the next eligible queue item

First trial: `R1-CHUNK-0074` completed on 2026-07-26 using a fresh
`gpt-5.6-sol` worker at medium reasoning effort. The worker changed only the
active ledger, recorded `BR-0024` and `BR-0158`, and identified
`R1-CHUNK-0075` as the next eligible item.

Current batch: the root orchestrator is assigned 30 consecutive calls beginning
with `R1-CHUNK-0075`. If no item is blocked, the inclusive target range is
`R1-CHUNK-0075` through `R1-CHUNK-0104`. Progress remains canonical in the
active ledger; this plan is updated at the 10, 20, and 30 item checkpoints.

Checkpoint 1: 10 of 30 calls completed through `R1-CHUNK-0084` on 2026-07-26.
Four medium-effort sol worker lifecycles were used, with no concurrent ledger
writers and no product changes made by review workers. The next item is
`R1-CHUNK-0085`.

Checkpoint 2: 20 of 30 calls completed through `R1-CHUNK-0094` on 2026-07-26.
Seven medium-effort sol worker lifecycles were used. Every call completed one
item, all per-call diff checks passed, and concurrent test-runner work remained
outside the review workers' scope. The next item is `R1-CHUNK-0095`.

Checkpoint 3: all 30 calls completed through `R1-CHUNK-0104` on 2026-07-26.
Eleven medium-effort sol worker lifecycles were used across the batch. The audit
found exactly 30 `DONE` rows and 30 completion notes, no `ACTIVE` or `BLOCKED`
queue row, no duplicate finding heading, and no review-worker product edit. The
next item is `R1-CHUNK-0105`.

## Roles

### Root orchestrator

The root thread:

1. Reads repository instructions and the review process
2. Checks the ledger for an `ACTIVE` item, otherwise identifies the first
   eligible `TODO` item
3. Ensures no other review or remediation worker is editing either ledger
4. Spawns or reuses exactly one review worker
5. Verifies the worker changed only the permitted ledger and completed exactly
   one queue item
6. Keeps only a compact handoff summary: item ID, result IDs, validation, and
   next item
7. Retains the worker for the next request until it has handled two or three
   chunks, then replaces it with a fresh worker

The root thread does not independently load and re-review the assigned source
scope. Its job is ordering, isolation, verification, and durable campaign state.

### Review worker

The worker:

1. Uses model `gpt-5.6-sol` with medium reasoning effort
2. Reads the canonical repository instructions, process, and both ledgers from
   the workspace instead of receiving their contents in its prompt
3. Performs exactly one queue item per turn
4. Reviews only the frozen snapshot and makes no product-code changes
5. Is the only ledger writer during its turn
6. Stops after recording findings or explicit no-finding evidence, a completion
   note, and the final queue state
7. Reports a compact result to the root thread

## Worker lifecycle

Use one worker for two or three consecutive chunks:

1. Spawn it with no inherited conversation turns:
   `fork_turns: "none"`, `model: "gpt-5.6-sol"`, and
   `reasoning_effort: "medium"`
2. Give it one named queue item for its first turn
3. After completion, have the root verify the ledger-only edit
4. On the next user request, reuse the same worker with `followup_task` and one
   newly named queue item
5. Reuse it once more only when the third chunk is adjacent and the prior turns
   stayed clear and correctly scoped
6. Retire it after two or three completed chunks and spawn a fresh worker for
   the next group

Rotate earlier when a worker leaves confusing state, encounters a policy or
tool restriction, crosses from source chunks into batches or sweeps, or needs a
different task type such as finding verification or remediation.

Each worker turn remains one review call. The two-to-three-chunk policy means
worker reuse across turns, never combining multiple queue items in one turn.

## Context budget

Keep delegation prompts referential rather than descriptive:

- Name the queue item and canonical files
- Do not paste the process, ledger, prior findings, diffs, or source
- Do not fork the root transcript into a new worker
- Let the worker read only the assigned scope and the context required by the
  review process
- Return only the item ID, result IDs, completion status, validation summary,
  and next eligible item

This makes each worker more expensive in reasoning quality while bounding
repeated prompt context. The ledger, not the conversation, remains the durable
memory.

## Single-writer and task boundaries

- Never run two review workers concurrently
- Never run review and remediation workers concurrently when either may edit a
  ledger
- Root may perform read-only ledger and Git checks while a worker runs, but
  should avoid writing shared files until the worker stops
- A review worker may edit only
  `branch_adversarial_review_ledger.md`
- Verification and remediation use separate workers or lifecycles and their
  specific prompts from the review process

## Restriction and interruption recovery

If a model restriction or cybersecurity flag blocks a chunk:

1. Leave or restore an auditable queue state
2. Record the exact restriction and unreviewed scope in the ledger
3. Do not treat partial inspection as coverage
4. Continue only according to user direction and the review process
5. When retried successfully, preserve the prior restriction record and mark it
   superseded rather than deleting its history

If a worker stops after claiming an item, the next turn resumes the `ACTIVE`
item and records recovery from the interrupted claim.

## Reusable initial worker prompt

```text
Review exactly <ITEM-ID> as the sole ledger writer.

Read AGENTS.md, .github/copilot-instructions.md,
android/ai tool plans/code management/branch_adversarial_review_process.md,
android/ai tool plans/code management/branch_adversarial_review_ledger.md, and
android/ai tool plans/code management/branch_adversarial_review_ledger.done.md
from the workspace. Follow the process exactly. Confirm the frozen commits,
claim or resume only <ITEM-ID>, inspect the frozen snapshot and required
context, search both ledgers for duplicate findings and used IDs, and edit only
the active ledger. Do not modify product code. Append the completion note,
finish the queue row, and stop after this one item.

Report the item status, result IDs or none, commands or validation, whether only
the active ledger changed during your turn, and the next eligible queue item.
```

For the second or third turn, send the same prompt through `followup_task` with
the new item ID. Do not attach the earlier transcript or restate earlier review
details.

## Root verification checklist

After every worker turn, the root checks:

- The named row is `DONE`, `BLOCKED`, or remains audibly `ACTIVE`
- Exactly one queue item was processed
- Completion evidence and every new finding use the required templates
- Both ledgers were searched before any new ID was assigned
- No product file changed during the review call
- The next eligible item is unambiguous
- The worker has completed no more than three chunks in its current lifecycle
