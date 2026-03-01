# RIFTLang Compiler — Codex Task Brief
**Author:** Nnamdi Michael Okpala
**Project:** OBINexus / RIFTLang
**Date:** 2026-03-01
**Session:** OBINexus Cowork Continuity — `riftlang` branch
**Transcript folder:** `.claude/transcripts/`

---

## What Is This Project

This is **RIFTLang** — a standalone, self-governing compiler toolchain built from scratch by Nnamdi Michael Okpala under the OBINexus Computing framework. It is **not** a wrapper around any existing compiler or language tool. Every component is original.

RIFTLang compiles `.rift` source files into target-language outputs. The toolchain identifier chain is:

```
riftlang.exe  →  .so / .a  →  rift.exe  →  gosilang
```

The build orchestration stack is `nlink → polybuild`. The current focus is the core compiler binary: `./bin/riftlang`.

**Constitutional Computing** is the design philosophy: memory governance, policy enforcement, and token lifecycle rules are embedded in the language itself via `!govern` directives.

**No external language toolchains own this project.** RIFTLang is the tool. The `.rift` language is the spec. The bindings (crift, go, lua, node, py, wasm) are downstream outputs, not the source of truth.

---

## Repository Layout

```
riftlang/
├── main.c                  # CLI entry point + transform_source() code generator
├── riftlang.c / .h         # Core runtime: tokens, policy, memory spans
├── rift_codec.c / .h       # Link codec (strncpy-based, see warnings)
├── Makefile                # Cross-platform: Windows (MinGW), Linux, macOS
├── bin/                    # riftlang, libriftlang.so, libriftlang.a
├── build/                  # Object files
├── docs/specification/     # PDFs + .md specs (source of truth)
├── projects/               # Binding targets (SEPARATION OF CONCERNS)
│   ├── crift/              # C binding  (targets/rift_binding.{c,h})
│   ├── go/                 # Go binding
│   ├── luarift/            # Lua binding
│   ├── node-rift/          # Node.js binding
│   ├── pyrift/             # Python binding
│   └── wasm/               # WebAssembly binding
├── test/                   # (empty — needs spec/ structure)
└── .claude/
    ├── todo.md             # ← this file
    └── transcripts/        # session logs
```

---

## Objective

Refine `./bin/riftlang` so that:

1. **The CLI compiles `.rift` → valid C** without generating broken C at file scope.
2. **The generated C compiles cleanly** with `gcc -std=c11` with zero errors.
3. **Spec test suite exists** at `spec/{unit,integration}/` with `.c` test files.
4. **Each binding target** in `projects/` has its own isolated `Makefile` demonstrating separation of concerns.
5. **Platform targets work** on Linux, macOS, and Windows (MinGW).

---

## Bug Report — Current Compiler Errors

All errors originate from `./bin/riftlang ./examples/go/src/quantum_bridge.rift -o examples/go/src/quantum_bridge.c`.

### BUG-001 — Block-Scope Blindness in `transform_source()` ★ CRITICAL

**File:** `main.c`, function `transform_source()` (~line 590)
**Symptom:**
```
quantum_bridge.c:37:5: error: expected identifier or '(' before '}' token
quantum_bridge.c:43:5: error: expected identifier or '(' before '}' token
quantum_bridge.c:49:5: error: expected identifier or '(' before '}' token
quantum_bridge.c:50:5: error: expected identifier or '(' before '}' token
```

**Root Cause:**
`transform_source()` processes the `.rift` file one line at a time using a `while (*line_start)` loop. It has no concept of multi-line block constructs. When the parser sees:

```rift
!govern quantum {
  token_memory: {
    alignment: dynamic(8),
    ...
  },
  ...
}
```

Only the first line (`!govern quantum {`) matches a pattern rule — it emits a comment. Every subsequent line inside the block (`token_memory: {`, `alignment: dynamic(8),`, `},`, `}`) falls through to the "no pattern match" branch. If `preserve_comments` is false, these lines are **silently dropped** or emitted as-is inside the generated `main()` body as literal C tokens. The closing `}` characters of the `.rift` block end up as bare `}` inside `main()`, corrupting the C AST.

