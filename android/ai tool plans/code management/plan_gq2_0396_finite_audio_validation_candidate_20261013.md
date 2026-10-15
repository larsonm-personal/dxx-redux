# GQ2 chunk0396 finite audio validation candidate, 2026-10-13

Canonical source-static finding GQF-0307 OPEN / GQR-0293 TODO, admitted under chunk0396 after consumerPASS312 and publicationPASS4419. Priority47MEDIUM/P2high. Future implementation plan only; no product/test/helper edits, NumPy/backend execution, codec/media generation, malformed-media probe or observed recipe false-pass claimed. Earlier candidate/proposed-ID sections preserve prepublication history.

## Actual validator and reporting path

store_asset_boss_video.py360-415 decodes final delivered audio and the reviewed master into finite int16 sample arrays cast to float64. For each nonsilent clip,392-398 computes RMS and correlation with np.corrcoef, then rejects only rms<10 or correlation<0.97. Neither metric is explicitly required finite, nor is an undefined/zero-variance correlation assigned a rejection policy. If correlation is NaN while RMS is at least10, both rejecting comparisons are false. Undefined correlation therefore supplies no evidence of fidelity but can continue to the success report. Empty windows can likewise make metrics undefined; ordinary configured windows are longer, so this is a separate boundary control rather than an observed short-window recipe failure.

Conditional algebraic witness: equal-length finite PCM windows with zero variance make Pearson correlation undefined; a nonzero constant delivered window can have RMS above10 while correlation is NaN. The source does not establish positive variance or a finite result before accepting it. This is valid decoded-sample shape, not a claim that the recorded boss recipe or pinned AAC encoder produced that signal. No executed NumPy, actual file, native capture or codec reproduction is credited.

The actual shared store_asset_media.save_json17-18 calls json.dumps without allow_nan=False. No normalizer intervenes before boss-validation.json or main435 stdout JSON. test_store_asset_pipeline.ps116-19 invokes the actual boss validate command, checks only child exit status, then emits its boss PASS and returns. Other picture/music/motion checks remain necessary, but none repairs the undefined audio metric. The failure is conditional audio-gate acceptance and report truth, not another JSON lexer ambiguity issue.

## Existing owner reconciliation and limits

Complete current archived BR0591 FIXED owns normalize_json.py parse constants/duplicate names and strict serialization. That repaired command is not called by the boss report writer; preserve its status and do not reopen it. BR0233 durable/generation publication, BR0173 process/temp ownership and BR0543 device/cleanup remain separate. Their remedies do not establish finite audio evidence. Current root-description keyword census parsed all306numbered GQF rows and relevant active/archived BR headings; no direct finite store-audio metric owner identified. This is discovery/dedup input, not complete semantic review of every ledger block; final constrained owner/root reconciliation remains required.

Prior0391 explicitly deferred this source follow-up to0396. Now actual writer and integrationcaller are traced. Do not claim an observed recipe/runtime failure or manufacture a corrupt encoded artifact in diagnosis. Preserve the real native music/sample/provenance checks and current rendering/format contracts.

## Proposed repair and acceptance

Define a focused audio-window admission boundary for the real boss validator. Require expected delivered sample cardinality, nonempty valid interior windows, finite RMS/correlation and an explicit zero/constant-variance policy that cannot treat absence of correlation evidence as success. Do not replace undefined correlation with1, clamp NaN or weaken the0.97/RMS thresholds to conceal unsupported evidence. If constant windows are supported, verify them through a documented independent amplitude/error contract; otherwise reject them clearly before validation success.

Require validation/report output to contain interoperable finite JSON, preferably strict serialization in the existing report helper with current callers checked. Preserve ordinary report shape and valid numbers. Do not add another generic media framework, mutate source/reference audio or add a fake validation override.

Future implementation must call the production validator/integration entry and cover ordinary finite matching audio, mismatching constant delivered audio, constant/zero expected windows, empty/short windows, missing/truncated decoded samples, nonsilent lowRMS, deliberate muted clips and cut-boundary AAC exclusion. Use valid PCM/native captures and actual existing decode/encode helpers where permitted; backend/malformed-media/fault probes remain separately deferred under the tranche. Require deterministic nonzero rejection with no new success report/PASS or stale success reuse, finite accepted metrics, preserved valid900frame/30second output, native MIDItempo and retained picture/master checks.

Provisional local rating47MEDIUM (23/0/4/10/10), P2/high for the conditional source acceptance defect: focused store-preview validator and real integration/report callers, authored paths absent1996, local finite/admission correction. This is a proposal, not canonical rating/admission or executed failure. Final confidence/owner/rating/IDs require consolidated qualification. No inherited saving.


## Final constrained source reconciliation, 2026-10-13

NativePASS225 qualifies bounded signed16 tap and complete current graphics/caller controls. Neither native sample format nor picture/native music/motion gates imposes positive waveform variance or finite delivered correlation. Exact actual validate360-415 and stdout435 were re-read; shared save_json and real pipeline caller remain qualified by entryPASS134. Undefined audio evidence can therefore escape this specific gate independently of correct metadata/pictures. This establishes a conditional source defect, not a demonstrated all-gates encoded artifact false pass.

Deduplication mechanically parsed all306 numbered general finding descriptions. All42 descriptions selected by audio/boss/store/finite/correlation/NaN/normalization/PCM/numeric terms were fully recovered without padding/truncation, plus the complete current GQR0283 dependency row. BR active/archive headings were searched; that search is discovery, not full-block semantic review. Six entire current relevant owners are separately qualified:0173/0233/0543OPEN and0199/0200/0591FIXED. Existing0145 threshold reconfiguration,0150 raw fingerprint fixture admission,0268 music-synth uninitialized observation tail and0297/0283 dependency provisioning have different trigger/producer/admission ownership. None establishes finite boss window evidence. Preserve their statuses and avoid broad duplicate serialization or resource roots.

Proposed canonical IDs GQF-0307 OPEN/GQR-0293 TODO are unused and will be admitted only with consolidated consumer and canonical publication gates. Final proposed rating47MEDIUM/P2high remains local23/0/4/10/10. No original-file saving or runtime acceptance. Full branch supplemental coverage and every pending campaign gate remain required.
