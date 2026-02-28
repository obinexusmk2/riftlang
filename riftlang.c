/**
 * @file riftlang.c
 * @brief RIFTLang Core Implementation - Polar Bipartite Pattern Matching
 * @author Nnamdi Michael Okpala
 * @version 1.0.0
 * 
 * Constitutional Computing Framework - OBINexus
 */

#include "riftlang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 * Internal Utilities & Memory Management
 * ============================================================================ */

/**
 * Safe zero-initializing allocation
 */
static void* rift_malloc(size_t size) {
    void* ptr = calloc(1, size);
    return ptr;
}

/**
 * Safe deallocation with null check
 */
static void rift_free(void* ptr) {
    if (ptr) free(ptr);
}

/**
 * Get current time in milliseconds for metrics
 */
double rift_get_time_ms(void) {
#ifdef _WIN32
    return (double)GetTickCount();   /* always available; 32-bit ms counter */
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

/* ============================================================================
 * Regex Utilities (Polar Bipartite Core)
 * ============================================================================ */

/**
 * Compile POSIX regex pattern
 */
static bool rift_regex_compile(regex_t* regex, const char* pattern, bool anchored, bool extended) {
    if (!regex || !pattern) return false;
    
    int flags = REG_EXTENDED;
    if (anchored) {
        flags |= REG_NEWLINE;
    }
    
    int result = regcomp(regex, pattern, flags);
    if (result != 0) {
        char errbuf[256];
        regerror(result, regex, errbuf, sizeof(errbuf));
        /* In production: log error */
        return false;
    }
    return true;
}

/**
 * Execute regex match
 */
static bool rift_regex_match(regex_t* regex, const char* input, regmatch_t* matches, size_t nmatch) {
    if (!regex || !input) return false;
    
    int result = regexec(regex, input, nmatch, matches, 0);
    return (result == 0);
}

/**
 * Simple match without capture groups
 */
static bool rift_regex_match_simple(regex_t* regex, const char* input) {
    return rift_regex_match(regex, input, NULL, 0);
}

/* ============================================================================
 * Token Lifecycle Implementation
 * ============================================================================ */

RIFT_API RiftToken* rift_token_create(RiftTokenType type, RiftMemorySpan* span) {
    RiftToken* token = (RiftToken*)rift_malloc(sizeof(RiftToken));
    if (!token) return NULL;
    
    /* Initialize core triplet */
    token->type = type;
    token->memory = span;
    token->validation_bits = RIFT_TOKEN_ALLOCATED;
    
    /* Initialize lock context for thread safety */
    token->lock_ctx = (RiftLockContext*)rift_malloc(sizeof(RiftLockContext));
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
    memset(&token->value, 0, sizeof(RiftTokenValue));
    
    /* Initialize source location */
    token->source_line = 0;
    token->source_column = 0;
    token->source_file = NULL;
    
    return token;
}

RIFT_API void rift_token_destroy(RiftToken* token) {
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
    
    /* Clean up string value if present (owned memory) */
    if ((token->type == RIFT_TOKEN_STRING || token->type == RIFT_TOKEN_QBYTE) 
        && token->value.s_val) {
        rift_free(token->value.s_val);
    }
    
    /* Clean up array/vector values */
    if ((token->type == RIFT_TOKEN_ARRAY || token->type == RIFT_TOKEN_VECTOR 
         || token->type == RIFT_TOKEN_DSA) && token->value.t_arr) {
        /* Note: Does not recursively destroy tokens - caller must manage */
        rift_free(token->value.t_arr);
    }
    
    /* Clean up quantum byte arrays */
    if ((token->type == RIFT_TOKEN_QBYTE || token->type == RIFT_TOKEN_QMATRIX)
        && token->value.q_val) {
        rift_free(token->value.q_val);
    }
    
    /* Clean up superposition states (references only, not owned) */
    if (token->superposed_states) {
        rift_free(token->superposed_states);
    }
    if (token->amplitudes) {
        rift_free(token->amplitudes);
    }
    
    /* Clean up entanglement links (references only) */
    if (token->entangled_with) {
        rift_free(token->entangled_with);
    }
    
    /* Clean up source file string */
    if (token->source_file) {
        rift_free(token->source_file);
    }
    
    /* Clear validation bits (security: clear state) */
    token->validation_bits = 0;
    
    rift_free(token);
}

RIFT_API bool rift_token_validate(RiftToken* token) {
    if (!token) return false;
    
    /* Check ALLOCATED bit - must be set */
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
        case RIFT_TOKEN_INT:
        case RIFT_TOKEN_FLOAT:
        case RIFT_TOKEN_QINT:
        case RIFT_TOKEN_QFLOAT:
            /* Numeric types must have initialized value */
            if (!(token->validation_bits & RIFT_TOKEN_INITIALIZED)) {
                return false;
            }
            break;
            
        case RIFT_TOKEN_STRING:
            /* String must have non-null pointer if initialized */
            if ((token->validation_bits & RIFT_TOKEN_INITIALIZED) 
                && !token->value.s_val) {
                return false;
            }
            break;
            
        case RIFT_TOKEN_SUPERPOSED:
        case RIFT_TOKEN_QBYTE:
            /* Quantum superposed tokens need states */
            if (RIFT_TOKEN_IS_SUPERPOSED(token)) {
                if (!token->superposed_states || token->superposition_count == 0) {
                    return false;
                }
                /* Validate amplitudes sum to ~1.0 */
                if (token->amplitudes) {
                    double sum = 0.0;
                    for (uint32_t i = 0; i < token->superposition_count; i++) {
                        sum += token->amplitudes[i] * token->amplitudes[i];
                    }
                    if (sum < 0.99 || sum > 1.01) {
                        return false; /* Probabilities don't normalize */
                    }
                }
            }
            break;
            
        case RIFT_TOKEN_ENTANGLED:
            /* Entangled tokens need links */
            if (RIFT_TOKEN_IS_ENTANGLED(token)) {
                if (!token->entangled_with || token->entanglement_count == 0) {
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

RIFT_API bool rift_token_lock(RiftToken* token) {
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

RIFT_API bool rift_token_unlock(RiftToken* token) {
    if (!token || !token->lock_ctx || !token->lock_ctx->initialized) {
        return false;
    }
    
    pthread_t self = pthread_self();
    
    /* Verify we own the lock */
    if (token->lock_ctx->owner != self) {
        return false; /* Cannot unlock token owned by another thread */
    }
    
    if (token->lock_ctx->lock_count > 0) {
        token->lock_ctx->lock_count--;
        
        if (token->lock_ctx->lock_count == 0) {
            token->lock_ctx->owner = 0;
            RIFT_CLEAR_BIT(token, RIFT_TOKEN_LOCKED);
            pthread_mutex_unlock(&token->lock_ctx->mutex);
        }
    } else {
        /* Lock count already 0, force unlock */
        RIFT_CLEAR_BIT(token, RIFT_TOKEN_LOCKED);
        pthread_mutex_unlock(&token->lock_ctx->mutex);
    }
    
    return true;
}

RIFT_API bool rift_token_is_valid(const RiftToken* token) {
    if (!token) return false;
    return RIFT_TOKEN_IS_VALID(token) && RIFT_TOKEN_IS_GOVERNED(token);
}

RIFT_API const char* rift_token_type_name(RiftTokenType type) {
    static const char* names[] = {
        "INT", "FLOAT", "STRING", "ROLE", "MASK", "OP",
        "ARRAY", "VECTOR", "MAP", "TUPLE", "DSA",
        "QBYTE", "QROLE", "QMATRIX", "QINT", "QFLOAT"
    };
    
    if (type < RIFT_TOKEN_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Memory Governance Implementation
 * ============================================================================ */

RIFT_API RiftMemorySpan* rift_span_create(RiftSpanType type, uint64_t bytes) {
    RiftMemorySpan* span = (RiftMemorySpan*)rift_malloc(sizeof(RiftMemorySpan));
    if (!span) return NULL;
    
    span->type = type;
    span->bytes = bytes;
    span->open = true;
    span->direction = true;  /* Default: right->left */
    span->access_mask = 0x0F; /* CREATE | READ | UPDATE | DELETE */
    
    /* Set default alignment based on span type and mode */
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
            span->alignment = 64; /* Cache-line alignment for DSA */
            break;
        default:
            span->alignment = RIFT_CLASSICAL_ALIGNMENT;
    }
    
    return span;
}

RIFT_API void rift_span_destroy(RiftMemorySpan* span) {
    rift_free(span);
}

RIFT_API bool rift_span_align(RiftMemorySpan* span, uint32_t alignment) {
    if (!span || alignment == 0) return false;
    
    /* Validate alignment is power of 2 */
    if ((alignment & (alignment - 1)) != 0) {
        return false;
    }
    
    span->alignment = alignment;
    return true;
}

RIFT_API bool rift_span_validate_alignment(uint32_t alignment) {
    if (alignment == 0) return false;
    return (alignment & (alignment - 1)) == 0;
}

RIFT_API uint32_t rift_span_get_default_alignment(RiftSpanType type, RiftExecutionMode mode) {
    if (mode == RIFT_MODE_QUANTUM || mode == RIFT_MODE_HYBRID) {
        switch (type) {
            case RIFT_SPAN_SUPERPOSED:
            case RIFT_SPAN_ENTANGLED:
                return RIFT_QUANTUM_ALIGNMENT;
            default:
                break;
        }
    }
    return RIFT_CLASSICAL_ALIGNMENT;
}

/* ============================================================================
 * Polar Bipartite Pattern Matching Engine
 * ============================================================================ */

RIFT_API RiftPatternEngine* rift_pattern_engine_create(RiftExecutionMode mode) {
    RiftPatternEngine* engine = (RiftPatternEngine*)rift_malloc(sizeof(RiftPatternEngine));
    if (!engine) return NULL;
    
    /* Initialize arrays */
    engine->pairs = NULL;
    engine->pair_count = 0;
    engine->capacity = 0;
    
    /* Initialize cache */
    engine->compiled_cache = NULL;
    engine->cache_patterns = NULL;
    engine->cache_size = 0;
    
    /* Set mode */
    engine->mode = mode;
    
    /* Initialize thread safety */
    engine->lock_ctx = (RiftLockContext*)rift_malloc(sizeof(RiftLockContext));
    if (engine->lock_ctx) {
        pthread_mutex_init(&engine->lock_ctx->mutex, NULL);
        engine->lock_ctx->initialized = true;
        engine->lock_ctx->lock_count = 0;
    }
    
    /* Initialize metrics */
    engine->total_matches = 0;
    engine->total_failures = 0;
    engine->average_match_time_ms = 0.0;
    
    return engine;
}

RIFT_API void rift_pattern_engine_destroy(RiftPatternEngine* engine) {
    if (!engine) return;
    
    /* Lock engine during destruction */
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_lock(&engine->lock_ctx->mutex);
    }
    
    /* Clean up all pattern pairs */
    for (uint32_t i = 0; i < engine->pair_count; i++) {
        RiftBipartitePair* pair = engine->pairs[i];
        if (pair) {
            /* Clean up left pattern */
            if (pair->left) {
                regfree(&pair->left->compiled_regex);
                rift_free(pair->left->pattern_str);
                rift_free(pair->left);
            }
            
            /* Clean up right pattern */
            if (pair->right) {
                if (!pair->right->is_literal) {
                    regfree(&pair->right->compiled_regex);
                }
                rift_free(pair->right->pattern_str);
                rift_free(pair->right);
            }
            
            rift_free(pair);
        }
    }
    
    rift_free(engine->pairs);
    
    /* Clean up cache */
    for (uint32_t i = 0; i < engine->cache_size; i++) {
        if (engine->compiled_cache && engine->compiled_cache[i]) {
            regfree(engine->compiled_cache[i]);
            rift_free(engine->compiled_cache[i]);
        }
        if (engine->cache_patterns && engine->cache_patterns[i]) {
            rift_free(engine->cache_patterns[i]);
        }
    }
    rift_free(engine->compiled_cache);
    rift_free(engine->cache_patterns);
    
    /* Unlock and clean up lock context */
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_unlock(&engine->lock_ctx->mutex);
        pthread_mutex_destroy(&engine->lock_ctx->mutex);
        rift_free(engine->lock_ctx);
    }
    
    rift_free(engine);
}

RIFT_API bool rift_pattern_engine_add_pair(
    RiftPatternEngine* engine,
    const char* left_pattern,
    const char* right_pattern,
    uint32_t priority,
    bool right_is_literal
) {
    if (!engine || !left_pattern || !right_pattern) return false;
    
    /* Lock engine for thread-safe modification */
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_lock(&engine->lock_ctx->mutex);
    }
    
    /* Expand capacity if needed */
    if (engine->pair_count >= engine->capacity) {
        uint32_t new_capacity = engine->capacity == 0 ? 16 : engine->capacity * 2;
        RiftBipartitePair** new_pairs = (RiftBipartitePair**)realloc(
            engine->pairs, 
            new_capacity * sizeof(RiftBipartitePair*)
        );
        if (!new_pairs) {
            if (engine->lock_ctx && engine->lock_ctx->initialized) {
                pthread_mutex_unlock(&engine->lock_ctx->mutex);
            }
            return false;
        }
        
        engine->pairs = new_pairs;
        engine->capacity = new_capacity;
    }
    
    /* Create left pattern (input/matcher) */
    RiftPattern* left = (RiftPattern*)rift_malloc(sizeof(RiftPattern));
    if (!left) {
        if (engine->lock_ctx && engine->lock_ctx->initialized) {
            pthread_mutex_unlock(&engine->lock_ctx->mutex);
        }
        return false;
    }
    
    left->pattern_str = strdup(left_pattern);
    left->polarity = RIFT_PATTERN_LEFT;
    left->priority = priority;
    left->anchored = (left_pattern[0] == '^');
    left->is_literal = false;
    
    /* Compile left regex immediately */
    if (!rift_regex_compile(&left->compiled_regex, left_pattern, left->anchored, true)) {
        rift_free(left->pattern_str);
        rift_free(left);
        if (engine->lock_ctx && engine->lock_ctx->initialized) {
            pthread_mutex_unlock(&engine->lock_ctx->mutex);
        }
        return false;
    }
    
    /* Create right pattern (output/generator) */
    RiftPattern* right = (RiftPattern*)rift_malloc(sizeof(RiftPattern));
    if (!right) {
        regfree(&left->compiled_regex);
        rift_free(left->pattern_str);
        rift_free(left);
        if (engine->lock_ctx && engine->lock_ctx->initialized) {
            pthread_mutex_unlock(&engine->lock_ctx->mutex);
        }
        return false;
    }
    
    right->pattern_str = strdup(right_pattern);
    right->polarity = RIFT_PATTERN_RIGHT;
    right->priority = priority;
    right->anchored = false;
    right->is_literal = right_is_literal;
    
    /* Compile right regex only if it's not a literal */
    if (!right_is_literal) {
        /* Check if it looks like a regex */
        if (strchr(right_pattern, '(') || strchr(right_pattern, '[') || 
            strchr(right_pattern, '*') || strchr(right_pattern, '+') ||
            strchr(right_pattern, '?') || strchr(right_pattern, '|')) {
            if (!rift_regex_compile(&right->compiled_regex, right_pattern, false, true)) {
                /* Compilation failed, but we can still use as literal */
                right->is_literal = true;
                memset(&right->compiled_regex, 0, sizeof(regex_t));
            }
        } else {
            right->is_literal = true;
            memset(&right->compiled_regex, 0, sizeof(regex_t));
        }
    } else {
        memset(&right->compiled_regex, 0, sizeof(regex_t));
    }
    
    /* Create bipartite pair */
    RiftBipartitePair* pair = (RiftBipartitePair*)rift_malloc(sizeof(RiftBipartitePair));
    if (!pair) {
        regfree(&left->compiled_regex);
        rift_free(left->pattern_str);
        rift_free(left);
        if (!right->is_literal) {
            regfree(&right->compiled_regex);
        }
        rift_free(right->pattern_str);
        rift_free(right);
        if (engine->lock_ctx && engine->lock_ctx->initialized) {
            pthread_mutex_unlock(&engine->lock_ctx->mutex);
        }
        return false;
    }
    
    pair->left = left;
    pair->right = right;
    pair->transform_fn = NULL;
    pair->is_governed = false;
    pair->transform_id = engine->pair_count + 1;
    
    engine->pairs[engine->pair_count++] = pair;
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_unlock(&engine->lock_ctx->mutex);
    }
    
    return true;
}

RIFT_API bool rift_pattern_engine_compile(RiftPatternEngine* engine) {
    if (!engine) return false;
    
    /* All patterns already compiled during add_pair */
    /* This function validates the engine state */
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_lock(&engine->lock_ctx->mutex);
    }
    
    /* Validate all pairs have compiled left patterns */
    for (uint32_t i = 0; i < engine->pair_count; i++) {
        RiftBipartitePair* pair = engine->pairs[i];
        if (!pair || !pair->left) {
            if (engine->lock_ctx && engine->lock_ctx->initialized) {
                pthread_mutex_unlock(&engine->lock_ctx->mutex);
            }
            return false;
        }
        /* Verify regex is compiled by checking pattern_str */
        if (!pair->left->pattern_str) {
            if (engine->lock_ctx && engine->lock_ctx->initialized) {
                pthread_mutex_unlock(&engine->lock_ctx->mutex);
            }
            return false;
        }
    }
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_unlock(&engine->lock_ctx->mutex);
    }
    
    return true;
}

