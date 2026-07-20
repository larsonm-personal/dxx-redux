# Adversarial Branch Review Process

## Purpose

This process turns the complete branch diff into deterministic review units that can be handled by sequential `sol-5.6-medium` calls. It is designed to survive interruptions, branch movement, remediation commits, and context loss

The process has two permanent artifacts:

- This file defines how review calls work
- `branch_adversarial_review_ledger.md` is the only canonical file for the queue, review notes, findings, dispositions, and closure evidence

Scratch patches, command output, and generated delta ledgers may be written under `temp/`. Do not create separate finding files

## Core decisions

### Freeze review snapshots

Each campaign records a target tip, merge base, and review head as full commit IDs. All calls review those immutable Git objects, not whatever happens to be in the working tree later

For the initial campaign, the generator uses the merge base of `upstream/main` and `HEAD`. This is the PR diff even when upstream has advanced since the branch split. The current upstream tip is also recorded so integration changes on the target side are visible

Fixes may proceed while review continues because the review snapshot stays stable. Before closure, all commits after the review head must be included in one or more delta campaigns

### Cover every path at the right depth

Every changed path must appear in a source chunk or a mechanical batch. Coverage does not mean pretending every artifact benefits from the same method

- Authored source, scripts, configuration, tests, and ordinary documentation receive line or hunk review
- Large new files are divided into non-overlapping line ranges
- Modified files are divided by changed hunks and weighted by additions plus deletions
- Small related files are packed together by risk, kind, and directory
- Generated fixtures, historical AI plans, lock files, binaries, logs, databases, and miscellaneous data receive explicit batch review and suitable mechanical validation

The default budgets are:

| Risk or content | Maximum assigned review lines |
|---|---:|
| Critical trust boundary | 600 |
| High-risk native or compatibility code | 750 |
| Ordinary authored source | 900 |
| Tests and documentation | 1,100 |

Up to 16 small related paths may share a call. These are assigned-line budgets, not hard context limits. A reviewer must expand to the enclosing function, relevant callers, tests, paired D1/D2 implementation, or interface definition when needed

### Separate discovery from remediation

Review calls inspect and report. They do not edit product code. This avoids silently invalidating later chunks and keeps each finding attributable to the frozen snapshot

Remediation calls are separate. They may fix one coherent finding or a tightly coupled group, run validation, and update the finding disposition in the same ledger

### Use one writer

Only one call may edit the ledger at a time. Sequential calls avoid duplicate IDs and Markdown merge conflicts. A call claims a queue item by changing it to `ACTIVE` before doing substantive work

## Generate the first ledger

From the repository root:

```powershell
.\android\helpers\new_adversarial_review_ledger.ps1
```

The generator:

- Resolves the target ref and head to full commits
- Uses their merge base as the review base
- Inventories every changed path
- Parses the complete modified-file diff once
- Classifies paths by review method and risk
- Produces deterministic chunk IDs and scopes
- Adds preflight calls, cross-file sweeps, and a closure gate
- Refuses to overwrite an existing ledger unless `-Overwrite` is explicit
- Writes UTF-8 without a byte order mark and normalizes to LF

Useful options:

```powershell
.\android\helpers\new_adversarial_review_ledger.ps1 `
    -BaseRef upstream/main `
    -HeadRef HEAD `
    -CampaignId R1 `
    -SourceLinesPerChunk 900 `
    -TestLinesPerChunk 1100
```

Do not regenerate the canonical ledger after review begins. Generate later snapshots into `temp/`, then append their snapshot, queue rows, and batch path lists to the canonical ledger with their distinct campaign ID

Example delta generation after the initial frozen head:

```powershell
.\android\helpers\new_adversarial_review_ledger.ps1 `
    -BaseRef c01d8fe4686c63d931b1e543a6305bbafaa944a9 `
    -HeadRef HEAD `
    -CampaignId R2 `
    -OutputPath temp/adversarial-review-r2.md