**Scope:** Affects all multi-line `.rift` constructs: `!govern {}`, `align span<X> {}`, `type Foo = {}`, `entanglement_registry {}`, `collapse_trigger {}`.

**Fix Strategy (for Codex):**
Implement a block-skip mechanism in `transform_source()`. When a line opens a `.rift`-level block (i.e. the pattern matches a governance/type/span directive that starts with `{`), set a `block_depth` counter. Increment on `{`, decrement on `}`. While `block_depth > 0`, consume lines without emitting them to C output. Emit only the transformed result from the opening line. Example:

```c
int block_depth = 0;
// After a line matches and the pattern outputs a "block open" rule:
// count '{' and '}' in the original rift line
for (const char* p = line; *p; p++) {
    if (*p == '{') block_depth++;
    if (*p == '}') block_depth--;
}
// On subsequent lines with no match, if block_depth > 0: skip silently
if (block_depth > 0 && !transformed) {
    for (const char* p = line; *p; p++) {
        if (*p == '{') block_depth++;
        if (*p == '}') block_depth--;
    }
    // Do NOT emit this line to output
    continue;
}
```

Mark governance directives, `align span<X>`, `type X =`, `entanglement_registry`, `collapse_trigger` patterns as `RIFT_RULE_BLOCK_OPEN = true` so the transformer knows to engage block-skip mode.

---

### BUG-002 — `RIFT_DECLARE_MEMORY` Macro at Non-Function Scope ★ HIGH

**File:** `riftlang.h`, line 883 + `main.c` pattern rule at line ~118
**Symptom:**
```
riftlang.h:883:28: error: initializer element is not constant
  883 |     RiftMemorySpan* name = rift_span_create((span_type), (bytes))
```

**Root Cause:**
The `RIFT_DECLARE_MEMORY` macro expands to a local variable declaration that calls `rift_span_create()`. A function call is not a compile-time constant. If this macro appears at file scope (outside any function), C rejects it.

Currently `transform_source()` emits `main()` at line ~641, then processes `.rift` lines inside it. The `align span<superposed>` pattern (BUG-001) emits `RIFT_DECLARE_MEMORY(span, RIFT_SPAN_SUPERPOSED, ` but without the closing `)` and with broken block parsing, this ends up at an unpredictable scope.

**Fix Strategy:**
Once BUG-001 is fixed and block bodies are consumed correctly, the `align span<X>` rule should emit the complete `RIFT_DECLARE_MEMORY(...)` call with all parameters resolved from the block body before emitting. The span `bytes` value is parsed from `bytes: 64` inside the block. This requires the block-body parser (from BUG-001 fix) to extract field values before emitting the macro call.

Alternatively: defer span initialization — emit `RiftMemorySpan* span = NULL;` at the top of `main()`, then emit `span = rift_span_create(RIFT_SPAN_SUPERPOSED, 64);` as a statement inside `main()`. This avoids the macro-at-scope problem entirely.

---

### BUG-003 — Cleanup Statements Emitted Outside Function Body ★ HIGH

**File:** `main.c`, `transform_source()` line ~715
**Symptom:**
```
quantum_bridge.c:100:5: warning: data definition has no type or storage class
  100 |     rift_policy_context_destroy(g_policy_ctx);
quantum_bridge.c:101:5: error: conflicting types for 'rift_policy_context_destroy'
quantum_bridge.c:102:5: error: expected identifier or '(' before 'return'
  102 |     return 0;
quantum_bridge.c:103:1: error: expected identifier or '(' before '}' token
  103 | }
```

**Root Cause:**
`transform_source()` appends the `main()` closing block (cleanup + `return 0; }`) unconditionally at the end. However, if BUG-001's orphaned `}` tokens have already closed `main()` early (by emitting unbalanced braces into the body), the closing footer is then emitted at file scope — outside any function. The C compiler sees `rift_policy_context_destroy(g_policy_ctx);` as a declaration attempt at global scope, not a function call.

