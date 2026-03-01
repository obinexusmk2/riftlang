/**
 * @file rift_codec.c
 * @brief RIFTLang Linkable-then-Fileformat Polyglot Codec — Implementation
 * @author Nnamdi Michael Okpala — OBINexus Constitutional Computing
 *
 * Implements two-phase codec:
 *   Phase 1 — rift_link():        RIFT source → RiftCIRProgram (Canonical IR)
 *   Phase 2 — rift_codec_emit():  RiftCIRProgram → target language text
 *
 * Rifter's Way: forward-only, single-pass, memory-first, no recursion.
 * No NSIGII code. No libpolycall code.
 */

#include "rift_codec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Internal string utilities (linker-local, not exported)
 * ============================================================================ */

static const char* cir_trim_left(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void cir_trim_right(char* s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static bool cir_starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/** Copy at most (dest_size-1) chars, always NUL-terminate. */
static void cir_safe_copy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return;
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/** Returns true if name already in vars[0..count-1]. */
static bool cir_var_seen(char vars[][RIFT_CIR_MAX_STR], uint32_t count,
                          const char* name) {
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(vars[i], name) == 0) return true;
    }
    return false;
}

/** Extract content between first '(' and last ')' on the line. */
static void cir_extract_parens(const char* line, char* out, size_t out_size) {
    const char* open = strchr(line, '(');
    if (!open) { cir_safe_copy(out, line, out_size); return; }
    open++;
    const char* close = strrchr(line, ')');
    if (!close || close <= open) { cir_safe_copy(out, open, out_size); return; }
    size_t len = (size_t)(close - open);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, open, len);
    out[len] = '\0';
}

/** Extract span kind from "align span<kind>" line. */
static void cir_extract_span_kind(const char* line, char* out, size_t out_size) {
    const char* open = strchr(line, '<');
    if (!open) { cir_safe_copy(out, "fixed", out_size); return; }
    open++;
    const char* close = strchr(open, '>');
    if (!close) { cir_safe_copy(out, "fixed", out_size); return; }
    size_t len = (size_t)(close - open);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, open, len);
    out[len] = '\0';
}

/* Strip line and block comment prefixes, write inner text to out. */
static void cir_strip_comment_prefix(const char* line, char* out, size_t out_size) {
    const char* s = line;
    if (cir_starts_with(s, "//")) {
        s += 2;
    } else if (cir_starts_with(s, "/*")) {
        s += 2;
        const char* end = strstr(s, "*/");
        if (end) s = end + 2;
    }
    s = cir_trim_left(s);
    cir_safe_copy(out, s, out_size);
    cir_trim_right(out);
}

/* ============================================================================
 * Phase 1 — Linker
 * ============================================================================ */

