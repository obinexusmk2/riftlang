/**
 * @file rift_binding.c
 * @brief C binding implementation for RiftLang
 * @version 1.0.0
 */

#include "rift_binding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Internal Utilities
 * ============================================================================ */

static void* rift_malloc(size_t size) {
    void* ptr = calloc(1, size);
    return ptr;
}

static void rift_free(void* ptr) {
    if (ptr) free(ptr);
}

/* ============================================================================
 * Token Lifecycle Implementation
 * ============================================================================ */

rift_token_t* rift_token_create(rift_token_type_t type, rift_memory_span_t* memory) {
    rift_token_t* token = (rift_token_t*)rift_malloc(sizeof(rift_token_t));
    if (!token) return NULL;
    
    /* Initialize core triplet */
    token->type = type;
    token->memory = memory;
    token->validation_bits = RIFT_TOKEN_ALLOCATED;
    
    /* Initialize lock context */
    token->lock_ctx = (rift_lock_context_t*)rift_malloc(sizeof(rift_lock_context_t));
    if (token->lock_ctx) {
        pthread_mutex_init(&token->lock_ctx->mutex, NULL);
        token->lock_ctx->initialized = true;
        token->lock_ctx->lock_count = 0;
        token->lock_ctx->owner = 0;
        token->lock_ctx->contention_count = 0;
    }
    
    /* Initialize quantum fields */
    token->superposed_states = NULL;
    token->superposition_count = 0;
    token->amplitudes = NULL;
    token->phase = 0.0;
    
    /* Initialize entanglement fields */
    token->entangled_with = NULL;
    token->entanglement_count = 0;
    token->entanglement_id = 0;
    
    /* Zero out value union */
    memset(&token->value, 0, sizeof(rift_token_value_t));
    
    /* Source location */
    token->source_line = 0;
    token->source_column = 0;
    token->source_file = NULL;
    
    return token;
}

void rift_token_destroy(rift_token_t* token) {
    if (!token) return;
    
    /* Release any active locks */
    if (RIFT_TOKEN_IS_LOCKED(token)) {
        rift_token_unlock(token);
    }
    
    /* Clean up lock context */
    if (token->lock_ctx && token->lock_ctx->initialized) {
        pthread_mutex_destroy(&token->lock_ctx->mutex);
        rift_free(token->lock_ctx);
    }
    
    /* Clean up string value if present */
    if (token->type == RIFT_TOKEN_CPOINTER && token->value.s_val) {
        rift_free(token->value.s_val);
    }
    
    /* Clean up superposition states */
    if (token->superposed_states) {
        rift_free(token->superposed_states);
    }
    if (token->amplitudes) {
        rift_free(token->amplitudes);
    }
    
    /* Clean up entanglement links */
    if (token->entangled_with) {
        rift_free(token->entangled_with);
    }
    
    /* Clean up source file string */
    if (token->source_file) {
        rift_free(token->source_file);
    }
    
    /* Clear validation bits */
    token->validation_bits = 0;
    
    rift_free(token);
}

bool rift_token_validate(rift_token_t* token) {
    if (!token) return false;
    
    /* Check ALLOCATED bit */
    if (!(token->validation_bits & RIFT_TOKEN_ALLOCATED)) {
        return false;
    }
    
    /* Memory span must exist and be valid */
    if (!token->memory || token->memory->alignment == 0) {
        return false;
    }
    
    /* Validate alignment is power of 2 */
    uint32_t align = token->memory->alignment;
    if ((align & (align - 1)) != 0) {
        return false;
    }
    
    /* Type-specific validation */
    switch (token->type) {
        case RIFT_TOKEN_CINT:
        case RIFT_TOKEN_CLONG:
            /* Numeric types must have initialized value */
            if (!(token->validation_bits & RIFT_TOKEN_INITIALIZED)) {
                return false;
            }
            break;
            
        case RIFT_TOKEN_QCINT:
            /* Quantum tokens need states if superposed */
            if (RIFT_TOKEN_IS_SUPERPOSED(token)) {
                if (!token->superposed_states || token->superposition_count == 0) {
                    return false;
                }
            }
            break;
            
        default:
            break;
    }
    
    /* Mark as governed */
    RIFT_SET_BIT(token, RIFT_TOKEN_GOVERNED);
    return true;
}

