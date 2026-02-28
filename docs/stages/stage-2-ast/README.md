# RIFT Stage 2: ast

## Overview
Documentation for RIFT compilation pipeline Stage 2.

## Architecture
- **Input**: Parse tree
- **Process**: AST construction & optimization
- **Output**: Abstract Syntax Tree

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