RiftCIRProgram* rift_link(const char* source, RiftExecutionMode mode) {
    if (!source) return NULL;

    RiftCIRProgram* prog = (RiftCIRProgram*)calloc(1, sizeof(RiftCIRProgram));
    if (!prog) return NULL;

    prog->mode         = mode;
    prog->consensus_ok = false;

    /* Linker state */
    bool seen_span        = false;
    bool in_span_block    = false;
    bool in_type_block    = false;
    bool in_policy_block  = false;
    int  block_depth      = 0;     /* tracks { depth inside while/if */
    uint32_t line_num     = 0;

    /* Pending multi-line nodes — accumulated before commit */
    RiftCIRNode pending;
    memset(&pending, 0, sizeof(pending));
    int pending_field_count = 0;   /* fields collected so far in type block */

    /* Variable tracking for is_first_use */
    char declared_vars[RIFT_CIR_MAX_VARS][RIFT_CIR_MAX_STR];
    uint32_t var_count = 0;

    /* Helper macro: commit a node if there is room */
#define COMMIT(node_ptr) do { \
    if (prog->count < RIFT_CIR_MAX_NODES) { \
        prog->nodes[prog->count++] = *(node_ptr); \
    } \
} while (0)

    /* Walk source line by line */
    const char* p = source;
    while (*p) {
        /* Find end of line */
        const char* eol = strchr(p, '\n');
        size_t line_len = eol ? (size_t)(eol - p) : strlen(p);

        line_num++;

        /* Copy line to a mutable buffer */
        char line_buf[4096];
        size_t copy_len = line_len < sizeof(line_buf) - 1 ? line_len : sizeof(line_buf) - 1;
        memcpy(line_buf, p, copy_len);
        line_buf[copy_len] = '\0';
        cir_trim_right(line_buf);

        const char* trimmed = cir_trim_left(line_buf);

        /* Advance source pointer */
        p = eol ? eol + 1 : p + strlen(p);

        /* Skip empty lines */
        if (!*trimmed) continue;

        /* ------------------------------------------------------------------ */
        /* Inside SPAN block accumulation                                       */
        /* ------------------------------------------------------------------ */
        if (in_span_block) {
            if (cir_starts_with(trimmed, "bytes:")) {
                pending.span_bytes = atoi(cir_trim_left(trimmed + 6));
            }
            if (strchr(trimmed, '}')) {
                /* Commit the SPAN node */
                pending.source_line = line_num;
                COMMIT(&pending);
                seen_span     = true;
                in_span_block = false;
                memset(&pending, 0, sizeof(pending));
            }
            continue;
        }

        /* ------------------------------------------------------------------ */
        /* Inside TYPE block accumulation                                       */
        /* ------------------------------------------------------------------ */
        if (in_type_block) {
            if (strchr(trimmed, '}')) {
                /* Mark the previous field as last (if any) */
                if (prog->count > 0 && pending_field_count > 0) {
                    /* Walk back to find the last CIR_TYPE_FIELD */
                    for (int back = (int)prog->count - 1; back >= 0; back--) {
                        if (prog->nodes[back].kind == CIR_TYPE_FIELD) {
                            prog->nodes[back].is_last_field = true;
                            break;
                        }
                        if (prog->nodes[back].kind == CIR_TYPE_DEF) break;
                    }
                }
                in_type_block       = false;
                pending_field_count = 0;
                memset(&pending, 0, sizeof(pending));
            } else {
                /* Parse "field_name: FIELD_TYPE" */
                const char* colon = strchr(trimmed, ':');
                if (colon) {
                    RiftCIRNode field;
                    memset(&field, 0, sizeof(field));
                    field.kind        = CIR_TYPE_FIELD;
                    field.source_line = line_num;
                    size_t name_len   = (size_t)(colon - trimmed);
                    if (name_len >= RIFT_CIR_MAX_STR) name_len = RIFT_CIR_MAX_STR - 1;
                    char fname[RIFT_CIR_MAX_STR];
                    memcpy(fname, trimmed, name_len);
                    fname[name_len] = '\0';
                    cir_trim_right(fname);
                    cir_safe_copy(field.field_name, fname, sizeof(field.field_name));
                    const char* ftype = cir_trim_left(colon + 1);
                    /* strip trailing comma */
                    char ftbuf[32];
                    cir_safe_copy(ftbuf, ftype, sizeof(ftbuf));
                    cir_trim_right(ftbuf);
                    size_t ftlen = strlen(ftbuf);
                    if (ftlen > 0 && ftbuf[ftlen - 1] == ',') ftbuf[ftlen - 1] = '\0';
                    cir_safe_copy(field.field_type, ftbuf, sizeof(field.field_type));
                    COMMIT(&field);
                    pending_field_count++;
                }
            }
            continue;
        }

        /* ------------------------------------------------------------------ */
        /* Inside POLICY block — consume body, do not emit                     */
        /* ------------------------------------------------------------------ */
        if (in_policy_block) {
            if (strchr(trimmed, '}')) {
                in_policy_block = false;
            }
            continue;
        }

        /* ================================================================== */
        /* Normal line classification                                          */
        /* ================================================================== */

        RiftCIRNode node;
        memset(&node, 0, sizeof(node));
        node.source_line = line_num;

        /* -- COMMENT -------------------------------------------------------- */
        if (cir_starts_with(trimmed, "//") || cir_starts_with(trimmed, "/*")) {
            node.kind = CIR_COMMENT;
            cir_strip_comment_prefix(trimmed, node.text, sizeof(node.text));
            COMMIT(&node);
            continue;
        }

        /* -- GOVERN --------------------------------------------------------- */
        if (cir_starts_with(trimmed, "!govern")) {
            node.kind = CIR_GOVERN;
            const char* m = cir_trim_left(trimmed + 7);
            /* strip trailing comment */
            char mbuf[32];
            cir_safe_copy(mbuf, m, sizeof(mbuf));
            char* space = strchr(mbuf, ' ');
            if (space) *space = '\0';
            char* slash = strchr(mbuf, '/');
            if (slash) *slash = '\0';
            cir_trim_right(mbuf);
            cir_safe_copy(node.mode, mbuf, sizeof(node.mode));
            /* update program mode */
            if (strcmp(mbuf, "quantum") == 0) prog->mode = RIFT_MODE_QUANTUM;
            else if (strcmp(mbuf, "hybrid") == 0) prog->mode = RIFT_MODE_HYBRID;
            else prog->mode = RIFT_MODE_CLASSICAL;
            COMMIT(&node);
            continue;
        }

        /* -- SPAN block start ----------------------------------------------- */
        if (cir_starts_with(trimmed, "align span<")) {
            memset(&pending, 0, sizeof(pending));
            pending.kind = CIR_SPAN;
            cir_extract_span_kind(trimmed, pending.span_kind, sizeof(pending.span_kind));
            pending.span_bytes = 4096;  /* default */
            in_span_block = true;
            continue;
        }

        /* -- TYPE block start ----------------------------------------------- */
        if (cir_starts_with(trimmed, "type ") && strchr(trimmed, '=')) {
            /* Consensus check: span must be declared first */
            if (!seen_span) {
                snprintf(prog->error_msg, sizeof(prog->error_msg),
                    "line %u: type declaration before span (violates memory-first ordering)", line_num);
                return prog;
            }
            node.kind = CIR_TYPE_DEF;
            /* parse "type Name = {" */
            const char* name_start = trimmed + 5;  /* skip "type " */
            const char* eq = strchr(name_start, '=');
            size_t name_len = (size_t)(eq - name_start);
            char tbuf[RIFT_CIR_MAX_STR];
            if (name_len >= sizeof(tbuf)) name_len = sizeof(tbuf) - 1;
            memcpy(tbuf, name_start, name_len);
            tbuf[name_len] = '\0';
            cir_trim_right(tbuf);
            cir_safe_copy(node.type_name, tbuf, sizeof(node.type_name));
            COMMIT(&node);
            in_type_block       = true;
            pending_field_count = 0;
            /* if { is on same line, check for } too */
            if (strchr(trimmed, '}')) {
                in_type_block = false;
            }
            continue;
        }

        /* -- POLICY block --------------------------------------------------- */
        if (cir_starts_with(trimmed, "policy_fn on")) {
            node.kind = CIR_POLICY;
            const char* pname = cir_trim_left(trimmed + 12);  /* skip "policy_fn on" */
            char pbuf[RIFT_CIR_MAX_STR];
            cir_safe_copy(pbuf, pname, sizeof(pbuf));
            /* strip " {" and beyond */
            char* brace = strchr(pbuf, '{');
            if (brace) *brace = '\0';
            cir_trim_right(pbuf);
            cir_safe_copy(node.policy_name, pbuf, sizeof(node.policy_name));
            COMMIT(&node);
            if (!strchr(trimmed, '}')) {
                in_policy_block = true;
            }
            continue;
        }

        /* -- WHILE ---------------------------------------------------------- */
        if (cir_starts_with(trimmed, "while ") || cir_starts_with(trimmed, "while(")) {
            node.kind = CIR_WHILE;
            cir_extract_parens(trimmed, node.condition, sizeof(node.condition));
            COMMIT(&node);
            /* absorb the { if present on this line */
            block_depth++;
            continue;
        }

        /* -- IF ------------------------------------------------------------- */
        if (cir_starts_with(trimmed, "if ") || cir_starts_with(trimmed, "if(")) {
            node.kind = CIR_IF;
            cir_extract_parens(trimmed, node.condition, sizeof(node.condition));
            COMMIT(&node);
            block_depth++;
            continue;
        }

        /* -- BLOCK CLOSE ---------------------------------------------------- */
        if (strcmp(trimmed, "}") == 0) {
            if (block_depth > 0) {
                node.kind = CIR_BLOCK_CLOSE;
                COMMIT(&node);
                block_depth--;
            }
            /* top-level } (e.g. stray) — ignore */
            continue;
        }

        /* -- VALIDATE ------------------------------------------------------- */
        if (cir_starts_with(trimmed, "validate(")) {
            node.kind = CIR_VALIDATE;
            cir_extract_parens(trimmed, node.validate_arg, sizeof(node.validate_arg));
            /* strip trailing ) if extract_parens left it (shouldn't, but guard) */
            size_t alen = strlen(node.validate_arg);
            if (alen > 0 && node.validate_arg[alen - 1] == ')') {
                node.validate_arg[alen - 1] = '\0';
            }
            COMMIT(&node);
            continue;
        }

        /* -- ASSIGN (:=) ---------------------------------------------------- */
        if (strstr(trimmed, ":=")) {
            /* Consensus check: span must precede any assignment */
            if (!seen_span) {
                snprintf(prog->error_msg, sizeof(prog->error_msg),
                    "line %u: assignment before span declaration (violates memory-first ordering)", line_num);
                return prog;
            }
            node.kind = CIR_ASSIGN;
            const char* op = strstr(trimmed, ":=");
            size_t vlen    = (size_t)(op - trimmed);
            char vbuf[RIFT_CIR_MAX_STR];
            if (vlen >= sizeof(vbuf)) vlen = sizeof(vbuf) - 1;
            memcpy(vbuf, trimmed, vlen);
            vbuf[vlen] = '\0';
            cir_trim_right(vbuf);
            cir_safe_copy(node.var_name, vbuf, sizeof(node.var_name));

            const char* expr_start = cir_trim_left(op + 2);
            char ebuf[RIFT_CIR_MAX_STR];
            cir_safe_copy(ebuf, expr_start, sizeof(ebuf));
            /* strip trailing comment */
            char* cmt = strstr(ebuf, "/*");
            if (cmt) { *cmt = '\0'; cir_trim_right(ebuf); }
            cmt = strstr(ebuf, "//");
            if (cmt) { *cmt = '\0'; cir_trim_right(ebuf); }
            cir_safe_copy(node.expr, ebuf, sizeof(node.expr));

            /* is_first_use: true if this name not yet seen */
            node.is_first_use = !cir_var_seen(declared_vars, var_count, node.var_name);
            if (node.is_first_use && var_count < RIFT_CIR_MAX_VARS) {
                cir_safe_copy(declared_vars[var_count], node.var_name, RIFT_CIR_MAX_STR);
                var_count++;
            }
            COMMIT(&node);
            continue;
        }

        /* -- LONE OPEN BRACE ------------------------------------------------ */
        if (strcmp(trimmed, "{") == 0) {
            /* Absorbed by while/if above; standalone { = deeper block */
            block_depth++;
            continue;
        }

        /* -- UNKNOWN -------------------------------------------------------- */
        node.kind = CIR_UNKNOWN;
        cir_safe_copy(node.text, trimmed, sizeof(node.text));
        COMMIT(&node);
    }

