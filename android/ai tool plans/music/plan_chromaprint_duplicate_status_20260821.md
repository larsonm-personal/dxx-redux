# Chromaprint duplicate status

## Goal

Treat routine duplicate-track detection as a successful terminal outcome rather
than a Chromaprint failure, while preserving real decode, hash, and cache-write
failures.

## Work plan

- [x] Review the attached precompute log and identify the reported failures
- [x] Trace duplicate outcomes through fingerprinting, cache writing, and progress accounting
- [x] Represent duplicate tracks as completed skips in logs and progress
- [x] Add regression coverage for HOG-contained tracks and true duplicate lookup keys
- [x] Run scoped code quality, focused tests, and the Android debug build

## Findings

The five failed Castaway tracks all report `Duplicate exact music path:
missions/castaway.hog`. Their audio was decoded and fingerprinted successfully;
the conflict was discovered while choosing a unique metadata name. This is a
normal result when imported content overlaps and should not be counted or shown
as a failed Chromaprint track.

The underlying collision was a path-projection bug: every distinct OGG inside
Castaway's HOG was assigned `missions/castaway.hog` as an exact engine lookup
key. HOG tracks now use their individual entry filenames. If imported content
really repeats an exact lookup key, the later duplicate is omitted from the
sidecar rather than aborting publication or marking its fingerprint failed.

Fully fingerprinted missions also republish their sidecar once when background
precompute starts. This repairs sidecars produced by the affected build from
the existing fingerprint cache without re-importing or hashing audio again.

Scoped Kotlin formatting, sidecar and coordinator-monitor tests, native debug
builds for all configured ABIs, and the debug APK build pass.
