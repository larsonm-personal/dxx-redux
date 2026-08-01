# Luna max adversarial review trial

## Status

Complete. The callable model ID is `gpt-5.6-luna` with
`model_reasoning_effort="max"`. The shorter IDs `luna-max` and `luna` are
rejected by the ChatGPT-backed Codex route.

The adoption decision is to keep Sol medium as the default model for the
remaining canonical queue. Luna max is useful as a selective escalation model,
not as the primary queue worker.

## Evidence from the completed Sol-medium audit

The active ledger currently contains 410 completed source chunks and 160
remaining source chunks with measurable assigned line counts.

| Assigned lines | Completed chunks | Finding references | Chunks with findings | Finding references per 1,000 lines |
|---|---:|---:|---:|---:|
| 1-300 | 112 | 199 | 72 | 16.10 |
| 301-600 | 131 | 493 | 125 | 7.48 |
| 601-750 | 123 | 395 | 117 | 4.48 |
| 751-900 | 24 | 164 | 24 | 8.18 |
| 901+ | 20 | 38 | 18 | 0.86 |

These are finding references, not unique new findings. A chunk can cite an
existing finding, and smaller chunks are often risk-selected, so the table
does not prove that smaller chunks cause higher quality. It does show that the
current 900-1,100 line ceiling is not a useful default for deep review: caller,
callee, test, paired-code, and interface expansion can easily exceed the
assigned source itself.

## Proposed Luna-max budgets

Start smaller than Sol-medium and use Luna's additional reasoning for wider
evidence tracing, not for feeding it more assigned source.

| Scope | Initial Luna-max budget |
|---|---:|
| Native, parser, filesystem, network, concurrency, or security boundary | 250-400 assigned lines |
| High-fan-out stateful Kotlin, PowerShell, Rust, or build logic | 300-450 assigned lines |
| Low-fan-out stateful application or build logic | 450-600 assigned lines |
| Cohesive data model or low-branching helper code | 600-750 assigned lines |
| Tests and ordinary documentation | 750-900 assigned lines |
| Mechanical or generated-data batches | Keep current batch units and use format-specific validation |

Additional limits:

- At most four related authored paths unless they form one small interface
  family.
- One state machine, trust boundary, or publication transaction per chunk.
- Split at functions, classes, state transitions, or data-format boundaries,
  not at an arbitrary line count.
- Reserve roughly half of the working context for callers, callees, tests,
  paired D1/D2 code, history, and duplicate checking.
- If useful context crosses three subsystems, split the assigned scope before
  continuing instead of silently narrowing context.
- At 10 minutes, require an evidence-map checkpoint: enumerate covered inputs,
  outputs, callers, tests, and live hypotheses. Continue exploration only when
  a concrete unresolved candidate needs more evidence; otherwise move to
  disproof and reporting.
- Treat path and subsystem count as budget dimensions. A 250-line chunk spanning
  UI wrappers, device policy, Play Core, and external intents can cost more than
  a 600-line slice of one state machine.

## Review strategy

Use three explicit passes inside each call:

1. Perform a local assigned-scope pass before expanding context. Map inputs,
   state, outputs, ownership, repeated factory/property calls, callback or task
   identity, failure paths, and maintained tests.
2. Generate concrete adversarial triggers across success, empty, malformed,
   partial-failure, cancellation, concurrency, and platform boundaries.
3. Try to disprove every candidate against guards, callers, tests, history,
   and paired implementations before reporting it.

Do not begin external documentation or broad caller exploration until the
assigned-scope pass has produced an evidence map. Record local candidates
before opening secondary paths so a long API-contract investigation cannot
displace an obvious same-file identity or control-flow defect.

For experiments, blind the reviewer from both ledgers and request candidate
findings without IDs. A separate comparison pass then maps candidates to
existing findings, verifies genuinely novel candidates, and records false
positives. Canonical review calls should continue reading both ledgers and
retain the one-writer rule.

Do not rewrite completed R1 coverage. For remaining 900-line R1 rows, first
record an experimental semantic split outside the canonical queue. Change the
canonical process only after the trial establishes a stable budget and a
maintainer chooses whether R1 rows should gain explicit child items.

## Initial blinded comparisons

### Experiment A: coherent smaller chunk

- Historical baseline: `R1-CHUNK-0385`, reviewed by Sol medium as 385 assigned
  lines.
- Frozen scope: `SetupAutomationApi.kt:L901-L1045` and all 240 lines of
  `SetupConfigFiles.kt`.
- Baseline roots for post-review comparison: imported config replacement
  metacharacters and empty custom-audio launch readiness, plus cited existing
  deletion, publication, pilot-selection, and resolution-domain findings.
- Goal: measure whether max reasoning retains focused edge-case recall without
  generating unsupported adjacent findings.

### Experiment B: split a larger mixed chunk