#undef COMMIT

    prog->consensus_ok = true;
    return prog;
}

/* ============================================================================
 * Helpers — Go type mapping
 * ============================================================================ */

static const char* cir_go_type(const char* rift_type) {
    if (!rift_type) return "interface{}";
    if (strcmp(rift_type, "INT")   == 0) return "int32";
    if (strcmp(rift_type, "FLOAT") == 0) return "float64";
    if (strcmp(rift_type, "STRING") == 0) return "string";
    return "interface{}";
}

/* ============================================================================
 * Phase 2 — Codec Emission
 * ============================================================================ */

/* Per-target comment prefix */
static const char* cir_comment_prefix(RiftTargetLanguage t) {
    switch (t) {
        case RIFT_TARGET_JS:     return "//";
        case RIFT_TARGET_PYTHON: return "#";
        case RIFT_TARGET_GO:     return "//";
        case RIFT_TARGET_LUA:    return "--";
        case RIFT_TARGET_WAT:    return ";;";
        default:                 return "//";
    }
}

/* Emit file header */
static void codec_emit_header(FILE* out, RiftTargetLanguage target, const char* mode_str) {
    switch (target) {
        case RIFT_TARGET_JS:
            fprintf(out,
                "'use strict';\n"
                "/* Generated by RIFTLang v1.0.0 - %s mode */\n"
                "const rift = require('./bindings/node-riftlang/rift_binding.cjs');\n\n",
                mode_str);
            break;
        case RIFT_TARGET_PYTHON:
            fprintf(out,
                "# -*- coding: utf-8 -*-\n"
                "# Generated by RIFTLang v1.0.0 - %s mode\n"
                "import sys, os\n"
                "sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),\n"
                "                'bindings', 'pyriftlang'))\n"
                "import rift_binding as rift\n\n",
                mode_str);
            break;
        case RIFT_TARGET_GO:
            fprintf(out,
                "// Generated by RIFTLang v1.0.0 - %s mode\n"
                "package main\n\n"
                "import \"fmt\"\n\n"
                "func main() {\n",
                mode_str);
            break;
        case RIFT_TARGET_LUA:
            fprintf(out,
                "-- Generated by RIFTLang v1.0.0 - %s mode\n"
                "local rift = dofile('bindings/lua-riftlang/rift_binding.lua')\n\n",
                mode_str);
            break;
        case RIFT_TARGET_WAT:
            fprintf(out,
                ";; Generated by RIFTLang v1.0.0 - %s mode\n"
                "(module\n"
                "  (import \"rift\" \"validate\" (func $rift_validate (param i32) (result i32)))\n"
                "  (memory (export \"memory\") 1)\n"
                "  (func $main (export \"main\")\n",
                mode_str);
            break;
        default:
            break;
    }
}

