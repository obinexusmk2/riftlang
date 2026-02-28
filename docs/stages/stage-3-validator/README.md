# RIFT Stage 3: validator

## Overview
Documentation for RIFT compilation pipeline Stage 3.

## Architecture
- **Input**: AST nodes
- **Process**: Semantic validation & constraint checking
- **Output**: Validated AST

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
