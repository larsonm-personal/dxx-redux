# Input-demo trace diagnosis continuation, 2026-10-09

Diagnosis and plans only. GQ2-CHUNK-0114 covers nine paths, 623 review lines, 51 assigned changed hunks plus two whole files. Preserve concurrent save-cost and metadata-snapshot work. No source/test/script changes, builds, tests, runtime/device/security/media/allocation/resource probes, formatters, generators, staging or commits.

- [x] Read and bind exact frozen assigned scopes, current deltas, manifest and original attribution
- [x] Trace current recorder/writer, replay/start/RNG, object/world/AI and paired engine consumers
- [x] Inspect maintained fixtures and registrations without execution; separate source evidence from runtime acceptance
- [x] Reconcile canonical owners and write actionable implementation acceptance
- [x] Publish/import immutable report and independently audit coverage, identities, counts, ranks and attribution

## First checkpoint

All nine current assigned files equal the frozen head. The state-trace assignment is only hunks 1-4 of 13; later hunks are consumer context or later queue work, not assigned coverage. All nine original counterparts are absent, so this scope has no inherited saving. Manifest identity verified against the frozen campaign. HEAD is 8a730eb6aa1d34382b4a76e6f1f5abd3e18aa96c.

Recorder encoding moves validation to capture and stores canonical lines plus flags, with events last. The production writer validates bookends before truncating the target but writes stored lines without revalidating each line. Capture publishes controls, line and flags in separate push_back operations; pending pulses/events clear afterward. Investigate existing GQR-0176 aggregate-memory and exception-transaction ownership, rather than admitting a duplicate root from this observation.

The actual replay trace hook short-circuits frame/object/world writes and stops tracing on returned failure. Object-history advancement before a failed write therefore does not imply the caller retries a damaged delta history. Trace reset closes plain and compressed streams without reporting their close outcomes; trace start resets an existing session, opens the selected backend and writes metadata. Current write checks fwrite/fflush or gzwrite/gzflush. Reconcile existing BR-0209 required-output/completion ownership and actual terminal consumers before deciding acceptance; no runtime behavior is established here.

Paired level-entry call sites were located in d1/main/gameseq.c and d2/main/gameseq.c. Still read their actual ordering and restore return conventions, native-D1 weapon-order domains, guidebot routing and checkpoint collision metadata. Read the complete RNG header selection and full parser relationship, replay loading/publication, maintained object/RNG/recorder fixtures and registration. Canonical owner/status bindings remain pending.

## Exact assigned identities

Frozen base: 7877ad30d05887b8e19869ed4c50075e41e2f88e
Frozen head: b4997ac4115a3b2ac6d6cf0b8e1459675a7744ef
Original: fb555eec75e1ed12c8348805ab335afb4c721b06
Manifest SHA256: e98f92caee38b30d0eebdd558e5739a7dd1ea58a025700914e8d51030a941bd4

