/**
 * @file main.c
 * @brief RIFTLang CLI - Polar Bipartite Pattern Matching Interface
 * @author Nnamdi Michael Okpala
 * @version 1.0.0
 * 
 * Constitutional Computing Framework - OBINexus
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>

/* Platform-specific exports */
#ifdef _WIN32
#  ifdef RIFTLANG_OPEN_MAIN
     /* CLI needs main - no DLL decoration */
#    define RIFT_API
#  else
#    define RIFT_API __declspec(dllexport)
#  endif
#else
#  define RIFT_API __attribute__((visibility("default")))
#endif

#include "riftlang.h"
#include "rift_codec.h"

/* ============================================================================
 * CLI Configuration & Constants
 * ============================================================================ */

#define RIFT_VERSION "1.0.0"
#define RIFT_BUILD_DATE "2026-02-28"
#define RIFT_MAX_LINE_LENGTH 8192
#define RIFT_MAX_PATTERNS 256

/* ============================================================================
 * CLI Options Structure
 * ============================================================================ */

typedef struct {
    const char* input_file;         /* Input .rift file */
    const char* output_file;        /* Output file (default: input.c) */
    RiftExecutionMode mode;         /* classical/quantum/hybrid */
    bool verbose;                   /* Verbose output */
    bool compile_only;              /* Compile only, don't link */
    bool show_ast;                  /* Display AST after parsing */
    bool show_tokens;               /* Display token stream */
    bool dry_run;                   /* Parse only, no output */
    double policy_threshold;        /* Policy validation threshold */
    bool emit_ast_json;             /* Emit .rift.ast.json */
    bool emit_ast_binary;           /* Emit .rift.astb */
    bool preserve_comments;         /* Keep comments in output */
    int optimization_level;         /* 0-3 optimization */
    bool quiet;                     /* Suppress non-error output (-q) */
} RiftCliOptions;

/* ============================================================================
 * Target Language Selection
 * RiftTargetLanguage is defined in riftlang.h (moved there for codec sharing)
 * ============================================================================ */

/* Detect target language from output file extension */
static RiftTargetLanguage rift_detect_target(const char* filename) {
    if (!filename) return RIFT_TARGET_C;
    const char* ext = strrchr(filename, '.');
    if (!ext) return RIFT_TARGET_C;
    if (strcmp(ext, ".js") == 0 || strcmp(ext, ".cjs") == 0 || strcmp(ext, ".mjs") == 0)
        return RIFT_TARGET_JS;
    if (strcmp(ext, ".go") == 0)  return RIFT_TARGET_GO;
    if (strcmp(ext, ".lua") == 0) return RIFT_TARGET_LUA;
    if (strcmp(ext, ".py") == 0)  return RIFT_TARGET_PYTHON;
    if (strcmp(ext, ".wat") == 0 || strcmp(ext, ".wasm") == 0) return RIFT_TARGET_WAT;
    return RIFT_TARGET_C;
}

/* ============================================================================
 * Pattern Transformation Rules
 * ============================================================================ */

typedef struct {
    const char* name;               /* Rule name for debugging */
    const char* left_pattern;       /* RIFT regex pattern */
    const char* right_template;     /* C code template */
    uint32_t priority;              /* Lower = higher priority */
    bool right_is_literal;          /* True if template is literal C code */
    RiftExecutionMode applicable_mode; /* Mode this rule applies to */
} RiftTransformRule;

/* Predefined transformation rules for RIFT -> C */
static RiftTransformRule g_transform_rules[] = {
    /* Governance directives - highest priority */
    {
        "govern_classical",
        "^[[:space:]]*!govern[[:space:]]+classical",
        "/* RIFT: Classical mode enabled */",
        1, true, RIFT_MODE_CLASSICAL
    },
    {
        "govern_quantum",
        "^[[:space:]]*!govern[[:space:]]+quantum",
        "/* RIFT: Quantum mode enabled */",
        1, true, RIFT_MODE_QUANTUM
    },
    {
        "govern_hybrid",
        "^[[:space:]]*!govern[[:space:]]+hybrid",
        "/* RIFT: Hybrid mode enabled */",
        1, true, RIFT_MODE_HYBRID
    },
    
    /* Memory-first declarations */
    {
        "span_fixed",
        "^[[:space:]]*align[[:space:]]+span<fixed>[[:space:]]*\\{",
        "RIFT_DECLARE_MEMORY(span, RIFT_SPAN_FIXED, ",
        10, true, RIFT_MODE_CLASSICAL
    },
    {
        "span_row",
        "^[[:space:]]*align[[:space:]]+span<row>[[:space:]]*\\{",
        "RIFT_DECLARE_MEMORY(span, RIFT_SPAN_ROW, ",
        10, true, RIFT_MODE_CLASSICAL
    },
    {
        "span_continuous",
        "^[[:space:]]*align[[:space:]]+span<continuous>[[:space:]]*\\{",
        "RIFT_DECLARE_MEMORY(span, RIFT_SPAN_CONTINUOUS, ",
        10, true, RIFT_MODE_CLASSICAL
    },
    {
        "span_superposed",
        "^[[:space:]]*align[[:space:]]+span<superposed>[[:space:]]*\\{",
        "RIFT_DECLARE_MEMORY(span, RIFT_SPAN_SUPERPOSED, ",
        10, true, RIFT_MODE_QUANTUM
    },
    {
        "span_entangled",
        "^[[:space:]]*align[[:space:]]+span<entangled>[[:space:]]*\\{",
        "RIFT_DECLARE_MEMORY(span, RIFT_SPAN_ENTANGLED, ",
        10, true, RIFT_MODE_QUANTUM
    },
    
    /* Type declarations */
    {
        "type_def",
        "^[[:space:]]*type[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=",
        "/* RIFT type: \\1 */ typedef struct {",
        20, true, RIFT_MODE_CLASSICAL
    },
    {
        "type_int",
        "^[[:space:]]*type[[:space:]]+INT[[:space:]]*=[[:space:]]*\\{[^}]*bit_width:[[:space:]]*32",
        "typedef int32_t RIFT_INT;",
        21, true, RIFT_MODE_CLASSICAL
    },
    {
        "type_float",
        "^[[:space:]]*type[[:space:]]+FLOAT[[:space:]]*=[[:space:]]*\\{[^}]*bit_width:[[:space:]]*64",
        "typedef double RIFT_FLOAT;",
        21, true, RIFT_MODE_CLASSICAL
    },
    {
        "type_qint",
        "^[[:space:]]*type[[:space:]]+QINT[[:space:]]*=",
        "/* Quantum integer type */ typedef struct { int32_t value; double phase; } RIFT_QINT;",
        21, true, RIFT_MODE_QUANTUM
    },
    
    /* Classical assignment - immediate binding */
    {
        "assign_classical",
        "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*:=[[:space:]]*([^;]+)",
        "RIFT_ASSIGN_CLASSICAL(\\1, \\2);",
        30, true, RIFT_MODE_CLASSICAL
    },
    
    /* Quantum assignment - deferred binding */
    {
        "assign_quantum",
        "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*=:[[:space:]]*([^;]+)",
        "RIFT_ASSIGN_QUANTUM(\\1, \\2);",
        30, true, RIFT_MODE_QUANTUM
    },
    
    /* Policy enforcement */
    {
        "policy_fn",
        "^[[:space:]]*policy_fn[[:space:]]+on[[:space:]]+([a-z_]+)",
        "RiftPolicyContext* policy_\\1 = rift_policy_context_create(\"\\1\", threshold, true);",
        40, true, RIFT_MODE_CLASSICAL
    },
    {
        "policy_validate",
        "^[[:space:]]*validate[[:space:]]*\\(",
        "rift_policy_validate(policy_matrix, true, true)",
        41, true, RIFT_MODE_CLASSICAL
    },
    
    /* Quantum operations */
    {
        "entangle",
        "entangle[[:space:]]*\\(([a-zA-Z_]+),[[:space:]]*([a-zA-Z_]+)\\)",
        "rift_token_entangle(\\1, \\2, 0)",
        50, true, RIFT_MODE_QUANTUM
    },
    {
        "superpose",
        "superpose[[:space:]]*\\(([a-zA-Z_]+),[[:space:]]*\\[",
        "rift_token_superpose(\\1, states, count, NULL)",
        50, true, RIFT_MODE_QUANTUM
    },
    {
        "collapse",
        "([a-zA-Z_]+)\\.collapse[[:space:]]*\\(([0-9]+)\\)",
        "rift_token_collapse(\\1, \\2, policy_ctx)",
        51, true, RIFT_MODE_QUANTUM
    },
    {
        "measure",
        "measure[[:space:]]*\\(([a-zA-Z_]+)\\)",
        "rift_token_measure(\\1, &collapsed_idx, &prob)",
        51, true, RIFT_MODE_QUANTUM
    },
    {
        "entropy",
        "calculate_entropy[[:space:]]*\\(([a-zA-Z_]+)\\)",
        "rift_token_calculate_entropy(\\1)",
        52, true, RIFT_MODE_QUANTUM
    },
    
    /* Memory structure constructs */
    {
        "vector_decl",
        "^[[:space:]]*vector[[:space:]]+<([A-Za-z_]+)>[[:space:]]+([a-zA-Z_]+)",
        "RiftToken* \\2 = rift_token_create(RIFT_TOKEN_VECTOR, span_\\1);",
        60, true, RIFT_MODE_CLASSICAL
    },
    {
        "array_decl",
        "^[[:space:]]*array[[:space:]]+<([A-Za-z_]+)>[[:space:]]+([a-zA-Z_]+)",
        "RiftToken* \\2 = rift_token_create(RIFT_TOKEN_ARRAY, span_\\1);",
        60, true, RIFT_MODE_CLASSICAL
    },
    {
        "map_decl",
        "^[[:space:]]*map[[:space:]]+<([A-Za-z_]+),[[:space:]]*([A-Za-z_]+)>[[:space:]]+([a-zA-Z_]+)",
        "RiftToken* \\3 = rift_token_create(RIFT_TOKEN_MAP, span_\\1_\\2);",
        60, true, RIFT_MODE_CLASSICAL
    },
    
    /* Control flow */
    {
        "if_stmt",
        "^[[:space:]]*if[[:space:]]*\\(",
        "if (",
        100, true, RIFT_MODE_CLASSICAL
    },
    {
        "while_stmt",
        "^[[:space:]]*while[[:space:]]*\\(",
        "while (",
        100, true, RIFT_MODE_CLASSICAL
    },
    {
        "for_stmt",
        "^[[:space:]]*for[[:space:]]*\\(",
        "for (",
        100, true, RIFT_MODE_CLASSICAL
    },
    {
        "block_start",
        "^[[:space:]]*\\{",
        "{",
        200, true, RIFT_MODE_CLASSICAL
    },
    {
        "block_end",
        "^[[:space:]]*\\}",
        "}",
        200, true, RIFT_MODE_CLASSICAL
    },
    
    /* Comments - preserve or transform */
    {
        "comment_single",
        "^[[:space:]]*//(.*)$",
        "/*\\1 */",
        1000, true, RIFT_MODE_CLASSICAL
    },
    {
        "comment_multi_start",
        "^[[:space:]]*/\\*",
        "/*",
        1000, true, RIFT_MODE_CLASSICAL
    },
    {
        "comment_multi_end",
        "\\*/[[:space:]]*$",
        "*/",
        1000, true, RIFT_MODE_CLASSICAL
    },
    
    /* End marker */
    { NULL, NULL, NULL, 0, false, RIFT_MODE_CLASSICAL }
};

