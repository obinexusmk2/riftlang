# RIFT Stage-0 Overview

This brief document summarizes the Stage‑0 tokenizer implementation contained in
`rift-0/` and how it relates to the broader documentation in `docs/`.

## Classical and Quantum Modes

The Stage‑0 tokenizer operates in **dual mode**. By default tokens are
processed classically, but the build system can enable quantum mode via the
`ENABLE_QUANTUM_MODE` CMake option. The `docs/rift-N/specification/` directory
contains detailed design notes on the combined classical/quantum token system.

## Quality Assurance Categories

Stage‑0 QA tests, defined in `rift-0/tests/`, are grouped into four result
classes:

- **True Positive** – valid tokens correctly identified
- **True Negative** – invalid tokens correctly rejected
- **False Positive** – invalid tokens incorrectly accepted
- **False Negative** – valid tokens incorrectly rejected

These tests are executed through the CMake/CTest harness with `make test`.

## Coding Style Note

Within Stage‑0 sources the equality comparisons place the variable before the
constant, e.g. `result == 0`. This inverted "Yoda" ordering avoids accidental
assignment and mirrors the style used throughout the project.