```json
[
  {
    "path": "android/app/src/main/cpp/shared/input_demo_object_trace.cpp",
    "base_path": "android/app/src/main/cpp/shared/input_demo_object_trace.cpp",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_object_trace.cpp"
    ],
    "assigned_first": 1,
    "assigned_last": 216,
    "whole_frozen_file_read": true,
    "assigned_hunks_read": [],
    "frozen_lines": 216,
    "whole_frozen_source_lf_sha256": "1f44167ae4b9bbf4fc7afac5f50df1d03f8552954d6418f64b596acd77386447",
    "whole_frozen_diff_lf_sha256": "b14f1f0e684ec8f28941fbefb8e52c967d5429db6cbf98db6448d2dfaa5ce559",
    "assigned_payload_lf_sha256": "1f44167ae4b9bbf4fc7afac5f50df1d03f8552954d6418f64b596acd77386447",
    "hunks": [],
    "current_whole_source_lf_sha256": "1f44167ae4b9bbf4fc7afac5f50df1d03f8552954d6418f64b596acd77386447",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "ABSENT",
    "head_blob": "181df1024b3338c1cc89b091a7c8bdea1714060d",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_object_trace.h",
    "base_path": "android/app/src/main/cpp/shared/input_demo_object_trace.h",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_object_trace.h"
    ],
    "assigned_first": 1,
    "assigned_last": 23,
    "whole_frozen_file_read": true,
    "assigned_hunks_read": [],
    "frozen_lines": 23,
    "whole_frozen_source_lf_sha256": "a2fdd333e90ed1159aea825948f28efcada258446021971b00f4c5c7c8fcb4a0",
    "whole_frozen_diff_lf_sha256": "8c9b0d3764382c0c691eb59b614bbad307c59c5c0a4d4037aef21fd847d98463",
    "assigned_payload_lf_sha256": "a2fdd333e90ed1159aea825948f28efcada258446021971b00f4c5c7c8fcb4a0",
    "hunks": [],
    "current_whole_source_lf_sha256": "a2fdd333e90ed1159aea825948f28efcada258446021971b00f4c5c7c8fcb4a0",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "ABSENT",
    "head_blob": "a248bf82bb32900d25fbff9e026cd7499983d567",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_recorder.cpp",
    "base_path": "android/app/src/main/cpp/shared/input_demo_recorder.cpp",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_recorder.cpp"
    ],
    "assigned_first": 23,
    "assigned_last": 724,
    "whole_frozen_file_read": false,
    "assigned_hunks_read": [
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,
      11,
      12,
      13,
      14,
      15,
      16,
      17,
      18,
      19,
      20
    ],
    "frozen_lines": 741,
    "whole_frozen_source_lf_sha256": "fd3164fa33af92d03fe701dae9221e292313f5f6f5875ba145613a629d33d9e5",
    "whole_frozen_diff_lf_sha256": "c0dc0968364798c6cfb507aaa890a06923b17a1642b3ec72fc1a109a1d12ead7",
    "assigned_payload_lf_sha256": "1e65df44f92aefc5973dc4a6c0e850bde76abeec6ae2918cf60b8c7044e01cb0",
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -22,0 +23,5 @@ using ordered_json = nlohmann::ordered_json;",
        "raw_hunk_lf_sha256": "f30f4b7682563270775ccca71ee222b081cab2aff1857a28b12d2b545c8a8db1"
      },
      {
        "ordinal": 2,
        "header": "@@ -64,6 +69,2 @@ struct input_demo_recorder_session {",
        "raw_hunk_lf_sha256": "6a710ed9fb55ba051d5f8cdf020b271da7fc655d0f3cd9dc4e3a7093d6054850"
      },
      {
        "ordinal": 3,
        "header": "@@ -162,11 +163,2 @@ static int input_demo_recorder_stage_direct_command_event(const ordered_json &ev",
        "raw_hunk_lf_sha256": "79e2692ac429a1719a0bcc677dbee2acd7716018eeccdbf32bb1459b735da097"
      },
      {
        "ordinal": 4,
        "header": "@@ -174,6 +166,10 @@ static bool input_demo_recorder_session_has_diag(const input_demo_recorder_sessi",
        "raw_hunk_lf_sha256": "c7ad7bcf8ed8ca8bb54c61f956e109f4c8b9bb02a5da95a30d044c18cfe5fbb8"
      },
      {
        "ordinal": 5,
        "header": "@@ -294,4 +290,4 @@ static void input_demo_recorder_build_result(input_demo_result *result,",
        "raw_hunk_lf_sha256": "f0641c3195d5f6126ea94940b0f024eaf9a371dedb747f1bbbf6809be3d9b417"
      },
      {
        "ordinal": 6,
        "header": "@@ -299,6 +294,0 @@ static bool input_demo_recorder_build_demo(input_demo_file *demo,",
        "raw_hunk_lf_sha256": "9f060100cabba631a077013be1e9cd7542bef6d29b13cff9b80fe7ec3c84bac3"
      },
      {
        "ordinal": 7,
        "header": "@@ -307,4 +297,6 @@ static bool input_demo_recorder_build_demo(input_demo_file *demo,",
        "raw_hunk_lf_sha256": "5ba938acd07260daa0b2836e0d68beab3fe6d15eae36b8031ad350869e3cf1b6"
      },
      {
        "ordinal": 8,
        "header": "@@ -333,37 +325 @@ static bool input_demo_recorder_build_demo(input_demo_file *demo,",
        "raw_hunk_lf_sha256": "a94293d71abb3937d4e339c8abe130cae71228df4728102f02cb521d9c860e11"
      },
      {
        "ordinal": 9,
        "header": "@@ -422,6 +378,2 @@ int input_demo_recorder_truncate(uint32_t frame_count)",
        "raw_hunk_lf_sha256": "98c0269b736bbc2a4f2e34cb9f3c9fccf3ef2588ebd430ae504b4c4b26d9903e"
      },
      {
        "ordinal": 10,
        "header": "@@ -507,5 +459,4 @@ int input_demo_recorder_capture_frame(int32_t frame_time,",
        "raw_hunk_lf_sha256": "7b37a95f2cef54fc2a2bc9b112bb755e466d4941deb5586fc73c60bab33eeddc"
      },
      {
        "ordinal": 11,
        "header": "@@ -513 +464 @@ int input_demo_recorder_capture_frame(int32_t frame_time,",
        "raw_hunk_lf_sha256": "2a80e7df2619047bf293e40d7abe3aab3319a5f9e31a66372f32dc0402dd253c"
      },
      {
        "ordinal": 12,
        "header": "@@ -519 +470 @@ int input_demo_recorder_capture_frame(int32_t frame_time,",
        "raw_hunk_lf_sha256": "bcab54ba7719d2e2b72902e1cdd8b772855ea24dfbad8d788bb23c01526bd0bf"
      },
      {
        "ordinal": 13,
        "header": "@@ -522 +473 @@ int input_demo_recorder_capture_frame(int32_t frame_time,",
        "raw_hunk_lf_sha256": "16b124329e49ffa435c3e611ba819314c87938a531ded017a32940d62143a36b"
      },
      {
        "ordinal": 14,
        "header": "@@ -524 +474,0 @@ int input_demo_recorder_capture_frame(int32_t frame_time,",
        "raw_hunk_lf_sha256": "8cae6e99a8c8c60ea5b12491f27dbe7551e958feba9b829bd80ac76107ad730f"
      },
      {
        "ordinal": 15,
        "header": "@@ -526,21 +476,42 @@ int input_demo_recorder_capture_frame(int32_t frame_time,",
        "raw_hunk_lf_sha256": "c2622249bc69bc722e09154a0a1d48ea79e1be48110e962bd1ba1e3c87c8a5c1"
      },
      {
        "ordinal": 16,
        "header": "@@ -580 +551 @@ int input_demo_recorder_append_frame_event_json(const char *json_text,",
        "raw_hunk_lf_sha256": "6efca25c0629e6b33dcb86114451ccaaf35881d346b592087221d539e8b43a45"
      },
      {
        "ordinal": 17,
        "header": "@@ -584 +555,4 @@ int input_demo_recorder_append_frame_event_json(const char *json_text,",
        "raw_hunk_lf_sha256": "c4dac4f6c775eed2fc35383eae660d7042d00fbc153bc13e2c3c2e33ccb0a8f1"
      },
      {
        "ordinal": 18,
        "header": "@@ -746 +720 @@ int input_demo_recorder_flush_with_result(const char *demo_path,",
        "raw_hunk_lf_sha256": "606c992cd1d66d0af5d52d7036de36cd2d757e5e2e920ff53ddac01e846df885"
      },
      {
        "ordinal": 19,
        "header": "@@ -748,9 +722 @@ int input_demo_recorder_flush_with_result(const char *demo_path,",
        "raw_hunk_lf_sha256": "7c947afbf30c82f4d2c91ba6883b65b3d295855df0c67631923ddc914ae422d8"
      },
      {
        "ordinal": 20,
        "header": "@@ -758 +724 @@ int input_demo_recorder_flush_with_result(const char *demo_path,",
        "raw_hunk_lf_sha256": "fee341a9d699d145eddfcb4d647c6689b711efba4e10856b2ca1a938ae286e42"
      }
    ],
    "current_whole_source_lf_sha256": "fd3164fa33af92d03fe701dae9221e292313f5f6f5875ba145613a629d33d9e5",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "4aae67d2eb6a9f3b00937a4cbcc2fc589adcc02a",
    "head_blob": "c977156020655c8171e81c8be9ff41388dd47ac4",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_recorder.h",
    "base_path": "android/app/src/main/cpp/shared/input_demo_recorder.h",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_recorder.h"
    ],
    "assigned_first": 16,
    "assigned_last": 41,
    "whole_frozen_file_read": false,
    "assigned_hunks_read": [
      1,
      2
    ],
    "frozen_lines": 105,
    "whole_frozen_source_lf_sha256": "755bdbcd8a212f8748ba773d130f81bd899d2938e255e6aacd58668300e99aa8",
    "whole_frozen_diff_lf_sha256": "809ed8037cec9af6586db65f800ce92c49736077bf72a6afe39fa9d64dc48670",
    "assigned_payload_lf_sha256": "a319eddd1cc16eaba9e507c954c3528bd3691ab56f27087700408acc8f409f72",
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -15,0 +16,4 @@ extern \"C\" {",
        "raw_hunk_lf_sha256": "658dd59b7c413506c0537330187977c6fd7719c1d1eb3c57f53c7a01c8997d23"
      },
      {
        "ordinal": 2,
        "header": "@@ -34,0 +39,3 @@ typedef struct input_demo_recorder_settings {",
        "raw_hunk_lf_sha256": "cd1924f6f97f5b03207359fa10038456b915fb6e020e17b7d470f50b775d6789"
      }
    ],
    "current_whole_source_lf_sha256": "755bdbcd8a212f8748ba773d130f81bd899d2938e255e6aacd58668300e99aa8",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "7b4f76d4f0a255d936ee60daadeb5899de01345b",
    "head_blob": "0d93f086429205e73588dea833aacc05aebc596e",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_replay.cpp",
    "base_path": "android/app/src/main/cpp/shared/input_demo_replay.cpp",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_replay.cpp"
    ],
    "assigned_first": 8,
    "assigned_last": 545,
    "whole_frozen_file_read": false,
    "assigned_hunks_read": [
      1,
      2,
      3
    ],
    "frozen_lines": 818,
    "whole_frozen_source_lf_sha256": "cc32d8938e6bd8d751ddb94907b549b722e88b7d37c7aac2d2ab6ff626f75390",
    "whole_frozen_diff_lf_sha256": "ddb81e2e0401411eb6dcbc0c4f1606de9cf2613ad00dea296d0dec8acd1b3d7f",
    "assigned_payload_lf_sha256": "4e7b5da32fed16e09c4e4086262b0f8790f4fa29d89946099bef042d89edd838",
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -7,0 +8 @@",
        "raw_hunk_lf_sha256": "5bbda88f3c57ce18e6e31e2dfab238c600ef0568f20d62ec0fdb490cb2407107"
      },
      {
        "ordinal": 2,
        "header": "@@ -541,2 +542,2 @@ int input_demo_replay_load(const char *demo_path, char *error, size_t error_size",
        "raw_hunk_lf_sha256": "528182cbc9604faf80cb25416ae5afff3f061e9f4ea91ec7b527277b399e1861"
      },
      {
        "ordinal": 3,
        "header": "@@ -544 +545 @@ int input_demo_replay_load(const char *demo_path, char *error, size_t error_size",
        "raw_hunk_lf_sha256": "925f9dbc9c55fbaef871edb33e8a6e995299eb46be4f66ad4d83230bf3e7fb94"
      }
    ],
    "current_whole_source_lf_sha256": "cc32d8938e6bd8d751ddb94907b549b722e88b7d37c7aac2d2ab6ff626f75390",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "143c5e8871d824c6c7b947f119de434310f12302",
    "head_blob": "cdac9e6ba9a00b3135cfb8cf0890557cbe1b7877",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_rng_mode.c",
    "base_path": "android/app/src/main/cpp/shared/input_demo_rng_mode.c",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_rng_mode.c"
    ],
    "assigned_first": 31,
    "assigned_last": 187,
    "whole_frozen_file_read": false,
    "assigned_hunks_read": [
      1,
      2,
      3,
      4,
      5,
      6
    ],
    "frozen_lines": 193,
    "whole_frozen_source_lf_sha256": "8f534c9b2241b4233603fb2f6f36a26f29cdbd38c0f4c8e1b1d5679415f28e6f",
    "whole_frozen_diff_lf_sha256": "bc9ae55cd379ebdf4be335a32419200d49d3ac7d39f819d79c4f12fe7507fb15",
    "assigned_payload_lf_sha256": "4a6cda02ab56ce839c1ef48351e15a1aa82a1fcbf823304e0660ffe783cbda63",
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -31 +31,2 @@ static const char *find_rng_mode_key(const char *text)",
        "raw_hunk_lf_sha256": "70e458758f0bce43a6237b0831089ee16b4a93e1434d8bffe40945cdf1b43fb5"
      },
      {
        "ordinal": 2,
        "header": "@@ -36,2 +36,0 @@ static const char *read_text_file(const char *path, char **text_out)",
        "raw_hunk_lf_sha256": "084b3307099f7d07d0602ae8fbc08c2a33040699f9805f9db4542d0611585b32"
      },
      {
        "ordinal": 3,
        "header": "@@ -61 +60,2 @@ static const char *read_text_file(const char *path, char **text_out)",
        "raw_hunk_lf_sha256": "5016cd8c90550e7dcf08f487466075429d9ce1586714731ba57d7f15dafae4f5"
      },
      {
        "ordinal": 4,
        "header": "@@ -66,5 +66,16 @@ static const char *read_text_file(const char *path, char **text_out)",
        "raw_hunk_lf_sha256": "a863f1ebbaca7cd52e60a5fb6949c873e5ef7930870a6cae626b23dc15518694"
      },
      {
        "ordinal": 5,
        "header": "@@ -72,3 +83,4 @@ static const char *read_text_file(const char *path, char **text_out)",
        "raw_hunk_lf_sha256": "be5bc4b1a40a7ecbd27ab3640022afe63352edc1644da9e686a4776a5a6a1a98"
      },
      {
        "ordinal": 6,
        "header": "@@ -175 +187 @@ const char *input_demo_rng_mode_validate_metadata_file(const char *path, int eng",
        "raw_hunk_lf_sha256": "04c91a4f06ced038645c48e8f80c9b1d2b10643e2e0c81d34ee085100bf66988"
      }
    ],
    "current_whole_source_lf_sha256": "8f534c9b2241b4233603fb2f6f36a26f29cdbd38c0f4c8e1b1d5679415f28e6f",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "d26aa1ce6c3de2876e2e38e5335051d6b093daed",
    "head_blob": "b100375bf4dfe663c40fb5440ac16b07e416ea17",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_start_shared.c",
    "base_path": "android/app/src/main/cpp/shared/input_demo_start_shared.c",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_start_shared.c"
    ],
    "assigned_first": 13,
    "assigned_last": 821,
    "whole_frozen_file_read": false,
    "assigned_hunks_read": [
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,
      11,
      12,
      13,
      14,
      15
    ],
    "frozen_lines": 830,
    "whole_frozen_source_lf_sha256": "1b33586c098e739c9fe2ba6238ccaaf11dd676bd8c329a5822531703f634f44c",
    "whole_frozen_diff_lf_sha256": "0a94e4c4b8a6fb88643bd762305b77a43fde45d529056d22c066209fc6a6dfdd",
    "assigned_payload_lf_sha256": "97f602bd1a9711df9d8771b6816dc1af9410df4e34bc1b8d36181b2fe5d12718",
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -12,0 +13 @@",
        "raw_hunk_lf_sha256": "b213fd745fb36c3576bac68fe005368ca9eb6cf9ca75fe53d6fdac13e02f8e72"
      },
      {
        "ordinal": 2,
        "header": "@@ -21,0 +23 @@",
        "raw_hunk_lf_sha256": "98aafefd0e63734140061bc36b91e2975d6b53a7410d3ef2fecaec9783806daf"
      },
      {
        "ordinal": 3,
        "header": "@@ -31,0 +34 @@",
        "raw_hunk_lf_sha256": "0186e13854cca9452f11cbdc57c2fd5ee06ef714a361e83265f770f0860fcda6"
      },
      {
        "ordinal": 4,
        "header": "@@ -45 +48 @@",
        "raw_hunk_lf_sha256": "1ead3436366040c97dc1c0797bb4ada2ea58794a2ecd5db0b54da4bf339878cc"
      },
      {
        "ordinal": 5,
        "header": "@@ -176,0 +180 @@ static void input_demo_apply_replay_player_cfg(const input_demo_player_cfg *play",
        "raw_hunk_lf_sha256": "2d7a05ff3285b0e15b2db64e86b1bd1e2676802346aa5d7f5eeaac1206e2978b"
      },
      {
        "ordinal": 6,
        "header": "@@ -178,4 +182,19 @@ static void input_demo_apply_replay_player_cfg(const input_demo_player_cfg *play",
        "raw_hunk_lf_sha256": "f5817275db046949077aa02c666733eadffc51db721cb2b4e112f2530c1bc8a2"
      },
      {
        "ordinal": 7,
        "header": "@@ -188,0 +208,13 @@ static void input_demo_apply_legacy_replay_homing_default(void)",
        "raw_hunk_lf_sha256": "bb5e2ef05ad4fb6e076ef18ec31d426d73aa10d52621fe967bd52792b45ab88f"
      },
      {
        "ordinal": 8,
        "header": "@@ -563 +595 @@ static int input_demo_start_replay_new_level(",
        "raw_hunk_lf_sha256": "e381762ba027f2e5540dbca2b0fb303f5f1f526c972d81551cb870d91c03b73c"
      },
      {
        "ordinal": 9,
        "header": "@@ -586,0 +619,3 @@ int input_demo_start_loaded_replay_common(void)",
        "raw_hunk_lf_sha256": "ce1688d92dfb21438c5b3f6c5cc1f36be17e7c2bf11855beb75ad33e5b19019b"
      },
      {
        "ordinal": 10,
        "header": "@@ -689,0 +725,15 @@ int input_demo_start_loaded_replay_common(void)",
        "raw_hunk_lf_sha256": "dae045469b5eafd6d0a15137d713d6e8bf47b270ff145f7dd3be5dc3d6247dde"
      },
      {
        "ordinal": 11,
        "header": "@@ -724,0 +775,6 @@ static void input_demo_capture_restored_player_diag(",
        "raw_hunk_lf_sha256": "0db7675644afd9d23517d25d15fd121aa5482e92866e55c18865fb5abc225963"
      },
      {
        "ordinal": 12,
        "header": "@@ -748 +804 @@ static void input_demo_log_restored_player_diag(",
        "raw_hunk_lf_sha256": "1c45f31d366875f32d69295614dd6b7dd7d95b93bd84ae989539d8e8222a801b"
      },
      {
        "ordinal": 13,
        "header": "@@ -752 +808 @@ static void input_demo_log_restored_player_diag(",
        "raw_hunk_lf_sha256": "e3e147f2e3bfc1e5a9b3302bb35aa5f66f2f59e49432f07a3fc91e55b9f2b658"
      },
      {
        "ordinal": 14,
        "header": "@@ -761 +817 @@ static void input_demo_log_restored_player_diag(",
        "raw_hunk_lf_sha256": "ddf86dde8ac2d518bc16425eb0f4545c40f82ed96a4677a5e2ac7426ab23ce48"
      },
      {
        "ordinal": 15,
        "header": "@@ -765 +821 @@ static void input_demo_log_restored_player_diag(",
        "raw_hunk_lf_sha256": "5ad0fa29b1f3a6e6c9c29602555f325b7c99cdefa8512dc8270357fc472d82ee"
      }
    ],
    "current_whole_source_lf_sha256": "1b33586c098e739c9fe2ba6238ccaaf11dd676bd8c329a5822531703f634f44c",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "d99cf1269e7281ae12f0e582ef0eb99ad5049e4a",
    "head_blob": "171e67c3a8e2d13e9ffbbaac860a75860dbfed10",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_start_shared.h",
    "base_path": "android/app/src/main/cpp/shared/input_demo_start_shared.h",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_start_shared.h"
    ],
    "assigned_first": 46,
    "assigned_last": 46,
    "whole_frozen_file_read": false,
    "assigned_hunks_read": [
      1
    ],
    "frozen_lines": 65,
    "whole_frozen_source_lf_sha256": "b2ee9c13e25669b829763b342ee35c5f4175b0599363a0e5631c2c60c0acc222",
    "whole_frozen_diff_lf_sha256": "6a9cef90a336fcb8cec1374f23db0ec633e6e6a2da4f2f5ba59c5b6f57ae552b",
    "assigned_payload_lf_sha256": "de38627e791271d6536d1a2850b844e4d68b53695b715e9e85f72cbd698a84ac",
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -45,0 +46 @@ void input_demo_set_skip_level_intro(int skip);",
        "raw_hunk_lf_sha256": "de38627e791271d6536d1a2850b844e4d68b53695b715e9e85f72cbd698a84ac"
      }
    ],
    "current_whole_source_lf_sha256": "b2ee9c13e25669b829763b342ee35c5f4175b0599363a0e5631c2c60c0acc222",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "9fa8a7f56deaba194172fb7b6c5a26d9a4154ddf",
    "head_blob": "ff4960d8b84537475eee310c344f2da16e4f2317",
    "original_blob": "ABSENT"
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_state_trace.cpp",
    "base_path": "android/app/src/main/cpp/shared/input_demo_state_trace.cpp",
    "frozen_diff_paths": [
      "android/app/src/main/cpp/shared/input_demo_state_trace.cpp"
    ],
    "assigned_first": 7,
    "assigned_last": 49,
    "whole_frozen_file_read": false,
    "assigned_hunks_read": [
      1,
      2,
      3,
      4
    ],
    "frozen_lines": 580,
    "whole_frozen_source_lf_sha256": "5878c9430a884ca186eb8ca8add9517b9f8945d855f05fd5bf22788a2c1ffebb",
    "whole_frozen_diff_lf_sha256": "5ff48710c0f5d83dd81bca791628e941ca0697b605f20c060e5db131dac35e2f",
    "assigned_payload_lf_sha256": "236d4531349d79dfb085bbed751cab2e61dfce2fbf31695460944db2e94af249",
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -6,0 +7 @@",
        "raw_hunk_lf_sha256": "f985853f3f2194663e15b2eb9fc56618aa55d00dfeecf31053bcc79b074d5c85"
      },
      {
        "ordinal": 2,
        "header": "@@ -27,0 +29 @@ typedef struct input_demo_state_trace_session {",
        "raw_hunk_lf_sha256": "12fdaeea4aa59f20e9a2ee1594788611fa6771fcc69d8c22dde4c9668200a2d0"
      },
      {
        "ordinal": 3,
        "header": "@@ -40,39 +41,0 @@ static int copy_error(const char *message, char *error, size_t error_size)",
        "raw_hunk_lf_sha256": "a5913e486ee68488ea565d4b161f2ab17e0779837a5985f8e1da6e24b55f7069"
      },
      {
        "ordinal": 4,
        "header": "@@ -84,0 +48,2 @@ static void reset_session(void)",
        "raw_hunk_lf_sha256": "873f421c3d6832784cbf1550882a032cd4dc2edd8fa068098896d0c2f38d2d34"
      }
    ],
    "current_whole_source_lf_sha256": "5878c9430a884ca186eb8ca8add9517b9f8945d855f05fd5bf22788a2c1ffebb",
    "current_delta_lf_sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "current_delta_bytes": 0,
    "current_delta_whole_read": true,
    "base_blob": "99bddf6c5535fedd29e40b32681752ba89c03804",
    "head_blob": "340c1f4b67a5366e7b353c3b37cf1f8336fa9b10",
    "original_blob": "ABSENT"
  }
]
```

