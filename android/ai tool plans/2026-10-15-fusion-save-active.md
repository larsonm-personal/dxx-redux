# Preserve optional Fusion autofire state across ordinary save/load

## Scope and witness

Native D1 and D2 write the inactive zero deadline relative to nonzero GameTime64,
then restore it at epoch zero as a negative nonzero (active) deadline. Conversely,
an active deadline exactly due at save time restores as inactive zero. The existing
runtime-save design intends to preserve charged Fusion state.

Public shareware does not include usable Fusion weapon assets. The integration
fixture explicitly copies its valid player laser definition into the Fusion slot,
identically on both legs and after each loader refresh. This synthetic asset fixture
exercises the actual no-input gameplay fire pipeline and ordinary save writer/reader;
it does not qualify retail Fusion parameters.

## Implementation

- Keep the existing relative deadline bytes unchanged
- Append a host-endian uint32 active flag (0 or 1) after wiggle phase in the single
  unpublished runtime extension: native D1 21 / Android 22, ordinary D2 native/Android 45 / imported D1-in-D2 native/Android 46
- Validate the complete flag before applying runtime state; reject other values
- Inactive forces deadline zero; active with rebased deadline zero uses -1, the
  existing nonzero active convention
- Preserve old-format decoding and D2 secret-return runtime policy
- Coordinate imported-D1 translation and Android metadata with the combined extension

## Validation

CLI: test_upstream_compat --fusion-save-runtime <public-data-dir> <scenario>
Scenarios: idle, due, past, soon, future, nonfusion, inactive-charged.
Each compares actual projectile creation and energy after no-input continuation (twelve frames through the future deadline, one otherwise),
checks charge preservation independently,
checks repeated saves at the rebased epoch and a complete opposite-endian restore, rejects flags 2/-1 and every 1..4-byte
truncation, and confirms a valid retry. Runtime validation is after level preparation;
these tests do not claim whole-world transactional rollback after malformed saves.
Existing guided ownership, phase continuation, cold load, and endian fixtures must
continue passing with the final combined tail lengths. Build/test artifacts live in
/tmp due to workspace disk limits. Targeted current changed translation units link
against reusable engine objects; no clean full-current-tree/Android qualification.

## Status

- [x] Reproduce inactive and due-now causal gameplay failures
- [x] Implement and compile both native engines
- [x] All fourteen optional timer scenarios pass, including malformed-tail retries
- [x] Rerun guided and phase integration coverage (nine guided, six phase cases)
- [ ] Combined extension review and translator/metadata integration

PowerShell catalogs/formatters are not run under the explicit no-PowerShell constraint.