```

This works when the old review head is an ancestor of the new head. If history was rewritten, first identify the intended old-to-new comparison and record the decision in the ledger

## Campaign order

### 1. Preflight

Complete all `PREFLIGHT` items before ordinary chunks

- Review PR composition and decide whether logs, databases, binaries, generated metadata, test game data, and historical AI plans belong in an upstream PR
- Build a subsystem and trust-boundary map
- Review commit history for superseded implementations, partial migrations, revert residue, and changes that should be split
- Record major design findings immediately even if they may make lower-level comments obsolete

Preflight does not waive later path coverage

### 2. Source chunks

Process source chunks in queue order. The generator orders critical and high-risk areas first

For a new file range, inspect the assigned lines at the frozen review head:

```powershell
git show <review-head>:<path>
```

For a modified file, inspect the exact diff and the full enclosing code:

```powershell
git diff --unified=80 <review-base> <review-head> -- <path>
git show <review-head>:<path>
git show <review-base>:<path>
```

Paths with spaces must be passed as distinct quoted arguments. Do not review only the current worktree copy

### 3. Mechanical batches

Mechanical batches are real review work, not automatic skips

| Kind | Required review |
|---|---|
| `generated-fixture` | Identify the generator and schema, run or locate normalization and regression checks, compare representative boundary cases, and verify that checked-in output is reproducible |
| `dependency-lock` | Tie changes to an intended manifest change, inspect source and license, check pinning, and use dependency or vulnerability review where available |
| `artifact` | Check whether the file belongs in version control, inspect type and provenance, check for private data or secrets, and record the reproducible source or removal action |
| `historical-plan` | Check path identity, upstream relevance, private or stale information, contradictions with the final implementation, and whether the plan should ship at all |
| `other-data` | Identify its consumer and format, then choose schema, parser, checksum, license, or provenance validation appropriate to the file |

A batch may be marked `SKIP` only when the ledger names why line review is inappropriate and records the substitute validation. Merely calling a file generated is not enough

### 4. Cross-file sweeps

Complete every `SWEEP` item after the ordinary chunks. These calls deliberately revisit related code from a different angle

The sweeps cover:

- Paired D1/D2 behavior and minimal changes to upstream-owned code
- JNI and native ownership, lifetimes, bounds, and thread rules
- Android lifecycle, cancellation, state restore, permissions, and touch-only operation
- Filesystem, archive, configuration, save, and metadata trust boundaries
- Matchmaking and multiplayer protocol abuse, denial of service, cleanup, and compatibility
- Concurrency and resource lifetime across subsystem boundaries
- Build, packaging, dependency pinning, ABI coverage, and host portability
- Test validity, false passes, runner cleanup, timeouts, and integration gaps
- Replay, RNG, floating point, serialization, and save-state determinism
- Performance on render, simulation, import, and metadata hot paths
- Fail-safe error handling, logging usefulness, and privacy
- API, ABI, protocol, schema, and duplicated-constant ownership
- Naming, unnecessary abstraction, duplication, dead code, and simplification
- PR hygiene, provenance, secrets, licenses, and reviewability

### 5. Verify findings

Every P0 and P1 finding must receive an independent verification call before remediation or dismissal. A medium-confidence P2 should also be verified when it would require a broad fix

The verification call must try to disprove the finding by tracing the actual caller, state, guard, cleanup, or test behavior. It records one of:

- Confirmed, with stronger evidence
- Narrowed, with corrected trigger or impact
- Duplicate of another finding
- Dismissed, with concrete contrary evidence
- Converted to an investigation because essential facts remain unknown

### 6. Remediate and disposition

Use a separate call for each coherent fix tranche. The call must:

- Read the finding and all related findings first
- Recheck the live code because it may differ from the frozen snapshot
- Make the smallest complete fix that addresses the cause
- Add or improve an integration test when the behavior is significant
- Run scoped formatting and lint checks
- Run the relevant host, Android, server, unit, integration, or emulator validations
- Update the finding status and append a disposition-log entry with commit or worktree evidence

Valid final finding states are `FIXED`, `DISMISSED`, `DEFERRED`, and `DUPLICATE`. A deferral requires an owner or future milestone, a reason, and an explicit statement of accepted risk

### 7. Close the campaign

`CLOSE-001` is complete only when:

- Every changed path is classified and covered
- Every queue item is `DONE` or has an approved `SKIP` record
- No item remains `ACTIVE` or `BLOCKED`
- Required independent finding verification is recorded
- Every finding has a final disposition or approved deferral
- Relevant validation evidence is present
- The frozen review head to live `HEAD` delta is empty or covered by appended delta campaigns
- A human maintainer has reviewed the open-risk and AI-disposition summary

The closing note must report counts by severity, category, and disposition, list accepted risks, list validation gaps, and state the exact reviewed commit range

## One-call protocol

Each review call performs exactly one queue item

1. Read `AGENTS.md`, `.github/copilot-instructions.md`, this process, and the ledger
2. Confirm the frozen commits named by the item still exist
3. If one item is already `ACTIVE`, resume it. Otherwise claim the first eligible `TODO` item by changing its state to `ACTIVE`
4. Create an internal task plan. The canonical ledger is the plan file for this campaign, so do not create a separate per-chunk plan file
5. Inspect the assigned diff or paths plus enough surrounding context to understand behavior
6. Search the ledger before adding a finding so related symptoms share one root-cause finding
7. Append each supported finding using the required template
8. Append a chunk completion note, including explicit no-finding evidence when applicable
9. Change the queue state to `DONE` and put finding IDs or `none` in the Result column
10. Check that only the ledger changed during a review-only call

If interrupted after claiming, leave the row `ACTIVE`. The next call resumes it and records that it recovered an interrupted claim

If truly blocked, use `BLOCKED` and record the exact missing evidence, failed command, or authority needed. Difficulty or uncertainty alone is not a blocker

## Standard prompt for a review call

```text
Continue the adversarial branch review for exactly one queue item using sol-5.6-medium