## Current physical reads in this checkpoint

Only the listed current ranges were physically read. Frozen source reads and current equality do not expand this credit. Merge with earlier checkpoints only after revalidating their identities.

```json
[
  {
    "path": "android/app/src/main/cpp/shared/input_demo_state_trace.cpp",
    "current_lines": 580,
    "current_whole_source_sha256": "5878c9430a884ca186eb8ca8add9517b9f8945d855f05fd5bf22788a2c1ffebb",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 180,
        "range_lf_sha256": "fac57f01fdea90bd1c793da0b97253fc6ad3dcba61c00b09c2de7235984d90b6"
      },
      {
        "first": 449,
        "last": 580,
        "range_lf_sha256": "1d79d26c177cc2b5b995d31b2471f51b01a3bd68739dd752da05269564c788d3"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_fixture.cpp",
    "current_lines": 1448,
    "current_whole_source_sha256": "1c71d445a5632f2775bdb738f9c8dfd0fcf108c9c7f78f0d56492833916b79d4",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1142,
        "last": 1219,
        "range_lf_sha256": "4cfa41ca545de425cb8c25281843650cc634bff3e6c9ffc6849c8656ce638797"
      },
      {
        "first": 1407,
        "last": 1448,
        "range_lf_sha256": "35101bf14f9908a98e7026405e0176e73800ccfecbdf1738f5e106345d956787"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_hooks_shared.c",
    "current_lines": 2357,
    "current_whole_source_sha256": "81fa8f08eb48b2129a98d62620ea51e25c58c8354d67cc71f6c7cda840d43164",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 740,
        "last": 799,
        "range_lf_sha256": "a92c934c215887d29c2b1714f80ecd5ab7e29d6319429f9f43f215339c8690c3"
      }
    ]
  }
]
```

Chunk 0114 remains TODO. No immutable report, canonical completion or new finding is claimed. The overall diagnosis goal remains active.

## Second current-consumer checkpoint

Full current RNG mode reader (193 lines) read. It still checks total file size before allocating a record-sized header buffer, skips blank/comment records, and uses a lexical rng_mode key scanner accepting quoted values. This helper is not the complete JSON parser; caller/full-parser ordering remains required before claiming metadata admission or output preservation. No malformed-input probe was performed.

Current replay load lines 465-570 read. Full fixture read, control/RNG expansion, positive frame times and checked accumulated duration precede reset_session. After reset it publishes loaded=true before allocating/copying metadata, loading checkpoint data and resizing frame_events. The moved frame count is used correctly after the move; frame events move from the parsed demo. Defined checkpoint/timing errors reset the new session. Exception and aggregate-resource acceptance require the existing owner; do not infer rollback from those ordinary return paths.

Paired level-entry ranges confirm replay player configuration is reapplied after highest-level bookkeeping. D2 explicitly reloads its player file on the alternate multiplayer/cheat branch immediately before this hook. The inspected D1 range lacks that alternate read; read set_highest_level itself before asserting its reload effect. These are engine consumer context, not new inherited diff-minimization assignments.

```json
[
  {
    "path": "android/app/src/main/cpp/shared/input_demo_rng_mode.c",
    "current_lines": 193,
    "current_whole_source_sha256": "8f534c9b2241b4233603fb2f6f36a26f29cdbd38c0f4c8e1b1d5679415f28e6f",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 193,
        "range_lf_sha256": "8f534c9b2241b4233603fb2f6f36a26f29cdbd38c0f4c8e1b1d5679415f28e6f"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_replay.cpp",
    "current_lines": 818,
    "current_whole_source_sha256": "cc32d8938e6bd8d751ddb94907b549b722e88b7d37c7aac2d2ab6ff626f75390",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 465,
        "last": 570,
        "range_lf_sha256": "75e95642167a8112df3e43a3196faf8bd79417159f8285153e5b2a5e4a5f8b42"
      }
    ]
  },
  {
    "path": "d1/main/gameseq.c",
    "current_lines": 1901,
    "current_whole_source_sha256": "edaddd98ebe9e26234394a7ff4a6339bb82c9419dc3371dc51206afdb960e914",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1488,
        "last": 1520,
        "range_lf_sha256": "57b6026c36c598694534bca745c259bade24b2419818b8abe0f1ebcb8f4138bb"
      }
    ]
  },
  {
    "path": "d2/main/gameseq.c",
    "current_lines": 2636,
    "current_whole_source_sha256": "4357f34f12df2f853d91b9f43b75b4b8d642528a976e35e72184a0fd0b19c69e",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 2090,
        "last": 2125,
        "range_lf_sha256": "596a1606fb796998c63f64f0e09670f22b25fb53ec9ea4b72b827aa018b2913c"
      }
    ]
  }
]
```

## Recorder, restore and fixture checkpoint

Recovered complete ranges after a truncated combined tool response; only the recovered ranges below receive physical credit. HEAD unchanged. Earlier checkpoint physical identities revalidated before this append. No numbered completion or runtime acceptance is claimed.

- Recorder start rejects ordinary invalid settings before resetting, but starts RNG tracing and sets active before allocating session strings/checkpoint storage. Truncate shrinks controls, encoded lines and extended flags and clears pending events/pulses. Returned capture failures before the three pushes preserve pending data; allocation exceptions between pushes or during in-place late-event append do not have an inspected complete transaction boundary. Flush preserves the session for envelope/demo writer errors; after a written demo, RNG sidecar failure resets it and returns an explicit partial-success error. Successful flush also resets it. Preserve these distinctions in later failure handling.
- Replay loading checks RNG metadata first, then invokes the full fixture parser, then checks game identity. The standalone -inputdemo-validate path uses only header RNG validation and prints an OK message without running the full parser. This is a scope limit of its current contract, not a newly admitted root. Reconcile its advertised use and existing validation owner before changing it.
- Both engines' set_highest_level call read_player_file before updating and writing progress. Replay configuration reapplication therefore follows real pilot reloads. D2's native-D1 domain stores 7 primary and 6 secondary order entries in PlayerCfg.D1WeaponOrder. Its setter checks exact counts and complete unique membership before copying. This differs from the D2 pilot order bank; the assigned replay hook selects the native bank by primary count. Full parser order/count coupling still needs inspection.
- New-level replay loads the mission and validates level/difficulty before StartNewGame. Native checkpoint restoration owns a generated temporary path, checks write and close, restores the save, then checks removal; errors unload replay. The outer native path verifies restored mission/level/difficulty. Translated D1 checkpoints instead parse metadata, load their mission, start a new game, restore time/difficulty history, then apply objects/player and return zero. These paths mutate before some later failures; do not claim atomic rollback. The common wrapper uses zero-success and applies checkpoint collision metadata then emits a restored boundary for either successful path.
- Guidebot restore prefers recorded routing policy, validates it or falls back to defaults, sets private routing, updates cooperative Netgame policy only for the master, then resets navigation under Android/planner guards. The earlier pre-restore routing call is not proof that save parsing cannot alter runtime state; inspect actual save consumers if needed.
- Maintained 3000-frame recorder fixture checks first-only frame time, diagnostics/state/RNG per frame, staged then two late events, truncation dropping the last frame's events, open-directory failure preserving an active session, successful retry and full parser round trip. It measures times but does not assert a performance ceiling or inject allocation/write/close failures. Plain/gzip object fixture checks unchanged-slot elision, orientation/rotthrust/final weapon hit-list entry, tombstone deletion, reused-slot replacement, allocator capacity, both RNG streams and unchanged RNG call counts. Its reader checks gzip completion; this is not producer-close failure coverage.
- The full RNG fixture checks matching/missing/legacy modes, comment/blank header skipping, ignoring modes in later records, oversized header rejection and compile-time backend compatibility. These tests were read, not run. Policy pack1/pack16 fixtures concern the direct-command policy header, not recorder-settings packing; do not transfer their ABI proof to this recorder change. CMake directly includes production recorder/fixture/replay/trace/RNG sources and lists RNG/recorder tests in its registered test list; final registration-loop and upstream object-test invocation context remain to inspect.

## Existing owners and proposed acceptance

Fresh canonical bindings confirm GQR-0176 TODO / GQF-0189 OPEN, priority 56 MEDIUM-HIGH (32/0/7/10/7). Preserve one aggregate/resource and exception-publication owner across recorder, fixture, replay and trace. Later implementation should budget retained controls, encoded lines, diagnostics, pending/late events, parsed frames, checkpoint bytes, sidecar expansion and tracing snapshots before growth. Stage session/frame/event publication or define a contained failed session; reject without uncontrolled exceptions across C boundaries. Acceptance needs production allocator accounting, compact and event/diagnostic-heavy recordings at boundaries, ordinary long controls, and failure at each publication/growth/append point, with exact prior-output/session assertions. Allocation/resource probes remain deferred in this tranche.