RIFT_API char* rift_pattern_engine_match(
    RiftPatternEngine* engine,
    const char* input,
    size_t* output_len,
    uint32_t* matched_priority
) {
    if (!engine || !input || !output_len) {
        if (output_len) *output_len = 0;
        return NULL;
    }
    
    double start_time = rift_get_time_ms();
    
    /* Lock engine for thread-safe matching */
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_lock(&engine->lock_ctx->mutex);
    }
    
    /* Search for matching pattern (respecting priority) */
    RiftBipartitePair* matched_pair = NULL;
    uint32_t best_priority = UINT32_MAX;
    regmatch_t matches[10]; /* Capture groups */
    
    for (uint32_t i = 0; i < engine->pair_count; i++) {
        RiftBipartitePair* pair = engine->pairs[i];
        if (!pair || !pair->left) continue;
        
        /* Check priority - lower number = higher priority */
        if (pair->left->priority > best_priority) continue;
        
        /* Try to match input against left pattern */
        if (rift_regex_match(&pair->left->compiled_regex, input, matches, 10)) {
            matched_pair = pair;
            best_priority = pair->left->priority;
            if (matched_priority) {
                *matched_priority = best_priority;
            }
            /* Continue searching for higher priority match */
        }
    }
    
    /* Generate output from right pattern */
    char* output = NULL;
    if (matched_pair && matched_pair->right) {
        const char* template = matched_pair->right->pattern_str;
        size_t template_len = strlen(template);
        
        /* Simple substitution: copy template as-is for literals */
        /* In production: support $1, $2 capture group substitution */
        if (matched_pair->right->is_literal) {
            output = (char*)rift_malloc(template_len + 1);
            if (output) {
                strcpy(output, template);
                *output_len = template_len;
            }
        } else {
            /* For regex patterns, we might need to process matches */
            /* Simplified: just copy for now */
            output = (char*)rift_malloc(template_len + 1);
            if (output) {
                strcpy(output, template);
                *output_len = template_len;
            }
        }
    }
    
    /* Update metrics */
    double elapsed = rift_get_time_ms() - start_time;
    if (matched_pair) {
        engine->total_matches++;
    } else {
        engine->total_failures++;
    }
    /* Running average */
    double total_time = engine->average_match_time_ms * (engine->total_matches + engine->total_failures - 1) + elapsed;
    engine->average_match_time_ms = total_time / (engine->total_matches + engine->total_failures);
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_unlock(&engine->lock_ctx->mutex);
    }
    
    return output;
}

