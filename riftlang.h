/**
 * @file riftlang.h
 * @brief RIFTLang Core Header - Polar Bipartite Pattern Matching System
 * @author Nnamdi Michael Okpala
 * @version 1.0.0
 * 
 * This header defines the token architecture, memory governance, and
 * polar bipartite pattern matching engine for the RIFT ecosystem.
 * 
 * Constitutional Computing Framework - OBINexus
 */

#ifndef RIFTLANG_H
#define RIFTLANG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
    #include <windows.h>
    #include <stdlib.h>   /* malloc, free */
    #include <string.h>   /* strstr, strlen, memcpy */
    /* Provide strdup inline — avoids relying on _strdup declaration in MinGW CRT */
    #ifndef strdup
    static inline char* strdup(const char* s) {
        if (!s) return NULL;
        size_t n = strlen(s) + 1;
        char* d = (char*)malloc(n);
        if (d) memcpy(d, s, n);
        return d;
    }
    #endif
    // MinGW doesn't have regex.h or pthread.h by default
    #ifdef __MINGW32__
        /* ---- Regex stub (simple substring matching) ---- */
        #define RIFT_USE_SIMPLE_MATCH 1
        #define REG_EXTENDED  0
        #define REG_NEWLINE   0
        #define REG_NOMATCH   1
        typedef struct { char* pattern; int flags; } regex_t;
        typedef struct { int rm_so; int rm_eo; }     regmatch_t;
        static inline int regcomp(regex_t* preg, const char* pattern, int cflags) {
            (void)cflags;
            preg->pattern = strdup(pattern);
            preg->flags   = cflags;
            return preg->pattern ? 0 : REG_NOMATCH;
        }
        static inline int regexec(const regex_t* preg, const char* string,
                                  size_t nmatch, regmatch_t* pmatch, int eflags) {
            (void)eflags;
            const char* found = strstr(string, preg->pattern);
            if (!found) return REG_NOMATCH;
            if (pmatch && nmatch > 0) {
                pmatch[0].rm_so = (int)(found - string);
                pmatch[0].rm_eo = pmatch[0].rm_so + (int)strlen(preg->pattern);
            }
            return 0;
        }
        static inline size_t regerror(int errcode, const regex_t* preg,
                                      char* errbuf, size_t errbuf_size) {
            (void)errcode; (void)preg;
            if (errbuf && errbuf_size > 0) errbuf[0] = '\0';
            return 0;
        }
        static inline void regfree(regex_t* preg) {
            free(preg->pattern);
            preg->pattern = NULL;
        }
    #else
        /* MSVC or other Windows compiler — assume system regex.h present */
        #include <regex.h>
    #endif

    /* ---- Thread stubs: map POSIX names to Windows CRITICAL_SECTION ---- */
    typedef CRITICAL_SECTION pthread_mutex_t;
    typedef HANDLE           pthread_t;
    static inline int  pthread_mutex_init(pthread_mutex_t* m, void* attr) {
        (void)attr; InitializeCriticalSection(m); return 0;
    }
    static inline int  pthread_mutex_destroy(pthread_mutex_t* m) {
        DeleteCriticalSection(m); return 0;
    }
    static inline int  pthread_mutex_lock(pthread_mutex_t* m) {
        EnterCriticalSection(m); return 0;
    }
    static inline int  pthread_mutex_unlock(pthread_mutex_t* m) {
        LeaveCriticalSection(m); return 0;
    }
    static inline pthread_t pthread_self(void) {
        return GetCurrentThread();
    }
#else
    #include <regex.h>
    #include <pthread.h>
#endif
        
/* ============================================================================
 * Platform Abstraction & Export Macros
 * ============================================================================ */

#ifndef RIFT_API
#  ifdef _WIN32
#    define RIFT_API __declspec(dllexport)
#  else
#    define RIFT_API __attribute__((visibility("default")))
#  endif
#endif

#ifndef RIFT_CALL
#  ifdef _WIN32
#    define RIFT_CALL __cdecl
#  else
#    define RIFT_CALL
#  endif
#endif

#ifdef _WIN32
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

/* ============================================================================
 * Memory Governance Constants
 * ============================================================================ */

#define RIFT_CLASSICAL_ALIGNMENT    4096    /* 4KB classical memory alignment */
#define RIFT_QUANTUM_ALIGNMENT      8       /* 8-qubit quantum alignment */
#define RIFT_TOKEN_BITFIELD_SIZE    32      /* Validation bits per token */
#define RIFT_DEFAULT_THRESHOLD      0.85    /* 85% policy validation threshold */
#define RIFT_DEFAULT_ENTROPY        0.25    /* Quantum entropy threshold */