bool rift_token_lock(rift_token_t* token) {
    if (!token || !token->lock_ctx || !token->lock_ctx->initialized) {
        return false;
    }
    
    pthread_t self = pthread_self();
    
    /* Check for recursive lock by same thread */
    if (token->lock_ctx->owner == self && token->lock_ctx->lock_count > 0) {
        token->lock_ctx->lock_count++;
        return true;
    }
    
    /* Acquire mutex */
    int result = pthread_mutex_lock(&token->lock_ctx->mutex);
    if (result != 0) {
        return false;
    }
    
    token->lock_ctx->owner = self;
    token->lock_ctx->lock_count = 1;
    RIFT_SET_BIT(token, RIFT_TOKEN_LOCKED);
    
    return true;
}

bool rift_token_unlock(rift_token_t* token) {
    if (!token || !token->lock_ctx || !token->lock_ctx->initialized) {
        return false;
    }
    
    pthread_t self = pthread_self();
    
    /* Verify we own the lock */
    if (token->lock_ctx->owner != self) {
        return false;
    }
    
    if (token->lock_ctx->lock_count > 0) {
        token->lock_ctx->lock_count--;
        
        if (token->lock_ctx->lock_count == 0) {
            token->lock_ctx->owner = 0;
            RIFT_CLEAR_BIT(token, RIFT_TOKEN_LOCKED);
            pthread_mutex_unlock(&token->lock_ctx->mutex);
        }
    }
    
    return true;
}

bool rift_token_is_valid(const rift_token_t* token) {
    if (!token) return false;
    return RIFT_TOKEN_IS_VALID(token) && RIFT_TOKEN_IS_GOVERNED(token);
}

