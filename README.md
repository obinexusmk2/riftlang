# RIFTLang - Polar Bipartite Pattern Matching System

**Version:** 1.0.0  
**Author:** Nnamdi Michael Okpala  
**Date:** 2026-02-28  
**License:** Constitutional Computing License (CCL) - OBINexus Framework

> *"Only compile what has been governed."*

---

## Table of Contents

1. [Overview](#overview)
2. [Philosophy](#philosophy)
3. [Architecture](#architecture)
4. [Installation](#installation)
5. [Quick Start](#quick-start)
6. [Language Reference](#language-reference)
7. [API Documentation](#api-documentation)
8. [Examples](#examples)
9. [Testing](#testing)
10. [Constitutional Computing](#constitutional-computing)
11. [Roadmap](#roadmap)
12. [Contributing](#contributing)
13. [License](#license)

---

## Overview

RIFTLang is the core language toolchain for the **RIFT ecosystem**, implementing **polar bipartite pattern matching** to transform `.rift` source files into executable C code. It embodies the principles of **constitutional computing**: governance-first, memory-first, and safety-as-first-class.

### Key Features

| Feature | Description |
|---------|-------------|
| **Token Triplet Architecture** | `(type, value, memory)` with 6-state bitfield validation |
| **Polar Bipartite Matching** | Left polarity (input/matcher) + Right polarity (output/generator) |
| **Dual Execution Modes** | Classical (4096b, deterministic) and Quantum (8-qubit, probabilistic) |
| **Memory-First Parsing** | Memory declared before type, type before value |
| **Policy Governance** | 85% validation threshold via `ResultMatrix2x2` |
| **Thread Safety** | POSIX mutex-based `LockContext` with deadlock prevention |
| **Quantum Operations** | Superposition, entanglement, collapse, entropy calculation |

### System Requirements

- **Compiler:** GCC 9+ or MinGW-w64 (Windows) or MSVC 2019+
- **Libraries:** POSIX threads (`pthread`), regex (`regex.h`), math (`libm`)
- **OS:** Linux (glibc 2.28+), Windows 10+, macOS 10.14+
- **Memory:** 4GB RAM minimum, 8GB recommended for large projects

---

## Philosophy

### The RIFTer's Way

1. **Governance over Chaos** — Every system must be governed, not guessed. Policies must be explicit.
2. **Intention Embedded** — Code reflects purpose. Bytecode expresses the same truth as source.
3. **Safety as First-Class** — Thread safety, memory safety, and user safety are not afterthoughts.
4. **Careful Binding** — Bindings are acts of care, not control. Drivers must be explicit and traceable.
5. **Memory Precedes Meaning** — You don't start with type—you start with **memory**.

### Core Principle

> *"When systems fail, build your own."*

RIFTLang emerged from the failure of safety-critical devices to operate transparently. It encodes **dignity into code**, making governance auditable and constitutionally grounded.

---

## Architecture

### Token Triplet Model

Every token in RIFTLang follows the triplet structure:

```
token = (token_type, token_value, token_memory)
```

**Memory governance** is enforced through validation bits:

| Bit | Flag | Description |
|-----|------|-------------|
| `0x01` | `ALLOCATED` | Memory reserved |
| `0x02` | `INITIALIZED` | Value assigned |
| `0x04` | `LOCKED` | Thread-safe access |
| `0x08` | `GOVERNED` | Policy validated |
| `0x10` | `SUPERPOSED` | Quantum superposition |
| `0x20` | `ENTANGLED` | Quantum entanglement |

### Polar Bipartite Pattern Matching

The transformation engine uses **polar bipartite pairs**:

```
LEFT PATTERN (Input/Matcher)  →  RIGHT PATTERN (Output/Generator)
     ^                                    |
     |                                    |
     +------------------------------------+
              Transformation
```

- **Left polarity** (`RIFT_PATTERN_LEFT`): POSIX regex matching source code
- **Right polarity** (`RIFT_PATTERN_RIGHT`): Generates target C code
- **Priority-based selection**: Lower number = higher priority

### Execution Modes

#### Classical Mode (Default)
- **Alignment:** 4096-bit (4KB pages)
- **Binding:** Immediate (`:=`)
- **Concurrency:** Locked, sequential
- **Policy:** Enforced at assignment time

#### Quantum Mode
- **Alignment:** 8-qubit dynamic
- **Binding:** Deferred (`=:`)
- **Concurrency:** Phase-locked, parallel
- **Policy:** Enforced at observation/collapse

#### Hybrid Mode
Switches between classical and quantum contexts using `!govern` directives.

---

## Installation

### From Source

#### Linux/macOS

```bash
# Clone repository
git clone https://github.com/obinexus/riftlang.git
cd riftlang

# Build
make all

# Run tests
make test

# Install system-wide (optional)
sudo make install
```

#### Windows (MinGW)

```powershell
# Using MinGW-w64
mingw32-make all

# Or using PowerShell
.\build.ps1
```

#### Windows (MSVC)

```cmd
# Using Visual Studio Developer Command Prompt
nmake /f Makefile.msvc all
```

### Pre-built Binaries

Download from [GitHub Releases](https://github.com/obinexus/riftlang/releases):

| Platform | Architecture | Download |
|----------|-------------|----------|
| Linux | x86_64 | `riftlang-1.0.0-linux-amd64.tar.gz` |
| Windows | x86_64 | `riftlang-1.0.0-windows-amd64.zip` |
| macOS | x86_64/ARM64 | `riftlang-1.0.0-macos-universal.tar.gz` |

### Verify Installation

```bash
riftlang --help
riftlang -v --version
```

---

## Quick Start

### 1. Create a RIFT Source File

```rift
/* hello.rift - Classical mode example */

!govern classical

/* Memory-first declaration */
align span<row> {
    direction: right -> left,
    bytes: 4096,
    type: continuous,
    open: true
}

/* Type declaration */
type Message = {
    content: STRING,
    length: INT
}

/* Classical assignment (immediate binding) */
msg := "Hello, Constitutional Computing!"

/* Policy enforcement */
policy_fn on message_memory {
    default_access: [READ, WRITE],
    reassert_lock: after every operation
}

/* Output */
print(msg)
```

### 2. Compile

```bash
riftlang -v -m classical -o hello.c hello.rift
```

### 3. Run Generated C Code

```bash
gcc -o hello hello.c -lriftlang
./hello
```

### 4. Quantum Example

```rift
/* quantum.rift - Quantum mode example */

!govern quantum

/* Quantum memory span */
align span<superposed> {
    direction: bidirectional,
    bytes: dynamic,
    type: entangled
}

/* Quantum type */
type Qubit = {
    value: QINT,
    superposition: enabled
}

/* Quantum assignment (deferred binding) */
q_state =: superpose(0, 1, 2, 3)

/* Entangle with another qubit */
other_q =: superpose(0, 1)
entangle(q_state, other_q)

/* Measure (collapses superposition) */
measure(q_state)
```

Compile with quantum mode:

```bash
riftlang -v -m quantum -t 0.90 quantum.rift
```

---

## Language Reference

### Syntax Overview

RIFTLang uses **memory-first parsing**: memory is declared before type, type before value.

### Memory Declarations

```rift
align span<TYPE> {
    direction: right -> left | left -> right,
    bytes: <size>,
    type: continuous | discrete,
    open: true | false
}
```

**Span Types:**
- `fixed` — Fixed-size allocation
- `row` — Row-ordered, expandable
- `continuous` — Continuous memory region
- `superposed` — Quantum superposition span
- `entangled` — Quantum entanglement span

### Type Declarations

```rift
type <Name> = {
    field: <Type>,
    memory: aligned(<bytes>),
    [superposition: enabled]  /* For quantum types */
}
```

**Built-in Types:**
- `INT` — 32-bit signed integer
- `FLOAT` — 64-bit floating point
- `STRING` — Dynamic character array
- `QINT` — Quantum integer (superposable)
- `QFLOAT` — Quantum float (superposable)

### Variable Assignment

**Classical (Immediate):**
```rift
variable := value        /* Binds immediately, type-checked */
```

**Quantum (Deferred):**
```rift
variable =: superpose(a, b, c)  /* Creates superposition, binds at observation */
```

### Governance Directives

```rift
!govern classical    /* Switch to classical mode */
!govern quantum      /* Switch to quantum mode */
!govern hybrid       /* Enable contextual switching */
```

### Policy Enforcement

```rift
policy_fn on <resource> {
    default_access: [CREATE, READ, UPDATE, DELETE],
    reassert_lock: after every operation | when entropy < threshold
}
```

### Quantum Operations

```rift
/* Create superposition */
qvar =: superpose(state1, state2, ..., stateN)

/* Entangle two variables */
entangle(var1, var2)

/* Collapse to specific state */
qvar.collapse(index)

/* Probabilistic measurement */
measure(qvar)

/* Calculate entropy */
entropy := calculate_entropy(qvar)
```

### Control Flow

```rift
if (condition) {
    /* classical block */
}

while (condition) {
    /* loop */
}

for (init; condition; increment) {
    /* loop */
}
```

### Memory Structures

```rift
vector<Type> name       /* Expandable array */
array<Type> name        /* Fixed-size array */
map<KeyType, ValueType> name  /* Key-value store */
```

---

## API Documentation

### Token Lifecycle

```c
/* Create token with memory span */
RiftToken* token = rift_token_create(RIFT_TOKEN_INT, span);

/* Validate token (sets GOVERNED bit) */
bool valid = rift_token_validate(token);

/* Thread-safe access */
rift_token_lock(token);
/* ... modify token ... */
rift_token_unlock(token);

/* Destroy and cleanup */
rift_token_destroy(token);
```

### Pattern Matching Engine

```c
/* Create engine for classical mode */
RiftPatternEngine* engine = rift_pattern_engine_create(RIFT_MODE_CLASSICAL);

/* Add transformation rule */
rift_pattern_engine_add_pair(
    engine,
    "^align[[:space:]]+span<([a-z]+)>",  /* Left: regex pattern */
    "RIFT_DECLARE_MEMORY(span, RIFT_SPAN_\\1, ",  /* Right: C template */
    10,  /* Priority */
    true  /* Right is literal */
);

/* Compile all patterns */
rift_pattern_engine_compile(engine);

/* Match and transform */
size_t output_len;
char* output = rift_pattern_engine_match(engine, input_line, &output_len, NULL);

/* Cleanup */
rift_pattern_engine_destroy(engine);
```

### Policy Validation

```c
/* Create policy matrix with 85% threshold */
RiftResultMatrix2x2* matrix = rift_result_matrix_create(0.85);

/* Validate input/output pair */
RiftPolicyResult result = rift_policy_validate(matrix, input_valid, output_valid);

/* Check if meets threshold */
bool passes = rift_policy_meets_threshold(matrix);

/* Get validation ratio */
double ratio = rift_policy_get_validation_ratio(matrix);

/* Cleanup */
rift_result_matrix_destroy(matrix);
```

### Quantum Operations

```c
/* Create superposition */
RiftToken* states[] = {state0, state1, state2};
rift_token_superpose(q_token, states, 3, NULL);

/* Entangle tokens */
rift_token_entangle(token_a, token_b, 0);

/* Collapse to specific state with policy validation */
rift_token_collapse(q_token, 1, policy_ctx);

/* Probabilistic measurement */
uint32_t collapsed_idx;
double prob;
rift_token_measure(q_token, &collapsed_idx, &prob);

/* Calculate entropy */
double entropy = rift_token_calculate_entropy(q_token);
```

### AST Operations

```c
/* Create AST node */
RiftAstNode* node = rift_ast_create_node(RIFT_AST_DECLARATION, token);

/* Add child node */
rift_ast_add_child(parent, child);

/* Validate entire tree */
rift_ast_validate(root, policy_ctx);

/* Serialize to JSON */
RiftSerialOptions opts = {
    .format = RIFT_SERIAL_JSON,
    .include_source_locs = true,
    .compact = false
};
size_t size;
char* json = rift_ast_serialize(root, &opts, &size);

/* Cleanup */
rift_ast_destroy_node(root);
```

---

## Examples

### Example 1: Classical Counter

```rift
/* counter.rift */

!govern classical

align span<fixed> {
    bytes: 4096,
    type: continuous
}

type Counter = {
    value: INT,
    step: INT
}

counter := 0
max_count := 100

policy_fn on counter_memory {
    default_access: [READ, WRITE],
    reassert_lock: after every operation
}

while (counter < max_count) {
    counter := counter + 1
}

validate(counter)
```

Compile:
```bash
riftlang -v counter.rift -o counter.c
gcc -o counter counter.c -lriftlang
./counter
```

### Example 2: Quantum Random Number Generator

```rift
/* quantum_rng.rift */

!govern quantum

align span<superposed> {
    bytes: 64,
    type: entangled
}

type QRNG = {
    states: QINT,
    entropy: FLOAT
}

/* Create superposition of 8 states */
rng =: superpose(0, 1, 2, 3, 4, 5, 6, 7)

/* Measure to collapse to random state */
result := measure(rng)

/* Verify entropy */
calculated_entropy := calculate_entropy(rng)
```

Compile:
```bash
riftlang -m quantum -v quantum_rng.rift
```

### Example 3: Hybrid Mode with Context Switching

```rift
/* hybrid.rift */

!govern classical

/* Classical setup */
align span<row> {
    bytes: 4096,
    type: continuous
}

data := load_data("input.txt")

/* Switch to quantum for processing */
!govern quantum

align span<superposed> {
    bytes: dynamic,
    type: entangled
}

/* Process in superposition */
processed =: superpose(
    transform_a(data),
    transform_b(data),
    transform_c(data)
)

/* Select best result */
best_result := collapse_by_probability(processed, 0.5)

/* Return to classical */
!govern classical

save(best_result, "output.txt")
```

---

## Testing

### Running Tests

```bash
# Run all tests
make test

# Run with verbose output
make test VERBOSE=1

# Run quantum-specific tests
make test-quantum

# Run with sanitizers
make test SANITIZE=1
```

### Test Structure

```
test/
├── sample.rift           # Basic classical test
├── quantum.rift            # Quantum operations test
├── hybrid.rift             # Mode switching test
├── stress/                 # Performance tests
│   ├── large_file.rift
│   └── many_tokens.rift
└── invalid/                # Error handling tests
    ├── bad_syntax.rift
    └── policy_violation.rift
```

### Writing Tests

Create `.rift` files with expected outputs:

```rift
/* test_case.rift */
!govern classical
x := 42
/* Expected: x = 42 */
```

Run with validation:
```bash
riftlang --dry-run -v test_case.rift
```

---

## Constitutional Computing

### Principles

RIFTLang implements the **OBINexus Constitutional Computing Framework**:

| Principle | Implementation |
|-----------|---------------|
| **Consent-first** | Explicit `!govern` directives required |
| **Half-rule safety** | 2:1 public-to-private key ratios in validation |
| **No silent changes** | All state changes tracked in `validation_bits` |
| **Humans in-the-loop** | Policy enforcement points for critical decisions |
| **Auditable governance** | `ResultMatrix2x2` metrics for accountability |

### Security Model

```
┌─────────────────────────────────────────┐
│           Application Layer              │
│    (RIFT Source, User Intent)           │
├─────────────────────────────────────────┤
│         Policy Enforcement             │
│    (ResultMatrix2x2, 85% threshold)   │
├─────────────────────────────────────────┤
│         Token Governance               │
│    (validation_bits, LockContext)      │
├─────────────────────────────────────────┤
│         Memory Safety                  │
│    (span alignment, use-after-free)    │
├─────────────────────────────────────────┤
│         Hardware Abstraction           │
│    (POSIX threads, virtual memory)     │
└─────────────────────────────────────────┘
```

### Validation Chain

1. **Lexical** — Regex pattern matching (Left polarity)
2. **Syntactic** — AST construction with memory-first ordering
3. **Semantic** — Type checking and policy validation
4. **Governance** — `ResultMatrix2x2` 85% threshold enforcement
5. **Runtime** — Thread-safe execution with lock ordering

---

## Roadmap

### Foundation Track (Current - v1.0.x)

- ✅ **Milestone 1:** Core Architecture (Token, Parser Boundary)
- ✅ **Milestone 2:** Memory & Thread Safety (LockContext, bitfields)
- ✅ **Milestone 3:** Policy Implementation (ResultMatrix2x2, 85% threshold)
- 🔄 **Milestone 4:** NLINK Integration (state minimization)

### Aspirational Track (v1.1.x - v2.0)

- **Milestone 5:** Advanced Parser (Chomsky Type-1/2 grammars)
- **Milestone 6:** Zero-Trust Security (continuous authentication, micro-segmentation)
- **Milestone 7:** Platform Extensions (embedded, WASM, GPU)

### Research Track (v2.0+)

- True quantum computing integration (Qiskit, Cirq bridges)
- Formal verification with Coq/Isabelle
- Self-healing code (automatic policy recovery)

---

## Contributing

### Code of Conduct

This project operates under the **Constitutional Computing Code of Conduct**:

1. **Respect the architecture** — No changes without understanding the compound model
2. **Governance before features** — Every addition must include policy enforcement
3. **Safety never optional** — Thread safety and memory safety are mandatory
4. **Document the intention** — Code must express its purpose clearly

### Development Setup

```bash
# Fork and clone
git clone https://github.com/yourusername/riftlang.git
cd riftlang

# Create branch
git checkout -b feature/your-feature-name

# Build with debug symbols
make DEBUG=1 SANITIZE=1

# Run tests
make test

# Format code
make format

# Submit pull request
```

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `governance`

Example:
```
feat(quantum): add entanglement persistence

Implement long-lived entanglement links with automatic
decoherence detection and recovery.

Closes #123
```

---

## License

**Constitutional Computing License (CCL) v1.0**

Copyright (c) 2026 Nnamdi Michael Okpala

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software with the following constitutional constraints:

1. **Governance Preservation** — Any derivative work must maintain the validation bit architecture and policy enforcement mechanisms.

2. **Safety Inheritance** — Thread safety and memory safety guarantees must be preserved or enhanced in derivative works.

3. **Attribution Chain** — The compound model lineage (grandfather's compound → OBINexus → Your Work) must be acknowledged in documentation.

4. **No Harm** — The Software shall not be used for systems that deny dignity, autonomy, or due process to users.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

---

## Acknowledgments

- **Grandfather's Compound** — The original operating system where trust is architecture
- **Nnamdi Michael Okpala** — Architect of the constitutional computing framework
- **OBINexus Community** — Contributors to the RIFT ecosystem

---

## Contact

**Project:** https://github.com/obinexus/riftlang  
**Issues:** https://github.com/obinexus/riftlang/issues  
**Discussions:** https://github.com/obinexus/riftlang/discussions  
**Email:** nnamdi@obinexus.org

**Respect the scope. Respect the architecture.**