RIFT_API bool rift_pattern_engine_match_all(
    RiftPatternEngine* engine,
    const char* input,
    RiftBipartitePair** matches,
    uint32_t* match_count
) {
    if (!engine || !input || !match_count) return false;
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_lock(&engine->lock_ctx->mutex);
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < engine->pair_count && count < *match_count; i++) {
        RiftBipartitePair* pair = engine->pairs[i];
        if (!pair || !pair->left) continue;
        
        if (rift_regex_match_simple(&pair->left->compiled_regex, input)) {
            if (matches) {
                matches[count] = pair;
            }
            count++;
        }
    }
    
    *match_count = count;
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_unlock(&engine->lock_ctx->mutex);
    }
    
    return true;
}

RIFT_API void rift_pattern_engine_get_metrics(
    RiftPatternEngine* engine,
    uint64_t* total_matches,
    uint64_t* total_failures,
    double* avg_time_ms
) {
    if (!engine) return;
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_lock(&engine->lock_ctx->mutex);
    }
    
    if (total_matches) *total_matches = engine->total_matches;
    if (total_failures) *total_failures = engine->total_failures;
    if (avg_time_ms) *avg_time_ms = engine->average_match_time_ms;
    
    if (engine->lock_ctx && engine->lock_ctx->initialized) {
        pthread_mutex_unlock(&engine->lock_ctx->mutex);
    }
}