**Fix Strategy:**
BUG-001 fix will prevent orphaned `}` from appearing in the output. Additionally, add a brace-balance counter to `transform_result` to track how many `{` and `}` have been emitted into `main()`. Before appending the footer, assert or verify that the balance is correct. If `main_brace_depth != 1` at footer emission time, emit a diagnostic and pad with missing `}` characters as recovery.

---

### BUG-004 — Unused Pattern Rule Tables ⚠ WARNING

**File:** `main.c`, lines ~318–449
**Symptom:**
```
main.c:383:26: warning: 'g_go_rules' defined but not used
main.c:417:26: warning: 'g_lua_rules' defined but not used
main.c:449:26: warning: 'g_wat_rules' defined but not used
```

**Root Cause:**
`g_go_rules`, `g_lua_rules`, `g_js_rules`, `g_py_rules`, `g_wat_rules` are declared as `static` arrays but never referenced. The output-target detection (which should select the appropriate rule table based on output file extension: `.go`, `.lua`, `.js`, `.py`, `.wat`) is not yet wired into `initialize_transform_engine()` or `transform_source()`.

**Fix Strategy:**
In `compile_rift_file()`, after determining `out_filename`, detect the output extension and pass a `RiftOutputTarget` enum to `initialize_transform_engine()`. The engine should load the appropriate rule table. This is the foundation of **binding separation of concerns** — each output target has its own transformation rules:

```c
typedef enum {
    RIFT_TARGET_C = 0,    // default
    RIFT_TARGET_GO,
    RIFT_TARGET_JS,
    RIFT_TARGET_PY,
    RIFT_TARGET_LUA,
    RIFT_TARGET_WAT,
} RiftOutputTarget;
```

---

### BUG-005 — `rift_codec.c` strncpy Truncation Warnings ⚠ WARNING

**File:** `rift_codec.c`, line 43
**Symptom:**
```
rift_codec.c:43:5: warning: '__builtin_strncpy' output may be truncated
  copying 255 bytes from a string of length 4095
```

**Root Cause:**
`strncpy(dest, src, dest_size - 1)` is called where `src` can be up to 4095 bytes but `dest` buffer is 255 bytes. The truncation is silent. This is a correctness issue in `rift_link()`.

**Fix Strategy:**
Replace `strncpy` with a bounds-safe copy that checks source length first and returns an error if truncation would occur:

```c
if (strlen(src) >= dest_size) {
    return RIFT_CODEC_ERR_TRUNCATION;
}
strncpy(dest, src, dest_size - 1);
dest[dest_size - 1] = '\0';
```

---

### BUG-006 — `emit_binding_output` Dead Code ⚠ WARNING

**File:** `main.c`, line ~1113
**Symptom:**
```
main.c:1113:13: warning: 'emit_binding_output' defined but not used
```

**Root Cause:**
A binding output function was declared but never wired to the output pipeline. This should be connected to BUG-004's output target routing once rule tables are activated.

---

## Structured Refactor Plan for Codex

Work through these phases in order. Each phase should be independently verifiable with `make` and a compile test.

### Phase 1 — Fix Code Generation (BUG-001, BUG-002, BUG-003)

**Goal:** `./bin/riftlang ./projects/crift/src/quantum_bridge.rift -o /tmp/qb.c && gcc -std=c11 -I. /tmp/qb.c -o /tmp/qb -lpthread -lm` exits with code 0.

Tasks:
- [ ] Add `block_depth` tracking to `transform_source()` in `main.c`
- [ ] Add a `is_block_open` flag to `RiftTransformRule` struct
- [ ] Mark all multi-line `.rift` constructs (`!govern`, `align span`, `type X =`, `entanglement_registry`, `collapse_trigger`) as block-open rules
- [ ] Implement block body field parser: extract `bytes`, `alignment`, `max_pairs`, etc. from block content before emitting the macro call
- [ ] Emit `RIFT_DECLARE_MEMORY` as a deferred statement (local variable inside `main()`) rather than as an inline expansion triggered mid-parse
- [ ] Add `main_brace_balance` counter to `TransformResult`; validate before emitting footer

