# Private D1/D2 base retail CI

Status: independently reviewed; all eleven uploaded files verified; trusted cmake deployment and retail runtime qualification pending

## Scope

Add trusted-cmake-only retail asset provisioning to the existing four-lane nightly sweep, with the same finite per-revision checklist and 45-minute lane budgets. Keep ordinary PR execution asset-free. Do not change PR60's frozen source or merge any branch.

Private repository: larsonm-personal/dxx-redux-test-assets
Expected secret: RETAIL_ASSETS_READ_TOKEN (user enters a fine-grained Contents:read token limited to that repository directly in GitHub)
Canonical staging layout: d1/descent.hog, d1/descent.pig; d2/descent2.hog, descent2.ham, descent2.s22, groupa.pig, alien1.pig, alien2.pig, fire.pig, ice.pig, water.pig

No credentials or retail files are created/uploaded by this implementation. The user uploaded originals at private commit f31a7a192c2e38eb70ea2382a5a39e5a1c5ceebe. Streaming verification confirmed all eleven SHA-256 values match the existing tested edition; only metadata was retained locally. The descriptive source directories are preserved by retail_asset_sources.json and mapped to isolated canonical test staging. Optional S11 is not part of the initial profile.

## Implementation boundaries

- Inventory actual supplied file sizes and SHA-256 values. Compare with the existing standard-data fixture identities; report different known/unknown editions rather than replacing pinned expectations silently
- Pin the private repository revision, bind data/manifest identity into plan and receipt provenance, and verify all required files before tests
- Read-only private checkout only for scheduled or explicitly dispatched trusted cmake execution; no PR or arbitrary workflow-call secret path, no persisted checkout credentials
- Missing configuration/data remains pending with a useful setup diagnosis, never a passing or exhausted test
- Retail execution must not upload asset-containing directories, screenshots, logcat dumps, or broad build/test artifacts. Publish an explicit sanitized assertion/result allowlist; preserve useful failures without raw asset bytes. Disable potentially asset-bearing cache writes
- Reuse existing standard-data staging and existing assertion owners. Base D1/D2 does not supply Vertigo, original CD installers/music, mods, custom campaigns, or missing manual recordings
- Add only individually audited base-data owners; record unqualified runtime until actual data is available

## Qualification

Portable synthetic controls for exact file identity, missing files, path/symlink rejection, wrong edition, provenance changes, trusted event/ref gating, missing token/configuration, pending setup accounting, and artifact/cache restrictions. Local PowerShell remains unavailable by prior permission decision. Hosted PR checks must remain data-free; actual retail qualification starts only after the user merges the reviewed stack into cmake. The supplied data and Environment secret are present; token functionality is not yet exercised.

## Admission and deployment

The policy adds 58 reviewed master defaults and four explicit host profiles requiring retail-base, plus two asset-free portable control suites (328 total selections). Every retail selection remains runtime-unqualified until executed with the verified files. retail_owner_admission.json records remaining base-data adapters, permission/tool prerequisites, over-budget owners, and non-base media gaps.

The read-only token belongs only to the retail-ci Environment, restricted to exact cmake branch with no reviewer/wait timer. The user enters it directly in GitHub; this code never creates or configures credentials. RETAIL_ASSETS_COMMIT is the nonsecret immutable private commit variable, configured and read back as f31a7a192c2e38eb70ea2382a5a39e5a1c5ceebe. The only privileged entry is scheduled/manual execution of the exact nightly-tooling-sweep.yml on cmake. PR validation remains asset-free, including same-repository PRs. Deployment requires the user to merge the reviewed stack; no branch merge is performed here.

Independent review passed 113 portable controls (32 asset helper, 38 host adapter, 34 planner, 9 retail boundary). An additional 21 existing Android fixture/security controls passed after workflow integration. Actionlint without local ShellCheck and git diff --check passed. No local PowerShell or retail gameplay execution was performed.
