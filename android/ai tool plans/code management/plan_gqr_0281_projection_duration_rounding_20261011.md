# Proposed GQR-0281: projection duration rounding, 2026-10-11

Diagnosis and implementation plan only. Proposed GQF0295/GQR0281 require canonical admission with0365; no product/test change or runtime acceptance. Proposed P3 priority27 (H/M/B/C/R12/0/1/10/4): rare extreme duration, precise source arithmetic, tiny shared fix and ordinary host/JVM validation, no inherited saving.

## Exact source witness

MissionMetadataProjection.kt projectTrack71-72 reads positive Int duration_ms then computes(duration+500)/1000 as Int. Values above2147483147 overflow before division; resulting length_s is negative. Native midi_metadata.c duration_for_ticks321-342 accepts division1/end_tick4294967/default tempo500000us and yields2147483500ms within INT32_MAX. The projection addition yields negative2147483296, divided toward zero to negative2147483s; correct rounded output2147484s. This is source-derived arithmetic, not a generated/decoded media or executed CLI fixture. Direct valid JSON input to shared projection is sufficient to exercise the admitted Int contract later.

The host CLI Main.kt project operation reads raw_path then invokes shared projection; host Get-CheckedInMissionJson sends project requests. Android LauncherScriptExecutor.levelMetadataResultJson1257-1260 delegates nativeJson to the same projection. Native active-music JSON emits original duration_ms unchanged. Source current=frozen for projection; shared core absent at GQ2 base. Existing GQR0183 owns import size/reservation, GQF0177 owns Chromaprint narrowing, GQR0047/0207 own fingerprint ambiguity, GQR0218 native snapshot allocation and GQR0212 resource work/admission; none is this positive already-representable projection rounding root.

## Smallest implementation

Widen duration to Long before addition, use500L and1000L, and retain zero for nonpositive duration. Maximum rounded seconds2147484 fits existing consumers; preserve JSON numeric representation/schema, track name/format/file ordering and parse-status rules. Keep calculation inline in shared projection; no new wrapper/parser/compatibility layer or native duration change.

Allowed product path: mission-metadata-core/src/main/kotlin/com/dxxredux/app/MissionMetadataProjection.kt. Validation: existing MissionMetadataProjectionTest.kt and a focused actual CLI server project integration through existing host transport plus Android shared projection caller where required. Required runner/catalog registration only if adding a new top-level test. Native producer is context, not an implementation target.

## Acceptance

Use actual project entry with small JSON track records. Check0/negative,499/500/999/1000, maximum safe sum2147483147, first overflow2147483148, admitted native-example2147483500 and Int.MAX_VALUE2147483647; assert expected nonnegative rounded numeric length_s and unchanged name/filename/format/parse_status. Both last two round to2147484. Include no_tags/ok/failed parse cases, no duration and unavailable behavior. Do not mirror the overflowing expression in the oracle; state explicit expected values.

Send a real --server project request with an ordinary tiny raw JSON file, parse DXXKOTLIN response through maintained host adapter and require identical output to shared projection. Check repeated requests and ordinary mission metadata controls; do not require days-long audio, external services or pressure/security probes. Relevant scoped Kotlin quality/JVM and CLI integration checks when implementation authorized. Current diagnosis tranche runs none.