/* ============================================================================
 * Language-Specific Transform Rules (non-C targets)
 * ============================================================================ */

/* --- JavaScript / Node.js (node-riftlang) --- */
static RiftTransformRule g_js_rules[] = {
    { "js_govern",    "^[[:space:]]*!govern[[:space:]]+[a-z]+",
      "// RIFT: classical mode", 1, true, RIFT_MODE_CLASSICAL },
    { "js_span",      "^[[:space:]]*align[[:space:]]+span<[a-z]+>",
      "// rift: memory span", 10, true, RIFT_MODE_CLASSICAL },
    { "js_span_attr", "^[[:space:]]*(bytes|type):[[:space:]]*[^,}]+",
      "", 11, true, RIFT_MODE_CLASSICAL },
    { "js_type_def",  "^[[:space:]]*type[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=",
      "// type: \\1", 20, true, RIFT_MODE_CLASSICAL },
    { "js_type_field","^[[:space:]]*([a-z_]+):[[:space:]]*[A-Z]+",
      "", 21, true, RIFT_MODE_CLASSICAL },
    { "js_assign",    "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*:=[[:space:]]*([^;]+)",
      "let \\1 = \\2;", 30, true, RIFT_MODE_CLASSICAL },
    { "js_policy",    "^[[:space:]]*policy_fn[[:space:]]+on[[:space:]]+([a-z_]+)",
      "// policy: \\1", 40, true, RIFT_MODE_CLASSICAL },
    { "js_policy_attr","^[[:space:]]*(default_access|reassert_lock):[[:space:]]*[^\n]+",
      "", 41, true, RIFT_MODE_CLASSICAL },
    { "js_validate",  "^[[:space:]]*validate[[:space:]]*\\(([^)]+)\\)",
      "rift.validate('\\1');", 42, true, RIFT_MODE_CLASSICAL },
    { "js_while",     "^[[:space:]]*while[[:space:]]*\\(",
      "while (", 100, true, RIFT_MODE_CLASSICAL },
    { "js_if",        "^[[:space:]]*if[[:space:]]*\\(",
      "if (", 100, true, RIFT_MODE_CLASSICAL },
    { "js_for",       "^[[:space:]]*for[[:space:]]*\\(",
      "for (", 100, true, RIFT_MODE_CLASSICAL },
    { "js_block_open","^[[:space:]]*\\{",  "{",  200, true, RIFT_MODE_CLASSICAL },
    { "js_block_close","^[[:space:]]*\\}", "}",  200, true, RIFT_MODE_CLASSICAL },
    { "js_comment_sl","^[[:space:]]*//(.*)$", "//\\1", 1000, true, RIFT_MODE_CLASSICAL },
    { "js_comment_ms","^[[:space:]]*/\\*",   "/*",   1000, true, RIFT_MODE_CLASSICAL },
    { "js_comment_me","\\*/[[:space:]]*$",   "*/",   1000, true, RIFT_MODE_CLASSICAL },
    { NULL, NULL, NULL, 0, false, RIFT_MODE_CLASSICAL }
};