/* Emit file footer */
static void codec_emit_footer(FILE* out, RiftTargetLanguage target) {
    switch (target) {
        case RIFT_TARGET_GO:
            fprintf(out, "\t_ = fmt.Sprintf  // suppress unused import\n}\n");
            break;
        case RIFT_TARGET_WAT:
            fprintf(out, "  )\n)\n");
            break;
        default:
            break;
    }
}

/* WAT: two-pass local declarations then body */
static void codec_emit_wat(FILE* out, RiftCIRProgram* prog) {
    const char* mode_str =
        prog->mode == RIFT_MODE_QUANTUM ? "quantum" :
        prog->mode == RIFT_MODE_HYBRID  ? "hybrid"  : "classical";

    codec_emit_header(out, RIFT_TARGET_WAT, mode_str);

    /* Pass 1: emit (local ...) declarations for all first-use assigns */
    for (uint32_t i = 0; i < prog->count; i++) {
        RiftCIRNode* n = &prog->nodes[i];
        if (n->kind == CIR_ASSIGN && n->is_first_use) {
            fprintf(out, "    (local $%s i32)\n", n->var_name);
        }
    }

    /* Pass 2: emit body nodes */
    int depth = 0;
    for (uint32_t i = 0; i < prog->count; i++) {
        RiftCIRNode* n = &prog->nodes[i];
        switch (n->kind) {
            case CIR_GOVERN:
                fprintf(out, "    ;; RIFT: %s mode\n", n->mode);
                break;
            case CIR_SPAN:
                fprintf(out, "    ;; rift: memory span (%s, %d bytes)\n",
                    n->span_kind, n->span_bytes);
                break;
            case CIR_TYPE_DEF:
                fprintf(out, "    ;; type: %s\n", n->type_name);
                break;
            case CIR_TYPE_FIELD:
                break;  /* suppress in WAT */
            case CIR_ASSIGN:
                /* skip local decl (already emitted); emit local.set */
                /* try to parse expr as integer constant */
                {
                    char* endp;
                    long val = strtol(n->expr, &endp, 10);
                    if (*endp == '\0' || isspace((unsigned char)*endp)) {
                        /* numeric literal */
                        fprintf(out, "    (local.set $%s (i32.const %ld))\n",
                            n->var_name, val);
                    } else {
                        /* expression reference — simplified: emit as comment + set 0 */
                        fprintf(out, "    ;; expr: %s = %s\n", n->var_name, n->expr);
                        fprintf(out, "    (local.set $%s (i32.const 0))\n", n->var_name);
                    }
                }
                break;
            case CIR_POLICY:
                fprintf(out, "    ;; policy: %s\n", n->policy_name);
                break;
            case CIR_WHILE:
                fprintf(out, "    (block\n    (loop\n");
                depth++;
                break;
            case CIR_IF:
                fprintf(out, "    (if (then\n");
                depth++;
                break;
            case CIR_BLOCK_CLOSE:
                if (depth > 0) depth--;
                fprintf(out, "    ))\n");
                break;
            case CIR_VALIDATE:
                fprintf(out, "    (call $rift_validate (local.get $%s))\n",
                    n->validate_arg);
                break;
            case CIR_COMMENT:
            case CIR_UNKNOWN:
                if (*n->text)
                    fprintf(out, "    ;; %s\n", n->text);
                break;
        }
    }

    codec_emit_footer(out, RIFT_TARGET_WAT);
}

