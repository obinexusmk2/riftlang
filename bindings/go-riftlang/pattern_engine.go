// go/target/pattern_engine.go
// Polar Bipartite Pattern Matching Engine - Go Implementation

package rift

import (
	"fmt"
	"regexp"
	"sync"
	"time"
)

// PatternPolarity defines the role of a pattern in bipartite matching
type PatternPolarity int

const (
	PatternLeft  PatternPolarity = 0 // Input/matcher
	PatternRight PatternPolarity = 1 // Output/generator
)

// ============================================================================
// RiftPattern
// ============================================================================

// RiftPattern represents a compiled pattern with metadata
type RiftPattern struct {
	PatternStr     string
	CompiledRegex  *regexp.Regexp
	Polarity       PatternPolarity
	Priority       uint32
	Anchored       bool
	IsLiteral      bool
}

// ============================================================================
// BipartitePair
// ============================================================================

// BipartitePair links input pattern (left) to output pattern (right)
type BipartitePair struct {
	Left        *RiftPattern
	Right       *RiftPattern
	TransformFn func(string) string
	IsGoverned  bool
	TransformID uint32
}

// ============================================================================
// MatchResult
// ============================================================================

// MatchResult represents the result of pattern matching
type MatchResult struct {
	Matched     bool
	Output      string
	Priority    uint32
	TransformID uint32
	Groups      map[string]string
}

// ============================================================================
// PatternEngine
// ============================================================================

// PatternEngine manages all pattern pairs and compilation cache
type PatternEngine struct {
	pairs               []*BipartitePair
	mode                string
	lock                sync.RWMutex
	totalMatches        uint64
	totalFailures       uint64
	averageMatchTimeMs  float64
}

// NewPatternEngine creates a new pattern engine
func NewPatternEngine(mode string) *PatternEngine {
	if mode == "" {
		mode = "classical"
	}
	return &PatternEngine{
		pairs: make([]*BipartitePair, 0),
		mode:  mode,
	}
}

// AddPair adds a bipartite pattern pair
func (e *PatternEngine) AddPair(leftPattern, rightPattern string, priority uint32, rightIsLiteral bool) bool {
	e.lock.Lock()
	defer e.lock.Unlock()

	// Create left pattern (input matcher)
	left := &RiftPattern{
		PatternStr: leftPattern,
		Polarity:   PatternLeft,
		Priority:   priority,
		Anchored:   len(leftPattern) > 0 && leftPattern[0] == '^',
		IsLiteral:  false,
	}

	// Compile left regex
	compiled, err := regexp.Compile(leftPattern)
	if err != nil {
		return false
	}
	left.CompiledRegex = compiled

	// Create right pattern (output generator)
	right := &RiftPattern{
		PatternStr: rightPattern,
		Polarity:   PatternRight,
		Priority:   priority,
		Anchored:   false,
		IsLiteral:  rightIsLiteral,
	}

	// Compile right if it's not a literal
	if !rightIsLiteral {
		compiled, err := regexp.Compile(rightPattern)
		if err != nil {
			right.IsLiteral = true
		} else {
			right.CompiledRegex = compiled
		}
	}

	// Create pair
	pair := &BipartitePair{
		Left:        left,
		Right:       right,
		TransformFn: nil,
		IsGoverned:  false,
		TransformID: uint32(len(e.pairs) + 1),
	}

	e.pairs = append(e.pairs, pair)
	return true
}

