# Descent2Workshop HAM library review plan

## Goal
- Clone InsanityBringer/Descent2Workshop into a local temporary folder
- Inspect its included library for HAM parsing or writing code relevant to UUD2SP patch generation and verification
- Compare its assumptions against the current `dxa_metadata_patch.cpp` field-level patching
- Apply small fixes or simplifications if the library exposes a concrete issue

## Tasks
- [x] Clone or refresh the external repo under `temp`
- [x] Locate HAM, HXM, robot, weapon, and sound table parsing code in the workshop library
- [x] Compare field offsets, record sizes, and trailing data handling with the UUD2SP tooling
- [x] Decide whether runtime code can reasonably be simplified by using or mirroring the library
- [x] Apply any small correctness or verification improvements found
- [x] Run targeted validation

## Result
- `LibDescent` confirmed the D2 HAM section order and field layouts used by the UUD2SP patch tooling and `dxa_metadata_patch.cpp`
- The C# library is useful as an independent reference, but not a good runtime dependency for the native Android/C++ HAM patch path
- Its HAM writer normalizes table lengths, so it is not a byte-for-byte verifier for the retail HAM prefix without a custom harness
- The UUD2SP metadata now records the GameBitmapXlat span explicitly and describes the omitted HAXMED bytes as an original trailer after the retail-length HAM prefix