/* Validation bit flags for token->validation_bits */
#define RIFT_TOKEN_ALLOCATED        0x01    /* Memory allocated */
#define RIFT_TOKEN_INITIALIZED      0x02    /* Value initialized */
#define RIFT_TOKEN_LOCKED           0x04    /* Thread lock active */
#define RIFT_TOKEN_GOVERNED         0x08    /* Policy validated */
#define RIFT_TOKEN_SUPERPOSED       0x10    /* Quantum superposition */
#define RIFT_TOKEN_ENTANGLED        0x20    /* Quantum entanglement */
#define RIFT_TOKEN_PERSISTENT       0x40    /* Persistent state storage */
#define RIFT_TOKEN_SHADOW           0x80    /* Shadow type/copy */

/* ============================================================================
 * Token Architecture - The Triplet Model
 * ============================================================================ */

/**
 * Token Type Enumeration
 * Classical types for deterministic execution
 * Quantum types for probabilistic execution
 */
typedef enum {
    /* Classical Token Types (Chomsky Type-2/3) */
    RIFT_TOKEN_INT = 0,         /* Integer value */
    RIFT_TOKEN_FLOAT,           /* Floating-point value */
    RIFT_TOKEN_STRING,          /* String value */
    RIFT_TOKEN_ROLE,            /* Governance role identifier */
    RIFT_TOKEN_MASK,            /* CRUD operation mask */
    RIFT_TOKEN_OP,              /* Operator token */
    RIFT_TOKEN_ARRAY,           /* Fixed-size array */
    RIFT_TOKEN_VECTOR,          /* Expandable vector */
    RIFT_TOKEN_MAP,             /* Key-value mapping */
    RIFT_TOKEN_TUPLE,           /* Ordered, non-indexed structure */
    RIFT_TOKEN_DSA,             /* Distributed Structural Array */
    
    /* Quantum Token Types (Probabilistic) */
    RIFT_TOKEN_QBYTE,           /* Quantum byte (8-qubit) */
    RIFT_TOKEN_QROLE,           /* Quantum role */
    RIFT_TOKEN_QMATRIX,         /* Quantum matrix */
    RIFT_TOKEN_QINT,            /* Quantum integer (superposed) */
    RIFT_TOKEN_QFLOAT,          /* Quantum float (superposed) */
    
    RIFT_TOKEN_COUNT            /* Total type count */
} RiftTokenType;

/**
 * Execution Mode Enumeration
 * Defines the operational context for token processing
 */
typedef enum {
    RIFT_MODE_CLASSICAL = 0,    /* Sequential, deterministic */
    RIFT_MODE_QUANTUM,          /* Parallel, probabilistic */
    RIFT_MODE_HYBRID            /* Context-aware switching */
} RiftExecutionMode;

/**
 * Target Language Enumeration
 * Defines the output language for polyglot codec emission.
 * Detected from output file extension by rift_detect_target().
 */
typedef enum {
    RIFT_TARGET_C = 0,   /* Default: emit C and invoke gcc */
    RIFT_TARGET_JS,      /* JavaScript via node-riftlang   */
    RIFT_TARGET_GO,      /* Go via go-riftlang             */
    RIFT_TARGET_LUA,     /* Lua via lua-riftlang           */
    RIFT_TARGET_PYTHON,  /* Python via pyriftlang          */
    RIFT_TARGET_WAT,     /* WebAssembly text via wat2wasm  */
} RiftTargetLanguage;

/**
 * Memory Span Type Enumeration
 * Defines memory allocation patterns
 */
typedef enum {
    RIFT_SPAN_FIXED = 0,        /* Fixed-size allocation */
    RIFT_SPAN_ROW,              /* Row-ordered, expandable */
    RIFT_SPAN_CONTINUOUS,       /* Continuous memory region */
    RIFT_SPAN_SUPERPOSED,       /* Quantum superposition span */
    RIFT_SPAN_ENTANGLED,        /* Quantum entanglement span */
    RIFT_SPAN_DISTRIBUTED       /* DSA distributed span */
} RiftSpanType;

/**
 * Memory Span Descriptor
 * 
 * CRITICAL: Memory is declared BEFORE type or value in RIFTLang.
 * This structure defines the governance contract for memory allocation.
 */