/* --- Python (pyriftlang) --- */
static RiftTransformRule g_py_rules[] = {
    { "py_govern",    "^[[:space:]]*!govern[[:space:]]+[a-z]+",
      "# RIFT: classical mode", 1, true, RIFT_MODE_CLASSICAL },
    { "py_span",      "^[[:space:]]*align[[:space:]]+span<[a-z]+>",
      "# rift: memory span", 10, true, RIFT_MODE_CLASSICAL },
    { "py_span_attr", "^[[:space:]]*(bytes|type):[[:space:]]*[^,}]+",
      "", 11, true, RIFT_MODE_CLASSICAL },
    { "py_type_def",  "^[[:space:]]*type[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=",
      "# type: \\1", 20, true, RIFT_MODE_CLASSICAL },
    { "py_type_field","^[[:space:]]*([a-z_]+):[[:space:]]*[A-Z]+",
      "", 21, true, RIFT_MODE_CLASSICAL },
    { "py_assign",    "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*:=[[:space:]]*([^;]+)",
      "\\1 = \\2", 30, true, RIFT_MODE_CLASSICAL },
    { "py_policy",    "^[[:space:]]*policy_fn[[:space:]]+on[[:space:]]+([a-z_]+)",
      "# policy: \\1", 40, true, RIFT_MODE_CLASSICAL },
    { "py_policy_attr","^[[:space:]]*(default_access|reassert_lock):[[:space:]]*[^\n]+",
      "", 41, true, RIFT_MODE_CLASSICAL },
    { "py_validate",  "^[[:space:]]*validate[[:space:]]*\\(([^)]+)\\)",
      "rift.validate(\\1)", 42, true, RIFT_MODE_CLASSICAL },
    { "py_while",     "^[[:space:]]*while[[:space:]]*\\(([^)]+)\\)[[:space:]]*\\{",
      "while \\1:", 100, true, RIFT_MODE_CLASSICAL },
    { "py_if",        "^[[:space:]]*if[[:space:]]*\\(([^)]+)\\)[[:space:]]*\\{",
      "if \\1:", 100, true, RIFT_MODE_CLASSICAL },
    { "py_block_close","^[[:space:]]*\\}", "", 200, true, RIFT_MODE_CLASSICAL },
    { "py_comment_sl","^[[:space:]]*//(.*)$", "#\\1", 1000, true, RIFT_MODE_CLASSICAL },
    { "py_comment_ms","^[[:space:]]*/\\*",    "\"\"\"", 1000, true, RIFT_MODE_CLASSICAL },
    { "py_comment_me","\\*/[[:space:]]*$",    "\"\"\"", 1000, true, RIFT_MODE_CLASSICAL },
    { NULL, NULL, NULL, 0, false, RIFT_MODE_CLASSICAL }
};

/* --- Go (go-riftlang) --- */
static RiftTransformRule g_go_rules[] = {
    { "go_govern",    "^[[:space:]]*!govern[[:space:]]+[a-z]+",
      "// RIFT: classical mode", 1, true, RIFT_MODE_CLASSICAL },
    { "go_span",      "^[[:space:]]*align[[:space:]]+span<[a-z]+>",
      "// rift: memory span", 10, true, RIFT_MODE_CLASSICAL },
    { "go_span_attr", "^[[:space:]]*(bytes|type):[[:space:]]*[^,}]+",
      "", 11, true, RIFT_MODE_CLASSICAL },
    { "go_type_def",  "^[[:space:]]*type[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=",
      "type \\1 struct {", 20, true, RIFT_MODE_CLASSICAL },
    { "go_type_field","^[[:space:]]*([a-z_]+):[[:space:]]*INT",
      "\\1 int32", 21, true, RIFT_MODE_CLASSICAL },
    { "go_assign",    "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*:=[[:space:]]*([^;]+)",
      "\\1 := \\2", 30, true, RIFT_MODE_CLASSICAL },
    { "go_policy",    "^[[:space:]]*policy_fn[[:space:]]+on[[:space:]]+([a-z_]+)",
      "// policy: \\1", 40, true, RIFT_MODE_CLASSICAL },
    { "go_policy_attr","^[[:space:]]*(default_access|reassert_lock):[[:space:]]*[^\n]+",
      "", 41, true, RIFT_MODE_CLASSICAL },
    { "go_validate",  "^[[:space:]]*validate[[:space:]]*\\(([^)]+)\\)",
      "rift.Validate(\\1)", 42, true, RIFT_MODE_CLASSICAL },
    { "go_while",     "^[[:space:]]*while[[:space:]]*\\(([^)]+)\\)[[:space:]]*\\{",
      "for \\1 {", 100, true, RIFT_MODE_CLASSICAL },
    { "go_if",        "^[[:space:]]*if[[:space:]]*\\(",
      "if (", 100, true, RIFT_MODE_CLASSICAL },
    { "go_for",       "^[[:space:]]*for[[:space:]]*\\(",
      "for (", 100, true, RIFT_MODE_CLASSICAL },
    { "go_block_open","^[[:space:]]*\\{",   "{", 200, true, RIFT_MODE_CLASSICAL },
    { "go_block_close","^[[:space:]]*\\}",  "}", 200, true, RIFT_MODE_CLASSICAL },
    { "go_comment_sl","^[[:space:]]*//(.*)$","//\\1", 1000, true, RIFT_MODE_CLASSICAL },
    { "go_comment_ms","^[[:space:]]*/\\*",   "/*",   1000, true, RIFT_MODE_CLASSICAL },
    { "go_comment_me","\\*/[[:space:]]*$",   "*/",   1000, true, RIFT_MODE_CLASSICAL },
    { NULL, NULL, NULL, 0, false, RIFT_MODE_CLASSICAL }
};

/* --- Lua (lua-riftlang) --- */
static RiftTransformRule g_lua_rules[] = {
    { "lua_govern",    "^[[:space:]]*!govern[[:space:]]+[a-z]+",
      "-- RIFT: classical mode", 1, true, RIFT_MODE_CLASSICAL },
    { "lua_span",      "^[[:space:]]*align[[:space:]]+span<[a-z]+>",
      "-- rift: memory span", 10, true, RIFT_MODE_CLASSICAL },
    { "lua_span_attr", "^[[:space:]]*(bytes|type):[[:space:]]*[^,}]+",
      "", 11, true, RIFT_MODE_CLASSICAL },
    { "lua_type_def",  "^[[:space:]]*type[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=",
      "-- type: \\1", 20, true, RIFT_MODE_CLASSICAL },
    { "lua_type_field","^[[:space:]]*([a-z_]+):[[:space:]]*[A-Z]+",
      "", 21, true, RIFT_MODE_CLASSICAL },
    { "lua_assign",    "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*:=[[:space:]]*([^;]+)",
      "local \\1 = \\2", 30, true, RIFT_MODE_CLASSICAL },
    { "lua_policy",    "^[[:space:]]*policy_fn[[:space:]]+on[[:space:]]+([a-z_]+)",
      "-- policy: \\1", 40, true, RIFT_MODE_CLASSICAL },
    { "lua_policy_attr","^[[:space:]]*(default_access|reassert_lock):[[:space:]]*[^\n]+",
      "", 41, true, RIFT_MODE_CLASSICAL },
    { "lua_validate",  "^[[:space:]]*validate[[:space:]]*\\(([^)]+)\\)",
      "rift.validate(\\1)", 42, true, RIFT_MODE_CLASSICAL },
    { "lua_while",     "^[[:space:]]*while[[:space:]]*\\(([^)]+)\\)[[:space:]]*\\{",
      "while \\1 do", 100, true, RIFT_MODE_CLASSICAL },
    { "lua_if",        "^[[:space:]]*if[[:space:]]*\\(([^)]+)\\)[[:space:]]*\\{",
      "if \\1 then", 100, true, RIFT_MODE_CLASSICAL },
    { "lua_block_open","^[[:space:]]*\\{",  "", 200, true, RIFT_MODE_CLASSICAL },
    { "lua_block_close","^[[:space:]]*\\}", "end", 200, true, RIFT_MODE_CLASSICAL },
    { "lua_comment_sl","^[[:space:]]*//(.*)$","--\\1", 1000, true, RIFT_MODE_CLASSICAL },
    { "lua_comment_ms","^[[:space:]]*/\\*",   "--[[", 1000, true, RIFT_MODE_CLASSICAL },
    { "lua_comment_me","\\*/[[:space:]]*$",   "]]",   1000, true, RIFT_MODE_CLASSICAL },
    { NULL, NULL, NULL, 0, false, RIFT_MODE_CLASSICAL }
};