Read AGENTS.md, .github/copilot-instructions.md, android/ai tool plans/code management/branch_adversarial_review_process.md, and the canonical ledger. Resume the existing ACTIVE item or claim the first eligible TODO item. Review the frozen Git snapshot, not the live worktree. Inspect the assigned scope and enough callers, callees, tests, paired D1/D2 code, and interface context to support or disprove issues. Do not modify product code. Record only evidence-backed, branch-caused findings using the exact ledger template. Search for duplicates first. Append a completion note even when there are no findings, update the queue row, and stop after this one item
```

For a named-item run, replace the claim sentence with `Claim and review <ID>`

## Standard prompt for a verification call

```text
Independently verify finding <BR-ID> using sol-5.6-medium. Read the review process and ledger, then try to disprove the finding against the frozen snapshot and relevant callers, guards, tests, and platform behavior. Do not fix code. Record confirmed, narrowed, duplicate, dismissed, or investigation status with concrete evidence in the canonical ledger
```

## Standard prompt for a remediation call

```text
Remediate finding <BR-ID> using sol-5.6-medium. Read repository instructions, the review process, the complete finding, related findings, and the live code. Implement the smallest complete root-cause fix, add appropriate regression coverage, run scoped code quality and relevant builds or tests, then update the canonical ledger with the disposition and exact validation evidence
```

## Review method for authored code

Reviewers must reason about behavior, not just pattern-match syntax. For each assigned unit, cover the applicable areas below

### Intent and design

- Infer the intended behavior from tests, callers, UI text, commit history, and adjacent interfaces
- Check whether the change belongs in this layer and whether a simpler design preserves the same behavior
- Check for duplicate sources of truth across C, Kotlin, Rust, scripts, schemas, and generated data
- Identify partial migrations where old and new paths can both run

### Correctness and edge cases

- Trace success, empty, boundary, cancellation, retry, partial-failure, and cleanup paths
- Check ordering assumptions, off-by-one ranges, integer conversion, overflow, truncation, nullability, initialization, and stale state
- Check ownership, object lifetime, reference validity, handle closure, and rollback
- Check whether tests would fail if the implementation were subtly wrong

### Security and abuse

- Identify untrusted sources and trace them to parsers, allocation sizes, filesystem paths, logs, database operations, network responses, and native calls
- Check validation after decoding and canonicalization
- Check path traversal, archive bombs, oversized messages, resource exhaustion, command construction, SQL use, secrets, and information disclosure
- For the public server, assume malicious clients will hold connections, send malformed or reordered messages, race state changes, and maximize CPU, memory, sockets, and lock hold time

### Concurrency and lifecycle

- Identify thread ownership and ordering across Kotlin, JNI, game, GL, audio, network, and server tasks
- Check cancellation, background and resume, activity recreation, stale callbacks, races, deadlocks, and cleanup after exceptions
- Verify that shared state has an explicit synchronization or single-thread rule

### Compatibility and portability

- Preserve Windows, Linux, and macOS behavior in inherited game code
- Check Android guards and both D1/D2 implementations where the base code is duplicated
- Check ABI, file format, protocol, database, configuration, and save compatibility claims
- Check path separators, spaces, case sensitivity, line endings, shell differences, and exit-code propagation
- Check dependency versions and external tool inputs are pinned

### Performance and resource use

- Look for new work in per-frame, per-object, audio, render, routing, metadata, import, and network loops
- Check repeated allocations, parsing, JNI crossings, copies, database queries, lock contention, and unbounded caches or queues
- Require measurements only when impact cannot be established from complexity and call frequency

### Maintainability

- Prefer names that state purpose and units
- Flag confusing control flow, unnecessary wrappers, speculative abstraction, duplicated logic, and comments that restate code
- Do not file personal-style findings already handled by a formatter or not grounded in repository conventions
- A naming or simplification finding must name the reader error or future bug it prevents and suggest a concrete direction

### Tests and observability

- Check whether production behavior has an integration test at the right boundary
- Inspect test assertions, fixtures, failure signals, timeouts, cleanup, and false-positive paths
- Keep the replay system transparent and fix engine nondeterminism rather than adding demo exceptions
- Use Android `debug_log()` routing and introspection conventions, not nonexistent Android `gamelog.txt` behavior
- Ensure logs help diagnose failure without exposing secrets or uncontrolled attacker input

## Finding admission standard

File a defect finding only when all of these are present:

- The branch introduced or materially worsened the issue
- A concrete trigger, state, input, or maintenance failure exists
- The relevant control flow or data flow is identified
- The impact is specific
- The location is narrow enough for another person to verify
- A plausible fix direction and validation method are included

Do not file:

- Generic warnings without a reachable scenario
- Purely pre-existing issues unaffected by the branch
- Formatter output or unsupported personal style preferences
- Duplicate symptoms when one root cause can cover them
- Claims based only on names without inspecting implementation and callers
- Low-confidence defects disguised as high severity

When an important hypothesis lacks essential evidence, create an `INV` investigation entry instead of asserting a defect

## Severity

| Level | Meaning |
|---|---|
| P0 | Credible remote compromise, secret exposure, irreversible widespread data loss, or an issue that makes the branch unsafe to test or distribute |
| P1 | Must fix before the PR: reachable crash, corruption, security control failure, major protocol or compatibility break, resource exhaustion, deadlock, or required build failure |
| P2 | Should fix: real edge-case bug, localized leak, meaningful test gap, confusing design likely to cause defects, or material performance problem |
| P3 | Optional improvement: specific naming, simplification, documentation, or low-impact maintainability issue grounded in repository standards |

Severity describes impact, not certainty. Confidence is recorded separately as `high` or `medium`. Low-confidence claims use `INV`

## Finding template

Use the next unused numeric ID. Keep the title short and action-oriented

```markdown
### BR-0001: P1 - Reject oversized relay frames before allocation