typedef struct {
    RiftSpanType type;          /* Span classification */
    uint64_t bytes;             /* Size in bytes */
    uint32_t alignment;         /* Alignment boundary (power of 2) */
    bool open;                  /* Mutable/appendable flag */
    bool direction;             /* true = right->left, false = left->right */
    uint32_t access_mask;       /* CRUD permissions: CREATE|READ|UPDATE|DELETE */
} RiftMemorySpan;

/**
 * Token Value Union
 * Polymorphic container for token data
 */
typedef union {
    int64_t i_val;              /* Integer value */
    double f_val;               /* Float value */
    char* s_val;                /* String pointer (owned) */
    void* p_val;                /* Generic pointer */
    struct RiftToken** t_arr;   /* Token array for composite types */
    uint8_t* q_val;             /* Quantum byte array (8-qubit aligned) */
} RiftTokenValue;

/**
 * Lock Context Structure
 * Thread safety context for token operations
 */
typedef struct {
    pthread_mutex_t mutex;      /* POSIX mutex for locking */
    pthread_t owner;            /* Current lock owner thread */
    uint32_t lock_count;        /* Recursive lock count */
    bool initialized;           /* Initialization flag */
    uint64_t contention_count;  /* Lock contention metrics */
} RiftLockContext;

/**
 * The Token Triplet: (type, value, memory) with governance
 * 
 * This is the fundamental unit of the RIFT ecosystem. Every token
 * carries its governance state in validation_bits.
 */
typedef struct RiftToken {
    /* Core triplet */
    RiftTokenType type;         /* Semantic classification */
    RiftTokenValue value;       /* Data payload */
    RiftMemorySpan* memory;     /* Memory governance (declared first) */
    
    /* Governance fields */
    uint32_t validation_bits;   /* Bitfield state (ALLOCATED, etc.) */
    RiftLockContext* lock_ctx;  /* Thread safety context */
    
    /* Quantum fields (valid when RIFT_TOKEN_SUPERPOSED set) */
    struct RiftToken** superposed_states;   /* Array of possible states */
    uint32_t superposition_count;           /* Number of superposed states */
    double* amplitudes;                     /* Probability amplitudes */
    double phase;                           /* Quantum phase angle */
    
    /* Entanglement links (valid when RIFT_TOKEN_ENTANGLED set) */
    struct RiftToken** entangled_with;        /* Array of entangled tokens */
    uint32_t entanglement_count;            /* Number of entanglements */
    uint32_t entanglement_id;               /* Unique entanglement group ID */
    
    /* Source location for error reporting */
    uint32_t source_line;
    uint32_t source_column;
    char* source_file;
} RiftToken;

/* ============================================================================
 * Polar Bipartite Pattern Matching Engine
 * ============================================================================ */

/**
 * Pattern Polarity Enumeration
 * Defines the role of a pattern in bipartite matching
 */
typedef enum {
    RIFT_PATTERN_LEFT = 0,      /* Left polarity: input/matcher */
    RIFT_PATTERN_RIGHT          /* Right polarity: output/generator */
} RiftPatternPolarity;

/**
 * Pattern Structure
 * Compiled regex pattern with metadata
 */
typedef struct {
    char* pattern_str;          /* Original pattern string (owned) */
    regex_t compiled_regex;     /* Compiled POSIX regex state */
    RiftPatternPolarity polarity;   /* LEFT or RIGHT */
    uint32_t priority;          /* Match priority (lower = higher) */
    bool anchored;              /* ^...$ anchored matching */
    bool is_literal;            /* True if pattern is literal string, not regex */
} RiftPattern;

/**
 * Bipartite Pattern Pair
 * Links input pattern (left) to output pattern (right)
 */
typedef struct {
    RiftPattern* left;          /* Input matching pattern */
    RiftPattern* right;         /* Output generation pattern */
    void* transform_fn;         /* Optional transformation callback */
    bool is_governed;           /* Has policy validation */
    uint32_t transform_id;      /* Unique transformation identifier */
} RiftBipartitePair;

/**
 * Pattern Matching Engine Context
 * Manages all pattern pairs and compilation cache
 */