/* --- WebAssembly Text (wat2wasm) --- */
static RiftTransformRule g_wat_rules[] = {
    { "wat_govern",   "^[[:space:]]*!govern[[:space:]]+[a-z]+",
      ";; RIFT: classical mode", 1, true, RIFT_MODE_CLASSICAL },
    { "wat_span",     "^[[:space:]]*align[[:space:]]+span<[a-z]+>",
      ";; rift: memory span", 10, true, RIFT_MODE_CLASSICAL },
    { "wat_span_attr","^[[:space:]]*(bytes|type):[[:space:]]*[^,}]+",
      "", 11, true, RIFT_MODE_CLASSICAL },
    { "wat_type_def", "^[[:space:]]*type[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=",
      ";; type: \\1", 20, true, RIFT_MODE_CLASSICAL },
    { "wat_type_field","^[[:space:]]*([a-z_]+):[[:space:]]*[A-Z]+",
      "", 21, true, RIFT_MODE_CLASSICAL },
    { "wat_assign_i", "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*:=[[:space:]]*([0-9]+)",
      "(local.set $\\1 (i32.const \\2))", 30, true, RIFT_MODE_CLASSICAL },
    { "wat_policy",   "^[[:space:]]*policy_fn[[:space:]]+on[[:space:]]+([a-z_]+)",
      ";; policy: \\1", 40, true, RIFT_MODE_CLASSICAL },
    { "wat_policy_attr","^[[:space:]]*(default_access|reassert_lock):[[:space:]]*[^\n]+",
      "", 41, true, RIFT_MODE_CLASSICAL },
    { "wat_validate", "^[[:space:]]*validate[[:space:]]*\\(([^)]+)\\)",
      "(call $rift_validate)", 42, true, RIFT_MODE_CLASSICAL },
    { "wat_while",    "^[[:space:]]*while[[:space:]]*\\([^)]+\\)[[:space:]]*\\{",
      "(block (loop", 100, true, RIFT_MODE_CLASSICAL },
    { "wat_block_close","^[[:space:]]*\\}", "))", 200, true, RIFT_MODE_CLASSICAL },
    { "wat_comment_sl","^[[:space:]]*//(.*)$",";; \\1", 1000, true, RIFT_MODE_CLASSICAL },
    { "wat_comment_ms","^[[:space:]]*/\\*",   ";; ",   1000, true, RIFT_MODE_CLASSICAL },
    { "wat_comment_me","\\*/[[:space:]]*$",   "",      1000, true, RIFT_MODE_CLASSICAL },
    { NULL, NULL, NULL, 0, false, RIFT_MODE_CLASSICAL }
};

/* ============================================================================
 * Command Line Interface
 * ============================================================================ */

static void print_banner(void) {
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  RIFTLang Compiler v%-8s - Constitutional Computing      ║\n", RIFT_VERSION);
    printf("║  Build: %-10s - OBINexus Framework                      ║\n", RIFT_BUILD_DATE);
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void print_usage(const char* program) {
    print_banner();
    printf("Usage: %s [options] <input.rift>\n", program);
    printf("\nOptions:\n");
    printf("  -o, --output <file>       Output file (default: input.c)\n");
    printf("  -m, --mode <mode>         Execution mode:\n");
    printf("                            classical | quantum | hybrid (default: classical)\n");
    printf("  -t, --threshold <val>     Policy validation threshold 0.0-1.0 (default: 0.85)\n");
    printf("  -c, --compile-only        Compile only, don't link\n");
    printf("  -a, --show-ast            Display AST after parsing\n");
    printf("  --show-tokens             Display token stream\n");
    printf("  --emit-ast-json           Emit .rift.ast.json file\n");
    printf("  --emit-ast-binary         Emit .rift.astb binary file\n");
    printf("  --dry-run                 Parse only, no output generation\n");
    printf("  -O<level>                 Optimization level (0-3, default: 1)\n");
    printf("  -v, --verbose             Verbose output\n");
    printf("  -q, --quiet               Suppress non-error output\n");
    printf("  -h, --help                Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s program.rift                      # Compile to program.c\n", program);
    printf("  %s -m quantum -O2 algo.rift           # Quantum mode, optimized\n", program);
    printf("  %s counter.rift -o counter.js         # JavaScript (node-riftlang)\n", program);
    printf("  %s counter.rift -o counter.py         # Python (pyriftlang)\n", program);
    printf("  %s counter.rift -o counter.go         # Go (go-riftlang)\n", program);
    printf("  %s counter.rift -o counter.lua        # Lua (lua-riftlang)\n", program);
    printf("  %s counter.rift -o counter.wat        # WebAssembly (wat2wasm)\n", program);
    printf("  %s -a --emit-ast-json test.rift       # Show AST + emit JSON\n", program);
    printf("\nOutput target is auto-detected from the output file extension.\n");
    printf("Constitutional Computing: Respect the scope. Respect the architecture.\n");
}

static bool parse_args(int argc, char* argv[], RiftCliOptions* opts) {
    memset(opts, 0, sizeof(RiftCliOptions));
    opts->mode = RIFT_MODE_CLASSICAL;
    opts->policy_threshold = 0.85;
    opts->optimization_level = 1;
    opts->preserve_comments = true;

    int positional_count = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opts->verbose = true;
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            opts->verbose = false;
            /* TODO: Implement quiet mode suppression */
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile-only") == 0) {
            opts->compile_only = true;
        }
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--show-ast") == 0) {
            opts->show_ast = true;
        }
        else if (strcmp(argv[i], "--show-tokens") == 0) {
            opts->show_tokens = true;
        }
        else if (strcmp(argv[i], "--dry-run") == 0) {
            opts->dry_run = true;
        }
        else if (strcmp(argv[i], "--emit-ast-json") == 0) {
            opts->emit_ast_json = true;
        }
        else if (strcmp(argv[i], "--emit-ast-binary") == 0) {
            opts->emit_ast_binary = true;
        }
        else if (strncmp(argv[i], "-O", 2) == 0 && strlen(argv[i]) == 3) {
            int level = argv[i][2] - '0';
            if (level >= 0 && level <= 3) {
                opts->optimization_level = level;
            } else {
                fprintf(stderr, "Error: Invalid optimization level '%s'\n", argv[i]);
                return false;
            }
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                opts->output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: -o requires an argument\n");
                return false;
            }
        }
        else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (i + 1 < argc) {
                const char* mode_str = argv[++i];
                if (strcmp(mode_str, "classical") == 0) {
                    opts->mode = RIFT_MODE_CLASSICAL;
                } else if (strcmp(mode_str, "quantum") == 0) {
                    opts->mode = RIFT_MODE_QUANTUM;
                } else if (strcmp(mode_str, "hybrid") == 0) {
                    opts->mode = RIFT_MODE_HYBRID;
                } else {
                    fprintf(stderr, "Error: Unknown mode '%s'\n", mode_str);
                    return false;
                }
            } else {
                fprintf(stderr, "Error: -m requires an argument\n");
                return false;
            }
        }
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threshold") == 0) {
            if (i + 1 < argc) {
                opts->policy_threshold = atof(argv[++i]);
                if (opts->policy_threshold < 0.0 || opts->policy_threshold > 1.0) {
                    fprintf(stderr, "Error: Threshold must be between 0.0 and 1.0\n");
                    return false;
                }
            } else {
                fprintf(stderr, "Error: -t requires an argument\n");
                return false;
            }
        }
        else if (argv[i][0] != '-') {
            if (positional_count == 0) {
                opts->input_file = argv[i];
            } else if (positional_count == 1 && !opts->output_file) {
                opts->output_file = argv[i];
            }
            positional_count++;
        }
        else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            return false;
        }
    }
    
    if (!opts->input_file) {
        fprintf(stderr, "Error: No input file specified\n");
        return false;
    }
    
    return true;
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

static char* read_file(const char* filename, size_t* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", filename, strerror(errno));
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = (char*)malloc(*size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(content, 1, *size, file);
    content[read] = '\0';
    fclose(file);
    
    if (read != *size) {
        fprintf(stderr, "Warning: Read %zu bytes, expected %zu\n", read, *size);
    }
    
    *size = read;
    return content;
}

static bool write_file(const char* filename, const char* content, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s': %s\n", filename, strerror(errno));
        return false;
    }
    
    size_t written = fwrite(content, 1, size, file);
    fclose(file);
    
    if (written != size) {
        fprintf(stderr, "Error: Wrote %zu bytes, expected %zu\n", written, size);
        return false;
    }
    
    return true;
}

static bool file_exists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/* ============================================================================
 * Pattern Engine Initialization
 * ============================================================================ */

