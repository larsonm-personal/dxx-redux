# Sort downloaded mission archives

## Goal

Identify archives in `C:\Users\first last\Desktop\d2x-xl levels` that are not exact duplicates of repository mission archives, then sort the new archives by D2X-XL-only versus vanilla/DXX-Rebirth compatibility.

## Plan

- [x] Inventory repository and downloaded archives with byte sizes and SHA-256 hashes
- [x] Inspect each new archive and its mission descriptors/readme files for compatibility requirements
- [x] Create sorted output folders without overwriting source archives
- [x] Verify moved files and record the duplicate and classification results
