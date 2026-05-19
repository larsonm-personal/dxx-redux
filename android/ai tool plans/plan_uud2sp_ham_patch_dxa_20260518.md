# UUD2SP HAM patch DXA plan

## Goal
- Convert the UUD2SP sound-pack DXA away from shipping a full `DESCENT2.HAM`
- Generate a semantic RFC 6902 HAM patch against the correct D2 base HAM
- Embed manifest compatibility metadata so launcher preflight can require the matching base file
- Provide verification that applying the patch to the base HAM reproduces the HAM from the original DXA

## Tasks
- [x] Inspect extracted UUD2SP archive, notes, and existing Xfing HAM patch tooling
- [x] Identify and annotate the required base `DESCENT2.HAM`
- [x] Generate UUD2SP HAM patch and metadata summary
- [x] Repack the DXA without the full HAM file
- [x] Add a verification script for patch plus base HAM versus original DXA HAM
- [x] Run script validation, repack verification, and affected build checks if needed