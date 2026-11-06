# Pumo functionality overnight, November 3-4 2026

User deadline: November 4 2026 at 09:00 America/Los_Angeles/PST (17:00 UTC).
Stop starting implementation at08:40 PST and finish restoration/handoff by09:00.
Latest user instruction supersedes prior deliverable gates: DO NOT BUILD APKs.
Focus on making original Pumo fully work, using existing binaries, native/JVM
builds and bounded original-resource gameplay probes. No APK packaging/delivery
work or UI cosmetics. Leave private XL asset CI failures to Dot; do not monitor
or repair those failures or their ancestors. Continue new stacked child branches.

Refreshed base: PR338 ac7aa160e292df660adbb9ace9ad137bb12b7afd,
PR335 95bbf3e99de84372af90466fff3647caa4ecbb75,
PR333 03bbe94f67b946f0d682a0e026a11e2d1a1b8d74,
PR332 55843b1f46e45772c04949b71d2272679fd45a13;
cmake ddf4da830 includes merged cleanup and hosted disk fixes. Fresh child
xl_pumo_launch_preparation_budget. Backup pre-refresh refs and all3stashes remain.

## Ordered work

1. Re-identify/restart owned AVD without resetting data. Verify retained APK29471
   and original resources. The old test restarted resource copying every30s;
   distinguish harness startup budget from an actual gameplay failure. Make only
   the smallest harness change needed to observe a complete live launch
2. Run protected full-original PMC inner gates88warm/23cold using existing APK,
   without Vertigo, grants or forced exits. Preserve original profile hashes and
   classify earned key/normal triggers/persistence versus local positioning
3. Investigate the first concrete progression/combat defect with original models
   and weapons. Audit normal robot contact/gunpoints, switch/master semantics,
   earned inventory and remaining boss/reactor objectives. Fix source consumers,
   validate native/JVM controls plus meaningful ordinary-player probes
4. Continue coherent child PRs toward later earned PMC/campaign progression.
   Do not repeat unchanged exhaustive qualifications or unsupported GuideBot
   routes. Read rendered-guidebot.md before any new bounded route/hypothesis

Follow AGENTS.md and .github/copilot-instructions.md. Fork-only GH_REPO and gh
stack, rebase/no merge commits; never push upstream or merge PRs. No subagents.
Preserve all work/stashes/bundles, pilots/settings/saves, ordinary non-XL loading,
loading/import progress, briefing pages/sound and readable errors. Phone permission
is read-only, no installs/stopping/launching or writes. Device tests serial on an
owned profile with verified restoration, no generic clear/reset. Keep full
original HAM/HXM/VHAM/ASE/OOF and unchanged mission bytes. Never weaken guards,
grant inventory or force exits for a pass. No proprietary bundled resources, DXA
builder/distributor or user CLI installer. Ordinary launcher import stays required.

Checkpoints still carry changes/WHY, exact test executable/resources/hashes and
existing artifact identities, tested/untested scope, stack/PR targets/heads,
validation/pending checks, preservation/cleanup and ranked next tasks. Each
handoff includes a copyable fresh-context prompt carrying ALL constraints, this
repeating requirement and the NO APK BUILDS instruction. For a following goal use
[USER-SUPPLIED ABSOLUTE CUTOFF AND TIME ZONE] until supplied; never auto-start it.

## Original-resource checkpoints

PR344 at a287637fdb7c05fd316c6bb8cf45fe31b0b5ed8e rebases the88warm/23cold
inner-gate proof onto Dot's refreshed stack after native-fix PR343 merged.
The next child xl_pumo_original_reactor adds106warm/7cold reactor combat/countdown
and117warm/24cold earned escape/PMC2 transition-save checks. Exact Android engine
is retained APK29471/ec7715, not current-head native qualification. No APK builds.
Original VHAM type76 is a gunless prop, so the old Vertigo seven/six-objective
assumptions were stale. Original reactor accounting is1->0 and a90-second countdown.

Next: run original PMC2 selectors/local gates and earned boost exit to PMC3, then
original PMC3 keys/reactor/ending. Preserve the exact source/assets/trace/terminal
status, use normal activation and protected restoration, and fix the first
source-backed gameplay defect. Do not treat historical fixture boss combat or
controlled next-level dispatch as full-original progression proof.

## Original PMC2 checkpoint

Original selectors187warm/59cold and earned PMC2 escape167warm/23cold pass on the
same retained29471, with exact protected restoration. The latter reaches PMC3
through the normal completion menu/briefing and saves/loads at segment1619.
No engine or test-script change was needed for these checks; historical evidence
is labeled separately. Next PMC3 opening/earned keys/reactor/ending, then audit
the remaining original branches: the authoritative MN2 lists14levels, not only
the three PMC maps. Preserve this wider mission scope in completion claims.

After progression, audit mission-owned informational text: original pmines.txt
contains Pulser and other weapon label replacements while retained screenshots
still show stock labels. Verify pinned XL selection/lookup semantics before
calling this a defect or implementing a mission-scoped override. Defer cosmetics.
