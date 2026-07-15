# Fix Headless Checkpoint Demo Crash Plan

Date: 2026-07-14
Status: In progress

## Objective

Find and fix the common `0xC0000005` crash affecting all committed D2 headless checkpoint demos, then rerun the complete headless regression corpus.

## Plan

- [x] Reproduce the failure across the complete 11-demo D2 headless corpus
- [ ] Resolve the crashing instruction and call stack from dump data or targeted restore tracing
- [ ] Identify the invalid state or initialization ordering that causes checkpoint restoration to crash
- [ ] Implement the smallest symmetric or shared fix required
- [ ] Add focused regression coverage for the root cause
- [ ] Run scoped code quality, Windows builds, native tests, Android assembly, and all 11 headless demos
- [ ] Record the root cause and complete verification results here
