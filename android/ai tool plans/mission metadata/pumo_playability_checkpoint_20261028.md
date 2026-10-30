# Pumo playability planning checkpoint

After the last two long runs and the October 29 audio/resource work, budget
50-80 further development hours, with about 60 hours as the working midpoint,
before a serious human campaign playthrough. That is roughly four to six
thirteen-hour runs including the remainder of the current run.
This is a judgment about implementation and
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
  The October 29 named runtime implements owned playback and saved voice
  positions, with actual output controls. All 60 original actions pass the
  production resource preparation path at 44.1 and 48 kHz; original event
  playback and campaign acceptance remain separate.
- Owned mission/normal/secret sound folders now have source, native and 65-step
  Android output/persistence evidence. All 644 Pumo ambient objects resolve to
  41 owned external WAVs with independent decoded-sample comparisons. This
  establishes resources, not the still-missing spatial ambient runtime. The
  other 220 external sound files require separate consumer/replacement auditing.
- The ordinary sound-bank audit now resolves 1,988 level/sample selections to
  196 external WAVs matching 142 of 183 retail sample names. Pinned XL source,
  independent folder/bank selection and decoded PCM agree. The production
  loader explicitly rejects these unapplied replacements; this is preparation,
  not ordinary or spatial sound playback qualification.
- Required CI selections now contain 43/46 public and 104 private controls.
  See [current audio evidence](../../XL_compat/evidence/named-audio-runtime.md)
  for completed-run receipts and boundaries. These protect foundations without
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
| Unsupported wall-action kinds                            |      9 |
| Effect targets                                           |     12 |
| Unsupported object-action kinds                          |      3 |
| Object-action effect targets                             |      7 |
| Wall flags                                               |      6 |
| Segment function 10                                      |      5 |
| Object owner/target lifecycle                            |      4 |
| Lightning / additional object types / segment properties | 3 each |
| Additive wall rendering                                  |      1 |

After the October 29 named-audio and texture-action implementations, outstanding
wall actions include nine countdowns, two mine shakes and one disarm. Object
actions add nine cameras. The unchanged-HOG inspection previously removed the
47 wall and thirteen object sound-action rejections. The subsequent texture
inspection removes exactly fifteen action rejections across `pmlev1`, `pmlev2`
and `pmlev3`, preserving every other inventory field and blocker. All fourteen
levels remain blocked. See [texture evidence](../../XL_compat/evidence/texture-actions.md).
Some apparent requirements may be representation
or classification issues; only source/runtime evidence may remove their gates.
Do not infer that a visual-looking effect is harmless to gameplay.

Package resources add requirements outside that level-only table. Five POGs
contain 26 true-color replacements. The October 29 implementation now verifies
their single-frame pixels, dimensions, installation and desktop shader output;
Arena supplies normal Android disk/rewind/context/stock-transition controls.
See [current POG evidence](../../XL_compat/evidence/truecolor-pog-runtime.md) for
remaining restrictions. This closes the reproduced indexed-pixel interpretation
gap for these original resources, without admitting Pumo's levels. Twelve
HXMs and fifteen mission/level INIs also need complete application and transition
qualification.
See the [earlier resource audit](../../XL_compat/evidence/truecolor-pog-admission.md).
INI presence alone is not counted as a missing feature: audit the authored
settings against source defaults and their actual consumers.
All fifteen authored INIs select the medium ship, disallow the light/heavy
ships, disable automap marker teleport and permit 3D powerups; `mvlev5.ini` also enables secret
saves. Fourteen correspond to descriptor levels; `pmend.ini` is an additional
member. None contains an audio setting. Source `CAutomap::ReadControls` consumes
the teleport option for F9 marker travel; authored teleport triggers are separate.
`Use3DPowerups` also requires the user's rendering option, and secret-save
permission affects negative mission ordinals. These consumer findings refine
the audit; they do not establish configuration application or transitions.

Do not infer a required hitbox profile from an absent `-collision_model` line.
Source `CMissionConfig::Load` inherits that field for a level INI. Pumo has no
`global.ini`, and a fresh source process begins with the field zero. The source
`Init` method omits its reset, making prior-mission inheritance an explicit
transition hazard. Optional hitbox profiles and static-object collision still
need separate qualification; model-radius evidence alone does not cover them.

## Order

1. Use the completed owned single-frame POG implementation while finishing the
   HXM/configuration audit and remaining texture profiles. Actual rendering and
   transparency must accompany decoding and persistence.
2. Complete ordinary replacement-sound output using the audited bank and folder
   rules, then use it alongside the owned named-sound foundation for ambient
   sound objects. Verify spatial output, ownership, shared channel priorities,
   pause, restoration and cleanup before enabling those consumers. The ambient
   effect-object lifecycle is still blocked.
3. Complete effect-object lifecycle and its enable/disable targets. Preserve
   gameplay cues and ownership even where optional visual polish is deferred.
4. Complete the remaining event actions and world semantics: countdowns,
   camera behavior, mine shaking, disarm, remaining wall variants,
   skybox boundaries and remaining segment properties. Reuse the earlier event,
   asset and persistence foundations; audit each against actual XL behavior.
5. Exercise combinations using controlled engine events and ordinary saves,
   refresh the original requirement inventory, and start human campaign
   playtesting when the real package passes admission. Resolve failures in the
   runtime instead of using mission-name exceptions or weakened validation.

This revises the earlier provisional three-to-six-run estimate after reproducing
the POG gap in the actual runtime and finding the same unverified format in an
existing mechanics fixture. The groups overlap and do not map one-to-one to
runs. Resource application, audio ownership and cross-feature save behavior
are the largest current uncertainties. Keep Redux as its existing alternate
where XL semantics differ. The full-support goal
remains broader than making Pumo the first playable modern campaign.
