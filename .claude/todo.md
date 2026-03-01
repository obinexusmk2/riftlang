# RIFTLang Compiler — Codex Task Brief
**Author:** Nnamdi Michael Okpala  
**Project:** OBINexus / RIFTLang  
**Date:** 2026-03-01  
**Session:** OBINexus Cowork Continuity — `riftlang` branch  
**Transcript folder:** `.claude/transcripts/`

---

## Mission and Scope

This repository is the **standalone RIFTLang compiler toolchain**. It is original work under OBINexus and is **not** a wrapper around external language compilers.

Toolchain continuity:

```text
riftlang.exe  →  .so / .a  →  rift.exe  →  gosilang
```

Core objective for this Codex session:

1. Refine `./bin/riftlang` as a reliable standalone CLI.
2. Ensure `.rift` transforms produce valid outputs (starting with C).
3. Add `spec/{unit,integration}` C test coverage for compiler/runtime behavior.
4. Validate cross-platform behavior for **Linux, macOS, Windows (MinGW)**.
5. Demonstrate binding generation as **separation of concerns** (`projects/*`).

---

## Repository Intent (Read Before Changes)

- **Source of truth:** `.rift` language + RIFTLang compiler runtime.
- **Bindings are outputs:** `crift`, `go`, `node-rift`, `pyrift`, `luarift`, `wasm` are downstream targets.
- **Do not reframe architecture** as external-tool-driven.
- **Constitutional Computing model** (policy + memory governance + token lifecycle) remains intact.

---

## Immediate Bugs to Address

### BUG-001 (Critical): Block-scope blindness in `transform_source()`
- File: `main.c`
- Problem: line-wise transform emits orphaned `}` from multi-line `.rift` blocks into generated C.
- Required: add block-depth handling for directives such as:
  - `!govern ... { ... }`
  - `align span<...> { ... }`
  - `type X = { ... }`
  - `entanglement_registry { ... }`
  - `collapse_trigger { ... }`

### BUG-002 (High): `RIFT_DECLARE_MEMORY` placement issues
- File: `main.c` interaction with `riftlang.h` macro behavior.
- Problem: invalid macro expansion can escape function-safe context.
- Required: emit safe in-function memory span initialization.

### BUG-003 (High): Footer emitted at invalid scope
- File: `main.c`
- Problem: cleanup/footer can be emitted outside function body after malformed brace output.
- Required: add brace-balance validation before final footer emission.

### BUG-004 (Warning): Output target rule tables are not wired
- File: `main.c`
- Problem: rule sets exist for non-C targets but routing is inactive.
- Required: route by output extension with explicit target enum.

### BUG-005 (Warning): `strncpy` truncation risk in codec
- File: `rift_codec.c`
- Problem: silent truncation warnings and correctness risk.
- Required: enforce bounds checks and return explicit truncation error.

### BUG-006 (Warning): dead `emit_binding_output`
- File: `main.c`
- Problem: function is defined but unused.
- Required: integrate into non-C emission path.

---

## Structured Execution Plan

## Phase 1 — Stabilize C code generation
- Add block-depth tracking in `transform_source()`.
- Extend transform rules with `is_block_open` metadata.
- Parse block bodies where needed (e.g., span `bytes` values).
- Ensure generated C compiles cleanly with `gcc -std=c11`.

**Verification target:**
```bash
./bin/riftlang ./projects/crift/src/quantum_bridge.rift -o /tmp/qb_test.c
gcc -std=c11 -I. /tmp/qb_test.c -o /tmp/qb_test -lpthread -lm -lrt
```

## Phase 2 — Activate target routing and bindings
- Add `RiftOutputTarget` enum and extension-based detection:
  - `.c`, `.go`, `.js`, `.py`, `.lua`, `.wat`
- Route engine rules by output target.
- Wire `emit_binding_output()` for non-C targets.

## Phase 3 — Add spec test suite
Create:

```text
spec/
├── unit/
├── integration/
└── fixtures/
```

Minimum tests:
- `spec/unit/test_tokenizer.c`
- `spec/unit/test_policy.c`
- `spec/unit/test_memory_span.c`
- `spec/unit/test_codec.c`
- `spec/integration/test_classical_compile.c`
- `spec/integration/test_quantum_compile.c`
- `spec/integration/test_output_targets.c`

Wire `make test` to compile and execute all test binaries.

## Phase 4 — Cross-platform build hardening
- Distinguish Linux vs macOS (`uname -s`) in `Makefile`.
- macOS specifics:
  - `clang`
  - `.dylib`
  - `-dynamiclib`
  - no `-lrt`
- Windows (MinGW): confirm threading and required guards.

## Phase 5 — Binding separation-of-concerns demonstration
For each project under `projects/`:
- include local `Makefile`
- compile `src/*.rift` into `targets/*` via `../../bin/riftlang`
- maintain target-specific output extension:
  - `crift -> .c`
  - `go -> .go`
  - `node-rift -> .js`
  - `pyrift -> .py`
  - `luarift -> .lua`
  - `wasm -> .wat`

---

## Deliverables for This Continuity Session

1. Updated compiler implementation for BUG-001..BUG-006.
2. `spec/unit` and `spec/integration` C tests + fixtures.
3. Updated top-level build flow with `make test`.
4. Platform-aware Makefile behavior for Linux/macOS/Windows.
5. Demonstrable generated binding outputs in `projects/*/targets`.
6. Session notes captured under `.claude/transcripts/`.

---

## Guardrails

- Keep RIFTLang standalone and self-governing.
- Do not treat external language ecosystems as source-of-truth.
- Do not alter spec PDFs in `docs/specification/`.
- Preserve architectural intent: governance, memory policy, token lifecycle.

---

## Session Handoff Note

This TODO is written for Codex continuity in OBINexus cowork workflow.

**By Nnamdi Michael Okpala**  
OBINexus Computing
