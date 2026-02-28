/**
 * node-rift/target/pattern_engine.cjs
 * Polar Bipartite Pattern Matching Engine - Node.js Implementation
 */

'use strict';

// Pattern polarity
const POLARITY = {
    LEFT: 0,   // Input/matcher
    RIGHT: 1   // Output/generator
};

// ============================================================================
// RiftPattern Class
// ============================================================================

class RiftPattern {
    constructor(patternStr, polarity, priority, anchored, isLiteral) {
        this.patternStr = patternStr;
        this.polarity = polarity;
        this.priority = priority;
        this.anchored = anchored;
        this.isLiteral = isLiteral;
        this.compiledRegex = null;
    }
}

// ============================================================================
// BipartitePair Class
// ============================================================================

class BipartitePair {
    constructor(left, right, transformFn = null, isGoverned = false, transformId = 0) {
        this.left = left;
        this.right = right;
        this.transformFn = transformFn;
        this.isGoverned = isGoverned;
        this.transformId = transformId;
    }
}

// ============================================================================
// MatchResult Class
// ============================================================================

class MatchResult {
    constructor(matched, output = null, priority = 0, transformId = 0, groups = {}) {
        this.matched = matched;
        this.output = output;
        this.priority = priority;
        this.transformId = transformId;
        this.groups = groups;
    }
}

// ============================================================================
// PatternEngine Class
// ============================================================================

class PatternEngine {
    constructor(mode = 'classical') {
        this.pairs = [];
        this.mode = mode;

        // Metrics
        this.totalMatches = 0;
        this.totalFailures = 0;
        this.averageMatchTimeMs = 0.0;
    }

    /**
     * Add a bipartite pattern pair
     */
    addPair(leftPattern, rightPattern, priority = 100, rightIsLiteral = false) {
        // Create left pattern (input matcher)
        const left = new RiftPattern(
            leftPattern,
            POLARITY.LEFT,
            priority,
            leftPattern.startsWith('^'),
            false
        );

        // Compile left regex
        try {
            left.compiledRegex = new RegExp(leftPattern);
        } catch (error) {
            return false;
        }

        // Create right pattern (output generator)
        const right = new RiftPattern(
            rightPattern,
            POLARITY.RIGHT,
            priority,
            false,
            rightIsLiteral
        );

        // Compile right if it's a regex
        if (!rightIsLiteral) {
            try {
                right.compiledRegex = new RegExp(rightPattern);
            } catch (error) {
                right.isLiteral = true;
            }
        }

        // Create pair
        const pair = new BipartitePair(
            left,
            right,
            null,
            false,
            this.pairs.length + 1
        );

        this.pairs.push(pair);
        return true;
    }

    /**
     * Match input against all left patterns, return best match
     */
    match(inputStr) {
        const startTime = process.hrtime.bigint();

        let bestPair = null;
        let bestPriority = Infinity;
        let bestMatch = null;
        let bestGroups = {};

        // Search for matching pattern (respecting priority)
        for (const pair of this.pairs) {
            if (pair.left.compiledRegex) {
                const match = inputStr.match(pair.left.compiledRegex);
                if (match) {
                    // Lower priority number = higher priority
                    if (pair.left.priority < bestPriority) {
                        bestPair = pair;
                        bestPriority = pair.left.priority;
                        bestMatch = match;
                        bestGroups = match.groups || {};
                    }
                }
            }
        }

        // Generate output
        if (bestPair) {
            let template = bestPair.right.patternStr;
            let output = template;

            if (bestPair.right.isLiteral) {
                output = template;
            } else {
                // Simple substitution for capture groups
                if (bestMatch) {
                    for (let i = 1; i < bestMatch.length; i++) {
                        output = output.replace(new RegExp(`\\$${i}`, 'g'), bestMatch[i] || '');
                    }
                    // Named groups
                    for (const [name, value] of Object.entries(bestGroups)) {
                        output = output.replace(new RegExp(`\\{${name}\\}`, 'g'), value || '');
                    }
                }
            }

            // Update metrics
            const elapsed = Number(process.hrtime.bigint() - startTime) / 1000000; // Convert to ms
            this.totalMatches++;
            this._updateMetrics(elapsed);

            return new MatchResult(
                true,
                output,
                bestPriority,
                bestPair.transformId,
                bestGroups
            );
        }

        // No match found
        const elapsed = Number(process.hrtime.bigint() - startTime) / 1000000;
        this.totalFailures++;
        this._updateMetrics(elapsed);

        return new MatchResult(false);
    }

    /**
     * Update running average match time
     */
    _updateMetrics(elapsedMs) {
        const total = this.totalMatches + this.totalFailures;
        if (total > 0) {
            this.averageMatchTimeMs = (
                (this.averageMatchTimeMs * (total - 1)) + elapsedMs
            ) / total;
        }
    }

    /**
     * Get engine metrics
     */
    getMetrics() {
        return {
            totalMatches: this.totalMatches,
            totalFailures: this.totalFailures,
            averageMatchTimeMs: this.averageMatchTimeMs,
            pairCount: this.pairs.length
        };
    }
}

// ============================================================================
// Default JavaScript Patterns
// ============================================================================

const DEFAULT_JS_PATTERNS = [
    // Const declaration transformation
    [/const\s+(\w+)\s*=\s*(.+)/, "rift.const('$1', $2)", 100],
    // Function declaration
    [/function\s+(\w+)\s*\(\s*\)/, "rift.function('$1', function()", 90],
    // Arrow function
    [/const\s+(\w+)\s*=\s*\(\)\s*=>/, "rift.function('$1', () =>", 85],
    // Async function
    [/async\s+function/, "rift.async(function", 80],
    // Class declaration
    [/class\s+(\w+)/, "rift.class('$1', class $1", 70],
    // Quantum decorator
    [/@quantum/, "@rift.quantumDecorator", 200],
];

/**
 * Create pattern engine with default JavaScript transformations
 */
function createDefaultEngine() {
    const engine = new PatternEngine();
    for (const [left, right, priority] of DEFAULT_JS_PATTERNS) {
        engine.addPair(left, right, priority, true);
    }
    return engine;
}

// ============================================================================
// Module Exports
// ============================================================================

module.exports = {
    POLARITY,
    RiftPattern,
    BipartitePair,
    MatchResult,
    PatternEngine,
    DEFAULT_JS_PATTERNS,
    createDefaultEngine
};