/* ============================================================================
 * Policy & Governance Implementation
 * ============================================================================ */

RIFT_API RiftResultMatrix2x2* rift_result_matrix_create(double threshold) {
    RiftResultMatrix2x2* matrix = (RiftResultMatrix2x2*)rift_malloc(sizeof(RiftResultMatrix2x2));
    if (!matrix) return NULL;
    
    /* Initialize 2x2 policy matrix */
    /* [input_valid][output_valid] = policy_result */
    matrix->matrix[0][0] = RIFT_POLICY_DENY;   /* Invalid input, invalid output */
    matrix->matrix[0][1] = RIFT_POLICY_DENY;   /* Invalid input, valid output */
    matrix->matrix[1][0] = RIFT_POLICY_DENY;   /* Valid input, invalid output */
    matrix->matrix[1][1] = RIFT_POLICY_ALLOW;  /* Valid input, valid output */
    
    /* Set thresholds */
    matrix->validation_threshold = (threshold > 0.0 && threshold <= 1.0) 
        ? threshold : RIFT_DEFAULT_THRESHOLD;
    matrix->entropy_threshold = RIFT_DEFAULT_ENTROPY;
    
    /* Initialize metrics */
    matrix->validations_passed = 0;
    matrix->validations_failed = 0;
    matrix->validations_deferred = 0;
    matrix->total_validations = 0;
    matrix->average_validation_time_ms = 0.0;
    matrix->policy_violations = 0;
    
    return matrix;
}