- [ ] OPEN
- Type: defect
- Confidence: high
- Category: security/resource-exhaustion
- Found by: R1-CHUNK-0001
- Location: `<review-head>:server/src/example.rs:L120` in `read_frame`
- Related: `server/src/other.rs:L44`, `server/tests/integration.rs:L300`
- Evidence: The length prefix is converted to an allocation size before any configured limit is checked
- Trigger: An unauthenticated client sends a valid header with a very large frame length and withholds the body
- Impact: Each connection can retain a large allocation and socket until timeout, allowing memory exhaustion
- Expected: Reject lengths above the protocol maximum before allocation or body read
- Suggested fix: Centralize the maximum in the protocol decoder, reject before allocation, and keep client and server constants synchronized
- Validation: Add a test that sends an oversized prefix without a body and asserts prompt rejection with bounded memory and connection cleanup
- Resolution: Pending
```

Use one category from this preferred set when possible:

- `correctness`
- `security`
- `resource-lifetime`
- `concurrency`
- `performance`
- `compatibility`
- `build-release`
- `api-data-format`
- `test-gap`
- `maintainability`
- `naming`
- `documentation`
- `pr-hygiene`

Subcategories may follow a slash as in `security/path-traversal`

## Investigation template

```markdown
### INV-0001: Confirm whether background import can outlive its activity