**Verification:**
```bash
./bin/riftlang ./projects/crift/src/quantum_bridge.rift -o /tmp/qb_test.c
gcc -std=c11 -I. /tmp/qb_test.c -o /tmp/qb_test -lpthread -lm -lrt
echo "Exit: $?"
```
Expected: `Exit: 0`

---

### Phase 2 — Wire Output Target Routing (BUG-004, BUG-006)

**Goal:** `./bin/riftlang counter.rift -o counter.go` and `./bin/riftlang counter.rift -o counter.py` produce valid target-language output; no unused-variable warnings.

Tasks:
- [ ] Define `RiftOutputTarget` enum in `main.c` (or `riftlang.h`)
- [ ] Implement `detect_output_target(const char* filename)` that maps `.c → C`, `.go → GO`, `.js → JS`, `.py → PY`, `.lua → LUA`, `.wat → WAT`
- [ ] Pass `RiftOutputTarget` to `initialize_transform_engine()`; select matching rule table
- [ ] Wire `emit_binding_output()` to the pipeline for non-C targets
- [ ] Each target's rule table lives in `projects/{go,pyrift,node-rift,luarift,wasm}/targets/` — copy the appropriate binding header/impl into the search path for that target

**Verification:**
```bash
./bin/riftlang ./projects/go/src/quantum_bridge.rift -o /tmp/qb.go
cat /tmp/qb.go  # should be valid Go structure
```

---

### Phase 3 — Spec Test Suite

**Goal:** `make test` runs all spec tests and exits 0.

Create the following directory layout:

```
spec/
├── unit/
│   ├── test_tokenizer.c         # test riftlang.h token lifecycle
│   ├── test_policy.c            # test RiftPolicyContext create/destroy
│   ├── test_memory_span.c       # test RIFT_DECLARE_MEMORY / rift_span_create
│   └── test_codec.c             # test rift_link() boundary conditions
├── integration/
│   ├── test_classical_compile.c # compile a classical .rift and exec
│   ├── test_quantum_compile.c   # compile a quantum .rift and exec
│   └── test_output_targets.c    # test each of the 6 output targets
└── fixtures/
    ├── minimal_classical.rift   # smallest valid classical .rift
    ├── minimal_quantum.rift     # smallest valid quantum .rift
    └── counter.rift             # simple counter for target tests
```

Each test in `spec/unit/*.c` and `spec/integration/*.c` follows this pattern:

```c
// spec/unit/test_tokenizer.c
#include "../../riftlang.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    // Test: token create/destroy roundtrip
    RiftMemorySpan* span = rift_span_create(RIFT_SPAN_FIXED, 64);
    assert(span != NULL);
    rift_span_destroy(span);
    printf("[PASS] test_tokenizer: span create/destroy\n");
    return 0;
}
```

Add to `Makefile`:
```makefile
SPEC_UNIT_SRCS   := $(wildcard spec/unit/*.c)
SPEC_INTG_SRCS   := $(wildcard spec/integration/*.c)
SPEC_UNIT_BINS   := $(patsubst spec/unit/%.c, build/spec/unit/%, $(SPEC_UNIT_SRCS))
SPEC_INTG_BINS   := $(patsubst spec/integration/%.c, build/spec/integration/%, $(SPEC_INTG_SRCS))

test: $(SPEC_UNIT_BINS) $(SPEC_INTG_BINS)
	@for t in $(SPEC_UNIT_BINS) $(SPEC_INTG_BINS); do $$t || exit 1; done
	@echo "[PASS] All spec tests passed"

build/spec/unit/%: spec/unit/%.c $(OBJ_DIR)/riftlang.o | build/spec/unit
	$(CC) $(CFLAGS) $< $(OBJ_DIR)/riftlang.o $(LIBS) -o $@

build/spec/integration/%: spec/integration/%.c $(OBJ_DIR)/riftlang.o | build/spec/integration
	$(CC) $(CFLAGS) $< $(OBJ_DIR)/riftlang.o $(LIBS) -o $@

build/spec/unit build/spec/integration:
	$(MKDIR) $@
```

---

### Phase 4 — Platform Targets (Windows / macOS / Linux)

**Goal:** `make` succeeds on all three platforms with no platform-specific hacks in source.

