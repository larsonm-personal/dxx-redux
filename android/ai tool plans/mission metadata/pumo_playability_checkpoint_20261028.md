# Pumo playability planning checkpoint

As of the completed message runtime, stereo decoder, named-audio source
contracts and full wall-key audit, the planning estimate is three to six further twelve-hour runs
(36-72 development hours), with four runs as the working midpoint, before a
serious human campaign playthrough. This is a judgment about implementation and
integration work, not a measured completion percentage or a release promise.
Human end-to-end acceptance and fixes it uncovers remain necessary before
calling the campaign verified. Do not replace that with automated navigation.

## Established foundations

- All fourteen original levels decode as canonical 27/40, with separate normal
  runtime, owned lighting/fog, asset, save/rewind and Redux controls.
- All 21 message actions resolve to owned MSG rows. The implemented display
  lifecycle has native/source and 80-step Android evidence.
- All 25 WAVs match an independent decoder sample-for-sample on Windows/Linux.
  The 60 sound actions resolve through twelve SND tables to 22 owned WAVs.
  Their resources are available; named playback still needs implementation.
- Required CI now includes 40/41 public and 96 private controls. A complete
  local private run passes. That evidence protects foundations without
  qualifying the original campaign.
- All wall-key byte values retain the distinct source player/robot semantics.
  Source/native and 90 Android controls remove 73 key-value rejections across
  nine levels without changing their other requirements.

## Current original requirements

A fresh inspection of the unchanged HOG
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043
still finds independent runtime blockers in all fourteen levels. Counts below
are levels containing a requirement, not separate work items or progress units.

| Requirement                                              | Levels |
| -------------------------------------------------------- | -----: |
| Effect objects / sounds                                  |     14 |
| Particle objects                                         |     13 |
| Unsupported wall-action kinds / effect targets           |     12 |
| Unsupported object-action kinds                          |     12 |
| Object-action effect targets                             |      7 |
| Wall flags                                               |      6 |
| Segment function 10                                      |      5 |
| Object owner/target lifecycle                            |      4 |
| Lightning / additional object types / segment properties | 3 each |
| Additive wall rendering                                  |      1 |

The outstanding wall actions include 47 named sounds, fifteen texture changes,
nine countdowns, two mine shakes and one disarm. Object actions add thirteen
named sounds and nine cameras. Some apparent requirements may be representation
or classification issues; only source/runtime evidence may remove their gates.
Do not infer that a visual-looking effect is harmless to gameplay.

## Order

1. Finish owned named-sound playback and persistence, then use that voice and
   resource foundation for ambient sound objects. Verify real mixed output,
   repetitions, pause, restoration and cleanup before enabling either consumer.
2. Complete effect-object lifecycle and its enable/disable targets. Preserve
   gameplay cues and ownership even where optional visual polish is deferred.
3. Complete the remaining event actions and world semantics: countdowns,
   texture changes, camera behavior, mine shaking, disarm, remaining wall variants,
   skybox boundaries and remaining segment properties. Reuse the earlier event,
   asset and persistence foundations; audit each against actual XL behavior.
4. Exercise combinations using controlled engine events and ordinary saves,
   refresh the original requirement inventory, and start human campaign
   playtesting when the real package passes admission. Resolve failures in the
   runtime instead of using mission-name exceptions or weakened validation.

These groups overlap and do not map one-to-one to runs. Audio ownership and
cross-feature save behavior are the largest current uncertainties. Keep Redux
as its existing alternate where XL semantics differ. The full-support goal
remains broader than making Pumo the first playable modern campaign.