typedef struct {
    RiftBipartitePair** pairs;      /* Array of pattern pairs */
    uint32_t pair_count;            /* Active pair count */
    uint32_t capacity;              /* Allocated capacity */
    
    /* Regex compilation cache for performance */
    regex_t** compiled_cache;       /* Cached compiled regexes */
    char** cache_patterns;          /* Original patterns for cache */
    uint32_t cache_size;            /* Cache entry count */
    
    /* Execution context */
    RiftExecutionMode mode;           /* Current execution mode */
    
    /* Thread safety */
    RiftLockContext* lock_ctx;      /* Engine-level locking */
    
    /* Metrics */
    uint64_t total_matches;
    uint64_t total_failures;
    double average_match_time_ms;
} RiftPatternEngine;

/* ============================================================================
 * Policy & Governance
 * ============================================================================ */

/**
 * Policy Result Enumeration
 * Outcomes of policy validation
 */
typedef enum {
    RIFT_POLICY_ALLOW = 0,      /* Validation passed */
    RIFT_POLICY_DENY,           /* Validation failed */
    RIFT_POLICY_DEFER           /* Quantum deferred validation */
} RiftPolicyResult;

/**
 * ResultMatrix2x2 Structure
 * 
 * Implements the 85% validation threshold policy framework.
 * Matrix: [input_valid][output_valid] = policy_result
 * 
 * Default matrix:
 *   [0][0] = DENY  (invalid input, invalid output)
 *   [0][1] = DENY  (invalid input, valid output)
 *   [1][0] = DENY  (valid input, invalid output)
 *   [1][1] = ALLOW (valid input, valid output)
 */
typedef struct {
    /* Policy decision matrix */
    RiftPolicyResult matrix[2][2];
    
    /* Thresholds */
    double validation_threshold;    /* Default: 0.85 (85%) */
    double entropy_threshold;       /* Quantum: default 0.25 */
    
    /* Metrics for policy validation ratio tracking */
    uint64_t validations_passed;
    uint64_t validations_failed;
    uint64_t validations_deferred;
    uint64_t total_validations;
    
    /* Performance metrics */
    double average_validation_time_ms;
    uint64_t policy_violations;
} RiftResultMatrix2x2;

/**
 * Policy Enforcement Context
 * Active policy configuration for validation
 */
typedef struct {
    RiftResultMatrix2x2* result_matrix; /* Validation matrix */
    bool enforce_immediate;             /* Classical: true, Quantum: false */
    void* policy_data;                  /* Policy-specific extension data */
    char* policy_name;                  /* Named policy identifier */
    uint32_t policy_version;            /* Policy version for compatibility */
} RiftPolicyContext;

/* ============================================================================
 * Parser Boundary Interface
 * ============================================================================ */

/**
 * AST Node Type Enumeration
 * Classification of AST node types
 */
typedef enum {
    RIFT_AST_INT = 0,           /* Integer literal */
    RIFT_AST_FLOAT,             /* Float literal */
    RIFT_AST_STRING,            /* String literal */
    RIFT_AST_IDENTIFIER,        /* Variable/function name */
    RIFT_AST_BINARY_OP,         /* Binary operation */
    RIFT_AST_UNARY_OP,          /* Unary operation */
    RIFT_AST_ASSIGNMENT,        /* Assignment statement */
    RIFT_AST_DECLARATION,       /* Variable declaration */
    RIFT_AST_MEMORY_DECL,       /* Memory span declaration */
    RIFT_AST_TYPE_DEF,          /* Type definition */
    RIFT_AST_GOVERNANCE,        /* Governance directive */
    RIFT_AST_POLICY,            /* Policy definition */
    RIFT_AST_BLOCK,             /* Code block */
    RIFT_AST_PROGRAM            /* Root program node */
} RiftAstNodeType;

/**
 * AST Node Structure
 * Tree node for abstract syntax representation
 */
typedef struct RiftAstNode {
    RiftAstNodeType type;           /* Node classification */
    RiftToken* token;               /* Associated token (if any) */
    
    /* Tree structure */
    struct RiftAstNode** children;  /* Child nodes array */
    uint32_t child_count;           /* Number of children */
    struct RiftAstNode* parent;     /* Parent node (NULL for root) */
    
    /* Source location */
    uint32_t line;
    uint32_t column;
    char* source_file;
    
    /* Governance */
    bool validated;                 /* AST validation status */
    RiftPolicyContext* policy_ctx;  /* Applied policy context */
    
    /* Serialization metadata */
    uint32_t node_id;               /* Unique node identifier */
    uint32_t schema_version;        /* Format version */
} RiftAstNode;

/**
 * Parser Boundary Interface
 * 
 * Defines the contract between parser and validator.
 * All functions are optional (can be NULL for default behavior).
 */
