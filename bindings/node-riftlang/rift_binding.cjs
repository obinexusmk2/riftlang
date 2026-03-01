/**
 * node-rift/target/rift_binding.cjs
 * Node.js binding for RiftLang - Generated from .rift policies
 * Governance: classic mode with memory-first enforcement
 */

'use strict';

// Rift validation bits (mirrored from riftlang.h)
const TOKEN_BITS = {
    ALLOCATED:   0x01,
    INITIALIZED: 0x02,
    LOCKED:      0x04,
    GOVERNED:    0x08,
    SUPERPOSED:  0x10,
    ENTANGLED:   0x20,
    PERSISTENT:  0x40,
    SHADOW:      0x80
};

// Memory alignment constants
const CLASSICAL_ALIGNMENT = 4096;
const QUANTUM_ALIGNMENT = 8;
const DEFAULT_THRESHOLD = 0.85;

// ============================================================================
// Memory Governance
// ============================================================================

/**
 * Memory span - declared BEFORE type or value per Rift spec
 */
class RiftMemorySpan {
    constructor(spanType = 'fixed', bytes = 4096) {
        this.spanType = spanType;
        this.bytes = bytes;
        this.alignment = CLASSICAL_ALIGNMENT;
        this.open = true;
        this.direction = true;  // true = right->left
        this.accessMask = 0x0F; // CREATE|READ|UPDATE|DELETE
    }

    /**
     * Validate alignment is power of 2
     */
    validateAlignment() {
        const align = this.alignment;
        return align > 0 && (align & (align - 1)) === 0;
    }
}

// ============================================================================
// Token Structure - The Triplet Model
// ============================================================================

/**
 * The Token Triplet: (type, value, memory) with governance
 * Memory is declared FIRST, then type, then value
 */
class RiftToken {
    constructor(tokenType, memory) {
        // Core triplet - memory declared first per Rift spec
        this.type = tokenType;
        this.memory = memory;
        this._value = undefined;

        // Governance fields
        this.validationBits = TOKEN_BITS.ALLOCATED;
        this._lock = false;
        this._lockOwner = null;

        // Quantum fields (initialized when needed)
        this._superposedStates = null;
        this._amplitudes = null;
        this._phase = 0.0;

        // Entanglement fields
        this._entangledWith = null;
        this._entanglementId = 0;

        // Source location
        this.sourceLine = 0;
        this.sourceColumn = 0;
        this.sourceFile = null;
    }

    /**
     * Get value with validation check
     */
    get value() {
        if (!(this.validationBits & TOKEN_BITS.INITIALIZED)) {
            throw new Error('Token value not initialized');
        }
        return this._value;
    }

    /**
     * Set value with immediate binding (classic mode)
     */
    set value(val) {
        this._value = val;
        this.validationBits |= TOKEN_BITS.INITIALIZED;
    }

    /**
     * Acquire token lock for thread safety
     */
    lock() {
        if (!this._lock) {
            this._lock = true;
            this._lockOwner = process.hrtime.bigint();
            this.validationBits |= TOKEN_BITS.LOCKED;
            return true;
        }
        return false;
    }

    /**
     * Release token lock
     */
    unlock() {
        if (this._lock) {
            this._lock = false;
            this._lockOwner = null;
            this.validationBits &= ~TOKEN_BITS.LOCKED;
            return true;
        }
        return false;
    }

    /**
     * Validate token against governance policy
     */
    validate() {
        // Check ALLOCATED bit
        if (!(this.validationBits & TOKEN_BITS.ALLOCATED)) {
            return false;
        }

        // Memory span must exist and be valid
        if (!this.memory || this.memory.alignment === 0) {
            return false;
        }

        // Validate alignment
        if (!this.memory.validateAlignment()) {
            return false;
        }

        // Mark as governed
        this.validationBits |= TOKEN_BITS.GOVERNED;
        return true;
    }

    /**
     * Put token into quantum superposition
     */
    superpose(states, amplitudes = null) {
        if (!states || states.length === 0) {
            return false;
        }

        this._superposedStates = states;
        this._amplitudes = amplitudes || [];
        if (this._amplitudes.length === 0) {
            const prob = 1.0 / states.length;
            for (let i = 0; i < states.length; i++) {
                this._amplitudes.push(Math.sqrt(prob));
            }
        }

        this.validationBits |= TOKEN_BITS.SUPERPOSED;
        return true;
    }