- [ ] OPEN
- Risk if confirmed: P1
- Found by: R1-CHUNK-0001
- Location: `<review-head>:android/app/src/main/java/com/dxxredux/app/Example.kt:L80`
- Evidence known: The task captures the activity and no cancellation is visible in the assigned scope
- Evidence missing: The owner may cancel it through a lifecycle observer registered elsewhere
- Next check: Trace task construction and destruction through the activity owner and reproduce activity recreation during import
- Resolution: Pending
```

## Chunk completion note template

Every completed item gets a note, even when it found nothing

```markdown
### R1-CHUNK-0001 completion

- Completed: 2026-07-19
- Model: sol-5.6-medium
- Result: BR-0001, BR-0002
- Assigned scope checked: all listed hunks or ranges
- Context checked: named callers, tests, interface definitions, and paired files
- Commands or validation: exact read-only commands or tests used
- No-finding areas: bounds, cleanup, error propagation, naming, and test behavior checked without another actionable issue
```

For a no-finding result, use `Result: none` and make the context and no-finding lines specific. A bare statement that the code looks good is not a checkpoint

## Disposition rules

Change the finding checkbox line rather than adding a second status:

- `[ ] OPEN`
- `[x] FIXED`
- `[x] DISMISSED`
- `[x] DEFERRED`
- `[x] DUPLICATE of BR-xxxx`

Then replace `Resolution: Pending` with dated evidence and add one entry to the disposition log

A dismissal must explain which premise, path, or impact was false. A fix must name the commit or worktree change and validation. A duplicate must point to the surviving root-cause finding

## Research basis

The process adapts the following public guidance to this repository:

- [GitHub Copilot code review](https://docs.github.com/en/copilot/concepts/agents/code-review) says AI review can miss issues or make mistakes, recommends validating feedback and supplementing it with human review, and emphasizes repository context and instructions
- [GitHub Copilot review exclusions](https://docs.github.com/en/copilot/reference/review-excluded-files) explicitly excludes many generated, vendor, lock, log, and build-output paths, supporting a separate mechanical-validation class instead of silent omission
- [Google: What to look for in a code review](https://google.github.io/eng-practices/review/reviewer/looking-for.html) covers design, functionality, complexity, tests, naming, comments, style, context, concurrency, and line coverage
- [Google: Navigating a change in review](https://google.github.io/eng-practices/review/reviewer/navigate.html) recommends understanding the broad change first, reporting major design problems early, then following a logical sequence without missing files
- [Google: How to write code review comments](https://google.github.io/eng-practices/review/reviewer/comments.html) recommends clear severity, reasoning, respectful language, and encouraging code simplification rather than explaining confusing code only in the review
- [OWASP Secure Code Review Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Secure_Code_Review_Cheat_Sheet.html) recommends architecture and trust-boundary preparation, diff impact analysis, source-to-sink tracing, abuse cases, error handling, concurrency, resource limits, and complementary tool validation

AI review is an evidence-generation and triage layer. It is not an approval authority and does not replace maintainer review, builds, tests, security tooling, or runtime validation