static RiftPatternEngine* initialize_transform_engine(RiftExecutionMode mode, bool verbose) {
    RiftPatternEngine* engine = rift_pattern_engine_create(mode);
    if (!engine) {
        fprintf(stderr, "Error: Failed to create pattern engine\n");
        return NULL;
    }
    
    if (verbose) {
        printf("[RIFTLang] Initializing pattern engine for %s mode\n",
            mode == RIFT_MODE_CLASSICAL ? "classical" :
            mode == RIFT_MODE_QUANTUM ? "quantum" : "hybrid");
    }
    
    /* Add transformation rules based on mode */
    int rules_added = 0;
    for (int i = 0; g_transform_rules[i].name != NULL; i++) {
        RiftTransformRule* rule = &g_transform_rules[i];
        
        /* Skip rules not applicable to current mode (unless hybrid) */
        if (mode != RIFT_MODE_HYBRID && rule->applicable_mode != mode) {
            continue;
        }
        
        bool added = rift_pattern_engine_add_pair(
            engine,
            rule->left_pattern,
            rule->right_template,
            rule->priority,
            rule->right_is_literal
        );
        
        if (added) {
            rules_added++;
            if (verbose) {
                printf("[RIFTLang] Registered rule: %s (priority %d)\n", 
                    rule->name, rule->priority);
            }
        } else {
            fprintf(stderr, "Warning: Failed to register rule '%s'\n", rule->name);
        }
    }
    
    if (!rift_pattern_engine_compile(engine)) {
        fprintf(stderr, "Error: Failed to compile pattern engine\n");
        rift_pattern_engine_destroy(engine);
        return NULL;
    }
    
    if (verbose) {
        printf("[RIFTLang] Pattern engine ready: %d rules active\n", rules_added);
    }
    
    return engine;
}

/* ============================================================================
 * Source Transformation
 * ============================================================================ */

typedef struct {
    char* output;
    size_t output_size;
    size_t output_capacity;
    uint32_t lines_processed;
    uint32_t patterns_matched;
    uint32_t patterns_failed;
    double processing_time_ms;
} TransformResult;

static const char* trim_left(const char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    return s;
}

static void trim_right(char* s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static bool starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool var_is_declared(char declared[][64], size_t count, const char* var) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(declared[i], var) == 0) {
            return true;
        }
    }
    return false;
}

static bool transform_append(TransformResult* result, const char* text) {
    size_t len = strlen(text);
    
    if (result->output_size + len + 1 > result->output_capacity) {
        size_t new_capacity = result->output_capacity == 0 ? 16384 : result->output_capacity * 2;
        if (new_capacity < result->output_size + len + 1) {
            new_capacity = result->output_size + len + 1;
        }
        
        char* new_output = (char*)realloc(result->output, new_capacity);
        if (!new_output) {
            return false;
        }
        
        result->output = new_output;
        result->output_capacity = new_capacity;
    }
    
    strcpy(result->output + result->output_size, text);
    result->output_size += len;
    
    return true;
}

