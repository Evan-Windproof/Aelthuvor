# Changelog

## 1.0.0 — 2026-11-17

- Stabilize the existing opaque Application API 1.0 contract and retain Kernel
  ABI 1.2, cooperative/preemptive profiles, first/best policies and legacy entry.
- Package a portable SDK with four runtimes, explicit entry/startup, static
  libraries, public ABI provenance, license, example sources and member hashes.
- Normalize SDK debug paths and archive metadata; label development packages
  and require a clean committed tree for official release bundles.
- Add ten runnable developer lessons, an external package-only consumer audit,
  multilingual README, API/build/debug references and maintainer handoff.
- Retain all Phase 1–5 regression and real RAM revocation/isolation coverage.

Earlier phases: Phase 1 runtime/allocator (`b10d337`), Phase 2 preemption and
revocation (`1d288be`), Phase 3 devices/Console (`46c7284`), Phase 4 Application
SDK/independent LibOS (`297d7a3`), Phase 5 resource reliability (`ccdf850`).
See docs/history.md for original validation records.
