# RIFT Stage 4: bytecode

## Overview
Documentation for RIFT compilation pipeline Stage 4.

## Architecture
- **Input**: Validated AST
- **Process**: Bytecode generation & IR optimization
- **Output**: Intermediate bytecode

## Contents
- [Specifications](specifications/)
- [Examples](examples/)
- [Implementation](implementation/)

## OBINexus Compliance
- Toolchain: riftlang.exe → .so.a → rift.exe → gosilang
- Build Stack: nlink → polybuild
- Policy: #NoGhosting + OpenSense

---
*Part of the RIFT ecosystem documentation - Session ID: rift-cleanup-20251021*
