# D2X-XL incompatible HOG user experience check

Goal: identify exactly what a user sees when importing an archive whose mission HOG uses the unsupported D2X-XL format.

- [x] Trace archive inspection and mission admission for a non-`DHF` HOG.
- [x] Trace the resulting launcher message shown to the user.
- [x] Report the current behavior and any important ambiguity.

Result: a `.zip` with only an unsupported D2X-XL mission is rejected by the
mission probe and normally falls through to generic archive extraction, which
ends at `No game files found in archive`. A `.7z` or `.rar` is sent directly to
mission import and ends at `Failed to import <filename>`. Neither path explains
that the contained HOG has an unsupported signature.