// Match matches input against all left patterns, returns best match
func (e *PatternEngine) Match(input string) *MatchResult {
	startTime := time.Now()

	e.lock.RLock()
	defer e.lock.RUnlock()

	var bestPair *BipartitePair
	var bestPriority uint32 = ^uint32(0) // Max uint32
	var bestMatch []string
	var bestGroups map[string]string

	// Search for matching pattern (respecting priority)
	for _, pair := range e.pairs {
		if pair.Left.CompiledRegex == nil {
			continue
		}

		// Check priority - lower number = higher priority
		if pair.Left.Priority > bestPriority {
			continue
		}

		// Try to match input against left pattern
		matches := pair.Left.CompiledRegex.FindStringSubmatch(input)
		if matches != nil {
			bestPair = pair
			bestPriority = pair.Left.Priority
			bestMatch = matches
			bestGroups = make(map[string]string)

			// Extract named groups
			for i, name := range pair.Left.CompiledRegex.SubexpNames() {
				if i > 0 && i < len(matches) && name != "" {
					bestGroups[name] = matches[i]
				}
			}
		}
	}

	// Generate output
	if bestPair != nil {
		template := bestPair.Right.PatternStr
		output := template

		if !bestPair.Right.IsLiteral {
			// Substitute capture groups
			for i, match := range bestMatch {
				if i > 0 {
					placeholder := fmt.Sprintf("$%d", i)
					output = regexp.MustCompile(regexp.QuoteMeta(placeholder)).
						ReplaceAllString(output, match)
				}
			}
			// Substitute named groups
			for name, value := range bestGroups {
				placeholder := fmt.Sprintf("{%s}", name)
				output = regexp.MustCompile(regexp.QuoteMeta(placeholder)).
					ReplaceAllString(output, value)
			}
		}

		// Update metrics
		elapsed := float64(time.Since(startTime).Nanoseconds()) / 1000000.0
		e.totalMatches++
		e.updateMetrics(elapsed)

		return &MatchResult{
			Matched:     true,
			Output:      output,
			Priority:    bestPriority,
			TransformID: bestPair.TransformID,
			Groups:      bestGroups,
		}
	}

	// No match found
	elapsed := float64(time.Since(startTime).Nanoseconds()) / 1000000.0
	e.totalFailures++
	e.updateMetrics(elapsed)

	return &MatchResult{Matched: false}
}

// updateMetrics updates running average match time
func (e *PatternEngine) updateMetrics(elapsedMs float64) {
	total := e.totalMatches + e.totalFailures
	if total > 0 {
		e.averageMatchTimeMs = ((e.averageMatchTimeMs * float64(total-1)) + elapsedMs) / float64(total)
	}
}

// GetMetrics returns engine metrics
func (e *PatternEngine) GetMetrics() map[string]interface{} {
	e.lock.RLock()
	defer e.lock.RUnlock()

	return map[string]interface{}{
		"totalMatches":       e.totalMatches,
		"totalFailures":      e.totalFailures,
		"averageMatchTimeMs": e.averageMatchTimeMs,
		"pairCount":          len(e.pairs),
	}
}

// GetPairCount returns the number of pattern pairs
func (e *PatternEngine) GetPairCount() int {
	e.lock.RLock()
	defer e.lock.RUnlock()
	return len(e.pairs)
}

// ============================================================================
// Default Patterns
// ============================================================================

// DefaultGoPatterns contains default Go code transformation patterns
var DefaultGoPatterns = []struct {
	Left           string
	Right          string
	Priority       uint32
	RightIsLiteral bool
}{
	// Variable declaration transformation
	{`var\s+(\w+)\s+(\w+)`, `riftVar$2("$1")`, 100, true},
	// Function declaration
	{`func\s+(\w+)\s*\(\s*\)`, `riftFunc("$1", func()`, 90, true},
	// Goroutine
	{`go\s+(\w+)\s*\(\)`, `riftGo(riftFunc("$1", func()`, 85, true},
	// Const declaration
	{`const\s+(\w+)\s*=\s*(.+)`, `riftConst("$1", $2)`, 80, true},
	// Type declaration
	{`type\s+(\w+)\s+struct`, `riftStruct("$1", struct`, 70, true},
	// Quantum decorator
	{`@quantum`, `@riftQuantumDecorator`, 200, true},
}

// CreateDefaultEngine creates a pattern engine with default Go transformations
func CreateDefaultEngine() *PatternEngine {
	engine := NewPatternEngine("classical")
	for _, pattern := range DefaultGoPatterns {
		engine.AddPair(pattern.Left, pattern.Right, pattern.Priority, pattern.RightIsLiteral)
	}
	return engine
}