RIFT_API void rift_result_matrix_destroy(RiftResultMatrix2x2* matrix) {
    rift_free(matrix);
}

RIFT_API RiftPolicyResult rift_policy_validate(
    RiftResultMatrix2x2* matrix,
    bool input_valid,
    bool output_valid
) {
    if (!matrix) return RIFT_POLICY_DENY;
    
    double start_time = rift_get_time_ms();
    
    int row = input_valid ? 1 : 0;
    int col = output_valid ? 1 : 0;
    
    RiftPolicyResult result = matrix->matrix[row][col];
    
    /* Update metrics */
    matrix->total_validations++;
    switch (result) {
        case RIFT_POLICY_ALLOW:
            matrix->validations_passed++;
            break;
        case RIFT_POLICY_DENY:
            matrix->validations_failed++;
            matrix->policy_violations++;
            break;
        case RIFT_POLICY_DEFER:
            matrix->validations_deferred++;
            break;
    }
    
    /* Update timing */
    double elapsed = rift_get_time_ms() - start_time;
    matrix->average_validation_time_ms = 
        ((matrix->average_validation_time_ms * (matrix->total_validations - 1)) + elapsed) 
        / matrix->total_validations;
    
    return result;
}

RIFT_API double rift_policy_get_validation_ratio(RiftResultMatrix2x2* matrix) {
    if (!matrix) return 0.0;
    
    uint64_t decided = matrix->validations_passed + matrix->validations_failed;
    if (decided == 0) return 0.0;
    
    return (double)matrix->validations_passed / (double)decided;
}