/* Emit one node for JS / Python / Go / Lua */
static void codec_emit_node(FILE* out, RiftTargetLanguage target,
                             RiftCIRNode* n, int* indent_depth) {
    const char* cpfx = cir_comment_prefix(target);
    bool is_python   = (target == RIFT_TARGET_PYTHON);
    bool is_go       = (target == RIFT_TARGET_GO);
    bool is_lua      = (target == RIFT_TARGET_LUA);
    bool is_js       = (target == RIFT_TARGET_JS);

    /* Compute indentation prefix for body lines.
     * Go:     1 tab per depth level (already inside func main at depth 0)
     * JS/Lua: 4 spaces per depth level
     * Python: 4 spaces per depth level (used for structural indentation) */
    char indent_str[64] = "";
    {
        int depth = *indent_depth;
        if (depth > 0 || is_go) {
            /* Go needs 1 base tab for func main body, plus depth tabs */
            int levels = is_go ? (depth + 1) : depth;
            if (is_go) {
                int tabs = levels < 15 ? levels : 15;
                memset(indent_str, '\t', (size_t)tabs);
                indent_str[tabs] = '\0';
            } else {
                int spaces = levels * 4;
                if (spaces > (int)(sizeof(indent_str) - 1))
                    spaces = (int)(sizeof(indent_str) - 1);
                memset(indent_str, ' ', (size_t)spaces);
                indent_str[spaces] = '\0';
            }
        }
    }

    switch (n->kind) {

        /* -- GOVERN --------------------------------------------------------- */
        case CIR_GOVERN:
            fprintf(out, "%s%s RIFT: %s mode\n", indent_str, cpfx, n->mode);
            break;

        /* -- SPAN ----------------------------------------------------------- */
        case CIR_SPAN:
            fprintf(out, "%s%s rift: memory span (%s, %d bytes)\n",
                indent_str, cpfx, n->span_kind, n->span_bytes);
            break;

        /* -- TYPE DEF ------------------------------------------------------- */
        case CIR_TYPE_DEF:
            if (is_go) {
                fprintf(out, "%stype %s struct {\n", indent_str, n->type_name);
            } else {
                fprintf(out, "%s%s type: %s\n", indent_str, cpfx, n->type_name);
            }
            break;

        /* -- TYPE FIELD ----------------------------------------------------- */
        case CIR_TYPE_FIELD:
            if (is_go) {
                fprintf(out, "%s\t%s %s\n", indent_str, n->field_name, cir_go_type(n->field_type));
                if (n->is_last_field) fprintf(out, "%s}\n\n", indent_str);
            }
            /* suppress for JS, Python, Lua */
            break;

        /* -- ASSIGN --------------------------------------------------------- */
        case CIR_ASSIGN:
            if (is_python || is_lua) {
                if (is_lua && n->is_first_use)
                    fprintf(out, "%slocal %s = %s\n", indent_str, n->var_name, n->expr);
                else
                    fprintf(out, "%s%s = %s\n", indent_str, n->var_name, n->expr);
            } else if (is_js) {
                if (n->is_first_use)
                    fprintf(out, "%slet %s = %s;\n", indent_str, n->var_name, n->expr);
                else
                    fprintf(out, "%s%s = %s;\n", indent_str, n->var_name, n->expr);
            } else if (is_go) {
                if (n->is_first_use)
                    fprintf(out, "%s%s := %s\n", indent_str, n->var_name, n->expr);
                else
                    fprintf(out, "%s%s = %s\n", indent_str, n->var_name, n->expr);
            }
            break;

        /* -- POLICY --------------------------------------------------------- */
        case CIR_POLICY:
            fprintf(out, "%s%s policy: %s\n", indent_str, cpfx, n->policy_name);
            break;

        /* -- WHILE ---------------------------------------------------------- */
        case CIR_WHILE:
            /* emit at current indent, then increase depth for body */
            if (is_python) {
                fprintf(out, "%swhile %s:\n", indent_str, n->condition);
            } else if (is_js) {
                fprintf(out, "%swhile (%s) {\n", indent_str, n->condition);
            } else if (is_go) {
                fprintf(out, "%sfor %s {\n", indent_str, n->condition);
            } else if (is_lua) {
                fprintf(out, "%swhile %s do\n", indent_str, n->condition);
            }
            (*indent_depth)++;
            break;

        /* -- IF ------------------------------------------------------------- */
        case CIR_IF:
            if (is_python) {
                fprintf(out, "%sif %s:\n", indent_str, n->condition);
            } else if (is_js) {
                fprintf(out, "%sif (%s) {\n", indent_str, n->condition);
            } else if (is_go) {
                fprintf(out, "%sif %s {\n", indent_str, n->condition);
            } else if (is_lua) {
                fprintf(out, "%sif %s then\n", indent_str, n->condition);
            }
            (*indent_depth)++;
            break;

        /* -- BLOCK CLOSE ---------------------------------------------------- */
        case CIR_BLOCK_CLOSE:
            if (*indent_depth > 0) (*indent_depth)--;
            /* recalculate indent for the closing brace */
            {
                char close_indent[64] = "";
                int d = *indent_depth;
                if (is_go) {
                    int tabs = (d + 1) < 15 ? (d + 1) : 15;
                    memset(close_indent, '\t', (size_t)tabs);
                    close_indent[tabs] = '\0';
                } else if (!is_python) {
                    int spaces = d * 4;
                    if (spaces > (int)(sizeof(close_indent) - 1)) spaces = (int)(sizeof(close_indent) - 1);
                    memset(close_indent, ' ', (size_t)spaces);
                    close_indent[spaces] = '\0';
                }
                if (is_python) {
                    /* no explicit close — indentation handles it */
                } else if (is_js || is_go) {
                    fprintf(out, "%s}\n", close_indent);
                } else if (is_lua) {
                    fprintf(out, "%send\n", close_indent);
                }
            }
            break;

        /* -- VALIDATE ------------------------------------------------------- */
        case CIR_VALIDATE:
            if (is_python || is_lua) {
                fprintf(out, "%srift.validate(%s)\n", indent_str, n->validate_arg);
            } else if (is_js) {
                fprintf(out, "%srift.validate('%s');\n", indent_str, n->validate_arg);
            } else if (is_go) {
                /* Go binding: emit as fmt.Printf until go-riftlang is imported */
                fprintf(out, "%sfmt.Printf(\"rift.validate: %%v\\n\", %s)\n",
                    indent_str, n->validate_arg);
            }
            break;

        /* -- COMMENT / UNKNOWN --------------------------------------------- */
        case CIR_COMMENT:
        case CIR_UNKNOWN:
            if (*n->text) {
                fprintf(out, "%s%s %s\n", indent_str, cpfx, n->text);
            }
            break;
    }
}

bool rift_codec_emit(RiftCIRProgram* prog, FILE* out, RiftTargetLanguage target) {
    if (!prog || !out) return false;

    if (!prog->consensus_ok) {
        fprintf(stderr, "[rift_codec] consensus failed: %s\n", prog->error_msg);
        return false;
    }

    /* WAT has its own two-pass emitter */
    if (target == RIFT_TARGET_WAT) {
        codec_emit_wat(out, prog);
        return true;
    }

    /* Mode string */
    const char* mode_str =
        prog->mode == RIFT_MODE_QUANTUM ? "quantum" :
        prog->mode == RIFT_MODE_HYBRID  ? "hybrid"  : "classical";

    codec_emit_header(out, target, mode_str);

    int indent_depth = 0;
    for (uint32_t i = 0; i < prog->count; i++) {
        codec_emit_node(out, target, &prog->nodes[i], &indent_depth);
    }

    codec_emit_footer(out, target);
    return true;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void rift_cir_program_free(RiftCIRProgram* prog) {
    free(prog);
}
