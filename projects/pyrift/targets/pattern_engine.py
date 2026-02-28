# pyrift/targets/pattern_engine.py
# Polar Bipartite Pattern Matching Engine - Python Implementation

import re
import threading
from typing import List, Optional, Tuple, Dict, Callable
from dataclasses import dataclass, field
from enum import IntEnum


class PatternPolarity(IntEnum):
    LEFT = 0   # Input/matcher
    RIGHT = 1  # Output/generator


@dataclass
class RiftPattern:
    """Compiled pattern with metadata"""
    pattern_str: str
    polarity: PatternPolarity
    priority: int
    anchored: bool
    is_literal: bool
    compiled_regex: Optional[re.Pattern] = None


@dataclass
class BipartitePair:
    """Links left pattern (input) to right pattern (output)"""
    left: RiftPattern
    right: RiftPattern
    transform_fn: Optional[Callable] = None
    is_governed: bool = False
    transform_id: int = 0


@dataclass
class MatchResult:
    """Result of pattern matching"""
    matched: bool
    output: Optional[str] = None
    priority: int = 0
    transform_id: int = 0
    groups: Dict[str, str] = field(default_factory=dict)


class PatternEngine:
    """
    Polar Bipartite Pattern Matching Engine
    Left patterns match input using regex
    Right patterns generate output from templates
    """
    
    def __init__(self, mode: str = "classical"):
        self.pairs: List[BipartitePair] = []
        self.mode = mode
        self._lock = threading.RLock()
        
        # Metrics
        self.total_matches = 0
        self.total_failures = 0
        self.average_match_time_ms = 0.0
    
    def add_pair(self, left_pattern: str, right_pattern: str, 
                 priority: int = 100, right_is_literal: bool = False) -> bool:
        """Add a bipartite pattern pair"""
        with self._lock:
            # Create left pattern (input matcher)
            left = RiftPattern(
                pattern_str=left_pattern,
                polarity=PatternPolarity.LEFT,
                priority=priority,
                anchored=left_pattern.startswith('^'),
                is_literal=False
            )
            
            # Compile left regex
            try:
                left.compiled_regex = re.compile(left_pattern)
            except re.error:
                return False
            
            # Create right pattern (output generator)
            right = RiftPattern(
                pattern_str=right_pattern,
                polarity=PatternPolarity.RIGHT,
                priority=priority,
                anchored=False,
                is_literal=right_is_literal
            )
            
            # Compile right if it's a regex
            if not right_is_literal:
                try:
                    right.compiled_regex = re.compile(right_pattern)
                except re.error:
                    right.is_literal = True
            
            # Create pair
            pair = BipartitePair(
                left=left,
                right=right,
                transform_id=len(self.pairs) + 1
            )
            
            self.pairs.append(pair)
            return True
    
    def match(self, input_str: str) -> MatchResult:
        """Match input against all left patterns, return best match"""
        import time
        
        start_time = time.time()
        
        with self._lock:
            best_pair: Optional[BipartitePair] = None
            best_priority = float('inf')
            best_match = None
            
            # Search for matching pattern (respecting priority)
            for pair in self.pairs:
                if pair.left.compiled_regex:
                    match = pair.left.compiled_regex.search(input_str)
                    if match:
                        # Lower priority number = higher priority
                        if pair.left.priority < best_priority:
                            best_pair = pair
                            best_priority = pair.left.priority
                            best_match = match
            
            # Generate output
            if best_pair:
                template = best_pair.right.pattern_str
                
                if best_pair.right.is_literal:
                    output = template
                else:
                    # Simple substitution for capture groups
                    output = template
                    if best_match:
                        for i, group in enumerate(best_match.groups() or [], 1):
                            output = output.replace(f'${i}', group or '')
                        # Named groups
                        for name, value in (best_match.groupdict() or {}).items():
                            output = output.replace(f'{{{name}}}', value or '')
                
                # Update metrics
                elapsed = (time.time() - start_time) * 1000
                self.total_matches += 1
                self._update_metrics(elapsed)
                
                return MatchResult(
                    matched=True,
                    output=output,
                    priority=best_priority,
                    transform_id=best_pair.transform_id,
                    groups=best_match.groupdict() if best_match else {}
                )
            
            # No match found
            elapsed = (time.time() - start_time) * 1000
            self.total_failures += 1
            self._update_metrics(elapsed)
            
            return MatchResult(matched=False)
    
    def _update_metrics(self, elapsed_ms: float) -> None:
        """Update running average match time"""
        total = self.total_matches + self.total_failures
        if total > 0:
            self.average_match_time_ms = (
                (self.average_match_time_ms * (total - 1)) + elapsed_ms
            ) / total
    
    def get_metrics(self) -> Dict[str, any]:
        """Get engine metrics"""
        with self._lock:
            return {
                'total_matches': self.total_matches,
                'total_failures': self.total_failures,
                'average_match_time_ms': self.average_match_time_ms,
                'pair_count': len(self.pairs)
            }


# Example usage patterns for Python code generation
DEFAULT_PYTHON_PATTERNS = [
    # Function definition transformation
    (r'def\s+(\w+)\s*\(\s*\)\s*:', r'def \1(self):', 100),
    # Class inheritance
    (r'class\s+(\w+)\s*:', r'class \1(RiftObject):', 90),
    # Variable annotation
    (r'(\w+)\s*:=\s*(.+)', r'self.\1 = \2', 80),
    # Quantum decorator
    (r'@quantum', r'@rift_quantum_decorator', 200),
]


def create_default_engine() -> PatternEngine:
    """Create pattern engine with default Python transformations"""
    engine = PatternEngine()
    for left, right, priority in DEFAULT_PYTHON_PATTERNS:
        engine.add_pair(left, right, priority, right_is_literal=True)
    return engine
