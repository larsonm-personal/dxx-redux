# Complete CD level metadata regressions

## Goal

Inventory extracted CD mission content, deduplicate repeated releases, add every
useful unique mission set to the declarative CD metadata manifest, and generate
reviewable checked-in metadata and route-corpus regressions.

## Plan

- [x] Inventory descriptors, referenced level files, and mission HOG identities
- [x] Separate unique useful missions from duplicates and multiplayer-only data
- [x] Extend the CD metadata source manifest
- [x] Generate normalized metadata regressions and update the route baseline
- [x] Run focused validation and review the final diff

## Results

- Scanned 1,775 extracted descriptor copies representing 208 normalized unique
  descriptors. The selected ordered sources cover all 189 unique non-anarchy
  descriptors; 19 unique anarchy descriptors are intentionally omitted.
- Generated metadata for 174 analyzable missions and 235 levels. The remaining
  15 non-anarchy descriptors are explicitly excluded in the manifest because
  their packaged payload is missing, the mission cannot load, or the headless
  analysis does not terminate within the bounded timeout.
- Ordered SHA-256 descriptor deduplication gives repeated compilation discs one
  canonical owner while retaining Levels of the World provenance and emitting
  only Anniversary-specific extras.
- The route corpus now contains 1,509 reviewed levels.
- The five CD metadata outputs regenerate byte-identically. Focused source
  manifest, JSON normalization, route corpus, travel-time, master regeneration,
  scoped code-quality, and whitespace validation passed.
