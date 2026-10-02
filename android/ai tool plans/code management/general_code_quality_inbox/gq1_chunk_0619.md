# GQ1-CHUNK-0619 frozen survey, resumed 2026-10-01

## Scope fingerprint

- `game_data/mods/d2x-xl/convert_d2xxl_sounds.ps1`: L1-L352; frozen blob `ddea227a8b062c5d98d626a1cc5d449239939a85`

Scope SHA-256: `d9588dda921e54d48989f3358318cc88c75a2f0392b324ad4fe064280be87faa`

## Diff-minimization assessment

Retain per-game sound format and checked RIFF spans; complete sound pack status, private stages and source-bound publication

## Context and observations

Read all352 frozen primary lines, complete current delta empty. Currentblobddea227a8b062c5d98d626a1cc5d449239939a85. Full archived0633/0635 read from doneledger afteractive lookup confirmed absent. Full active0169/0233/0628/0634 reused617, fullshared pack/progresspolicy read617. Full maintained test_d2xxl_sound_format.ps1 read AND executed exit0PASS temp/gq619_sound_format.log; completepaired ds_load bodies readD1bmread289..337 andD2bmread268..316. Source-contract regression checks game-specific rates/names, call sites and unsupportedgame rejection; it is not actual playback or PhysFS runtime selection. Full game_data/mods/d2x-xl/test_wav_parser.ps1 read NOT executed; it creates system-temp GUIDfixture withoutrepo retention andtests valid3byte PCM, shortfmt,0xfffffff8declareddata and misalignedstereo. Existing GQF-0165/GQR-0152 registration/deadline owner retained from599, no duplicated test-gap finding. No real WAV/sourcearchive read, AcoustID/network,7Zip/fullpack conversion, nativebuild or emulator execution

Branch-added sound converter already reuses shared packlibrary for per-game naming/rate and shared checked7Zipprogresshelper. Paired engine remains narrow original ds_load path/rate contract; no inherited movementcandidate or new format abstraction. Completed0633 current D1 emits11025Hz Sounds/name.raw,D2 emits22050Hz Sounds/name.r22 andactual staticcontract passes. Default D2 source chooses44kHzfallback22kHz, native sample rate choosesr22vsraw; existing acceptance stillrequires played/sample identity rather than onlytexturecoverage. No change to engine compatibility or speculative D1 22kHzsupport

Current RIFF reads unsigned32-bitlengths, checkedwidebounds fordeclaredRIFF/header/payload/wordpadding/strictforwardprogress, rejects duplicatefmt/data, nonPCM/zerochannels/nonpositive sample rate/unsupportedbitdepth andmisalignedframes beforedataadmission. Preserve archived0635 repair; no current 0xfffffff8 infinite-loopregrowth. FileReadAllBytes pluscopiedPCM,doublearray/outputarray have no shared cumulativefile/live-memory/worklimit, declaredsamples/channel/rate notbounded beyondpositive format, but nativeapp boundeddecoder45 andimportadmission0018 remain separateconsumers. Existing shared resourcepolicy212 acceptance must include this hosttypedsource reader beforematerialization and fixedsourcearchive expansion; nohostOOM/giantfixture or measuredpeakclaimed. ConverterPCM linearinterpolation, channelaverage and24bitsigned littleendianhandling retained; focused soundformat test doesnot prove bit-exact conversion8/16/24mono/stereo/length/bounds, priorarchived realconversionevidence retained without rerun

Sound pipeline stillfixed systemtemp dxx_snd_convert_game recursivelydelete/recreate, noexclusivegeneration/finally, invokes7Zip andholdsfinalZIP outsideoutercleanup, so existing0634 exact stage/archive/streamowner unchanged. Missingselectedsubtree returnsnormal; every WAV/archiveentry failure caughtandcounted butsound functionfinalizesarchive andreportsDonewitherrors. Existing0169 complete per-source/per-game outcome andnonzero aggregate contract staysopen despitearchived0635 resolution wording asserting nonzero batchresult. Canonical current source continuesaftercatch anddoesnot throw basederrors; do not treat narrower parserfix asclosureofwholebatch. Existing0233 deletes priorfinalbeforecompression, exposespartialgeneration andclose/unexpectedexceptiondoesnot preserveprior. Fixmust validateone exact complete source/payloadinventory andretain oldarchive afterevery failed generation; no destructiveactualconversionprobehere

EntrybaseName ignorescase/collidinglogicalaliases andsourceextensions; fullnamespace/destinationmap636 acceptanceincludes soundnames and generatedREADMEentries ratherthan nativeacceptance byarchivecount. Provenance0628 missing source/tool/rate/actualchosen44vs22 subtree andconverterpolicy; generatedbytehashalone insufficient. Catalog-derived consumer names/nonempty soundpayload andsource-to-outputcounts needed beforecommit. System-temp rawsource/privatefinallyrepair634 andbounded admittedsource bytes212 coordinate with typedserializer/writer233; no newfindingadmitted. Analysis only,no productedits

## Clean dimensions and evidence gaps

Complete frozen primary and complete current delta were read. Evidence above distinguishes executed fixtures from static source and test reads; no APK build, manifest merge, Android instrumentation or hostile-peer integration was run. No inherited D1/D2 reduction credit

Provisional impact rating: 47 (H/M/B/C/R = 23/0/7/10/7); proposed owner: BR-0634

Coverage outcome: ISSUES
