# Original Pumo P0 requirements, November 7, 2026

This ledger is a fresh static `xl_inspect` decode of the user-provided original
mission HOG at source `55e67309066cead2f73a9c91541ae43bab8a7500`.
All 14 levels decoded and `admission.cpp::runtime_blockers` returned zero for
each. Inspection JSON SHA-256 was
`99245665cd919ede8db29bdbe36262404e3cbc75a250be0b97e607f0058091a8`.
The read-only 2,670-file original Pumo and XL-addon root was verified against
a private complete manifest, SHA-256
`5a971e04170af58c001d393e1b9a5b50d0eac8a67b4f60170c25fb5f3e8545bd`.
The user-supplied camera addon is the documented `e7351d66...` HOG variant;
its camera member matched the expected source. Retail D2 files were separately
verified. No proprietary file contents or mine geometry are published here.

Mission ordinals below come from `pmines.mn2`, not HOG member order. Counts are
decoded records, not evidence that a player can reach, kill or operate them.
Robot, hostage and reactor counts use object types 2, 3 and 9 respectively;
the separate authored reactor-count field is not a count of reactor objects.

| Level | Member     | Robots | Hostages | Reactor objects | Wall triggers | Object triggers | Static blockers |
| ----: | ---------- | -----: | -------: | --------------: | ------------: | --------------: | --------------: |
|     1 | pm-hub.rl2 |      0 |        0 |               0 |             9 |               0 |               0 |
|     2 | mvlev1.rl2 |     21 |        2 |               1 |            22 |               2 |               0 |
|     3 | mvlev2.rl2 |     59 |        4 |               1 |            33 |               2 |               0 |
|     4 | mvlev3.rl2 |    117 |        6 |               1 |            48 |               4 |               0 |
|     5 | mvlev4.rl2 |     78 |        3 |               1 |            52 |               2 |               0 |
|     6 | mvlev6.rl2 |    113 |        0 |               1 |            41 |               2 |               0 |
|     7 | tulev1.rl2 |    167 |       14 |               0 |            80 |               3 |               0 |
|     8 | tulev2.rl2 |    138 |       10 |               1 |            42 |               3 |               0 |
|     9 | tulev4.rl2 |    140 |        0 |               0 |            55 |               1 |               0 |
|    10 | mvend.rl2  |      0 |        0 |               0 |             1 |               0 |               0 |
|    11 | pmlev1.rl2 |     53 |        0 |               1 |            52 |               3 |               0 |
|    12 | pmlev2.rl2 |    133 |        3 |               0 |            74 |               7 |               0 |
|    13 | pmlev3.rl2 |    210 |        0 |               1 |           101 |               7 |               0 |
|    14 | mvlev5.rl2 |     55 |        7 |               1 |            26 |               4 |               0 |

The source-derived late-route contract is deliberately limited:

- Level 7 has keys, 80 wall triggers including indices above 63, three object
  trigger records, a master/enable chain and an authored 99-second countdown.
  Its exit value targets level 8. It has **zero decoded reactor objects** even
  though its authored reactor-count field is 3. Required actor deaths, key
  order, gate sequence and safe approach remain unproven.
- Level 8 has keys, three object trigger records, a master/enable chain and an
  authored 99-second countdown. Its exit value targets level 9. Required
  objectives and actor or gate sequence remain unproven.
- Level 9 has one object trigger record, five teleport actions, 26 boost
  actions, a master/enable chain and an authored 99-second countdown. Its exit
  value targets level 10. It has no decoded reactor object. Required combat,
  teleport and boost use and the normal escape remain unproven.
- Level 10 is a terminal interlude with one automatic message trigger and no
  exit trigger. An exit must not be invented for it.
- Level 14 has four object trigger records, a reactor object, an authored
  99-second countdown and an exit value targeting level 5. The existing
  Coldrom pickup/weapon control does not prove reactor combat, exit operation
  or branch rejoin. Required combat and gate sequence remain unproven.

These authored destinations and trigger relationships are static source
requirements. The separate native Sophodo readiness check establishes an
earned level-7 entry on a controlled input-only variant, not gameplay through
levels 7 to 10 or 14 to 5. Zero static blockers must not be reported as
all-level native admission or a feature pass. The next executable route probe
must start from its exact earned entry save and preserve producer identity,
ordinary actions, normal transitions and fresh-process recovery. Raw original
resources, saves and detailed route geometry remain private.