BR-0209 remains OPEN for observed replay completion and required-output success. Preserve the source evidence that trace hooks currently stop the stream and return void to callers, and reset drops plain/compressed close outcomes. Later implementation must carry first failure and actual completed cursor/clock through paired stepping, finish and headless result; required trace/result open, write, flush and close failures must reach the process and wrapper. Use actual completed state and exact terminal-boundary controls, not declared totals. Read final consumers again before implementing the historical subclaims in this owner.

Preserve GQR-0245 DONE / GQF-0259 FIXED. Its historical robot-frame homing repair is distinct from this object/recorder scope and receives no new runtime credit here. No new finding, status/rating change or inherited reduction is admitted by this checkpoint.

Remaining for gates 2-4: complete recorder checkpoint/result helpers and event paths, full fixture order/count and version/record-size admission, recorder-settings packing callers, AI/world snapshot caller bounds, replay/start/trace terminal consumers, and maintained fixture/registration details. Then consolidate exact evidence and implementation acceptance before report/import/terminal audit.

## Additional current physical reads

```json
[
  {
    "path": "android/app/src/main/cpp/shared/input_demo_recorder.cpp",
    "current_lines": 741,
    "current_whole_source_sha256": "fd3164fa33af92d03fe701dae9221e292313f5f6f5875ba145613a629d33d9e5",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 180,
        "range_lf_sha256": "c4ac12432d01f83886aa799b4415184b4aa14d725cddcaaff18145ffabd3b7c1"
      },
      {
        "first": 290,
        "last": 570,
        "range_lf_sha256": "b5e251009898efbae4915a948e35cad2095a11a5d07561464f055848441e0f74"
      },
      {
        "first": 690,
        "last": 741,
        "range_lf_sha256": "7439d17faadf79133fdfb70a1405eafc6accc04b4d82bdbd88a79de3b3260c79"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_start_shared.c",
    "current_lines": 830,
    "current_whole_source_sha256": "1b33586c098e739c9fe2ba6238ccaaf11dd676bd8c329a5822531703f634f44c",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 160,
        "last": 320,
        "range_lf_sha256": "491071c94f0167fd0dde64aa2cbacca04bcf874be4b510f188116bb1c8344761"
      },
      {
        "first": 425,
        "last": 742,
        "range_lf_sha256": "92a97cb5c8bb1f5516421acd055c77090914bbb385583b17dc0bf6cbc3e57d3c"
      }
    ]
  },
  {
    "path": "d1/main/playsave.c",
    "current_lines": 2112,
    "current_whole_source_sha256": "65d1a83108908c174ed7d74b26dedb5ad5f05dd88de206058d630402dc91753c",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1470,
        "last": 1523,
        "range_lf_sha256": "db0c8a2f404b6a6f6dce8d3fae676722d57281dddbc273c25478fed986195b11"
      }
    ]
  },
  {
    "path": "d2/main/playsave.c",
    "current_lines": 1877,
    "current_whole_source_sha256": "2c256a542274ebde36f76b372383fbe392f7a98147f2780b25091ef4ab1a7411",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1271,
        "last": 1325,
        "range_lf_sha256": "8b7aad8ac08cab637dc47b91e463372bd681eee622bb023ecb328e2dd99b7eaa"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_rng_mode.c",
    "current_lines": 139,
    "current_whole_source_sha256": "3522796b9671a4a65d4b7a3baf31b839e1b57815c13c0e13184818a7ab5f62b2",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 139,
        "range_lf_sha256": "3522796b9671a4a65d4b7a3baf31b839e1b57815c13c0e13184818a7ab5f62b2"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_policy_pack1.c",
    "current_lines": 10,
    "current_whole_source_sha256": "4cb888d5b3e5a8cce2ab3a42395013ec0e0d516267e594758126b64b9f04f846",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 10,
        "range_lf_sha256": "4cb888d5b3e5a8cce2ab3a42395013ec0e0d516267e594758126b64b9f04f846"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_policy_pack16.c",
    "current_lines": 10,
    "current_whole_source_sha256": "dacc5fbef7a2b15726a7178e51519002b7865c641891cbeba605903b8cb872f4",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 10,
        "range_lf_sha256": "dacc5fbef7a2b15726a7178e51519002b7865c641891cbeba605903b8cb872f4"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_recorder.cpp",
    "current_lines": 1080,
    "current_whole_source_sha256": "424d3951557295609a917787362cbd96cf029a797ed1133758ea9e67ec69e100",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 986,
        "last": 1080,
        "range_lf_sha256": "1b0c081d057fd829cb4a3e69549570230aed185f6a0dce23459be82afc70e019"
      }
    ]
  },
  {
    "path": "android/tests/test_upstream_compat.cpp",
    "current_lines": 11576,
    "current_whole_source_sha256": "fb09c0d0dfef2d62e6a7d3b17618673481e87966215fdb7c55246fc372957c53",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 906,
        "last": 954,
        "range_lf_sha256": "6be0047a0734dda44740f7e678c0f6dd0aca3f538fcd37e191a56fe87d59ac58"
      }
    ]
  },
  {
    "path": "android/tests/CMakeLists.txt",
    "current_lines": 546,
    "current_whole_source_sha256": "7301bfa05c6a7d6ba0e44048ff8f51901cb26c17daced4c75cdeb1c2837443e3",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 294,
        "last": 310,
        "range_lf_sha256": "f0b42e4e40edc6c6abfc3cc43b37090dc8594b1f39d3792b794c0b396c57c2a0"
      },
      {
        "first": 416,
        "last": 434,
        "range_lf_sha256": "9b41397dc8398d6194aa91ea3b1f7c437bd4d30307433a0c29c03321edb6dc5d"
      },
      {
        "first": 498,
        "last": 520,
        "range_lf_sha256": "fec0511e1acad56237b8cb2b2cb5005009498dbb6410fcf4902f2b3744b44d72"
      }
    ]
  },
  {
    "path": "d2/main/d1_in_d2/d1_in_d2_weapons.c",
    "current_lines": 671,
    "current_whole_source_sha256": "0cc861332988ec31ec9802c144335cd0a9578975d7f436a83984ffff36d63aa2",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 75,
        "range_lf_sha256": "6dc4669a04412fcdb2241e9bdc4ce52c7b85de1208dffb4e5b39d6e2df26bf68"
      }
    ]
  },
  {
    "path": "d2/main/guidebot_routing.c",
    "current_lines": 103,
    "current_whole_source_sha256": "f723148411e816b0f2dcabdf3efc1f9edc3ea67f1deeba98f26bd8234bfbebd2",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 103,
        "range_lf_sha256": "f723148411e816b0f2dcabdf3efc1f9edc3ea67f1deeba98f26bd8234bfbebd2"
      }
    ]
  }
]
```

## Canonical bindings read before publication