Current state: The `Makefile` distinguishes `Windows_NT` vs everything else (treating Linux and macOS as identical). macOS needs explicit handling.

Tasks:
- [ ] Add `$(shell uname -s)` detection to disambiguate `Darwin` (macOS) from `Linux`
- [ ] macOS: use `-dynamiclib` instead of `-shared` for `.dylib`; no `-lrt` (not available on macOS)
- [ ] macOS: default CC should prefer `clang` over `gcc` since Apple ships clang as `gcc`
- [ ] Windows: verify MinGW `-mthreads` is sufficient for `pthread.h` emulation; add `#ifdef _WIN32` guards around `<pthread.h>` includes in `riftlang.h`
- [ ] Add `TARGET_DYLIB := $(BIN_DIR)/lib$(PROJECT_NAME).dylib` for macOS

Suggested Makefile platform block:

```makefile
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    # ... existing Windows block
else ifeq ($(UNAME_S),Darwin)
    PLATFORM  := macos
    CC        := clang
    DLL_EXT   := .dylib
    DLLFLAGS  := -dynamiclib
    LIBS      := -lpthread -lm   # no -lrt on macOS
    CFLAGS    += -D_DARWIN_C_SOURCE
else
    PLATFORM  := linux
    # ... existing Linux block
endif
```

---

### Phase 5 — Binding Separation of Concerns

**Goal:** Each `projects/<target>/` directory is a self-contained binding module with its own `Makefile`, `README.md`, and `targets/` output that is generated by `./bin/riftlang`.

Each binding target demonstrates the following architecture:

```
projects/crift/
├── Makefile                    # builds rift_binding.{c,h} from .rift sources
├── README.md                   # explains crift binding: C ↔ rift token bridge
├── src/
│   ├── quantum_bridge.rift     # quantum governance policy
│   └── governance.rift         # classical governance policy
└── targets/
    ├── rift_binding.h          # generated or hand-verified binding header
    └── rift_binding.c          # generated or hand-verified binding impl
```

Each `projects/<target>/Makefile` should do:

```makefile
# Example: projects/crift/Makefile
RIFTLANG := ../../bin/riftlang
SRC      := src/quantum_bridge.rift src/governance.rift
TARGETS  := $(patsubst src/%.rift, targets/%.c, $(SRC))

all: $(TARGETS)

targets/%.c: src/%.rift
    $(RIFTLANG) -m quantum $< -o $@
```

This enforces the contract: **the compiler produces the bindings; the bindings do not exist independently of the compiler.**

The six bindings and their output extensions are:
- `crift` → `.c`
- `go` → `.go`
- `luarift` → `.lua`
- `node-rift` → `.js`
- `pyrift` → `.py`
- `wasm` → `.wat`

---

## Files Codex Must NOT Touch

- `riftlang.h` — public API header; changes here require spec review
- `docs/specification/` — PDFs are the canonical spec; do not regenerate
- `.claude/transcripts/` — session logs, read-only

---

## How to Run the Compiler Today

```bash
# Build the compiler
make

# Compile a .rift file to C (classical mode, default)
./bin/riftlang ./projects/crift/src/quantum_bridge.rift -o /tmp/qb.c

# Compile with quantum mode, verbose
./bin/riftlang -m quantum -v ./projects/crift/src/quantum_bridge.rift -o /tmp/qb.c

# Show AST + emit JSON
./bin/riftlang -a --emit-ast-json ./test.rift

# Dry run (parse only)
./bin/riftlang --dry-run ./test.rift
```

---

## Session Continuity Note (OBINexus)

This session is part of the OBINexus structured development methodology. Session state is tracked under `.claude/transcripts/`. The #NoGhosting policy applies: work suspended here must resume with full context from this file. Do not reduce scope. Do not forget the toolchain chain: `riftlang → nlink → polybuild → gosilang`.

The Medium references (HACC philosophy, Anti-Ghosting architecture, Rifter ecosystem whitepaper) are in `docs/specification/`. Consult the PDFs before making architectural decisions about the token lifecycle, memory governance model, or policy enforcement timing.

**By Nnamdi Michael Okpala — OBINexus Computing**