RIFT_API bool rift_policy_meets_threshold(RiftResultMatrix2x2* matrix) {
    if (!matrix) return false;
    return rift_policy_get_validation_ratio(matrix) >= matrix->validation_threshold;
}

RIFT_API RiftPolicyContext* rift_policy_context_create(
    const char* policy_name,
    double threshold,
    bool immediate
) {
    RiftPolicyContext* ctx = (RiftPolicyContext*)rift_malloc(sizeof(RiftPolicyContext));
    if (!ctx) return NULL;
    
    ctx->result_matrix = rift_result_matrix_create(threshold);
    if (!ctx->result_matrix) {
        rift_free(ctx);
        return NULL;
    }
    
    ctx->enforce_immediate = immediate;
    ctx->policy_data = NULL;
    ctx->policy_name = policy_name ? strdup(policy_name) : strdup("default");
    ctx->policy_version = 1;
    
    return ctx;
}

RIFT_API void rift_policy_context_destroy(RiftPolicyContext* context) {
    if (!context) return;
    
    if (context->result_matrix) {
        rift_result_matrix_destroy(context->result_matrix);
    }
    
    if (context->policy_name) {
        rift_free(context->policy_name);
    }
    
    rift_free(context);
}

RIFT_API bool rift_policy_context_set_threshold(RiftPolicyContext* context, double new_threshold) {
    if (!context || !context->result_matrix) return false;
    if (new_threshold <= 0.0 || new_threshold > 1.0) return false;
    
    context->result_matrix->validation_threshold = new_threshold;
    return true;
}

/* ============================================================================
 * Parser Boundary Implementation
 * ============================================================================ */