- Historical baseline: `R1-CHUNK-0401`, reviewed by Sol medium as one 837-line,
  four-path chunk.
- Split B1: `TouchOverlayView.kt:L4501-L5072` (572 lines).
- Split B2: all of `TvButtons.kt`, `TvDetection.kt`, and `UpdateChecker.kt`
  (265 lines).
- Baseline roots for post-review comparison: mutable admin-tray action identity
  and pointer ownership in B1; mismatched Play Core tasks and unguarded fallback
  intent in B2.
- Goal: see whether semantic splitting improves evidence depth while preserving
  the aggregate findings from the original mixed chunk.

## Metrics

Record these separately for each call:

- Recall of independently verified baseline roots.
- Confirmed novel findings after a separate verification pass.
- Duplicate rate and dismissed or unsupported candidate rate.
- P0/P1 misses, which are adoption blockers.
- Evidence completeness: location, reachable trigger, control flow, impact,
  branch causation, and disproof attempt.
- Caller, callee, test, paired-code, and history coverage.
- Wall time, tokens, tool calls, and confirmed unique findings per unit cost.

Finding count alone is not a quality metric.

## Suggested adoption gate

Run at least eight blinded chunks balanced across:

- Two known high-yield chunks.
- Two known no-new-finding chunks.
- Two stateful Android or script chunks.
- One native or parser trust-boundary chunk.
- One mixed-path chunk reviewed both whole and semantically split.

Adopt Luna max for continued canonical review only if it misses no independently
verified P0/P1 baseline, has an acceptable dismissed-candidate rate, and either
finds verified novel roots or produces materially stronger evidence at a cost
the maintainer accepts. Use the trial to select the final budget rather than
assuming that a larger model should receive larger chunks.

## Preliminary results

| Experiment | Assigned scope | Approximate wall time | Baseline recall | Other candidates | Outcome |
|---|---:|---:|---|---|---|
| A | 385 lines, high fan-out | Over 20 minutes | No final report | Five incomplete hypotheses | Stopped after context compaction and repeated exploration |
| B1 | 572 lines, one touch state machine | 16 minutes | Mutable action/pointer identity recalled | Held-gamepad repeat candidate | Completed |
| B2 | 265 lines, four subsystems across three paths | 15 minutes | Store fallback recalled; Play Core task identity missed | Three P3/investigation candidates | Completed |

### Experiment A

The 385-line setup/configuration scope exceeded 20 minutes, compacted its own
context, reopened already traced D1/D2 consumers, and still had not produced a
final report. It was stopped and is recorded as an exploration-budget failure.
Before termination it had identified five likely risks, including raw imported
configuration values, exported destructive automation, and cross-layer music
or reset behavior, but incomplete hypotheses are not counted as findings.

This scope was small by line count but central by fan-out. A Luna-max chunk
containing configuration publication, launch preparation, music policy,
automation deletion, and paired native consumers should be split by behavior,
not kept together because it is under a nominal line limit.

### Experiment B1

The 572-line `TouchOverlayView` state-machine slice completed with three
candidates:

- Secondary-pointer gesture ownership and dynamic action-list retargeting both
  rediscovered the independently recorded mutable-action-identity root.
- Held gamepad buttons repeatedly dispatching tray actions was a new candidate
  not found by a direct text search of either ledger. It needs an independent
  verification call before it can be treated as a finding.

Luna recalled the baseline root but split two symptoms of that root into
separate candidates. Canonical Luna prompts still need the existing ledger
deduplication pass.

### Experiment B2

The 265-line three-path Android slice completed with four candidates:

- It rediscovered the unguarded HTTPS fallback after market-intent failure.
- It missed the independently established defect where success and failure
  listeners attach to two different Play Core update-info tasks.
- It proposed Android TV update availability, one-shot resume behavior, and
  custom focus-wrapper bypass candidates. These are plausible P3 or
  investigation items, but none was independently verified in this trial.

The smaller line count did not make this the cheaper or more accurate call. It
crossed UI wrappers, device classification, lifecycle, Play Core, external
intents, and Kotlin name resolution, which led to broad exploration and one
missed baseline P2.

### Initial conclusion

Do not increase chunks for Luna max. Start stateful or trust-boundary review at
300-450 assigned lines, lower than the first proposal when fan-out is high.
Use one subsystem and one behavioral question per chunk, add the 10-minute
evidence checkpoint, and require the final disproof/report phase to begin once
the evidence map covers the concrete candidates. A larger evaluation sample is
needed before changing the canonical R1 process.

## Paired eight-scope trial

The final trial used eight frozen scopes. Each model received the same blinded,
read-only prompt for a scope. Reviewers could read repository instructions and
frozen source context but could not read either adversarial ledger, BR text, or
chunk completion notes. The canonical ledgers and queue were not edited.

The scopes covered:

- The 572-line admin-tray touch state machine.
- The 265-line TV, focus-wrapper, and Play update group.
- The 433-line native autoselect implementation.
- The 537-line shared level-metadata scanner.
- The 152-line D1 texmap header control.
- A 330-line mission-import test range.
- A 604-line six-asset controller, touch, and fingerprint configuration group.
- A 251-line mission tracklist, AcoustID, and fingerprint parser range.

There were seven independently known roots in the five positive scopes:
BR-0448, BR-0454, BR-0455, BR-0261, BR-0262, BR-0301, and BR-0177.

| Metric | Luna max | Sol medium |
|---|---:|---:|
| Known-root recall | 6 of 7 | 7 of 7 |
| Completed paired scopes | 6 of 8 | 8 of 8 |
| Runs stopped at 15-16 minute cap | 2 | 0 |
| Approximate completed-run median | about 13 minutes | about 4.5 minutes |
| Independently confirmed novel roots from the paired trial | 1 | 4 |
| Clean native-header control | Correct no-finding result | Correct no-finding result |

The Luna completion count excludes the earlier unpaired 385-line setup trial,
which was also stopped after more than 20 minutes and context compaction.

### Known-root recall

Both models recalled the mutable admin-tray action identity, malformed weapon
orders, tied pilot timestamps, multi-link trigger completion, and regex JSON5
comment stripping.

Sol also recalled both Play update roots. Luna recalled the unguarded fallback
intent but missed the separate-task success and failure callback root. This was
the only known-root miss in the paired sample.

### Confirmed additional signal

Separate frozen-code checks confirmed one Luna-only root:

- Admin-tray controller handling runs before the existing `repeatCount` and
  edge gate. Repeated button-down events can invoke one tray action multiple
  times for one held button.

The preliminary active-file-set candidate was dismissed during ledger
integration. File sets contain selected game data, while pilots intentionally
remain in the per-game preference directories scanned by the native bridge.

Separate frozen-code checks confirmed four Sol-only roots:

- `sqrt_ll` overflows `mid + 1` when the valid squared distance reaches
  `INT_MAX * INT_MAX`, so hostile or extreme level geometry can make metadata
  scanning undefined or nonterminating.
- Kotlin writes an unbounded number of active mod path lines, while the paired
  native reader silently retains only 64. Multi-path mission packages can be
  split at that boundary.
- The bundled Simple touch preset exposes only left-stick axes, which the
  bundled controller mapping assigns to strafe and throttle. It has no
  pitch/turn control and disables gyro.
- The bundled controller asset assigns 30 percent thresholds to all four stick
  axes, while generated defaults use the maintained 10 percent stick default.
  Applying the named default can therefore triple the stick dead zones.

Other candidates were either already represented by an existing root or did
not affect the model choice:

- Both touch reviews folded secondary-pointer replacement into BR-0448.
- Sol's mission replacement and canonical archive-collision candidates matched
  BR-0186 and BR-0105.
- Sol's AcoustID confidence candidate matched BR-0413.
- Luna's TV update, resume retry, and focus-wrapper candidates remained weak or
  policy-dependent.
- Luna's tracklist provenance and request-timeout candidates remain plausible
  investigation items but were not needed to decide the trial.
- Sol's outside-panel dismissal and redundant duplicate-audio candidates were
  lower-priority behavior or efficiency observations.

### Reliability and cost

Sol completed every paired review in roughly 3-9 minutes. Luna's completed
reviews generally took about 12-16 minutes, apart from the small native header.
The 330-line test scope and 604-line asset scope both crossed the cap without a
final answer; the asset run compacted its own context after declaring its
evidence pass complete. The explicit prompt instruction to consolidate at ten
minutes did not reliably control Luna's exploration.

This makes assigned line count a poor enough Luna budget by itself. A clean
330-line test range can expand into a large production graph, while a cohesive
native header can finish quickly. Using Luna for every remaining queue item
would likely reduce throughput by about threefold and introduce incomplete
items that need reruns or manual recovery.

## Adoption decision

Use Sol medium for the remaining canonical audit.

Keep the current semantic chunking policy for Sol. Do not enlarge chunks merely
because Sol completed this sample quickly; the existing 600-750 line default
still leaves room for callers, tests, paired D1/D2 code, history, and duplicate
checking.

Use Luna max selectively for:

- A second opinion on a disputed or high-severity candidate.
- A focused question where the initial Sol review identifies one concrete
  boundary but cannot prove or disprove it.
- A small high-risk parser, ownership, or concurrency slice where slower deep
  exploration is worth the cost.

For those Luna escalations, assign one behavioral question, normally 250-400
lines and no more than one tightly related subsystem. Treat ten minutes as a
checkpoint and 15 minutes as a hard trial cap. If Luna has not produced a
report by then, preserve the hypotheses as investigation notes and return the
item to Sol or manual verification instead of allowing repeated context
compaction.
