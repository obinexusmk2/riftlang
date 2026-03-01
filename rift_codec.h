/**
 * @file rift_codec.h
 * @brief RIFTLang Linkable-then-Fileformat Polyglot Codec
 * @author Nnamdi Michael Okpala — OBINexus Constitutional Computing
 *
 * Two-phase codec pipeline:
 *
 *   RIFT source
 *     │
 *     ▼  Phase 1: LINK  (rift_link)
 *   RiftCIRProgram  — Canonical Intermediate Representation
 *     │  consensus_ok: SPAN → TYPE → ASSIGN ordering enforced
 *     │  "you can't send a message before you know where you are"
 *     ▼  Phase 2: CODEC  (rift_codec_emit)
 *   Target file  — JS / Python / Go / Lua / WAT
 *     pure language-specific emission, no C headers
 *
 * Rifter's Way principles applied:
 *   - Forward-only, single-pass linker (no backtracking)
 *   - Memory-first ordering enforced (CIR_SPAN before CIR_ASSIGN)
 *   - Fixed-size flat node array (memory declared before population)
 *   - No NSIGII code, no libpolycall code
 */

#ifndef RIFT_CODEC_H
#define RIFT_CODEC_H

#include "riftlang.h"   /* RiftExecutionMode, RiftTargetLanguage */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define RIFT_CIR_MAX_STR    256
#define RIFT_CIR_MAX_NODES  1024
#define RIFT_CIR_MAX_VARS   64

/* ============================================================================
 * Canonical IR Node Kind
 * ============================================================================ */

/**
 * RiftCIRKind — every RIFT source construct maps to exactly one kind.
 * The linker classifies each line during the single forward pass.
 */
typedef enum {
    CIR_GOVERN = 0,  /* !govern classical|quantum|hybrid          */
    CIR_SPAN,        /* align span<kind> { bytes: N, type: T }    */
    CIR_TYPE_DEF,    /* type Name = {                             */
    CIR_TYPE_FIELD,  /* field: TYPE  (inside type block)          */
    CIR_ASSIGN,      /* name := expr                              */
    CIR_POLICY,      /* policy_fn on name { ... } (body consumed) */
    CIR_WHILE,       /* while (cond) — opening { absorbed         */
    CIR_IF,          /* if (cond)   — opening { absorbed          */
    CIR_BLOCK_CLOSE, /* }  closing a while/if block               */
    CIR_VALIDATE,    /* validate(name)                            */
    CIR_COMMENT,     /* line or block comment */
    CIR_UNKNOWN,     /* unrecognized line → emitted as comment    */
} RiftCIRKind;

/* ============================================================================
 * Canonical IR Node
 * ============================================================================ */

/**
 * RiftCIRNode — one resolved RIFT construct.
 * Fields used depend on kind; unused fields are zero-initialized.
 */
typedef struct {
    RiftCIRKind kind;
    uint32_t    source_line;

    /* CIR_GOVERN */
    char mode[32];

    /* CIR_SPAN */
    char span_kind[32];
    int  span_bytes;

    /* CIR_TYPE_DEF */
    char type_name[RIFT_CIR_MAX_STR];

    /* CIR_TYPE_FIELD */
    char field_name[RIFT_CIR_MAX_STR];
    char field_type[32];
    bool is_last_field;       /* true → close struct brace after this field (Go) */

    /* CIR_ASSIGN */
    char var_name[RIFT_CIR_MAX_STR];
    char expr[RIFT_CIR_MAX_STR];
    bool is_first_use;        /* true → declaration occurrence (let/local/var decl) */

    /* CIR_WHILE / CIR_IF */
    char condition[RIFT_CIR_MAX_STR];

    /* CIR_VALIDATE */
    char validate_arg[RIFT_CIR_MAX_STR];

    /* CIR_POLICY */
    char policy_name[RIFT_CIR_MAX_STR];

    /* CIR_COMMENT / CIR_UNKNOWN */
    char text[RIFT_CIR_MAX_STR];
} RiftCIRNode;

/* ============================================================================
 * Canonical IR Program
 * ============================================================================ */

/**
 * RiftCIRProgram — flat ordered array of resolved CIR nodes.
 *
 * Memory is declared upfront (fixed-size array) before any node is
 * populated — reflecting the Rifter's Way memory-first principle.
 * consensus_ok is set only after the entire source has been validated.
 */
typedef struct {
    RiftCIRNode       nodes[RIFT_CIR_MAX_NODES];
    uint32_t          count;
    RiftExecutionMode mode;          /* from !govern directive, or default CLASSICAL */
    bool              consensus_ok;  /* SPAN→TYPE→ASSIGN ordering passed */
    char              error_msg[256];
} RiftCIRProgram;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * rift_link — Phase 1: parse RIFT source into CIR with consensus validation.
 *
 * Single-pass, forward-only. Enforces:
 *   - CIR_SPAN must appear before any CIR_ASSIGN
 *   - CIR_TYPE_DEF must appear before its first CIR_ASSIGN (if type-checked)
 *   - All multi-line blocks (span, type, policy) are accumulated and
 *     collapsed to single nodes before being committed
 *
 * Returns heap-allocated RiftCIRProgram. Caller frees with rift_cir_program_free().
 * On allocation failure returns NULL.
 * On consensus failure returns non-NULL program with consensus_ok = false
 * and error_msg set.
 *
 * @param source  NUL-terminated RIFT source text
 * @param mode    Execution mode (used as default if !govern is absent)
 */
RiftCIRProgram* rift_link(const char* source, RiftExecutionMode mode);

/**
 * rift_codec_emit — Phase 2: emit CIR program as target language text.
 *
 * Checks consensus_ok first; returns false if consensus failed.
 * Writes file header, then walks nodes in order emitting language-specific
 * text, then writes file footer.
 *
 * @param prog    Linked CIR program (must have consensus_ok == true)
 * @param out     Open FILE* for writing
 * @param target  Target language
 */
bool rift_codec_emit(RiftCIRProgram* prog, FILE* out, RiftTargetLanguage target);

/**
 * rift_cir_program_free — release heap-allocated RiftCIRProgram.
 */
void rift_cir_program_free(RiftCIRProgram* prog);

#endif /* RIFT_CODEC_H */