RIFT_API RiftParserBoundary* rift_parser_boundary_create(void) {
    RiftParserBoundary* boundary = (RiftParserBoundary*)rift_malloc(sizeof(RiftParserBoundary));
    if (!boundary) return NULL;
    
    /* Set default no-op functions */
    boundary->validate_input = NULL;
    boundary->generate_tokens = NULL;
    boundary->validate_ast = NULL;
    boundary->on_parse_error = NULL;
    boundary->on_validation_error = NULL;
    boundary->policy_ctx = NULL;
    
    /* Initialize metrics */
    boundary->tokens_generated = 0;
    boundary->parse_errors = 0;
    boundary->parse_time_ms = 0.0;
    
    return boundary;
}

RIFT_API void rift_parser_boundary_destroy(RiftParserBoundary* boundary) {
    rift_free(boundary);
}

RIFT_API bool rift_parser_boundary_set_policy(
    RiftParserBoundary* boundary,
    RiftPolicyContext* policy
) {
    if (!boundary || !policy) return false;
    boundary->policy_ctx = policy;
    return true;
}

RIFT_API bool rift_parser_boundary_set_callbacks(
    RiftParserBoundary* boundary,
    bool (*validate_input)(const char*, size_t, RiftPolicyContext*),
    RiftToken** (*generate_tokens)(const char*, uint32_t*, RiftExecutionMode),
    bool (*validate_ast)(RiftAstNode*, RiftPolicyContext*),
    void (*on_error)(const char*, uint32_t, uint32_t, const char*)
) {
    if (!boundary) return false;
    
    boundary->validate_input = validate_input;
    boundary->generate_tokens = generate_tokens;
    boundary->validate_ast = validate_ast;
    boundary->on_parse_error = on_error;
    
    return true;
}

RIFT_API RiftAstNode* rift_parser_parse(
    RiftParserBoundary* boundary,
    const char* input,
    const char* filename
) {
    if (!boundary || !input) return NULL;
    
    double start_time = rift_get_time_ms();
    
    /* Validate input if callback provided */
    if (boundary->validate_input && boundary->policy_ctx) {
        if (!boundary->validate_input(input, strlen(input), boundary->policy_ctx)) {
            boundary->parse_errors++;
            return NULL;
        }
    }
    
    /* Generate tokens if callback provided */
    uint32_t token_count = 0;
    RiftToken** tokens = NULL;
    if (boundary->generate_tokens) {
        tokens = boundary->generate_tokens(input, &token_count, RIFT_MODE_CLASSICAL);
        boundary->tokens_generated += token_count;
    }
    
    /* Create root AST node */
    RiftAstNode* root = rift_ast_create_node(RIFT_AST_PROGRAM, NULL);
    if (root && filename) {
        root->source_file = strdup(filename);
    }
    
    /* Validate AST if callback provided */
    if (boundary->validate_ast && boundary->policy_ctx) {
        if (!boundary->validate_ast(root, boundary->policy_ctx)) {
            if (boundary->on_validation_error) {
                boundary->on_validation_error(RIFT_POLICY_DENY, boundary->policy_ctx->policy_name);
            }
            rift_ast_destroy_node(root);
            if (tokens) rift_free(tokens);
            return NULL;
        }
    }
    
    /* Clean up tokens (AST nodes own their tokens) */
    if (tokens) rift_free(tokens);
    
    boundary->parse_time_ms = rift_get_time_ms() - start_time;
    
    return root;
}

/* ============================================================================
 * AST Operations Implementation
 * ============================================================================ */

RIFT_API RiftAstNode* rift_ast_create_node(RiftAstNodeType type, RiftToken* token) {
    RiftAstNode* node = (RiftAstNode*)rift_malloc(sizeof(RiftAstNode));
    if (!node) return NULL;
    
    node->type = type;
    node->token = token;
    node->children = NULL;
    node->child_count = 0;
    node->parent = NULL;
    node->line = 0;
    node->column = 0;
    node->source_file = NULL;
    node->validated = false;
    node->policy_ctx = NULL;
    node->node_id = 0;
    node->schema_version = 1;
    
    return node;
}

RIFT_API void rift_ast_destroy_node(RiftAstNode* node) {
    if (!node) return;
    
    /* Recursively destroy children */
    for (uint32_t i = 0; i < node->child_count; i++) {
        rift_ast_destroy_node(node->children[i]);
    }
    
    rift_free(node->children);
    
    /* Note: We don't destroy the token - tokens have separate lifecycle */
    
    if (node->source_file) {
        rift_free(node->source_file);
    }
    
    rift_free(node);
}