```json
[
  {
    "path": "android/ai tool plans/code management/general_code_quality_ledger_20260811.md",
    "prepublication_whole_source_sha256": "d49f46c5211f5e281a8ec5a1ade1957fc4e6178bd67feece0903ee5899d7d6f8",
    "read_lines": [
      {
        "line": 149,
        "text": "| 21 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQR-0176` | `TODO` | `GQF-0189` | Bound aggregate input-demo recording, fixture and replay memory |",
        "line_lf_sha256": "64c00582612e3394db2382fa36b66860e1f5643b73762d19423df404fd2bb98a"
      },
      {
        "line": 379,
        "text": "| 251 | 33 | LOW | 12/0/4/10/7 | `GQR-0245` | `DONE` | `GQF-0259` | Restore effective homing coverage in the robot-frame matrix |",
        "line_lf_sha256": "bb267d805df248a8b6979d8d61c2bf054bba767c96de46b8553a5f501c8e470f"
      },
      {
        "line": 8023,
        "text": "| `GQF-0189` | `OPEN`  | P1/high             | security/resource-exhaustion/allocation-admission             | Aggregate input-demo recording, typed fixture and replay memory                                           | Compact frames fit raw file limit but each retains a 268-byte result plus controls/strings even without state; whole-file frame count/typed vector/event memory is unbounded until after growth, with additional replay vectors and no complete C-facing load allocation boundary. Current streaming still retains all typed frames and raises raw ceiling to one GiB. Recording likewise retains all controls and encoded lines, stages unbounded events and lacks complete C-facing allocation containment; start/capture/string mutation can partially publish session state before failure. Bound checked retained/expanded allocation before growth and contain failure; extends aggregate gap beyond archived BR-0219 checkpoint ceilings, separate from GQF-0168 single-frame dispatch cost                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |",
        "line_lf_sha256": "b1ab8db8da92a9f3435b79f032761e2457bda9a3e03e44df36b7b6532cb8aecb"
      },
      {
        "line": 8093,
        "text": "| `GQF-0259` | `FIXED` | P3/high | test-correctness/scenario-setup | android/tests/test_upstream_compat.cpp write_robot_frame_trace | Variant 6 sets Weapon_info[0].homing_flag before the common weapon loop zeroes that record. All 126 variant-6 scenarios have identical four-frame results to variant 0 in fresh rebuilt native/imported traces, so the passing comparison does not exercise its intended homing scenario. Separate firing tests retain direct homing coverage. GQR-0245 owns setup-after-initialization and an effective-scenario oracle; no engine mismatch or inherited reduction claimed |",
        "line_lf_sha256": "bf4684b52bb6e78b6e4be5432e70d2174143c7bac2c4e450986018e5e87af7e4"
      },
      {
        "line": 10822,
        "text": "| `GQR-0176` | `TODO`     | `GQF-0189`                         | Bound aggregate input-demo recording, fixture and replay memory                   | Shared limits/recorder/fixture/replay/result/trace leaf owners; legacy FX sidecar file/record admission and whole-load staged session publication; recording retained-budget boundaries and injected allocation failure between session publication, container growth and encoded-line mutation, compact high-frame counts and event/diag-heavy cases at/over aggregate limits with production allocator accounting, allocation failure before/after parsing, unchanged prior outputs/session and bounded expansion, ordinary long recording controls and paired native/Android replay validation                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |",
        "line_lf_sha256": "094ec9a34b0d87000d57a2bc0e421f0f4ba72b3879ca414c39db40c98c10ca43"
      },
      {
        "line": 10891,
        "text": "| `GQR-0245` | `DONE` | `GQF-0259` | Restore effective homing coverage in the robot-frame matrix | Completed 2026-10-08: variant flag follows common initialization with effective weapon assertion; maintained public-frame off-cone firing oracle distinguishes homing from ordinary weapon. Fresh baseline proved all 126 old homing cases duplicated controls. Final paired Windows builds and maintained test_d1_ai_frames pass 1260 cases/5040 frames per engine, traces byte-identical; all 1134 ordinary cases unchanged, 14 homing cases change. Scoped quality and exact whole-file byte transform pass; six net branch-test lines added, no engine/inherited edit. Immutable evidence: GQR-0245 effective robot-frame homing coverage remediation 20261008 |",
        "line_lf_sha256": "8c1d6d6dfedf183adbe557a8348b53abbadf498a7401a030fc90b8fafef35fd7"
      }
    ]
  },
  {
    "path": "android/ai tool plans/code management/branch_adversarial_review_ledger.md",
    "prepublication_whole_source_sha256": "0a1dda634ec40d320cad6de0c598a2cbcba92ac0f1ba49bd730bb0918cf0a394",
    "read_lines": [
      {
        "line": 13470,
        "text": "### BR-0209: P2 - Fail headless execution when replay validation or requested output fails",
        "line_lf_sha256": "fb1f788182d93fea536796e2ebf3ac235b6f2733d6100270a03c2ce57e61994c"
      },
      {
        "line": 13471,
        "text": "",
        "line_lf_sha256": "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b"
      },
      {
        "line": 13472,
        "text": "- [ ] OPEN",
        "line_lf_sha256": "b51f310dfc99aa47b58cba7fed1dcfd6b13bab97a2e98b391a97d32ca57cef17"
      },
      {
        "line": 13473,
        "text": "- Type: defect",
        "line_lf_sha256": "c35c251f5479984ecc3bbf292b2b96bf30c6e0e55e1ed672b025c4eb0125e918"
      },
      {
        "line": 13474,
        "text": "- Confidence: high",
        "line_lf_sha256": "666adfafe63b7fd4e579910b19eecf81f6e25bc211a7165f4bffdf849ca7e334"
      },
      {
        "line": 13475,
        "text": "- Category: test-gap/false-pass",
        "line_lf_sha256": "15ee323aee824e6ac25f6eae8ce317f2a7c864c09d7319f73f1ee989d6b124f9"
      },
      {
        "line": 13476,
        "text": "- Found by: R1-CHUNK-0108, R1-CHUNK-0123, R1-CHUNK-0135",
        "line_lf_sha256": "79020281868d28fa4ef2b77cc2d5864b95aa61286563f6b59d0cb46f78766dc7"
      },
      {
        "line": 13477,
        "text": "- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/headless/input_demo_headless_main.cpp:L229-L245` in headless replay completion",
        "line_lf_sha256": "ac51e0fdf67fa5631400f9defa4270ef0cd2ab012bf69cfc9199b7eb8c747c3b"
      },
      {
        "line": 13478,
        "text": "- Related: `android/app/src/main/cpp/shared/input_demo_hooks_shared.c:L622-L642,L665-L694`, `android/app/src/main/cpp/shared/input_demo_state_trace.cpp:L646-L791`, `d2/main/input_demo_hooks.c:L6915-L6958,L7083-L7087`, and `android/tests/run_input_demo_replay.ps1:L1577-L1592,L1668-L1736`",
        "line_lf_sha256": "515ceaf0291aca328ab4b687e2e0a756b74ada2244a4a21950892924776a7250"
      },
      {
        "line": 13479,
        "text": "- Evidence: The headless loop breaks when `input_demo_step_replay_frame` returns false, but it neither records that failure nor verifies that all declared frames completed. If the session remains loaded it calls a void finish helper, then unconditionally prints `HEADLESS-RUN OK` and returns zero. The D2 result writer likewise returns void and converts output-write, expected-result setup, and embedded-result comparison failures into console messages only, so even a normally completed replay whose actual result mismatches its embedded oracle receives the same success marker and exit status. The maintained PowerShell wrapper mitigates the ordinary test path by waiting for and independently comparing the actual JSON, but direct executable users and automation that rely on the process contract still receive a false pass.",
        "line_lf_sha256": "56096446c67a81fae250c55970ea36d61a5d67da96839db2b34ed3420f1ea754"
      },
      {
        "line": 13480,
        "text": "- Additional evidence (R1-CHUNK-0135): When state tracing is requested, the shared per-frame hook treats a trace write or flush failure as diagnostic-only: it logs once and closes/disables the trace, but returns no failure to replay stepping or headless completion. State-trace startup also ignores its metadata flush result, so it can report successful startup after the first output failure. Direct headless execution can therefore print `HEADLESS-RUN OK` and exit zero without producing the requested complete trace; the PowerShell wrapper detects some ordinary missing or malformed outputs only when its independent comparison is enabled.",
        "line_lf_sha256": "f625e144196b5cf9f42f89e7f75e7d2ff8c70b88bdd72b638cdcd54d5d3c9e00"
      },
      {
        "line": 13481,
        "text": "- Additional evidence (R1-CHUNK-0184): `input_demo_result_write_json_file` truncates the requested actual-result path, writes the JSON, and checks `output.good()` but never explicitly flushes or closes it. The first close occurs in the local stream destructor after the function has committed to return success, so a delayed flush or close failure cannot reach the void paired result-writer callback or the headless outcome. The final path can therefore be incomplete even when native completion prints no write error and later reports `HEADLESS-RUN OK`.",
        "line_lf_sha256": "e66f29c3d17d349ced1a2ee171202bde2669f2ffd9e06ff06ca651e5488cc153"
      },
      {
        "line": 13482,
        "text": "- Additional location (R1-CHUNK-0268): `d2/main/input_demo_hooks.c:L6880-L6958,L6978-L7012`, `android/app/src/main/cpp/shared/input_demo_replay.cpp:L490-L518`, and `android/tests/run_input_demo_replay.ps1:L1129-L1158,L1273-L1341,L1614-L1624` in terminal result overrides and subset comparison",
        "line_lf_sha256": "0b49180c9d21958a2eaea5a102cd19856ee73287a557f9ba16097977ba99260a"
      },
      {
        "line": 13483,
        "text": "- Additional evidence (R1-CHUNK-0268): D2 level-exit and mine-exit finish callbacks do not retain the observed replay cursor or current `GameTime64`. They set the actual result's `frame_count` to `input_demo_replay_frame_count()`, the loaded vector's complete declared size, and set `game_time64` to `input_demo_replay_final_game_time64()`, which is precomputed from every loaded frame plus checkpoint start time. They also force the terminal marker and, for level exit, `endlevel_completed`. Native comparison then clears expected player and position presence and replaces expected `endlevel_completed` with the manufactured actual value. The maintained wrapper repeats and broadens this policy: its terminal subset takes `player0` and `position` from the actual result and replaces expected `endlevel_completed` with actual before comparison. Therefore a simulation regression that reaches the same level or mine exit one or more frames early can still report the expected total frame count and final clock and can pass when remaining level-summary counts are unchanged. The wrapper's independent JSON comparison no longer mitigates this false success.",
        "line_lf_sha256": "b12b77c27f3f933e584f28f9e5a9c0bc9af44ebf918186056a2ef21531ad447a"
      },
      {
        "line": 13484,
        "text": "- Trigger: Run the headless executable directly on a replay whose embedded expected result differs from the simulated result, whose actual-result or requested state-trace path cannot be written completely, or whose frame preparation or RNG restoration fails after startup",
        "line_lf_sha256": "a9e5312e1cfd876eb355f01387c35bad5845997788f83b4f76cc84ddbd52fa51"
      },
      {
        "line": 13485,
        "text": "- Additional trigger (R1-CHUNK-0268): Replay a D2 recording with a level-exit or mine-exit marker after a gameplay or timing regression makes the same exit occur before the recorded final frame while robot, hostage, powerup, and control-center summary values remain unchanged",
        "line_lf_sha256": "8c7f6ef0070b25a4d22adee7bb07b547b0e6a4ba45ee74c1890c944d7b8b057e"
      },
      {
        "line": 13486,
        "text": "- Impact: CI, bisect scripts, and developers invoking the advertised headless runner can accept an incomplete or divergent deterministic replay or a run missing its requested diagnostic as successful, masking regressions precisely when the native log already reports the failure.",
        "line_lf_sha256": "e59ccf585854e72fcd2d156bb04418200e83bee2e83ab99bf3d01671c324473f"
      },
      {
        "line": 13487,
        "text": "- Additional impact (R1-CHUNK-0268): The maintained wrapper can also certify an early terminal exit because the native writer replaces the two direct timing witnesses and the wrapper trusts or substitutes every other volatile witness. This defeats the defense-in-depth comparison cited above and can hide exactly the frame-order, movement, trigger, countdown, or timing regression the deterministic replay suite is meant to detect.",
        "line_lf_sha256": "14d82d042414885fd8344769e5d1b9203c5362b859b1cbd5ef5e84c2e162a9e9"
      },
      {
        "line": 13488,
        "text": "- Expected: `HEADLESS-RUN OK` and exit zero are emitted only after every replay frame completes, every required output is written and closed, and the selected embedded-result comparison succeeds; every other terminal outcome emits `HEADLESS-RUN FAIL` and exits nonzero.",
        "line_lf_sha256": "9d7d1a20ee6558665fd94ac50a4d2c730a049c11d7b37c89f2b519ae87507d1c"
      },
      {
        "line": 13489,
        "text": "- Suggested fix: Propagate a typed replay outcome through stepping and finishing, including frame completion, result-write, and comparison status. Have the headless main retain the first failure, avoid converting an interrupted loaded session into success, print the matching failure reason, and return nonzero. Keep the wrapper's independent comparison as defense in depth.",
        "line_lf_sha256": "d403f1c610e3a8ce5b092cf75dec0b662d237c9d246229b5ef272cb8587bf14c"
      },
      {
        "line": 13490,
        "text": "- Additional suggested fix (R1-CHUNK-0268): Capture the observed cursor, current `GameTime64`, terminal kind, and immutable terminal snapshot before unloading, and write those observed values without consulting the loaded final-frame aggregate. Define only the specific post-transition fields that cannot be compared, rather than copying them from actual into expected. Require a terminal exit to occur at the recorded frame boundary and clock before any subset comparison can pass.",
        "line_lf_sha256": "9b818a10dadbedd22828adde67272952767afd4324c3779229866614b9330373"
      },
      {
        "line": 13491,
        "text": "- Validation: Add headless process tests for successful exact replay plus injected prepare, direct-command, RNG-restore, actual-result-write, state-trace start, write, flush, and close, and embedded-result-mismatch failures. Assert only the exact replay with all requested outputs complete returns zero and prints `HEADLESS-RUN OK`; every failure must return nonzero, print `HEADLESS-RUN FAIL`, and remain a failure through the PowerShell wrapper.",
        "line_lf_sha256": "e6eb81b08107ca883f36bca653bfd5d9a894a531fe28ef444eb7f5de6331fea6"
      },
      {
        "line": 13492,
        "text": "- Additional validation (R1-CHUNK-0184): Inject actual-result failures separately at open, body write, explicit flush, and close, with and without a prior valid result. Require each failure to propagate through the paired callback and headless exit, preserve or transactionally replace the prior result, and leave no partial file that a wrapper can mistake for the current run.",
        "line_lf_sha256": "ab7cd37e5f8a72a8287c6c7b219f2722dfb8094a0d593473b4ed2c02f9698f0c"
      },
      {
        "line": 13493,
        "text": "- Additional validation (R1-CHUNK-0268): Add level-exit and mine-exit fixtures whose exit is exact, one frame early, several frames early, and absent at the recorded boundary while all summary inventory counts remain identical. Assert actual `frame_count` and `game_time64` reflect the observed boundary, only the exact exit passes native and PowerShell comparison, and every early, late, missing, or wrong-kind terminal event returns a correlated nonzero headless result. Mutate player state, position, `endlevel_completed`, and each retained summary field independently to document and enforce the minimal intentional terminal subset.",
        "line_lf_sha256": "b3696a5387eb30f92e224a73232095fed9b1a4f2385831c43a96a54aca16bbe7"
      },
      {
        "line": 13494,
        "text": "- Resolution: Pending",
        "line_lf_sha256": "9283e8d2b02a60dd95afcd32c737e52c949ba2cc15180e9d8967d03ca1e1d976"
      }
    ]
  }
]
```

## Consumer diagnosis and acceptance consolidated

Gates 2-4 complete for source diagnosis and planning, with the evidence limits below. Gate 5 remains pending. Historical partial-checkpoint remaining-read lists are superseded by this checkpoint; they do not claim additional physical reads beyond the identities saved here.

Full fixture admission enforces game-specific 7/6 D1 and 11/11 D2 order lengths, unique supported weapon domains, paired optional imported-D1 orders only on D2, and routing modes 0/1. The frame constructor clears input/RNG/state and initializes presence flags before recorder capture fills it. Complete stream parsing requires a first header, checkpoint before frames, contiguous frame count, final result trailer and envelope validation before moving parsed output. Raw record/file byte checks happen before line append, while all typed frames/events remain retained. Stored-line writing validates bookends but has no corresponding full output byte/aggregate admission. This supports existing GQR-0176 acceptance; it does not establish rejection of every oversized emitted recording.

Recorder checkpoint helper checks the checkpoint ceiling, hashes original bytes, compresses into compressBound storage, chooses compressed bytes only when smaller, then base64 encodes. Those simultaneously retained buffers belong in aggregate accounting. Start and capture use shared recorder-settings ABI; the MSVC push-8/pop restores caller packing while nested fixture scalars retain their own push-1/pop definitions. This is a source layout inspection, not a fresh compiler/ABI test. Live recording preparation rejects multiplayer and non-LCG recording, fills real engine settings and captures a checkpoint for mid-level starts. After start the caller releases owned checkpoint bytes because the recorder copies them. The real newdemo flush caller cancels after returned writer failure, so the maintained leaf fixture's retryable session is not the production caller's policy.

Object trace uses bounded engine slots for its main loop. The endlevel snapshot is the actual out-of-array explosion passed with slot -1; both producer paths copy an object created as OBJ_FIREBALL/CT_EXPLOSION/RT_FIREBALL, so this ordinary caller does not select the robot-only Ai_local_info[slot] branch. The AI snapshot itself relies on a valid caller slot and encodes full named local fields. Do not admit a new negative-slot robot defect from this endlevel call. The observed preexisting endlevel explosion allocation ordering is consumer context, outside this assigned change's new-root attribution.

Fresh terminal consumers confirm BR-0209's surviving behavior: headless main breaks on failed stepping, optionally invokes void finish, then prints OK and returns zero. Shared finish writes a boundary and invokes a void result callback before unloading. Both engine result callbacks log output/compare errors. The standalone result writer returns after output.good without explicit close; the stored recording writer, in contrast, explicitly closes and checks its stream. D2 level/mine exits replace cursor and clock with loaded totals and suppress/substitute some expected fields. These observations strengthen the existing required-output/observed-completion owner, without adding a duplicate finding or claiming wrapper acceptance.

Maintained event and diagnostic recorder fixtures check version 4, frame-associated score/damage events, diagnostic canonical SHA/order plus ordinary fields/arrays and full parser round trips. Long-recording, object plain/gzip and RNG fixtures and registrations were inspected without execution. The host test list actually calls add_test for RNG/recorder targets, and each engine registers test_upstream_compat; its ordinary main invokes object/world trace tests. No fresh compile, device, replay, allocator-pressure or injected I/O-failure outcome is claimed.

Implementation handoff: preserve shared Android ownership and narrowly paired hooks, desktop/native save and wire formats, handmade comments and deterministic engine semantics. All nine assigned original counterparts are absent, so NO_INHERITED_EFFECT and zero saving remain appropriate. Do not widen into upstream deduplication or replay-specific compensation. GQR-0176/GQF-0189 remains the highest existing owner at 56 MEDIUM-HIGH (32/0/7/10/7), with BR-0209 as a distinct completion/output owner; preserve GQR-0245 DONE/GQF-0259 FIXED. Use the actionable aggregate publication and actual-output acceptance already recorded above. No finding, remediation, rating, product status or code change is introduced.

Next: prepare immutable report with exact nine assigned identities, all zero current deltas, manifest rows, independently merged actual current reads and canonical owner bindings. Run required retention before report creation, import exact body, publish GQC/GQD/observation/queue and sorted terminal annotation while preserving all existing product rows. Independently audit source/blob/diff/assigned hunks/payload/manifest/current/union/scope/import/canonical preservation/counts/ranks, then mark gate 5 only after PASS. Chunk 0114 stays TODO until terminal publication.

## Additional physical bindings for completed consumer gates

```json
[
  {
    "path": "android/app/src/main/cpp/shared/input_demo_fixture.cpp",
    "current_lines": 1448,
    "current_whole_source_sha256": "1c71d445a5632f2775bdb738f9c8dfd0fcf108c9c7f78f0d56492833916b79d4",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 129,
        "last": 148,
        "range_lf_sha256": "9d4e4989628cc10ff960a7168c4677a5c4f79465736327eee61b2a300454f65f"
      },
      {
        "first": 254,
        "last": 327,
        "range_lf_sha256": "706156c684527ddbbd88902f3592e07f645cd1dbc65c9b8b815ebc3851779aac"
      },
      {
        "first": 560,
        "last": 640,
        "range_lf_sha256": "72535861b567dcefb5ab1c0411107530f2dfbc5a000ef2c32d806b7a91ed52fa"
      },
      {
        "first": 986,
        "last": 1034,
        "range_lf_sha256": "8e78d578ac286e905bdf7a8120206818bf2315183b2dc5a9bdea7a5da78dddf4"
      },
      {
        "first": 1215,
        "last": 1391,
        "range_lf_sha256": "8b832444c8c9d6e785546b3e04220302e346f018d7def0746e05a450e16f029b"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_recorder.cpp",
    "current_lines": 741,
    "current_whole_source_sha256": "fd3164fa33af92d03fe701dae9221e292313f5f6f5875ba145613a629d33d9e5",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 181,
        "last": 289,
        "range_lf_sha256": "8f8fc4fb12b08fe33db45ef7fdf978a007110cf3c9a538410ccfa63345a27713"
      },
      {
        "first": 571,
        "last": 689,
        "range_lf_sha256": "04224ed2e32e462627beac267505eefbe57c2fb93f2839dc06af9f89b03ef298"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_recorder.h",
    "current_lines": 105,
    "current_whole_source_sha256": "755bdbcd8a212f8748ba773d130f81bd899d2938e255e6aacd58668300e99aa8",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 60,
        "range_lf_sha256": "8ecafdf6368c4605f325a061d250394c1172b065f5a050650456879f6503114d"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/headless/input_demo_headless_main.cpp",
    "current_lines": 247,
    "current_whole_source_sha256": "b083bf298556ab8930becf8a919d26519728073ac17449e165d93e4f953ad7d1",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 150,
        "last": 247,
        "range_lf_sha256": "569b6887e3d337767cfabe2d9f0b82f0c8724639efb79bb609c891e6fe634d56"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_hooks_shared.c",
    "current_lines": 2357,
    "current_whole_source_sha256": "81fa8f08eb48b2129a98d62620ea51e25c58c8354d67cc71f6c7cda840d43164",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 594,
        "last": 646,
        "range_lf_sha256": "7ac4f8e1754dd320796faaab2b983a73c91f5b02ee6484d5b1739e40ad37e776"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_ai_trace.cpp",
    "current_lines": 167,
    "current_whole_source_sha256": "751f9f665c5ab46f76f4785744cd4ef4015f0e6f2daf22dc8f5f8ec780f0e6e0",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 167,
        "range_lf_sha256": "751f9f665c5ab46f76f4785744cd4ef4015f0e6f2daf22dc8f5f8ec780f0e6e0"
      }
    ]
  },
  {
    "path": "d1/main/input_demo_hooks.c",
    "current_lines": 1343,
    "current_whole_source_sha256": "c2b35ada709461ba3a1801c7663265b3a167d6dff19334d64a923174b1f0aab5",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1253,
        "last": 1306,
        "range_lf_sha256": "9efbf9582a041931169e50487517259ccb8f6d39711904141bb3085755a2dab8"
      }
    ]
  },
  {
    "path": "d2/main/input_demo_hooks.c",
    "current_lines": 7147,
    "current_whole_source_sha256": "36fb8e95d40ef9c314e03484068bfc3716218190c5533c00182c1bb5e5ca04cf",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 6933,
        "last": 7043,
        "range_lf_sha256": "4aea27684e835abe142563922f0dbf05237d331aae3aa8ced9d9a47908de1be3"
      },
      {
        "first": 7135,
        "last": 7147,
        "range_lf_sha256": "ab3924c473459cae0e5199b35f91ae5adf8d045006075b859a92911aafb6aa0b"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_world_trace.cpp",
    "current_lines": 417,
    "current_whole_source_sha256": "71fcf3c1773eb9d55dfdcc071535aeb638b2a36c1dec53c44fdc3301a12356c6",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 85,
        "last": 135,
        "range_lf_sha256": "614a4dc11714541e6d25aaf5c009162f889eed3ad029e2b4d0727240efcc104d"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_newdemo_shared.c",
    "current_lines": 715,
    "current_whole_source_sha256": "f10ca627d872dbcc8253c041a02d1d2e34fc798e2021c9fb6231e04b79e75a22",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 202,
        "last": 218,
        "range_lf_sha256": "80da6129aaa0771a971a16402c01cebe06997d4ca782085ef23ac5751a69968f"
      },
      {
        "first": 403,
        "last": 483,
        "range_lf_sha256": "068b4ef594f6e004f5cb9f307e069a7586622ac11703002b275111a4c7257d24"
      },
      {
        "first": 540,
        "last": 650,
        "range_lf_sha256": "3a6fa1a3d53f7fc3803e2c140cfb50a4eeaec23348b5e06f8f3e17ad5605e7bb"
      }
    ]
  },
  {
    "path": "android/tests/CMakeLists.txt",
    "current_lines": 546,
    "current_whole_source_sha256": "7301bfa05c6a7d6ba0e44048ff8f51901cb26c17daced4c75cdeb1c2837443e3",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 521,
        "last": 537,
        "range_lf_sha256": "ccb921ddd112be5269aa52705db7e33fed79f299ca2c2703e27f43633e87c323"
      }
    ]
  },
  {
    "path": "android/tests/test_upstream_compat.cpp",
    "current_lines": 11576,
    "current_whole_source_sha256": "fb09c0d0dfef2d62e6a7d3b17618673481e87966215fdb7c55246fc372957c53",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 11460,
        "last": 11474,
        "range_lf_sha256": "4f33c1c56148d5b51e6de0c6f6a72a69f7f9f86dc328a857fcb84b253d6cc7ad"
      }
    ]
  },
  {
    "path": "d1/main/endlevel.c",
    "current_lines": 1564,
    "current_whole_source_sha256": "9843ef2d88d4df09541b17566b11206b4c19f270bb9c2a37457cd834cf473ab4",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 480,
        "last": 509,
        "range_lf_sha256": "fde7703f4d000a82afe8d25b9bbcb9bb9f50919e7589bffbf3591fc92898377e"
      }
    ]
  },
  {
    "path": "d2/main/endlevel.c",
    "current_lines": 1747,
    "current_whole_source_sha256": "478e2fe5464724dcc9766b2ea22b193c3b3f7435ed2f7ebc1a48d2fe338ca0a3",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 668,
        "last": 697,
        "range_lf_sha256": "fde7703f4d000a82afe8d25b9bbcb9bb9f50919e7589bffbf3591fc92898377e"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_result.cpp",
    "current_lines": 706,
    "current_whole_source_sha256": "9a14157d8ddba4479632a052ef33c6a3ff635aaf8664a912b61c27ad63164817",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 640,
        "last": 677,
        "range_lf_sha256": "66df57569595354d64611d99f4ca7a72141800ed89e96a6fff2501d7deee8cc4"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_fixture.h",
    "current_lines": 228,
    "current_whole_source_sha256": "95eb68785a886afab43a87c6523cabe10f8ad4bf7713d2f6194eb3802fcd330c",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 190,
        "range_lf_sha256": "7215e631fd838e51ededfb46066d79e21ec48c1fda34cf2819f96e88a2b1ee29"
      }
    ]
  },
  {
    "path": "d1/main/fireball.c",
    "current_lines": 1501,
    "current_whole_source_sha256": "85ef4b07c8a490d0d5c7efe495d73687705d770f3a585f68da75d9dd843380f2",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 58,
        "last": 82,
        "range_lf_sha256": "b4aa0786b8d726a3f2ac9c6a5a9cc766aa6418d283cfc8b4efe851d28ea7fc92"
      },
      {
        "first": 239,
        "last": 246,
        "range_lf_sha256": "79eb2d579dc04c5a8e7284f1bfea3b86f9b6e5f060721a5104208e27a342fba8"
      }
    ]
  },
  {
    "path": "d2/main/fireball.c",
    "current_lines": 1781,
    "current_whole_source_sha256": "43319b53b372fddabc1355321933c02485b76838ec27ea72af1dbe40e29fb324",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 77,
        "last": 101,
        "range_lf_sha256": "5734d9b8cd3e5db31acd2552611a956f1a22f3e2cf97ece36f95d3324c843499"
      },
      {
        "first": 359,
        "last": 366,
        "range_lf_sha256": "79eb2d579dc04c5a8e7284f1bfea3b86f9b6e5f060721a5104208e27a342fba8"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_recorder.cpp",
    "current_lines": 1080,
    "current_whole_source_sha256": "424d3951557295609a917787362cbd96cf029a797ed1133758ea9e67ec69e100",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 729,
        "last": 985,
        "range_lf_sha256": "2fe1c4d34e51a0a06828d46e99cff63f3394d9d11231b4a3deea5e7612401f9b"
      }
    ]
  },
  {
    "path": "d1/main/CMakeLists.txt",
    "current_lines": 335,
    "current_whole_source_sha256": "0259c679b4c2262f037202e9abe2c1146d0bc31097b6c9794e0ff251c99fed91",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 78,
        "last": 91,
        "range_lf_sha256": "82a265c0d2d1bb3ce4928b9705fea0c89bf1a5819268f562845552b7ade76819"
      },
      {
        "first": 310,
        "last": 325,
        "range_lf_sha256": "2b66b176085378e46c92e50f932a1db319fae4604a629a02ba2443d10d9c48b5"
      }
    ]
  },
  {
    "path": "d2/main/CMakeLists.txt",
    "current_lines": 414,
    "current_whole_source_sha256": "d832841951211d1387dc8a31ee1952eb85f57c590df04453ca7df58db85f8252",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 106,
        "last": 119,
        "range_lf_sha256": "abd2b8acfabdc27ee395071c09ddf6e56995b5727932c36c4069f85f8b208cf0"
      },
      {
        "first": 382,
        "last": 400,
        "range_lf_sha256": "d28dc350a5fd150d09e1f3d9646fc835ebf015c16c2925bfe14abd9e187e1e8c"
      }
    ]
  }
]
```

## Consolidated current physical union for report preparation

```json
[
  {
    "path": "android/app/src/main/cpp/shared/input_demo_state_trace.cpp",
    "current_lines": 580,
    "current_whole_source_sha256": "5878c9430a884ca186eb8ca8add9517b9f8945d855f05fd5bf22788a2c1ffebb",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 180,
        "range_lf_sha256": "fac57f01fdea90bd1c793da0b97253fc6ad3dcba61c00b09c2de7235984d90b6"
      },
      {
        "first": 449,
        "last": 580,
        "range_lf_sha256": "1d79d26c177cc2b5b995d31b2471f51b01a3bd68739dd752da05269564c788d3"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_fixture.cpp",
    "current_lines": 1448,
    "current_whole_source_sha256": "1c71d445a5632f2775bdb738f9c8dfd0fcf108c9c7f78f0d56492833916b79d4",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 129,
        "last": 148,
        "range_lf_sha256": "9d4e4989628cc10ff960a7168c4677a5c4f79465736327eee61b2a300454f65f"
      },
      {
        "first": 254,
        "last": 327,
        "range_lf_sha256": "706156c684527ddbbd88902f3592e07f645cd1dbc65c9b8b815ebc3851779aac"
      },
      {
        "first": 560,
        "last": 640,
        "range_lf_sha256": "72535861b567dcefb5ab1c0411107530f2dfbc5a000ef2c32d806b7a91ed52fa"
      },
      {
        "first": 986,
        "last": 1034,
        "range_lf_sha256": "8e78d578ac286e905bdf7a8120206818bf2315183b2dc5a9bdea7a5da78dddf4"
      },
      {
        "first": 1142,
        "last": 1391,
        "range_lf_sha256": "3090d64021a592e0fec8a92b5b67b0860d2eded17f8132bd515aa2bb8d2572ef"
      },
      {
        "first": 1407,
        "last": 1448,
        "range_lf_sha256": "35101bf14f9908a98e7026405e0176e73800ccfecbdf1738f5e106345d956787"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_hooks_shared.c",
    "current_lines": 2357,
    "current_whole_source_sha256": "81fa8f08eb48b2129a98d62620ea51e25c58c8354d67cc71f6c7cda840d43164",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 594,
        "last": 646,
        "range_lf_sha256": "7ac4f8e1754dd320796faaab2b983a73c91f5b02ee6484d5b1739e40ad37e776"
      },
      {
        "first": 740,
        "last": 799,
        "range_lf_sha256": "a92c934c215887d29c2b1714f80ecd5ab7e29d6319429f9f43f215339c8690c3"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_rng_mode.c",
    "current_lines": 193,
    "current_whole_source_sha256": "8f534c9b2241b4233603fb2f6f36a26f29cdbd38c0f4c8e1b1d5679415f28e6f",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 193,
        "range_lf_sha256": "8f534c9b2241b4233603fb2f6f36a26f29cdbd38c0f4c8e1b1d5679415f28e6f"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_replay.cpp",
    "current_lines": 818,
    "current_whole_source_sha256": "cc32d8938e6bd8d751ddb94907b549b722e88b7d37c7aac2d2ab6ff626f75390",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 465,
        "last": 570,
        "range_lf_sha256": "75e95642167a8112df3e43a3196faf8bd79417159f8285153e5b2a5e4a5f8b42"
      }
    ]
  },
  {
    "path": "d1/main/gameseq.c",
    "current_lines": 1901,
    "current_whole_source_sha256": "edaddd98ebe9e26234394a7ff4a6339bb82c9419dc3371dc51206afdb960e914",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1488,
        "last": 1520,
        "range_lf_sha256": "57b6026c36c598694534bca745c259bade24b2419818b8abe0f1ebcb8f4138bb"
      }
    ]
  },
  {
    "path": "d2/main/gameseq.c",
    "current_lines": 2636,
    "current_whole_source_sha256": "4357f34f12df2f853d91b9f43b75b4b8d642528a976e35e72184a0fd0b19c69e",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 2090,
        "last": 2125,
        "range_lf_sha256": "596a1606fb796998c63f64f0e09670f22b25fb53ec9ea4b72b827aa018b2913c"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_recorder.cpp",
    "current_lines": 741,
    "current_whole_source_sha256": "fd3164fa33af92d03fe701dae9221e292313f5f6f5875ba145613a629d33d9e5",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 741,
        "range_lf_sha256": "fd3164fa33af92d03fe701dae9221e292313f5f6f5875ba145613a629d33d9e5"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_start_shared.c",
    "current_lines": 830,
    "current_whole_source_sha256": "1b33586c098e739c9fe2ba6238ccaaf11dd676bd8c329a5822531703f634f44c",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 160,
        "last": 320,
        "range_lf_sha256": "491071c94f0167fd0dde64aa2cbacca04bcf874be4b510f188116bb1c8344761"
      },
      {
        "first": 425,
        "last": 742,
        "range_lf_sha256": "92a97cb5c8bb1f5516421acd055c77090914bbb385583b17dc0bf6cbc3e57d3c"
      }
    ]
  },
  {
    "path": "d1/main/playsave.c",
    "current_lines": 2112,
    "current_whole_source_sha256": "65d1a83108908c174ed7d74b26dedb5ad5f05dd88de206058d630402dc91753c",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1470,
        "last": 1523,
        "range_lf_sha256": "db0c8a2f404b6a6f6dce8d3fae676722d57281dddbc273c25478fed986195b11"
      }
    ]
  },
  {
    "path": "d2/main/playsave.c",
    "current_lines": 1877,
    "current_whole_source_sha256": "2c256a542274ebde36f76b372383fbe392f7a98147f2780b25091ef4ab1a7411",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1271,
        "last": 1325,
        "range_lf_sha256": "8b7aad8ac08cab637dc47b91e463372bd681eee622bb023ecb328e2dd99b7eaa"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_rng_mode.c",
    "current_lines": 139,
    "current_whole_source_sha256": "3522796b9671a4a65d4b7a3baf31b839e1b57815c13c0e13184818a7ab5f62b2",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 139,
        "range_lf_sha256": "3522796b9671a4a65d4b7a3baf31b839e1b57815c13c0e13184818a7ab5f62b2"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_policy_pack1.c",
    "current_lines": 10,
    "current_whole_source_sha256": "4cb888d5b3e5a8cce2ab3a42395013ec0e0d516267e594758126b64b9f04f846",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 10,
        "range_lf_sha256": "4cb888d5b3e5a8cce2ab3a42395013ec0e0d516267e594758126b64b9f04f846"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_policy_pack16.c",
    "current_lines": 10,
    "current_whole_source_sha256": "dacc5fbef7a2b15726a7178e51519002b7865c641891cbeba605903b8cb872f4",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 10,
        "range_lf_sha256": "dacc5fbef7a2b15726a7178e51519002b7865c641891cbeba605903b8cb872f4"
      }
    ]
  },
  {
    "path": "android/tests/test_input_demo_recorder.cpp",
    "current_lines": 1080,
    "current_whole_source_sha256": "424d3951557295609a917787362cbd96cf029a797ed1133758ea9e67ec69e100",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 729,
        "last": 1080,
        "range_lf_sha256": "244a9e5b53ac7ac94f2352a540b7e1f2770b53de5daf754632b3b561d86b2968"
      }
    ]
  },
  {
    "path": "android/tests/test_upstream_compat.cpp",
    "current_lines": 11576,
    "current_whole_source_sha256": "fb09c0d0dfef2d62e6a7d3b17618673481e87966215fdb7c55246fc372957c53",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 906,
        "last": 954,
        "range_lf_sha256": "6be0047a0734dda44740f7e678c0f6dd0aca3f538fcd37e191a56fe87d59ac58"
      },
      {
        "first": 11460,
        "last": 11474,
        "range_lf_sha256": "4f33c1c56148d5b51e6de0c6f6a72a69f7f9f86dc328a857fcb84b253d6cc7ad"
      }
    ]
  },
  {
    "path": "android/tests/CMakeLists.txt",
    "current_lines": 546,
    "current_whole_source_sha256": "7301bfa05c6a7d6ba0e44048ff8f51901cb26c17daced4c75cdeb1c2837443e3",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 294,
        "last": 310,
        "range_lf_sha256": "f0b42e4e40edc6c6abfc3cc43b37090dc8594b1f39d3792b794c0b396c57c2a0"
      },
      {
        "first": 416,
        "last": 434,
        "range_lf_sha256": "9b41397dc8398d6194aa91ea3b1f7c437bd4d30307433a0c29c03321edb6dc5d"
      },
      {
        "first": 498,
        "last": 537,
        "range_lf_sha256": "a8162d906e9a02a508799d8eb6c5a3e93cb2375bb8fbe4fbc22ef616170f34e8"
      }
    ]
  },
  {
    "path": "d2/main/d1_in_d2/d1_in_d2_weapons.c",
    "current_lines": 671,
    "current_whole_source_sha256": "0cc861332988ec31ec9802c144335cd0a9578975d7f436a83984ffff36d63aa2",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 75,
        "range_lf_sha256": "6dc4669a04412fcdb2241e9bdc4ce52c7b85de1208dffb4e5b39d6e2df26bf68"
      }
    ]
  },
  {
    "path": "d2/main/guidebot_routing.c",
    "current_lines": 103,
    "current_whole_source_sha256": "f723148411e816b0f2dcabdf3efc1f9edc3ea67f1deeba98f26bd8234bfbebd2",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 103,
        "range_lf_sha256": "f723148411e816b0f2dcabdf3efc1f9edc3ea67f1deeba98f26bd8234bfbebd2"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_recorder.h",
    "current_lines": 105,
    "current_whole_source_sha256": "755bdbcd8a212f8748ba773d130f81bd899d2938e255e6aacd58668300e99aa8",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 60,
        "range_lf_sha256": "8ecafdf6368c4605f325a061d250394c1172b065f5a050650456879f6503114d"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/headless/input_demo_headless_main.cpp",
    "current_lines": 247,
    "current_whole_source_sha256": "b083bf298556ab8930becf8a919d26519728073ac17449e165d93e4f953ad7d1",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 150,
        "last": 247,
        "range_lf_sha256": "569b6887e3d337767cfabe2d9f0b82f0c8724639efb79bb609c891e6fe634d56"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_ai_trace.cpp",
    "current_lines": 167,
    "current_whole_source_sha256": "751f9f665c5ab46f76f4785744cd4ef4015f0e6f2daf22dc8f5f8ec780f0e6e0",
    "current_whole_read": true,
    "read_ranges": [
      {
        "first": 1,
        "last": 167,
        "range_lf_sha256": "751f9f665c5ab46f76f4785744cd4ef4015f0e6f2daf22dc8f5f8ec780f0e6e0"
      }
    ]
  },
  {
    "path": "d1/main/input_demo_hooks.c",
    "current_lines": 1343,
    "current_whole_source_sha256": "c2b35ada709461ba3a1801c7663265b3a167d6dff19334d64a923174b1f0aab5",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1253,
        "last": 1306,
        "range_lf_sha256": "9efbf9582a041931169e50487517259ccb8f6d39711904141bb3085755a2dab8"
      }
    ]
  },
  {
    "path": "d2/main/input_demo_hooks.c",
    "current_lines": 7147,
    "current_whole_source_sha256": "36fb8e95d40ef9c314e03484068bfc3716218190c5533c00182c1bb5e5ca04cf",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 6933,
        "last": 7043,
        "range_lf_sha256": "4aea27684e835abe142563922f0dbf05237d331aae3aa8ced9d9a47908de1be3"
      },
      {
        "first": 7135,
        "last": 7147,
        "range_lf_sha256": "ab3924c473459cae0e5199b35f91ae5adf8d045006075b859a92911aafb6aa0b"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_world_trace.cpp",
    "current_lines": 417,
    "current_whole_source_sha256": "71fcf3c1773eb9d55dfdcc071535aeb638b2a36c1dec53c44fdc3301a12356c6",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 85,
        "last": 135,
        "range_lf_sha256": "614a4dc11714541e6d25aaf5c009162f889eed3ad029e2b4d0727240efcc104d"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_newdemo_shared.c",
    "current_lines": 715,
    "current_whole_source_sha256": "f10ca627d872dbcc8253c041a02d1d2e34fc798e2021c9fb6231e04b79e75a22",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 202,
        "last": 218,
        "range_lf_sha256": "80da6129aaa0771a971a16402c01cebe06997d4ca782085ef23ac5751a69968f"
      },
      {
        "first": 403,
        "last": 483,
        "range_lf_sha256": "068b4ef594f6e004f5cb9f307e069a7586622ac11703002b275111a4c7257d24"
      },
      {
        "first": 540,
        "last": 650,
        "range_lf_sha256": "3a6fa1a3d53f7fc3803e2c140cfb50a4eeaec23348b5e06f8f3e17ad5605e7bb"
      }
    ]
  },
  {
    "path": "d1/main/endlevel.c",
    "current_lines": 1564,
    "current_whole_source_sha256": "9843ef2d88d4df09541b17566b11206b4c19f270bb9c2a37457cd834cf473ab4",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 480,
        "last": 509,
        "range_lf_sha256": "fde7703f4d000a82afe8d25b9bbcb9bb9f50919e7589bffbf3591fc92898377e"
      }
    ]
  },
  {
    "path": "d2/main/endlevel.c",
    "current_lines": 1747,
    "current_whole_source_sha256": "478e2fe5464724dcc9766b2ea22b193c3b3f7435ed2f7ebc1a48d2fe338ca0a3",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 668,
        "last": 697,
        "range_lf_sha256": "fde7703f4d000a82afe8d25b9bbcb9bb9f50919e7589bffbf3591fc92898377e"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_result.cpp",
    "current_lines": 706,
    "current_whole_source_sha256": "9a14157d8ddba4479632a052ef33c6a3ff635aaf8664a912b61c27ad63164817",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 640,
        "last": 677,
        "range_lf_sha256": "66df57569595354d64611d99f4ca7a72141800ed89e96a6fff2501d7deee8cc4"
      }
    ]
  },
  {
    "path": "android/app/src/main/cpp/shared/input_demo_fixture.h",
    "current_lines": 228,
    "current_whole_source_sha256": "95eb68785a886afab43a87c6523cabe10f8ad4bf7713d2f6194eb3802fcd330c",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 1,
        "last": 190,
        "range_lf_sha256": "7215e631fd838e51ededfb46066d79e21ec48c1fda34cf2819f96e88a2b1ee29"
      }
    ]
  },
  {
    "path": "d1/main/fireball.c",
    "current_lines": 1501,
    "current_whole_source_sha256": "85ef4b07c8a490d0d5c7efe495d73687705d770f3a585f68da75d9dd843380f2",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 58,
        "last": 82,
        "range_lf_sha256": "b4aa0786b8d726a3f2ac9c6a5a9cc766aa6418d283cfc8b4efe851d28ea7fc92"
      },
      {
        "first": 239,
        "last": 246,
        "range_lf_sha256": "79eb2d579dc04c5a8e7284f1bfea3b86f9b6e5f060721a5104208e27a342fba8"
      }
    ]
  },
  {
    "path": "d2/main/fireball.c",
    "current_lines": 1781,
    "current_whole_source_sha256": "43319b53b372fddabc1355321933c02485b76838ec27ea72af1dbe40e29fb324",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 77,
        "last": 101,
        "range_lf_sha256": "5734d9b8cd3e5db31acd2552611a956f1a22f3e2cf97ece36f95d3324c843499"
      },
      {
        "first": 359,
        "last": 366,
        "range_lf_sha256": "79eb2d579dc04c5a8e7284f1bfea3b86f9b6e5f060721a5104208e27a342fba8"
      }
    ]
  },
  {
    "path": "d1/main/CMakeLists.txt",
    "current_lines": 335,
    "current_whole_source_sha256": "0259c679b4c2262f037202e9abe2c1146d0bc31097b6c9794e0ff251c99fed91",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 78,
        "last": 91,
        "range_lf_sha256": "82a265c0d2d1bb3ce4928b9705fea0c89bf1a5819268f562845552b7ade76819"
      },
      {
        "first": 310,
        "last": 325,
        "range_lf_sha256": "2b66b176085378e46c92e50f932a1db319fae4604a629a02ba2443d10d9c48b5"
      }
    ]
  },
  {
    "path": "d2/main/CMakeLists.txt",
    "current_lines": 414,
    "current_whole_source_sha256": "d832841951211d1387dc8a31ee1952eb85f57c590df04453ca7df58db85f8252",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 106,
        "last": 119,
        "range_lf_sha256": "abd2b8acfabdc27ee395071c09ddf6e56995b5727932c36c4069f85f8b208cf0"
      },
      {
        "first": 382,
        "last": 400,
        "range_lf_sha256": "d28dc350a5fd150d09e1f3d9646fc835ebf015c16c2925bfe14abd9e187e1e8c"
      }
    ]
  }
]
```

## Terminal current-context reconciliation

Concurrent metadata-snapshot/checkpoint-slot work added 13 lines to android/tests/CMakeLists.txt and one source registration to d2/main/CMakeLists.txt after immutable report creation. Report bytes/import remain unchanged and bind historical context. HEAD blobs reproduce historical whole-source hashes. Old read lines are identical at explicitly mapped positions: test lines at/after old 53 move +13; D2 lines at/after old 107 move +1; preceding lines stay unchanged. Complete one-hunk deltas and current registration ranges were read. Preserve external edits; no coverage of the new metadata/checkpoint implementation is claimed. These bindings supersede two report context identities for terminal live reconciliation; assigned scope/report SHA unchanged.

```json
[
  {
    "path": "android/tests/CMakeLists.txt",
    "historical_report_source_lf_sha256": "7301bfa05c6a7d6ba0e44048ff8f51901cb26c17daced4c75cdeb1c2837443e3",
    "current_lines": 559,
    "current_whole_source_sha256": "57af1d4f4db264aa6774c2ece235a3b1423180f199d14a65a90e3844a83e558d",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 47,
        "last": 72,
        "range_lf_sha256": "da4d286ad2e902bff0f50437419dbae6f8872f9fc581e0e59ed5dd2e73b9e29e"
      },
      {
        "first": 307,
        "last": 323,
        "range_lf_sha256": "f0b42e4e40edc6c6abfc3cc43b37090dc8594b1f39d3792b794c0b396c57c2a0"
      },
      {
        "first": 429,
        "last": 447,
        "range_lf_sha256": "9b41397dc8398d6194aa91ea3b1f7c437bd4d30307433a0c29c03321edb6dc5d"
      },
      {
        "first": 511,
        "last": 550,
        "range_lf_sha256": "a8162d906e9a02a508799d8eb6c5a3e93cb2375bb8fbe4fbc22ef616170f34e8"
      }
    ],
    "current_delta_lf_sha256": "445e8f08425ecf6f8e3505de9634567702fbb897aac137c6421bfe36be1d81e6",
    "current_delta_bytes": 1064,
    "current_delta_whole_read": true,
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -52,0 +53,13 @@ find_package(ZLIB REQUIRED)",
        "raw_hunk_lf_sha256": "d86fe2623fd35ece2f251437485fdf2fc7521c8c4a8068f297cfd41efa953824"
      }
    ],
    "old_read_line_shift": 13,
    "old_read_shift_from_line": 53
  },
  {
    "path": "d2/main/CMakeLists.txt",
    "historical_report_source_lf_sha256": "d832841951211d1387dc8a31ee1952eb85f57c590df04453ca7df58db85f8252",
    "current_lines": 415,
    "current_whole_source_sha256": "73acb72c4419fb6f33223cef603a074c60bcfd33b7a48f744eeabe49b340d47e",
    "current_whole_read": false,
    "read_ranges": [
      {
        "first": 104,
        "last": 121,
        "range_lf_sha256": "060fed47f4f021827c7538e230ccff23315411fb4527b0fee711e35afcd184ff"
      },
      {
        "first": 383,
        "last": 401,
        "range_lf_sha256": "d28dc350a5fd150d09e1f3d9646fc835ebf015c16c2925bfe14abd9e187e1e8c"
      }
    ],
    "current_delta_lf_sha256": "947995dfa4ed1ca6658f1fb5cf2bc4bcdad726a6901f545ad03b504b98fe23b8",
    "current_delta_bytes": 264,
    "current_delta_whole_read": true,
    "hunks": [
      {
        "ordinal": 1,
        "header": "@@ -106,0 +107 @@ set(D2X_MAIN_SOURCES",
        "raw_hunk_lf_sha256": "38dfe1683fac90b8be6ee7e24ea746fc95477f7ea7df4e9fab0f0acb1f063f52"
      }
    ],
    "old_read_line_shift": 1,
    "old_read_shift_from_line": 107
  }
]
```

### Chunk 0114 terminal verification, 2026-10-09

- All five diagnosis gates complete. GQC-1083 ISSUES / GQD-0963 NO_INHERITED_EFFECT. Immutable report temp/general_cleanup_20261006/gq2-review-0114-20261009.md SHA256 0c0d1d65c5ed3e9e2b50a06989920c16c0d34838f7ff21735e5caf08a2445889; scope fingerprint a7722f0b0b288a880e81149991fef60f4adb73e9b95580b252d43bace381da5c. Exact imported body verified; report bytes unchanged. Historical pending checkpoint/report/ledger prose is superseded by this terminal handoff
- Independent audit PASS: exact assigned blobs/source/diff/51 hunks/623 review lines/payload/4350-path manifest/current assigned deltas/scope; 34 independently reconstructed physical read unions and explicit live reconciliation for two concurrent CMake changes; raw report/import; preservation of all prior canonical semantic/product rows; 1070 unique contiguous sorted terminal ranks and 260 product priorities; queue/findings/remediations/owner states and inherited attribution. Four gates checked before PASS; gate 5 then marked complete
- Existing highest GQR-0176 TODO/GQF-0189 OPEN PRIMARY 56 MEDIUM-HIGH (32/0/7/10/7), distinct BR-0209 OPEN observed-completion/required-output owner retained; preserve GQR-0245 DONE/GQF-0259 FIXED. No new root/remediation/reopening/rating/status. All nine original counterparts absent; zero inherited saving
- GQ2 main: 645 units, 255 DONE / 390 TODO. Numbered: 627 chunks, 255 DONE / 372 TODO. Findings 274 (199 OPEN / 75 FIXED); remediations 260 (72 DONE / 187 TODO / 1 DEFERRED). GQ1 remains 818 DONE / 1 TODO. Pending preflights, sweeps, investigations, final-head/worktree reconciliation and closure remain required; overall goal active
- HEAD 8a730eb6aa1d34382b4a76e6f1f5abd3e18aa96c unchanged. Diagnosis documents only; no source/test/script edits, builds/tests/runtime/device/probes/formatters/generators/staging/commits. Concurrent metadata-snapshot/checkpoint-slot work preserved; new implementation itself is not covered by CMake context reads
- Next numbered scope: GQ2-CHUNK-0115, gameplay-view header and route cache/collision/confirmation modules. Discover exact assigned frozen scopes and current deltas before diagnosis; do not widen coverage from prior consumer reads
