# RiftFile Example Project Structure

This repository contains example RiftLang bindings for multiple programming languages, demonstrating the governance-first, memory-safe architecture of the RIFT ecosystem.

## Project Structure

```
projects/
├── pyrift/              # Python binding
│   ├── src/
│   │   ├── governance.rift      # Classic mode governance policy
│   │   └── quantum_bridge.rift  # Quantum mode support
│   └── targets/
│       ├── rift_binding.py      # Generated Python binding
│       └── pattern_engine.py    # Pattern matching engine
│
├── crift/               # C binding
│   ├── src/
│   │   ├── governance.rift      # Classic mode governance policy
│   │   └── quantum_bridge.rift  # Quantum mode support
│   └── targets/
│       ├── rift_binding.h       # Generated C header
│       └── rift_binding.c       # Generated C implementation
│
├── luarift/             # Lua binding
│   ├── src/
│   │   ├── governance.rift      # Classic mode governance policy
│   │   └── quantum_bridge.rift  # Quantum mode support
│   └── targets/
│       ├── rift_binding.lua     # Generated Lua binding
│       └── pattern_engine.lua   # Pattern matching engine
│
├── node-rift/           # Node.js binding
│   ├── src/
│   │   ├── governance.rift      # Classic mode governance policy
│   │   └── quantum_bridge.rift  # Quantum mode support
│   └── target/
│       ├── rift_binding.cjs     # Generated CommonJS binding
│       └── pattern_engine.cjs   # Pattern matching engine
│
├── wasm/                # WebAssembly binding
│   ├── src/
│   │   ├── governance.rift      # Classic mode governance policy
│   │   └── quantum_bridge.rift  # Quantum mode support
│   └── target/
│       ├── rift_binding.wat     # Generated WASM text format
│       └── rift_binding.wasm    # Generated WASM binary
│
└── go/                  # Go binding
    ├── src/
    │   ├── governance.rift      # Classic mode governance policy
    │   └── quantum_bridge.rift  # Quantum mode support
    └── target/
        ├── rift_binding.go      # Generated Go binding
        └── pattern_engine.go    # Pattern matching engine
```

## RiftLang Architecture

### Token Triplet Model

Every token in RiftLang follows the triplet model:

```rift
token = (token_type, token_value, token_memory)
```

**Memory is declared BEFORE type or value** - this is a fundamental principle of RiftLang.

### Governance Modes

#### Classic Mode
- Fixed 4096-bit alignment
- Immediate binding with `:=`
- Eager type checking
- Deterministic execution

#### Quantum Mode
- Dynamic 8-qubit alignment
- Deferred binding with `=:`
- Lazy type checking
- Probabilistic execution with superposition and entanglement

### Key Concepts

1. **Memory-First Declaration**: Memory spans are declared before types or values
2. **Policy Enforcement**: All code must pass governance policies before compilation
3. **Thread Safety**: Built-in lock contexts for concurrent access
4. **Quantum Support**: Superposition, entanglement, and collapse operations
5. **Polar Bipartite Pattern Matching**: Left patterns match input, right patterns generate output

## File Types

### `.rift` Files (Source)

Rift policy files that define:
- Governance directives (`!govern`)
- Memory span declarations (`align span`)
- Type definitions (`type`)
- Pattern matching rules (`pattern`)
- Policy functions (`policy_fn`)
- Entanglement registries (`entanglement_registry`)

### Target Language Files

Generated bindings that implement:
- Token lifecycle management
- Memory governance
- Pattern matching engines
- Quantum operations (superpose, entangle, collapse)
- Thread safety mechanisms

## Example: Creating a Token

### Rift Policy (.rift)

```rift
!govern classic {
  token_memory: {
    alignment: fixed(4096),
    access: [CREATE, READ, UPDATE, DELETE],
    phase: deterministic
  }
}

align span<fixed> {
  direction: right -> left,
  bytes: 4096,
  type: continuous,
  open: true
}

type MyInt = {
  bit_width: 32,
  signed: true,
  memory: aligned(4)
}
```