RIFT_API bool rift_ast_add_child(RiftAstNode* parent, RiftAstNode* child) {
    if (!parent || !child) return false;
    
    /* Expand children array */
    RiftAstNode** new_children = (RiftAstNode**)realloc(
        parent->children,
        (parent->child_count + 1) * sizeof(RiftAstNode*)
    );
    if (!new_children) return false;
    
    parent->children = new_children;
    parent->children[parent->child_count] = child;
    child->parent = parent;
    parent->child_count++;
    
    return true;
}

RIFT_API bool rift_ast_remove_child(RiftAstNode* parent, uint32_t index) {
    if (!parent || index >= parent->child_count) return false;
    
    /* Shift remaining children */
    for (uint32_t i = index; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    
    parent->child_count--;
    
    /* Shrink array */
    if (parent->child_count > 0) {
        RiftAstNode** new_children = (RiftAstNode**)realloc(
            parent->children,
            parent->child_count * sizeof(RiftAstNode*)
        );
        if (new_children) {
            parent->children = new_children;
        }
    } else {
        rift_free(parent->children);
        parent->children = NULL;
    }
    
    return true;
}

RIFT_API bool rift_ast_validate(RiftAstNode* root, RiftPolicyContext* policy) {
    if (!root || !policy || !policy->result_matrix) return false;
    
    /* Validate current node */
    bool input_valid = (root->token != NULL && RIFT_TOKEN_IS_VALID(root->token));
    bool output_valid = (root->child_count > 0 || root->token != NULL);
    
    RiftPolicyResult result = rift_policy_validate(
        policy->result_matrix,
        input_valid,
        output_valid
    );
    
    if (result == RIFT_POLICY_DENY) {
        return false;
    }
    
    root->validated = true;
    root->policy_ctx = policy;
    
    /* Recursively validate children */
    for (uint32_t i = 0; i < root->child_count; i++) {
        if (!rift_ast_validate(root->children[i], policy)) {
            return false;
        }
    }
    
    return true;
}

RIFT_API RiftAstNode* rift_ast_find_node(RiftAstNode* root, RiftAstNodeType type, bool recursive) {
    if (!root) return NULL;
    
    if (root->type == type) {
        return root;
    }
    
    if (recursive) {
        for (uint32_t i = 0; i < root->child_count; i++) {
            RiftAstNode* found = rift_ast_find_node(root->children[i], type, true);
            if (found) return found;
        }
    }
    
    return NULL;
}

RIFT_API uint32_t rift_ast_count_nodes(RiftAstNode* root) {
    if (!root) return 0;
    
    uint32_t count = 1; /* Count self */
    for (uint32_t i = 0; i < root->child_count; i++) {
        count += rift_ast_count_nodes(root->children[i]);
    }
    
    return count;
}

RIFT_API void rift_ast_print(RiftAstNode* root, int indent) {
    if (!root) return;
    
    /* Print indentation */
    for (int i = 0; i < indent; i++) printf("  ");
    
    /* Print node info */
    printf("Node[%d] type=%d", root->node_id, root->type);
    if (root->token) {
        printf(" token_type=%s", rift_token_type_name(root->token->type));
    }
    printf(" children=%d", root->child_count);
    if (root->validated) {
        printf(" [VALIDATED]");
    }
    printf("\n");
    
    /* Print children */
    for (uint32_t i = 0; i < root->child_count; i++) {
        rift_ast_print(root->children[i], indent + 1);
    }
}

/* ============================================================================
 * Serialization Implementation (Stubs - Full implementation in Phase 2)
 * ============================================================================ */

RIFT_API char* rift_ast_serialize(RiftAstNode* root, RiftSerialOptions* options, size_t* output_size) {
    /* TODO: Full JSON and binary serialization implementation */
    /* This stub returns a minimal valid JSON object */
    
    if (!root || !output_size) return NULL;
    
    const char* stub = "{\"ast\":\"stub\",\"version\":1}";
    size_t len = strlen(stub);
    
    char* output = (char*)rift_malloc(len + 1);
    if (output) {
        strcpy(output, stub);
        *output_size = len;
    }
    
    return output;
}

RIFT_API RiftAstNode* rift_ast_deserialize(const char* data, size_t data_len, RiftSerialOptions* options) {
    /* TODO: Full deserialization from JSON and binary formats */
    (void)data;
    (void)data_len;
    (void)options;
    return NULL;
}