typedef struct {
    /* Input validation */
    bool (*validate_input)(const char* input, size_t len, RiftPolicyContext* policy);
    
    /* Token stream generation */
    RiftToken** (*generate_tokens)(const char* input, uint32_t* count, 
                                  RiftExecutionMode mode);
    
    /* AST validation with policy enforcement */
    bool (*validate_ast)(RiftAstNode* root, RiftPolicyContext* policy);
    
    /* Error handling callbacks */
    void (*on_parse_error)(const char* msg, uint32_t line, uint32_t col, 
                           const char* file);
    void (*on_validation_error)(RiftPolicyResult result, const char* policy_name);
    
    /* Policy integration */
    RiftPolicyContext* policy_ctx;
    
    /* Metrics */
    uint64_t tokens_generated;
    uint64_t parse_errors;
    double parse_time_ms;
} RiftParserBoundary;

/* ============================================================================
 * Serialization Formats
 * ============================================================================ */

/**
 * Serialization Format Enumeration
 */
typedef enum {
    RIFT_SERIAL_JSON = 0,       /* .rift.ast.json - Human readable */
    RIFT_SERIAL_BINARY,         /* .rift.astb - Compact binary */
    RIFT_SERIAL_XML,            /* .rift.ast.xml - Alternative markup */
    RIFT_SERIAL_PROTOBUF        /* Protocol Buffers (future) */
} RiftSerialFormat;

/**
 * Serialization Options
 */
typedef struct {
    RiftSerialFormat format;
    bool include_source_locs;   /* Include line/column info */
    bool include_types;         /* Include full type info */
    bool compact;               /* Minimize output size */
    uint32_t version;           /* Schema version */
} RiftSerialOptions;

/* ============================================================================
 * API Function Declarations
 * ============================================================================ */

/* ---------------------------------------------------------------------------
 * Token Lifecycle Management
 * --------------------------------------------------------------------------- */

RIFT_API RiftToken* RIFT_CALL rift_token_create(
    RiftTokenType type, 
    RiftMemorySpan* span
);

RIFT_API void RIFT_CALL rift_token_destroy(
    RiftToken* token
);

RIFT_API bool RIFT_CALL rift_token_validate(
    RiftToken* token
);

RIFT_API bool RIFT_CALL rift_token_lock(
    RiftToken* token
);

RIFT_API bool RIFT_CALL rift_token_unlock(
    RiftToken* token
);

RIFT_API bool RIFT_CALL rift_token_is_valid(
    const RiftToken* token
);

RIFT_API const char* RIFT_CALL rift_token_type_name(
    RiftTokenType type
);

/* ---------------------------------------------------------------------------
 * Memory Governance
 * --------------------------------------------------------------------------- */

RIFT_API RiftMemorySpan* RIFT_CALL rift_span_create(
    RiftSpanType type, 
    uint64_t bytes
);

RIFT_API void RIFT_CALL rift_span_destroy(
    RiftMemorySpan* span
);

RIFT_API bool RIFT_CALL rift_span_align(
    RiftMemorySpan* span, 
    uint32_t alignment
);

RIFT_API bool RIFT_CALL rift_span_validate_alignment(
    uint32_t alignment
);

RIFT_API uint32_t RIFT_CALL rift_span_get_default_alignment(
    RiftSpanType type, 
    RiftExecutionMode mode
);

/* ---------------------------------------------------------------------------
 * Polar Bipartite Pattern Matching Engine
 * --------------------------------------------------------------------------- */

RIFT_API RiftPatternEngine* RIFT_CALL rift_pattern_engine_create(
    RiftExecutionMode mode
);

RIFT_API void RIFT_CALL rift_pattern_engine_destroy(
    RiftPatternEngine* engine
);

RIFT_API bool RIFT_CALL rift_pattern_engine_add_pair(
    RiftPatternEngine* engine, 
    const char* left_pattern,       /* POSIX regex for input matching */
    const char* right_pattern,      /* Output template or regex */
    uint32_t priority,              /* Lower = higher priority */
    bool right_is_literal           /* true if right is literal string */
);

RIFT_API bool RIFT_CALL rift_pattern_engine_compile(
    RiftPatternEngine* engine
);

RIFT_API char* RIFT_CALL rift_pattern_engine_match(
    RiftPatternEngine* engine, 
    const char* input,
    size_t* output_len,
    uint32_t* matched_priority      /* OUT: priority of matched pattern */
);