### Python Target

```python
from rift_binding import RiftMemorySpan, RiftToken

# Memory-first declaration
memory = RiftMemorySpan("fixed", 4096)
token = RiftToken("MyInt", memory)

# Value assignment (immediate binding in classic mode)
token.value = 42

# Validate against governance policy
token.validate()
```

### C Target

```c
#include "rift_binding.h"

// Memory-first declaration
rift_memory_span_t* memory = rift_span_create(RIFT_SPAN_FIXED, 4096);
rift_token_t* token = rift_token_create(RIFT_TOKEN_CINT, memory);

// Value assignment (immediate binding)
RIFT_ASSIGN_CLASSICAL(token, 42);

// Validate against governance policy
rift_token_validate(token);
```

### Go Target

```go
import "rift"

// Memory-first declaration
memory := rift.NewRiftMemorySpan(rift.SpanFixed, 4096)
token := rift.NewRiftToken(rift.TokenGoInt, memory)

// Value assignment (immediate binding)
token.SetValue(rift.RiftTokenValue{IntVal: 42})

// Validate against governance policy
token.Validate()
```

## Quantum Mode Example

### Rift Policy (.rift)

```rift
!govern quantum {
  token_memory: {
    alignment: dynamic(8),
    access: [CREATE, READ, UPDATE, DELETE, SUPERPOSE, ENTANGLE],
    phase: probabilistic
  }
}

align span<superposed> {
  direction: right -> left,
  bytes: 64,
  type: distributed,
  open: true
}
```

### Python Target

```python
from rift_binding import RiftMemorySpan, RiftToken, superpose, entangle, collapse

# Create superposed token
states = [1, 2, 3]
token = superpose(*states)

# Entangle with another token
other_token = RiftToken("QObject", RiftMemorySpan("superposed", 64))
entangle_id = entangle(token, other_token)

# Collapse to single state
collapse(token, selected_index=0)
```

## Pattern Matching Engine

The polar bipartite pattern matching engine links input patterns (left) to output patterns (right):

```python
from pattern_engine import PatternEngine

engine = PatternEngine()

# Add pattern pair: left matches input, right generates output
engine.add_pair(
    left_pattern=r"def\s+(\w+)\s*\(\)",
    right_pattern=r"def \1(self):",
    priority=100,
    right_is_literal=True
)

# Match input and generate output
result = engine.match("def myfunc():")
print(result.output)  # "def myfunc(self):"
```

## Building

Each language binding can be built independently:

```bash
# Python
pip install -e pyrift/

# C
make -C crift/

# Lua
lua luarift/targets/rift_binding.lua

# Node.js
npm install node-rift/

# WebAssembly
wat2wasm wasm/target/rift_binding.wat -o wasm/target/rift_binding.wasm

# Go
go build ./go/target/
```

## License

Constitutional Computing Framework - OBINexus

## Quick Start: Counter Example (Classical)

If you are building a simple `counter.rift` program in any project, prefer this flow:

```bash
cd /path/to/riftlang
./bin/riftlang -m classical -o counter.c counter.rift
gcc -o counter counter.c -I. -L./bin -lriftlang -lm -lpthread
./counter
```

### Common compile errors and fixes

- `expected expression before '/' token`:
  usually caused by malformed comment emission in generated C. Keep comments in valid `/* ... */` or `// ...` form.
- `unterminated argument list invoking macro "RIFT_DECLARE_MEMORY"`:
  usually caused by partial `align span<...> { ... }` translation. Ensure the whole span block is closed and translated to one complete `RIFT_DECLARE_MEMORY(...);` line.
- `expected declaration or statement at end of input`:
  usually caused by broken block translation (for example truncated `while (...)` lines). Verify control-flow lines are emitted in full.

The current compiler emits safer C output for these cases by collapsing memory blocks, preserving control-flow lines, and converting `:=` assignments into valid C declarations/assignments.