    /**
     * Create entanglement with another token
     */
    entangleWith(other, entanglementId) {
        if (!this._entangledWith) {
            this._entangledWith = [];
        }
        this._entangledWith.push(other);
        this._entanglementId = entanglementId;
        this.validationBits |= TOKEN_BITS.ENTANGLED;
        return true;
    }

    /**
     * Collapse superposition to single state
     */
    collapse(selectedIndex = 0) {
        if (!(this.validationBits & TOKEN_BITS.SUPERPOSED)) {
            return false;
        }

        if (this._superposedStates && selectedIndex < this._superposedStates.length) {
            const collapsed = this._superposedStates[selectedIndex];
            this._value = collapsed._value;
            this.type = collapsed.type;
            this._superposedStates = null;
            this._amplitudes = null;
            this.validationBits &= ~TOKEN_BITS.SUPERPOSED;
            return true;
        }
        return false;
    }

    /**
     * Check if token is valid and governed
     */
    isValid() {
        return (this.validationBits & TOKEN_BITS.INITIALIZED) !== 0 &&
               (this.validationBits & TOKEN_BITS.GOVERNED) !== 0;
    }

    /**
     * String representation
     */
    toString() {
        return `RiftToken(type=${this.type}, governed=${this.isValid()})`;
    }
}

// ============================================================================
// Rift Object Base Class
// ============================================================================

/**
 * Base class for JavaScript objects governed by Rift policies
 */
class RiftObject {
    constructor() {
        // Memory-first declaration
        this._memory = new RiftMemorySpan('fixed', 4096);
        this._token = new RiftToken('JSObject', this._memory);
        this._token.validate();
    }

    async [Symbol.asyncDispose]() {
        this._token.unlock();
    }
}

// ============================================================================
// Quantum Functions
// ============================================================================

/**
 * Create a superposed token from multiple states
 */
function superpose(...states) {
    const memory = new RiftMemorySpan('superposed', 64);
    memory.alignment = QUANTUM_ALIGNMENT;

    const token = new RiftToken('QJSObject', memory);

    // Create child tokens for each state
    const stateTokens = states.map(state => {
        const stateMemory = new RiftMemorySpan('fixed', 64);
        const stateToken = new RiftToken('JSObject', stateMemory);
        stateToken.value = state;
        return stateToken;
    });

    token.superpose(stateTokens);
    return token;
}

/**
 * Entangle two tokens
 */
function entangle(a, b) {
    const entanglementId = Math.floor(Math.random() * 1000000);
    a.entangleWith(b, entanglementId);
    b.entangleWith(a, entanglementId);
    return entanglementId;
}

/**
 * Collapse a superposed token
 */
function collapse(token, selectedIndex = 0) {
    return token.collapse(selectedIndex);
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Safe token access pattern
 */
async function withToken(token, fn) {
    if (token.lock()) {
        try {
            const result = await fn(token);
            token.unlock();
            return result;
        } catch (error) {
            token.unlock();
            throw error;
        }
    }
    throw new Error('Failed to acquire token lock');
}

/**
 * Rift-governed const declaration
 */
function const_(name, value) {
    const memory = new RiftMemorySpan('fixed', 64);
    const token = new RiftToken('JSObject', memory);
    token.value = value;
    token.validate();
    return token;
}

/**
 * Rift-governed function declaration
 */
function function_(name, fn) {
    const memory = new RiftMemorySpan('row', 4096);
    const token = new RiftToken('JSFunction', memory);
    token.value = fn;
    token.validate();
    return token;
}

/**
 * Rift-governed async function
 */
function async_(fn) {
    const memory = new RiftMemorySpan('row', 4096);
    const token = new RiftToken('JSFunction', memory);
    token.value = async (...args) => {
        return await fn(...args);
    };
    token.validate();
    return token;
}

// ============================================================================
// Module Exports
// ============================================================================

module.exports = {
    // Constants
    TOKEN_BITS,
    CLASSICAL_ALIGNMENT,
    QUANTUM_ALIGNMENT,
    DEFAULT_THRESHOLD,

    // Classes
    RiftMemorySpan,
    RiftToken,
    RiftObject,

    // Quantum functions
    superpose,
    entangle,
    collapse,

    // Utility functions
    withToken,
    const: const_,
    function: function_,
    async: async_
};
