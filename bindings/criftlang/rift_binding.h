/**
 * @file rift_binding.h
 * @brief C binding for RiftLang - Generated from .rift policies
 * @version 1.0.0
 * 
 * This header provides C bindings for the RiftLang token architecture
 * with memory-first governance and policy enforcement.
 */

#ifndef RIFT_BINDING_H
#define RIFT_BINDING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

/* ============================================================================
 * Rift Constants (from .rift governance)
 * ============================================================================ */

#define RIFT_CLASSICAL_ALIGNMENT    4096
#define RIFT_QUANTUM_ALIGNMENT      8
#define RIFT_DEFAULT_THRESHOLD      0.85
#define RIFT_DEFAULT_ENTROPY        0.25

/* Validation bits for token governance */
#define RIFT_TOKEN_ALLOCATED        0x01
#define RIFT_TOKEN_INITIALIZED      0x02
#define RIFT_TOKEN_LOCKED           0x04
#define RIFT_TOKEN_GOVERNED         0x08
#define RIFT_TOKEN_SUPERPOSED       0x10
#define RIFT_TOKEN_ENTANGLED        0x20
#define RIFT_TOKEN_PERSISTENT       0x40
#define RIFT_TOKEN_SHADOW           0x80

/* ============================================================================
 * Token Types
 * ============================================================================ */

typedef enum {
    RIFT_TOKEN_CINT = 0,
    RIFT_TOKEN_CLONG,
    RIFT_TOKEN_CPOINTER,
    RIFT_TOKEN_CSTRUCT,
    RIFT_TOKEN_CARRAY,
    RIFT_TOKEN_CFUNC,
    RIFT_TOKEN_QCINT,
    RIFT_TOKEN_QCPointer,
    RIFT_TOKEN_COUNT
} rift_token_type_t;

typedef enum {
    RIFT_MODE_CLASSICAL = 0,
    RIFT_MODE_QUANTUM,
    RIFT_MODE_HYBRID
} rift_execution_mode_t;

typedef enum {
    RIFT_SPAN_FIXED = 0,
    RIFT_SPAN_ROW,
    RIFT_SPAN_CONTINUOUS,
    RIFT_SPAN_SUPERPOSED,
    RIFT_SPAN_ENTANGLED,
    RIFT_SPAN_DISTRIBUTED
} rift_span_type_t;

/* ============================================================================
 * Memory Governance
 * ============================================================================ */

/**
 * Memory Span - declared BEFORE type or value per Rift spec
 */
typedef struct {
    rift_span_type_t type;
    uint64_t bytes;
    uint32_t alignment;
    bool open;
    bool direction;      /* true = right->left */
    uint32_t access_mask; /* CREATE|READ|UPDATE|DELETE */
} rift_memory_span_t;

/**
 * Lock context for thread safety
 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_t owner;
    uint32_t lock_count;
    bool initialized;
    uint64_t contention_count;
} rift_lock_context_t;

/* ============================================================================
 * Token Structure - The Triplet Model
 * ============================================================================ */

/**
 * Token value union - polymorphic container
 */
typedef union {
    int64_t i_val;
    double f_val;
    char* s_val;
    void* p_val;
    struct rift_token** t_arr;
    uint8_t* q_val;
} rift_token_value_t;

/**
 * The Token Triplet: (type, value, memory) with governance
 */
typedef struct rift_token {
    /* Core triplet - memory declared first */
    rift_token_type_t type;
    rift_token_value_t value;
    rift_memory_span_t* memory;
    
    /* Governance fields */
    uint32_t validation_bits;
    rift_lock_context_t* lock_ctx;
    
    /* Quantum fields */
    struct rift_token** superposed_states;
    uint32_t superposition_count;
    double* amplitudes;
    double phase;
    
    /* Entanglement fields */
    struct rift_token** entangled_with;
    uint32_t entanglement_count;
    uint32_t entanglement_id;
    
    /* Source location */
    uint32_t source_line;
    uint32_t source_column;
    char* source_file;
} rift_token_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Token lifecycle */
rift_token_t* rift_token_create(rift_token_type_t type, rift_memory_span_t* memory);
void rift_token_destroy(rift_token_t* token);
bool rift_token_validate(rift_token_t* token);
bool rift_token_lock(rift_token_t* token);
bool rift_token_unlock(rift_token_t* token);
bool rift_token_is_valid(const rift_token_t* token);
const char* rift_token_type_name(rift_token_type_t type);

/* Memory governance */
rift_memory_span_t* rift_span_create(rift_span_type_t type, uint64_t bytes);
void rift_span_destroy(rift_memory_span_t* span);
bool rift_span_align(rift_memory_span_t* span, uint32_t alignment);
bool rift_span_validate_alignment(uint32_t alignment);

/* Quantum operations */
bool rift_token_superpose(rift_token_t* token, rift_token_t** states, 
                          uint32_t count, const double* amplitudes);
bool rift_token_entangle(rift_token_t* a, rift_token_t* b, uint32_t entanglement_id);
bool rift_token_disentangle(rift_token_t* token, uint32_t entanglement_id);
bool rift_token_collapse(rift_token_t* token, uint32_t selected_index);
double rift_token_calculate_entropy(rift_token_t* token);

/* Context switching */
bool rift_context_switch_mode(rift_token_t* token, rift_execution_mode_t new_mode);
rift_execution_mode_t rift_context_get_mode(rift_token_t* token);

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

#define RIFT_TOKEN_IS_VALID(tok)    ((tok)->validation_bits & RIFT_TOKEN_INITIALIZED)
#define RIFT_TOKEN_IS_LOCKED(tok)   ((tok)->validation_bits & RIFT_TOKEN_LOCKED)
#define RIFT_TOKEN_IS_GOVERNED(tok) ((tok)->validation_bits & RIFT_TOKEN_GOVERNED)
#define RIFT_TOKEN_IS_SUPERPOSED(tok) ((tok)->validation_bits & RIFT_TOKEN_SUPERPOSED)
#define RIFT_TOKEN_IS_ENTANGLED(tok) ((tok)->validation_bits & RIFT_TOKEN_ENTANGLED)

#define RIFT_SET_BIT(tok, bit)      ((tok)->validation_bits |= (bit))
#define RIFT_CLEAR_BIT(tok, bit)    ((tok)->validation_bits &= ~(bit))
#define RIFT_CHECK_BIT(tok, bit)    (((tok)->validation_bits & (bit)) != 0)

/* Memory-first declaration helper */
#define RIFT_DECLARE_MEMORY(name, span_type, bytes) \
    rift_memory_span_t* name = rift_span_create((span_type), (bytes))

/* Classical assignment: immediate binding */
#define RIFT_ASSIGN_CLASSICAL(token, val) \
    do { \
        (token)->value.i_val = (int64_t)(val); \
        RIFT_SET_BIT((token), RIFT_TOKEN_INITIALIZED); \
    } while(0)

/* Safe token access pattern */
#define RIFT_WITH_TOKEN(tok, block) \
    do { \
        if (rift_token_lock((tok))) { \
            do { block } while(0); \
            rift_token_unlock((tok)); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* RIFT_BINDING_H */