RIFT_API bool RIFT_CALL rift_pattern_engine_match_all(
    RiftPatternEngine* engine,
    const char* input,
    RiftBipartitePair** matches,    /* OUT: array of all matching pairs */
    uint32_t* match_count           /* OUT: number of matches */
);

RIFT_API void RIFT_CALL rift_pattern_engine_get_metrics(
    RiftPatternEngine* engine,
    uint64_t* total_matches,
    uint64_t* total_failures,
    double* avg_time_ms
);

/* ---------------------------------------------------------------------------
 * Policy & Governance
 * --------------------------------------------------------------------------- */

RIFT_API RiftResultMatrix2x2* RIFT_CALL rift_result_matrix_create(
    double threshold
);

RIFT_API void RIFT_CALL rift_result_matrix_destroy(
    RiftResultMatrix2x2* matrix
);

RIFT_API RiftPolicyResult RIFT_CALL rift_policy_validate(
    RiftResultMatrix2x2* matrix,
    bool input_valid,
    bool output_valid
);

RIFT_API double RIFT_CALL rift_policy_get_validation_ratio(
    RiftResultMatrix2x2* matrix
);

RIFT_API bool RIFT_CALL rift_policy_meets_threshold(
    RiftResultMatrix2x2* matrix
);

RIFT_API RiftPolicyContext* RIFT_CALL rift_policy_context_create(
    const char* policy_name,
    double threshold,
    bool immediate
);

RIFT_API void RIFT_CALL rift_policy_context_destroy(
    RiftPolicyContext* context
);

RIFT_API bool RIFT_CALL rift_policy_context_set_threshold(
    RiftPolicyContext* context,
    double new_threshold
);

/* ---------------------------------------------------------------------------
 * Parser Boundary
 * --------------------------------------------------------------------------- */

RIFT_API RiftParserBoundary* RIFT_CALL rift_parser_boundary_create(
    void
);

RIFT_API void RIFT_CALL rift_parser_boundary_destroy(
    RiftParserBoundary* boundary
);

RIFT_API bool RIFT_CALL rift_parser_boundary_set_policy(
    RiftParserBoundary* boundary,
    RiftPolicyContext* policy
);

RIFT_API bool RIFT_CALL rift_parser_boundary_set_callbacks(
    RiftParserBoundary* boundary,
    bool (*validate_input)(const char*, size_t, RiftPolicyContext*),
    RiftToken** (*generate_tokens)(const char*, uint32_t*, RiftExecutionMode),
    bool (*validate_ast)(RiftAstNode*, RiftPolicyContext*),
    void (*on_error)(const char*, uint32_t, uint32_t, const char*)
);

RIFT_API RiftAstNode* RIFT_CALL rift_parser_parse(
    RiftParserBoundary* boundary,
    const char* input,
    const char* filename
);

/* ---------------------------------------------------------------------------
 * AST Operations
 * --------------------------------------------------------------------------- */

RIFT_API RiftAstNode* RIFT_CALL rift_ast_create_node(
    RiftAstNodeType type, 
    RiftToken* token
);

RIFT_API void RIFT_CALL rift_ast_destroy_node(
    RiftAstNode* node
);

RIFT_API bool RIFT_CALL rift_ast_add_child(
    RiftAstNode* parent, 
    RiftAstNode* child
);

RIFT_API bool RIFT_CALL rift_ast_remove_child(
    RiftAstNode* parent,
    uint32_t index
);

RIFT_API bool RIFT_CALL rift_ast_validate(
    RiftAstNode* root, 
    RiftPolicyContext* policy
);

RIFT_API RiftAstNode* RIFT_CALL rift_ast_find_node(
    RiftAstNode* root,
    RiftAstNodeType type,
    bool recursive
);

RIFT_API uint32_t RIFT_CALL rift_ast_count_nodes(
    RiftAstNode* root
);

RIFT_API void RIFT_CALL rift_ast_print(
    RiftAstNode* root,
    int indent
);

/* ---------------------------------------------------------------------------
 * Serialization (.rift.ast.json, .rift.astb)
 * --------------------------------------------------------------------------- */

RIFT_API char* RIFT_CALL rift_ast_serialize(
    RiftAstNode* root, 
    RiftSerialOptions* options,
    size_t* output_size
);

RIFT_API RiftAstNode* RIFT_CALL rift_ast_deserialize(
    const char* data, 
    size_t data_len,
    RiftSerialOptions* options
);

