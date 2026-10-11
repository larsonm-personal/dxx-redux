# Additional mod candidates

Research checked 2026-10-10. Candidates are not verified downloads. Compatibility work below is an engineering assessment, not an author claim that Redux supports the package

| Candidate and primary source | What it adds | Next compatibility check |
| --- | --- | --- |
| [Irontower Descent 3 inspired textures](https://pumosoftware.com/d2x-xl/) | Alternative high-resolution appearance | Inspect TGA names, animation strips and alpha; convert and test against actual bitmap slots |
| [Irontower tech/natural textures](https://pumosoftware.com/d2x-xl/) | Another replacement texture set | Compare coverage and conflicts with the current high-res and UUD packs |
| [Pumo Mines Style v0.6](https://pumosoftware.com/d2x-xl/) | D1/D2 texture overhaul | Separate ordinary replacements from assets requiring XL behavior |
| [Pumo weapons, explosions and effects](https://pumosoftware.com/d2x-xl/) | Effects-focused variant | Verify frame counts, timing, transparency and runtime references |
| [Descent 3 style weapon/effect packs](https://pumosoftware.com/d2x-xl/) | Separate D1 and D2 downloads | Check whether visual replacements need XL models or effect code |
| [D2X-XL high-resolution models](https://pumosoftware.com/d2x-xl/) | Replacement 3D models | Redux has an ASE loader; inspect model names, material paths, attachments and render cost before claiming support |
| [D2X-XL menu wallpapers](https://pumosoftware.com/d2x-xl/) | Menu backgrounds | Map names/formats to the presentation loader and verify cropping and readability |
| [UUD2SP by Xfing](https://descentbb.net/viewtopic.php?t=21386) | Restores sound assignments and effects, including D1 material | Existing local semantic HAM conversion needs combined UUD2TP, high-res PCM, rate and event-playback acceptance |

Pumo's page also hosts XL-exclusive campaigns, including Pumo Mines and The Sphere. Treat these as a separate engine-compatibility project: successful archive import would not establish support for their level effects and extensions. [Author-hosted catalog](https://pumosoftware.com/d2x-xl/)

The existing model loader comes from XL and calls `ASE::CModel::Read` in `d1/xmodel/xmodel.cpp` and its D2 counterpart. This makes ASE packages plausible candidates, but it is not evidence that every XL model format or feature works

No candidate above has been added to the public recommendation list during this audit. Retain author credits and inspect each actual archive before conversion or publication
