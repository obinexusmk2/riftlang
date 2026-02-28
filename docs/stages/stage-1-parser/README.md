# RIFT Stage 1: parser

## Overview
Documentation for RIFT compilation pipeline Stage 1.

## Architecture
- **Input**: Token stream
- **Process**: Syntax parsing & grammar validation
- **Output**: Parse tree

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