RIFT_API bool RIFT_CALL rift_ast_save_to_file(
    RiftAstNode* root,
    const char* filename,
    RiftSerialOptions* options
);

RIFT_API RiftAstNode* RIFT_CALL rift_ast_load_from_file(
    const char* filename,
    RiftSerialOptions* options
);

/* ---------------------------------------------------------------------------
 * Quantum Operations
 * --------------------------------------------------------------------------- */

RIFT_API bool RIFT_CALL rift_token_superpose(
    RiftToken* token, 
    RiftToken** states, 
    uint32_t count,
    const double* amplitudes      /* Optional: probability amplitudes */
);

RIFT_API bool RIFT_CALL rift_token_entangle(
    RiftToken* a, 
    RiftToken* b,
    uint32_t entanglement_id     /* Shared ID for entanglement group */
);

RIFT_API bool RIFT_CALL rift_token_disentangle(
    RiftToken* token,
    uint32_t entanglement_id
);

RIFT_API bool RIFT_CALL rift_token_collapse(
    RiftToken* token, 
    uint32_t selected_index,
    RiftPolicyContext* policy     /* For deferred validation */
);

RIFT_API bool RIFT_CALL rift_token_collapse_by_probability(
    RiftToken* token,
    double probability_threshold,
    RiftPolicyContext* policy
);

RIFT_API double RIFT_CALL rift_token_calculate_entropy(
    RiftToken* token
);

RIFT_API double RIFT_CALL rift_token_get_probability(
    RiftToken* token,
    uint32_t state_index
);

RIFT_API bool RIFT_CALL rift_token_measure(
    RiftToken* token,
    uint32_t* collapsed_index,      /* OUT: selected state */
    double* measured_probability    /* OUT: measured probability */
);

/* ---------------------------------------------------------------------------
 * Contextual Switching (Classical ↔ Quantum)
 * --------------------------------------------------------------------------- */

RIFT_API bool RIFT_CALL rift_context_switch_mode(
    RiftToken* token,
    RiftExecutionMode new_mode,
    RiftPolicyContext* policy
);

RIFT_API RiftExecutionMode RIFT_CALL rift_context_get_mode(
    RiftToken* token
);

RIFT_API bool RIFT_CALL rift_context_is_valid_switch(
    RiftToken* token,
    RiftExecutionMode target_mode
);

/* ---------------------------------------------------------------------------
 * Utility & Diagnostics
 * --------------------------------------------------------------------------- */

RIFT_API const char* RIFT_CALL rift_error_string(
    int error_code
);

RIFT_API void RIFT_CALL rift_get_version(
    char* version_str,
    size_t buffer_size
);

RIFT_API void RIFT_CALL rift_print_token_info(
    const RiftToken* token,
    bool verbose
);

RIFT_API uint64_t RIFT_CALL rift_get_memory_usage(
    void
);

/* Timing utility — returns current time in milliseconds */
RIFT_API double RIFT_CALL rift_get_time_ms(void);

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/* Token validation bit macros */
#define RIFT_TOKEN_IS_VALID(tok)        ((tok)->validation_bits & RIFT_TOKEN_INITIALIZED)
#define RIFT_TOKEN_IS_LOCKED(tok)       ((tok)->validation_bits & RIFT_TOKEN_LOCKED)
#define RIFT_TOKEN_IS_GOVERNED(tok)     ((tok)->validation_bits & RIFT_TOKEN_GOVERNED)
#define RIFT_TOKEN_IS_SUPERPOSED(tok)   ((tok)->validation_bits & RIFT_TOKEN_SUPERPOSED)
#define RIFT_TOKEN_IS_ENTANGLED(tok)    ((tok)->validation_bits & RIFT_TOKEN_ENTANGLED)
#define RIFT_TOKEN_IS_ALLOCATED(tok)    ((tok)->validation_bits & RIFT_TOKEN_ALLOCATED)

#define RIFT_SET_BIT(tok, bit)          ((tok)->validation_bits |= (bit))
#define RIFT_CLEAR_BIT(tok, bit)        ((tok)->validation_bits &= ~(bit))
#define RIFT_CHECK_BIT(tok, bit)        (((tok)->validation_bits & (bit)) != 0)
#define RIFT_TOGGLE_BIT(tok, bit)       ((tok)->validation_bits ^= (bit))