const char* rift_token_type_name(rift_token_type_t type) {
    static const char* names[] = {
        "CINT", "CLONG", "CPOINTER", "CSTRUCT", "CARRAY", "CFUNC",
        "QCINT", "QCPointer"
    };
    
    if (type < RIFT_TOKEN_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Memory Governance Implementation
 * ============================================================================ */

rift_memory_span_t* rift_span_create(rift_span_type_t type, uint64_t bytes) {
    rift_memory_span_t* span = (rift_memory_span_t*)rift_malloc(sizeof(rift_memory_span_t));
    if (!span) return NULL;
    
    span->type = type;
    span->bytes = bytes;
    span->open = true;
    span->direction = true;  /* Default: right->left */
    span->access_mask = 0x0F; /* CREATE | READ | UPDATE | DELETE */
    
    /* Set default alignment based on span type */
    switch (type) {
        case RIFT_SPAN_FIXED:
        case RIFT_SPAN_ROW:
        case RIFT_SPAN_CONTINUOUS:
            span->alignment = RIFT_CLASSICAL_ALIGNMENT;
            break;
        case RIFT_SPAN_SUPERPOSED:
        case RIFT_SPAN_ENTANGLED:
            span->alignment = RIFT_QUANTUM_ALIGNMENT;
            break;
        case RIFT_SPAN_DISTRIBUTED:
            span->alignment = 64; /* Cache-line alignment */
            break;
        default:
            span->alignment = RIFT_CLASSICAL_ALIGNMENT;
    }
    
    return span;
}

void rift_span_destroy(rift_memory_span_t* span) {
    rift_free(span);
}

bool rift_span_align(rift_memory_span_t* span, uint32_t alignment) {
    if (!span || alignment == 0) return false;
    
    /* Validate alignment is power of 2 */
    if ((alignment & (alignment - 1)) != 0) {
        return false;
    }
    
    span->alignment = alignment;
    return true;
}

bool rift_span_validate_alignment(uint32_t alignment) {
    if (alignment == 0) return false;
    return (alignment & (alignment - 1)) == 0;
}

/* ============================================================================
 * Quantum Operations Implementation
 * ============================================================================ */

bool rift_token_superpose(rift_token_t* token, rift_token_t** states, 
                          uint32_t count, const double* amplitudes) {
    if (!token || !states || count == 0) return false;
    
    /* Allocate superposition arrays */
    token->superposed_states = (rift_token_t**)rift_malloc(count * sizeof(rift_token_t*));
    if (!token->superposed_states) return false;
    
    memcpy(token->superposed_states, states, count * sizeof(rift_token_t*));
    token->superposition_count = count;
    
    /* Allocate and set amplitudes */
    if (amplitudes) {
        token->amplitudes = (double*)rift_malloc(count * sizeof(double));
        if (token->amplitudes) {
            memcpy(token->amplitudes, amplitudes, count * sizeof(double));
        }
    }
    
    RIFT_SET_BIT(token, RIFT_TOKEN_SUPERPOSED);
    return true;
}

bool rift_token_entangle(rift_token_t* a, rift_token_t* b, uint32_t entanglement_id) {
    if (!a || !b) return false;
    
    /* Add b to a's entanglement list */
    if (!a->entangled_with) {
        a->entangled_with = (rift_token_t**)rift_malloc(sizeof(rift_token_t*));
        if (!a->entangled_with) return false;
        a->entanglement_count = 0;
    } else {
        rift_token_t** new_list = realloc(a->entangled_with, 
                                          (a->entanglement_count + 1) * sizeof(rift_token_t*));
        if (!new_list) return false;
        a->entangled_with = new_list;
    }
    
    a->entangled_with[a->entanglement_count++] = b;
    a->entanglement_id = entanglement_id;
    
    RIFT_SET_BIT(a, RIFT_TOKEN_ENTANGLED);
    RIFT_SET_BIT(b, RIFT_TOKEN_ENTANGLED);
    
    return true;
}

bool rift_token_disentangle(rift_token_t* token, uint32_t entanglement_id) {
    if (!token || !token->entangled_with) return false;
    
    /* Simple disentanglement - clear all */
    rift_free(token->entangled_with);
    token->entangled_with = NULL;
    token->entanglement_count = 0;
    token->entanglement_id = 0;
    
    RIFT_CLEAR_BIT(token, RIFT_TOKEN_ENTANGLED);
    return true;
}

bool rift_token_collapse(rift_token_t* token, uint32_t selected_index) {
    if (!token || !RIFT_TOKEN_IS_SUPERPOSED(token)) return false;
    
    if (!token->superposed_states || selected_index >= token->superposition_count) {
        return false;
    }
    
    /* Collapse to selected state */
    rift_token_t* collapsed = token->superposed_states[selected_index];
    token->value = collapsed->value;
    token->type = collapsed->type;
    
    /* Clear superposition */
    rift_free(token->superposed_states);
    token->superposed_states = NULL;
    
    if (token->amplitudes) {
        rift_free(token->amplitudes);
        token->amplitudes = NULL;
    }
    
    token->superposition_count = 0;
    RIFT_CLEAR_BIT(token, RIFT_TOKEN_SUPERPOSED);
    
    return true;
}

double rift_token_calculate_entropy(rift_token_t* token) {
    if (!token || !token->amplitudes || token->superposition_count == 0) {
        return 0.0;
    }
    
    /* Calculate Shannon entropy from amplitudes */
    double entropy = 0.0;
    for (uint32_t i = 0; i < token->superposition_count; i++) {
        double p = token->amplitudes[i] * token->amplitudes[i];
        if (p > 0) {
            entropy -= p * log2(p);
        }
    }
    
    return entropy;
}

/* ============================================================================
 * Context Switching Implementation
 * ============================================================================ */

bool rift_context_switch_mode(rift_token_t* token, rift_execution_mode_t new_mode) {
    if (!token) return false;
    
    /* Validate mode switch is allowed */
    rift_execution_mode_t current = rift_context_get_mode(token);
    
    /* Cannot switch from quantum to classical if superposed */
    if (current == RIFT_MODE_QUANTUM && new_mode == RIFT_MODE_CLASSICAL) {
        if (RIFT_TOKEN_IS_SUPERPOSED(token)) {
            /* Must collapse first */
            return false;
        }
    }
    
    /* Update memory alignment based on mode */
    if (token->memory) {
        switch (new_mode) {
            case RIFT_MODE_CLASSICAL:
                token->memory->alignment = RIFT_CLASSICAL_ALIGNMENT;
                break;
            case RIFT_MODE_QUANTUM:
                token->memory->alignment = RIFT_QUANTUM_ALIGNMENT;
                break;
            case RIFT_MODE_HYBRID:
                /* Keep current alignment */
                break;
        }
    }
    
    return true;
}

rift_execution_mode_t rift_context_get_mode(rift_token_t* token) {
    if (!token || !token->memory) {
        return RIFT_MODE_CLASSICAL;
    }
    
    /* Infer mode from memory span type */
    switch (token->memory->type) {
        case RIFT_SPAN_SUPERPOSED:
        case RIFT_SPAN_ENTANGLED:
            return RIFT_MODE_QUANTUM;
        case RIFT_SPAN_FIXED:
        case RIFT_SPAN_ROW:
        case RIFT_SPAN_CONTINUOUS:
            return RIFT_MODE_CLASSICAL;
        default:
            return RIFT_MODE_HYBRID;
    }
}