static TransformResult* transform_source(
    RiftPatternEngine* engine,
    const char* source,
    RiftCliOptions* opts
) {
    TransformResult* result = (TransformResult*)calloc(1, sizeof(TransformResult));
    if (!result) return NULL;
    
    double start_time = rift_get_time_ms();
    
    /* Add header comment */
    char header[512];
    snprintf(header, sizeof(header),
        "/* Generated by RIFTLang v%s - %s mode */\n"
        "/* Policy threshold: %.2f | Optimization: O%d */\n"
        "#include \"riftlang.h\"\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <math.h>\n\n",
        RIFT_VERSION,
        opts->mode == RIFT_MODE_CLASSICAL ? "classical" :
        opts->mode == RIFT_MODE_QUANTUM ? "quantum" : "hybrid",
        opts->policy_threshold,
        opts->optimization_level
    );
    transform_append(result, header);
    
    /* Add policy context initialization */
    transform_append(result, 
        "/* Policy governance context */\n"
        "static RiftPolicyContext* g_policy_ctx = NULL;\n"
        "static RiftResultMatrix2x2* g_policy_matrix = NULL;\n\n"
        "static void rift_init_policy(void) {\n"
        "    g_policy_matrix = rift_result_matrix_create("
    );
    
    char threshold_str[32];
    snprintf(threshold_str, sizeof(threshold_str), "%.2f", opts->policy_threshold);
    transform_append(result, threshold_str);
    transform_append(result, 
        ");\n"
        "    g_policy_ctx = rift_policy_context_create(\"rift_main\", "
    );
    transform_append(result, threshold_str);
    transform_append(result, 
        ", true);\n"
        "    g_policy_ctx->result_matrix = g_policy_matrix;\n"
        "}\n\n"
    );

    /* Emit main() entry point */
    transform_append(result,
        "int main(int argc, char* argv[]) {\n"
        "    (void)argc; (void)argv;\n"
        "    rift_init_policy();\n\n"
    );

    /* Process line by line */
    const char* line_start = source;
    uint32_t line_num = 0;
    bool in_align_span = false;
    bool in_type_block = false;
    bool in_policy_block = false;
    char span_macro[64] = "RIFT_SPAN_FIXED";
    int span_bytes = 4096;
    char current_type[64] = {0};
    char declared_vars[256][64] = {{0}};
    size_t declared_count = 0;
    
    while (*line_start) {
        /* Find end of line */
        const char* line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);
        
        size_t line_len = line_end - line_start;
        
        /* Extract line (preserve original for comments) */
        char line[RIFT_MAX_LINE_LENGTH];
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        strncpy(line, line_start, line_len);
        line[line_len] = '\0';
        trim_right(line);
        const char* trimmed = trim_left(line);
        
        line_num++;
        
        /* Skip empty lines but preserve them */
        bool is_empty = (strspn(trimmed, " \t\r") == strlen(trimmed));
        if (is_empty) {
            transform_append(result, "\n");
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        /* Preserve C-style comments as-is */
        if (starts_with(trimmed, "/*") || starts_with(trimmed, "//")) {
            transform_append(result, "    ");
            transform_append(result, trimmed);
            transform_append(result, "\n");
            result->patterns_matched++;
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        /* Collapse align span block into a complete memory declaration */
        if (!in_align_span && starts_with(trimmed, "align span<")) {
            in_align_span = true;
            span_bytes = 4096;
            if (strstr(trimmed, "<fixed>")) strcpy(span_macro, "RIFT_SPAN_FIXED");
            else if (strstr(trimmed, "<row>")) strcpy(span_macro, "RIFT_SPAN_ROW");
            else if (strstr(trimmed, "<continuous>")) strcpy(span_macro, "RIFT_SPAN_CONTINUOUS");
            else if (strstr(trimmed, "<superposed>")) strcpy(span_macro, "RIFT_SPAN_SUPERPOSED");
            else if (strstr(trimmed, "<entangled>")) strcpy(span_macro, "RIFT_SPAN_ENTANGLED");

            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        if (in_align_span) {
            int parsed = 0;
            if (sscanf(trimmed, "bytes: %d", &parsed) == 1) {
                span_bytes = parsed;
            }
            if (starts_with(trimmed, "}")) {
                char mem_decl[160];
                snprintf(mem_decl, sizeof(mem_decl), "    RIFT_DECLARE_MEMORY(span, %s, %d);\n", span_macro, span_bytes);
                transform_append(result, mem_decl);
                result->patterns_matched++;
                in_align_span = false;
            }

            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        /* Convert type blocks to C structs */
        if (!in_type_block && starts_with(trimmed, "type ") && strchr(trimmed, '=')) {
            if (sscanf(trimmed, "type %63s =", current_type) == 1) {
                char* brace = strchr(current_type, '{');
                if (brace) *brace = '\0';
                transform_append(result, "    typedef struct {\n");
                in_type_block = true;
                result->patterns_matched++;
                line_start = line_end;
                if (*line_start == '\n') line_start++;
                continue;
            }
        }

        if (in_type_block) {
            if (starts_with(trimmed, "}")) {
                char type_close[128];
                snprintf(type_close, sizeof(type_close), "    } %s;\n", current_type);
                transform_append(result, type_close);
                in_type_block = false;
            } else {
                char field[64] = {0};
                char rift_type[64] = {0};
                if (sscanf(trimmed, "%63[^:]: %63[^,]", field, rift_type) == 2) {
                    const char* ctype = strcmp(rift_type, "FLOAT") == 0 ? "double" : "int32_t";
                    char field_decl[160];
                    snprintf(field_decl, sizeof(field_decl), "        %s %s;\n", ctype, field);
                    transform_append(result, field_decl);
                }
            }

            result->patterns_matched++;
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        /* Policy block is metadata; keep as comments */
        if (!in_policy_block && starts_with(trimmed, "policy_fn on ")) {
            in_policy_block = true;
            transform_append(result, "    /* policy_fn block omitted in C output */\n");
            result->patterns_matched++;
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        if (in_policy_block) {
            if (starts_with(trimmed, "}")) {
                in_policy_block = false;
            }
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        /* Handle validation call with initialized matrix variable */
        if (starts_with(trimmed, "validate(")) {
            transform_append(result, "    (void)rift_policy_validate(g_policy_matrix, true, true);\n");
            result->patterns_matched++;
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        /* Preserve control flow / block delimiters */
        if (starts_with(trimmed, "while ") || starts_with(trimmed, "if ") || starts_with(trimmed, "for ") ||
            strcmp(trimmed, "{") == 0 || strcmp(trimmed, "}") == 0 || strcmp(trimmed, "}") == 0 ||
            starts_with(trimmed, "} else")) {
            transform_append(result, "    ");
            transform_append(result, trimmed);
            transform_append(result, "\n");
            result->patterns_matched++;
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }

        /* Convert ':=' assignments to valid C statements with first-use declaration */
        const char* assign = strstr(trimmed, ":=");
        if (assign) {
            char var_name[64] = {0};
            char expr[512] = {0};
            size_t var_len = (size_t)(assign - trimmed);
            if (var_len >= sizeof(var_name)) var_len = sizeof(var_name) - 1;
            strncpy(var_name, trimmed, var_len);
            var_name[var_len] = '\0';
            strncpy(expr, assign + 2, sizeof(expr) - 1);
            trim_right(var_name);
            trim_right(expr);
            const char* expr_trimmed = trim_left(expr);

            char statement[700];
            if (!var_is_declared(declared_vars, declared_count, var_name) && declared_count < 256) {
                strcpy(declared_vars[declared_count++], var_name);
                snprintf(statement, sizeof(statement), "    int %s = %s;\n", var_name, expr_trimmed);
            } else {
                snprintf(statement, sizeof(statement), "    %s = %s;\n", var_name, expr_trimmed);
            }
            transform_append(result, statement);
            result->patterns_matched++;
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            continue;
        }
        
        /* Try pattern matching */
        size_t match_len = 0;
        uint32_t priority = 0;
        char* transformed = rift_pattern_engine_match(engine, line, &match_len, &priority);
        
        if (transformed) {
            /* Pattern matched - use transformed output */
            transform_append(result, "    ");  /* Indent */
            transform_append(result, transformed);
            transform_append(result, "\n");
            
            result->patterns_matched++;
            
            if (opts->verbose) {
                printf("[RIFTLang] Line %d: matched (priority %d) -> %s\n", 
                    line_num, priority, transformed);
            }
            
            free(transformed);
        } else {
            /* No pattern match - preserve as comment if not whitespace */
            if (opts->preserve_comments) {
                transform_append(result, "    // UNMATCHED: ");
                transform_append(result, line);
                transform_append(result, "\n");
            }
            
            result->patterns_failed++;
            
            if (opts->verbose) {
                printf("[RIFTLang] Line %d: unmatched\n", line_num);
            }
        }
        
        /* Move to next line */
        line_start = line_end;
        if (*line_start == '\n') line_start++;
    }

    /* Close main() with cleanup */
    transform_append(result,
        "\n"
        "    /* Policy cleanup */\n"
        "    rift_policy_context_destroy(g_policy_ctx);\n"
        "    rift_result_matrix_destroy(g_policy_matrix);\n"
        "    return 0;\n"
        "}\n"
    );

    result->lines_processed = line_num;
    result->processing_time_ms = rift_get_time_ms() - start_time;

    return result;
}

/* ============================================================================
 * Binding Output Emitter (non-C targets)
 * ============================================================================ */

static bool emit_binding_output(
    const char* source,
    size_t source_size,
    const char* out_filename,
    RiftTargetLanguage target,
    RiftCliOptions* opts
) {
    (void)source_size;

    FILE* out = fopen(out_filename, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot create '%s': %s\n", out_filename, strerror(errno));
        return false;
    }

    const char* mode_str =
        opts->mode == RIFT_MODE_CLASSICAL ? "classical" :
        opts->mode == RIFT_MODE_QUANTUM   ? "quantum"   : "hybrid";

    /* Write language-specific binding header */
    switch (target) {
        case RIFT_TARGET_JS:
            fprintf(out,
                "'use strict';\n"
                "/* Generated by RIFTLang v" RIFT_VERSION " - %s mode */\n"
                "const rift = require('./bindings/node-riftlang/rift_binding.cjs');\n\n",
                mode_str);
            break;
        case RIFT_TARGET_PYTHON:
            fprintf(out,
                "# -*- coding: utf-8 -*-\n"
                "# Generated by RIFTLang v" RIFT_VERSION " - %s mode\n"
                "import sys, os\n"
                "sys.path.insert(0, os.path.join(os.path.dirname("
                "os.path.abspath(__file__)), 'bindings', 'pyriftlang'))\n"
                "from rift_binding import *\n\n",
                mode_str);
            break;
        case RIFT_TARGET_GO:
            fprintf(out,
                "// Generated by RIFTLang v" RIFT_VERSION " - %s mode\n"
                "package main\n\nimport \"fmt\"\n\nfunc main() {\n",
                mode_str);
            break;
        case RIFT_TARGET_LUA:
            fprintf(out,
                "-- Generated by RIFTLang v" RIFT_VERSION " - %s mode\n"
                "local rift = dofile('bindings/lua-riftlang/rift_binding.lua')\n\n",
                mode_str);
            break;
        case RIFT_TARGET_WAT:
            fprintf(out,
                ";; Generated by RIFTLang v" RIFT_VERSION " - %s mode\n"
                "(module\n"
                "  (import \"rift\" \"validate\""
                " (func $rift_validate (param i32) (result i32)))\n"
                "  (memory (export \"memory\") 1)\n"
                "  (func $main (export \"main\")\n",
                mode_str);
            break;
        default:
            break;
    }

    /* Comment prefix for unrecognised lines */
    const char* cpfx =
        (target == RIFT_TARGET_LUA || target == RIFT_TARGET_WAT) ? "--" :
        (target == RIFT_TARGET_PYTHON) ? "#" : "//";

    /* State */
    int skip_depth   = 0;
    int indent_level = 0;
    const char* p = source;

    while (*p) {
        /* Extract one line */
        const char* nl = strchr(p, '\n');
        if (!nl) nl = p + strlen(p);
        size_t len = (size_t)(nl - p);

        char line[RIFT_MAX_LINE_LENGTH];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        strncpy(line, p, len);
        line[len] = '\0';

        const char* trimmed = trim_left(line);
        bool is_empty = (*trimmed == '\0');

        p = nl;
        if (*p == '\n') p++;

        if (is_empty) {
            fprintf(out, "\n");
            continue;
        }

        /* Inside a skipped block – look for closing } */
        if (skip_depth > 0) {
            if (strcmp(trimmed, "}") == 0 || starts_with(trimmed, "}")) {
                skip_depth--;
            }
            continue;
        }

        /* Detect multi-line blocks to skip */
        if (starts_with(trimmed, "align span<")) {
            fprintf(out, "%s rift: memory span\n", cpfx);
            if (strstr(trimmed, "{")) skip_depth = 1;
            continue;
        }
        if (starts_with(trimmed, "type ") && strstr(trimmed, "=")) {
            char type_name[64] = {0};
            const char* tn_start = trimmed + 5;
            size_t tn_len = 0;
            while (tn_start[tn_len] && tn_start[tn_len] != ' ' &&
                   tn_start[tn_len] != '=' && tn_start[tn_len] != '{') {
                tn_len++;
            }
            if (tn_len >= sizeof(type_name)) tn_len = sizeof(type_name) - 1;
            strncpy(type_name, tn_start, tn_len);
            fprintf(out, "%s type: %s\n", cpfx, type_name);
            if (strstr(trimmed, "{")) skip_depth = 1;
            continue;
        }
        if (starts_with(trimmed, "policy_fn ")) {
            fprintf(out, "%s policy omitted\n", cpfx);
            skip_depth = 1;
            continue;
        }

        /* Governance directive */
        if (starts_with(trimmed, "!govern")) {
            fprintf(out, "%s RIFT: %s mode\n", cpfx,
                strstr(trimmed, "classical") ? "classical" :
                strstr(trimmed, "quantum")   ? "quantum"   : "hybrid");
            continue;
        }

        /* C-style comments */
        if (starts_with(trimmed, "/*") || starts_with(trimmed, "//")) {
            const char* ctext = trimmed + 2;
            while (*ctext == ' ') ctext++;
            char ctmp[512];
            strncpy(ctmp, ctext, sizeof(ctmp) - 1);
            ctmp[sizeof(ctmp) - 1] = '\0';
            char* star = strstr(ctmp, "*/");
            if (star) *star = '\0';
            trim_right(ctmp);
            if (*ctmp) fprintf(out, "%s %s\n", cpfx, ctmp);
            continue;
        }

        /* while loop open (e.g. "while (cond) {") */
        if (starts_with(trimmed, "while ") || starts_with(trimmed, "while(")) {
            const char* cs = strchr(trimmed, '(');
            char cond[256] = {0};
            if (cs) {
                cs++;
                const char* ce = strrchr(trimmed, ')');
                if (ce && ce > cs) {
                    size_t cl = (size_t)(ce - cs);
                    if (cl >= sizeof(cond)) cl = sizeof(cond) - 1;
                    strncpy(cond, cs, cl);
                }
            }
            switch (target) {
                case RIFT_TARGET_JS:
                    fprintf(out, "while (%s) {\n", cond);
                    indent_level++;
                    break;
                case RIFT_TARGET_PYTHON:
                    fprintf(out, "while %s:\n", cond);
                    indent_level++;
                    break;
                case RIFT_TARGET_GO:
                    fprintf(out, "\tfor %s {\n", cond);
                    indent_level++;
                    break;
                case RIFT_TARGET_LUA:
                    fprintf(out, "while %s do\n", cond);
                    indent_level++;
                    break;
                case RIFT_TARGET_WAT:
                    fprintf(out, "    (block (loop\n");
                    indent_level++;
                    break;
                default: break;
            }
            continue;
        }

        /* Standalone { */
        if (strcmp(trimmed, "{") == 0) {
            if (target != RIFT_TARGET_PYTHON) indent_level++;
            continue;
        }

        /* Standalone } */
        if (strcmp(trimmed, "}") == 0) {
            if (indent_level > 0) indent_level--;
            switch (target) {
                case RIFT_TARGET_JS:     fprintf(out, "}\n");     break;
                case RIFT_TARGET_GO:     fprintf(out, "\t}\n");   break;
                case RIFT_TARGET_LUA:    fprintf(out, "end\n");   break;
                case RIFT_TARGET_WAT:    fprintf(out, "    ))\n");break;
                case RIFT_TARGET_PYTHON: break;
                default: break;
            }
            continue;
        }

        /* validate() call */
        if (starts_with(trimmed, "validate(")) {
            char arg[64] = {0};
            sscanf(trimmed + 9, "%63[^)]", arg);
            switch (target) {
                case RIFT_TARGET_JS:
                    fprintf(out, "rift.validate('%s');\n", arg);
                    break;
                case RIFT_TARGET_PYTHON:
                    fprintf(out, "rift.validate(%s)\n", arg);
                    break;
                case RIFT_TARGET_GO:
                    fprintf(out, "\t_ = rift.Validate(%s)\n", arg);
                    break;
                case RIFT_TARGET_LUA:
                    fprintf(out, "rift.validate(%s)\n", arg);
                    break;
                case RIFT_TARGET_WAT:
                    fprintf(out, "    (call $rift_validate (local.get $%s))\n", arg);
                    break;
                default: break;
            }
            continue;
        }

        /* := assignment */
        const char* ceq = strstr(trimmed, ":=");
        if (ceq) {
            char var_name[64] = {0};
            char expr[512]    = {0};
            size_t vlen = (size_t)(ceq - trimmed);
            if (vlen >= sizeof(var_name)) vlen = sizeof(var_name) - 1;
            strncpy(var_name, trimmed, vlen);
            trim_right(var_name);
            const char* expr_raw = trim_left(ceq + 2);
            strncpy(expr, expr_raw, sizeof(expr) - 1);
            trim_right(expr);

            /* Indent for nested context */
            const char* ind = (indent_level > 0) ? "    " : "";
            switch (target) {
                case RIFT_TARGET_JS:
                    fprintf(out, "%s%s%s = %s;\n", ind,
                        (indent_level == 0 ? "let " : ""), var_name, expr);
                    break;
                case RIFT_TARGET_PYTHON:
                    fprintf(out, "%s%s = %s\n", ind, var_name, expr);
                    break;
                case RIFT_TARGET_GO:
                    fprintf(out, "\t%s%s %s %s\n", ind,
                        var_name,
                        (indent_level == 0 ? ":=" : "="),
                        expr);
                    break;
                case RIFT_TARGET_LUA:
                    fprintf(out, "%s%s%s = %s\n", ind,
                        (indent_level == 0 ? "local " : ""), var_name, expr);
                    break;
                case RIFT_TARGET_WAT:
                    fprintf(out, "    (local $%s i32)\n"
                                 "    (local.set $%s (i32.const %s))\n",
                        var_name, var_name, expr);
                    break;
                default: break;
            }
            continue;
        }

        /* Fallback: emit as comment */
        fprintf(out, "%s %s\n", cpfx, trimmed);
    }

    /* Closing tokens */
    if (target == RIFT_TARGET_GO) {
        fprintf(out, "\t_ = fmt.Sprintf  // suppress unused import\n}\n");
    } else if (target == RIFT_TARGET_WAT) {
        fprintf(out, "  )\n)\n");
    }

    fclose(out);

    if (!opts->quiet) {
        printf("[RIFTLang] Output written to: %s\n", out_filename);
    }

    const char* run_hint =
        (target == RIFT_TARGET_JS)     ? "node" :
        (target == RIFT_TARGET_PYTHON) ? "python3" :
        (target == RIFT_TARGET_GO)     ? "go run" :
        (target == RIFT_TARGET_LUA)    ? "lua" :
        (target == RIFT_TARGET_WAT)    ? "wat2wasm" : "";

    if (!opts->quiet && *run_hint) {
        printf("[RIFTLang] Run with: %s %s\n", run_hint, out_filename);
    }

    return true;
}

/* ============================================================================
 * Compilation Pipeline
 * ============================================================================ */

static bool compile_rift_file(RiftCliOptions* opts) {
    if (!opts->quiet) {
        print_banner();
        printf("[RIFTLang] Input:  %s\n", opts->input_file);
        printf("[RIFTLang] Mode:   %s\n", 
            opts->mode == RIFT_MODE_CLASSICAL ? "classical" :
            opts->mode == RIFT_MODE_QUANTUM ? "quantum" : "hybrid");
        printf("[RIFTLang] Policy: %.0f%% validation threshold\n", opts->policy_threshold * 100);
        printf("[RIFTLang] Opt:    O%d\n", opts->optimization_level);
        printf("\n");
    }
    
    /* Check input file exists */
    if (!file_exists(opts->input_file)) {
        fprintf(stderr, "Error: Input file not found: %s\n", opts->input_file);
        return false;
    }
    
    /* Read input file */
    size_t source_size = 0;
    char* source = read_file(opts->input_file, &source_size);
    if (!source) {
        return false;
    }

    if (opts->verbose) {
        printf("[RIFTLang] Read %zu bytes from %s\n", source_size, opts->input_file);
    }

    /* Determine output filename early (needed for target detection) */
    char out_filename[256];
    out_filename[sizeof(out_filename) - 1] = '\0';
    if (opts->output_file) {
        strncpy(out_filename, opts->output_file, sizeof(out_filename) - 1);
    } else {
        strncpy(out_filename, opts->input_file, sizeof(out_filename) - 1);
        char* dot = strrchr(out_filename, '.');
        if (dot) {
            strcpy(dot, ".c");
        } else {
            strncat(out_filename, ".c", sizeof(out_filename) - strlen(out_filename) - 1);
        }
    }

    /* Detect target language from output extension */
    RiftTargetLanguage target = rift_detect_target(out_filename);

    if (target != RIFT_TARGET_C) {
        /* Non-C binding path: link → CIR → codec emit (linkable-then-fileformat) */
        if (opts->verbose) {
            const char* tname =
                target == RIFT_TARGET_JS     ? "JavaScript" :
                target == RIFT_TARGET_PYTHON ? "Python"     :
                target == RIFT_TARGET_GO     ? "Go"         :
                target == RIFT_TARGET_LUA    ? "Lua"        : "WAT";
            printf("[RIFTLang] Target language: %s (link+codec path)\n", tname);
        }
        RiftCIRProgram* prog = rift_link(source, opts->mode);
        free(source);
        if (!prog) {
            fprintf(stderr, "Error: CIR linker allocation failed\n");
            return false;
        }
        if (!prog->consensus_ok) {
            fprintf(stderr, "Error: Consensus validation failed: %s\n", prog->error_msg);
            rift_cir_program_free(prog);
            return false;
        }
        FILE* out_fp = fopen(out_filename, "w");
        if (!out_fp) {
            fprintf(stderr, "Error: Cannot create '%s': %s\n", out_filename, strerror(errno));
            rift_cir_program_free(prog);
            return false;
        }
        bool ok = rift_codec_emit(prog, out_fp, target);
        fclose(out_fp);
        rift_cir_program_free(prog);
        if (!opts->quiet && ok) {
            const char* run_hint =
                (target == RIFT_TARGET_JS)     ? "node" :
                (target == RIFT_TARGET_PYTHON) ? "python3" :
                (target == RIFT_TARGET_GO)     ? "go run" :
                (target == RIFT_TARGET_LUA)    ? "lua" : "wat2wasm";
            printf("[RIFTLang] Output written to: %s\n", out_filename);
            printf("[RIFTLang] Run with: %s %s\n", run_hint, out_filename);
        }
        return ok;
    }

    /* C target: existing pipeline */
    /* Initialize pattern engine */
    RiftPatternEngine* engine = initialize_transform_engine(opts->mode, opts->verbose);
    if (!engine) {
        free(source);
        return false;
    }

    /* Transform source to C */
    TransformResult* result = transform_source(engine, source, opts);

    rift_pattern_engine_destroy(engine);
    free(source);

    if (!result || !result->output) {
        fprintf(stderr, "Error: Transformation failed\n");
        return false;
    }

    if (opts->verbose) {
        printf("\n[RIFTLang] Transformation complete:\n");
        printf("  Lines processed: %d\n", result->lines_processed);
        printf("  Patterns matched: %d\n", result->patterns_matched);
        printf("  Patterns failed: %d\n", result->patterns_failed);
        printf("  Time: %.2f ms\n", result->processing_time_ms);
        printf("  Output size: %zu bytes\n", result->output_size);
    }
    
    /* Write output if not dry run */
    if (!opts->dry_run) {
        if (!write_file(out_filename, result->output, result->output_size)) {
            free(result->output);
            free(result);
            return false;
        }
        
        if (!opts->quiet) {
            printf("[RIFTLang] Output written to: %s\n", out_filename);
        }
    } else {
        if (!opts->quiet) {
            printf("[RIFTLang] Dry run - no output written\n");
        }
    }
    
    /* Emit AST files if requested */
    if (opts->emit_ast_json) {
        char ast_filename[256];
        snprintf(ast_filename, sizeof(ast_filename), "%s.ast.json", opts->input_file);
        
        /* Create stub AST for now */
        const char* ast_stub = "{\"ast\":\"stub\",\"version\":1,\"source\":\"rift\"}";
        write_file(ast_filename, ast_stub, strlen(ast_stub));
        
        if (!opts->quiet) {
            printf("[RIFTLang] AST JSON written to: %s\n", ast_filename);
        }
    }
    
    if (opts->emit_ast_binary) {
        char astb_filename[256];
        snprintf(astb_filename, sizeof(astb_filename), "%s.astb", opts->input_file);
        
        /* Create stub binary AST for now */
        uint8_t astb_stub[8] = {0x52, 0x49, 0x46, 0x54, 0x01, 0x00, 0x00, 0x00};
        write_file(astb_filename, (char*)astb_stub, 8);
        
        if (!opts->quiet) {
            printf("[RIFTLang] AST binary written to: %s\n", astb_filename);
        }
    }
    
    /* Show AST if requested */
    if (opts->show_ast) {
        printf("\n[RIFTLang] AST Representation:\n");
        printf("--------------------------------\n");
        printf("Stub AST - full implementation in Phase 2\n");
        printf("--------------------------------\n");
    }
    
    /* Show tokens if requested */
    if (opts->show_tokens) {
        printf("\n[RIFTLang] Token Stream:\n");
        printf("--------------------------------\n");
        printf("Stub tokens - full lexer in Phase 2\n");
        printf("--------------------------------\n");
    }
    
    /* Invoke C compiler if not compile-only */
    if (!opts->compile_only && !opts->dry_run) {
        if (!opts->quiet) {
            printf("\n[RIFTLang] Invoking C compiler...\n");
        }
        
        char compile_cmd[512];
        const char* cc = getenv("CC") ? getenv("CC") : "gcc";

        /* Derive exe name from input file base (strip directory and extension) */
        char exe_name[256];
        const char* base = strrchr(opts->input_file, '/');
        base = base ? base + 1 : opts->input_file;
        strncpy(exe_name, base, sizeof(exe_name) - 1);
        exe_name[sizeof(exe_name) - 1] = '\0';
        char* exe_dot = strrchr(exe_name, '.');
        if (exe_dot) *exe_dot = '\0';

        snprintf(compile_cmd, sizeof(compile_cmd),
            "%s -o %s %s -I. -L./bin -lriftlang -O%d -lm -lpthread %s",
            cc,
            exe_name,
            out_filename,
            opts->optimization_level,
            opts->verbose ? "-v" : ""
        );

        if (opts->verbose) {
            printf("[RIFTLang] Compile command: %s\n", compile_cmd);
        }

        int ret = system(compile_cmd);
        if (ret != 0) {
            fprintf(stderr, "Warning: C compiler returned non-zero exit code\n");
        } else if (!opts->quiet) {
            printf("[RIFTLang] Compilation successful -> %s\n", exe_name);
        }
    }
    
    free(result->output);
    free(result);
    
    return true;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

#ifdef RIFTLANG_OPEN_MAIN
int main(int argc, char* argv[]) {
    RiftCliOptions opts = {0};
    opts.quiet = false;  /* Will be set by -q flag */
    
    if (!parse_args(argc, argv, &opts)) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (!compile_rift_file(&opts)) {
        return 1;
    }
    
    if (!opts.quiet) {
        printf("\n[RIFTLang] Done.\n");
    }
    
    return 0;
}
#endif /* RIFTLANG_OPEN_MAIN */