/* Memory-first declaration helper */
#define RIFT_DECLARE_MEMORY(name, span_type, bytes) \
    RiftMemorySpan* name = rift_span_create((span_type), (bytes))

/* Classical assignment: immediate binding with type inference */
#define RIFT_ASSIGN_CLASSICAL(token, val) \
    do { \
        (token)->value.i_val = (int64_t)(val); \
        RIFT_SET_BIT((token), RIFT_TOKEN_INITIALIZED); \
        if ((token)->type == RIFT_TOKEN_INT || (token)->type == RIFT_TOKEN_QINT) { \
            /* Type already set */ \
        } else if ((token)->type == RIFT_TOKEN_FLOAT || (token)->type == RIFT_TOKEN_QFLOAT) { \
            (token)->value.f_val = (double)(val); \
        } \
    } while(0)

/* Quantum assignment: deferred binding with superposition */
#define RIFT_ASSIGN_QUANTUM(token, states, count) \
    rift_token_superpose((token), (states), (count), NULL)

/* Policy validation helper */
#define RIFT_VALIDATE_POLICY(matrix, in, out) \
    rift_policy_validate((matrix), (in), (out))

/* Thread safety helpers */
#define RIFT_LOCK_TOKEN(tok) \
    rift_token_lock((tok))

#define RIFT_UNLOCK_TOKEN(tok) \
    rift_token_unlock((tok))

/* Safe token access pattern */
#define RIFT_WITH_TOKEN(tok, block) \
    do { \
        if (RIFT_LOCK_TOKEN((tok))) { \
            do { block } while(0); \
            RIFT_UNLOCK_TOKEN((tok)); \
        } \
    } while(0)

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define RIFT_OK                     0
#define RIFT_ERROR_NOMEM            -1      /* Memory allocation failed */
#define RIFT_ERROR_INVALID_TOKEN    -2      /* Token validation failed */
#define RIFT_ERROR_LOCK_FAILED      -3      /* Thread lock acquisition failed */
#define RIFT_ERROR_POLICY_VIOLATION -4      /* Policy validation failed */
#define RIFT_ERROR_INVALID_PATTERN  -5      /* Regex compilation failed */
#define RIFT_ERROR_MATCH_FAILED     -6      /* Pattern matching failed */
#define RIFT_ERROR_SERIALIZATION    -7      /* AST serialization failed */
#define RIFT_ERROR_DESERIALIZATION  -8      /* AST deserialization failed */
#define RIFT_ERROR_QUANTUM_INVALID  -9      /* Invalid quantum operation */
#define RIFT_ERROR_ENTANGlement     -10     /* Entanglement operation failed */
#define RIFT_ERROR_COLLAPSE         -11     /* Quantum collapse failed */

#ifdef __cplusplus
}
#endif

#endif /* RIFTLANG_H */

/**
 * =============================================================================
 * DOCUMENTATION
 * =============================================================================
 * 
 * TOKEN ARCHITECTURE:
 * ------------------
 * The triplet model (type, value, memory) enforces memory-first declaration.
 * Validation bits track token state through the lifecycle:
 *   ALLOCATED → INITIALIZED → GOVERNED → (SUPERPOSED|ENTANGLED)
 * 
 * POLAR BIPARTITE PATTERN MATCHING:
 * --------------------------------
 * Left patterns (input) use POSIX regex to match source code.
 * Right patterns (output) generate target code or templates.
 * Priority-based selection ensures deterministic matching order.
 * 
 * RESULTMATRIX2x2:
 * ---------------
 * Default policy matrix (85% threshold):
 *                 Output Invalid    Output Valid
 * Input Invalid   [DENY]            [DENY]
 * Input Valid     [DENY]            [ALLOW]
 * 
 * QUANTUM OPERATIONS:
 * ------------------
 * Superposition: Token exists in multiple states simultaneously
 * Entanglement: Linked tokens share state changes
 * Collapse: Superposition resolves to single state on observation
 * 
 * SERIALIZATION:
 * -------------
 * .rift.ast.json  - Human-readable JSON with full metadata
 * .rift.astb      - Binary format for compact storage/transmission
 * 
 * CONSTITUTIONAL COMPUTING:
 * ------------------------
 * This implementation follows OBINexus principles:
 * - Consent-first: Explicit governance directives required
 * - No silent changes: All state changes tracked in validation_bits
 * - Humans in-the-loop: Policy enforcement points for critical decisions
 * - 2:1 accountability: Public validation metrics vs private state
 * 
 * =============================================================================
 */