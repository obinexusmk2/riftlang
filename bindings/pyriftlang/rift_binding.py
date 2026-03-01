# pyrift/targets/rift_binding.py
# Python binding for RiftLang - Generated from .rift policies
# Governance: classic mode with memory-first enforcement

from __future__ import annotations
import ctypes
import threading
from typing import Any, Optional, List, Dict, Union
from dataclasses import dataclass
from enum import IntEnum

# Rift validation bits (mirrored from riftlang.h)
class RiftTokenBits(IntEnum):
    ALLOCATED = 0x01
    INITIALIZED = 0x02
    LOCKED = 0x04
    GOVERNED = 0x08
    SUPERPOSED = 0x10
    ENTANGLED = 0x20
    PERSISTENT = 0x40
    SHADOW = 0x80

# Memory alignment constants
RIFT_CLASSICAL_ALIGNMENT = 4096
RIFT_QUANTUM_ALIGNMENT = 8
RIFT_DEFAULT_THRESHOLD = 0.85


@dataclass
class RiftMemorySpan:
    """Memory governance span - declared BEFORE type or value"""
    span_type: str  # fixed, row, continuous, superposed, entangled
    bytes: int
    alignment: int = RIFT_CLASSICAL_ALIGNMENT
    open: bool = True
    direction: bool = True  # True = right->left
    access_mask: int = 0x0F  # CREATE|READ|UPDATE|DELETE
    
    def validate_alignment(self) -> bool:
        """Validate alignment is power of 2"""
        return self.alignment > 0 and (self.alignment & (self.alignment - 1)) == 0


class RiftToken:
    """
    RiftLang Token Triplet: (type, value, memory) with governance
    Memory is declared FIRST, then type, then value
    """
    
    def __init__(self, token_type: str, memory: RiftMemorySpan):
        # Core triplet - memory declared first per Rift spec
        self.type = token_type
        self.memory = memory
        self._value: Any = None
        
        # Governance fields
        self.validation_bits = RiftTokenBits.ALLOCATED
        self._lock = threading.RLock()
        self._lock_count = 0
        
        # Quantum fields (initialized when needed)
        self._superposed_states: Optional[List['RiftToken']] = None
        self._amplitudes: Optional[List[float]] = None
        self._phase: float = 0.0
        
        # Entanglement fields
        self._entangled_with: Optional[List['RiftToken']] = None
        self._entanglement_id: int = 0
        
        # Source location
        self.source_line: int = 0
        self.source_column: int = 0
        self.source_file: Optional[str] = None
    
    @property
    def value(self) -> Any:
        """Get value with validation check"""
        if not self.validation_bits & RiftTokenBits.INITIALIZED:
            raise RuntimeError("Token value not initialized")
        return self._value
    
    @value.setter
    def value(self, val: Any) -> None:
        """Set value with immediate binding (classic mode)"""
        with self._lock:
            self._value = val
            self.validation_bits |= RiftTokenBits.INITIALIZED
    
    def lock(self) -> bool:
        """Acquire token lock for thread safety"""
        if self._lock.acquire(blocking=False):
            self._lock_count += 1
            self.validation_bits |= RiftTokenBits.LOCKED
            return True
        return False
    
    def unlock(self) -> bool:
        """Release token lock"""
        if self._lock_count > 0:
            self._lock_count -= 1
            if self._lock_count == 0:
                self.validation_bits &= ~RiftTokenBits.LOCKED
            self._lock.release()
            return True
        return False
    
    def validate(self) -> bool:
        """Validate token against governance policy"""
        # Check ALLOCATED bit
        if not self.validation_bits & RiftTokenBits.ALLOCATED:
            return False
        
        # Memory span must exist and be valid
        if not self.memory or self.memory.alignment == 0:
            return False
        
        # Validate alignment
        if not self.memory.validate_alignment():
            return False
        
        # Mark as governed
        self.validation_bits |= RiftTokenBits.GOVERNED
        return True
    
    def superpose(self, states: List['RiftToken'], amplitudes: Optional[List[float]] = None) -> bool:
        """Put token into quantum superposition"""
        if not states:
            return False
        
        self._superposed_states = states
        self._amplitudes = amplitudes or [1.0 / len(states)] * len(states)
        self.validation_bits |= RiftTokenBits.SUPERPOSED
        return True
    
    def entangle_with(self, other: 'RiftToken', entanglement_id: int) -> bool:
        """Create entanglement with another token"""
        if self._entangled_with is None:
            self._entangled_with = []
        self._entangled_with.append(other)
        self._entanglement_id = entanglement_id
        self.validation_bits |= RiftTokenBits.ENTANGLED
        return True
    
    def collapse(self, selected_index: int = 0) -> bool:
        """Collapse superposition to single state"""
        if not self.validation_bits & RiftTokenBits.SUPERPOSED:
            return False
        
        if self._superposed_states and 0 <= selected_index < len(self._superposed_states):
            collapsed = self._superposed_states[selected_index]
            self._value = collapsed._value
            self.type = collapsed.type
            self._superposed_states = None
            self._amplitudes = None
            self.validation_bits &= ~RiftTokenBits.SUPERPOSED
            return True
        return False
    
    def is_valid(self) -> bool:
        """Check if token is valid and governed"""
        return (self.validation_bits & RiftTokenBits.INITIALIZED and
                self.validation_bits & RiftTokenBits.GOVERNED)
    
    def __repr__(self) -> str:
        return f"RiftToken(type={self.type}, governed={self.is_valid()})"


class RiftObject:
    """Base class for Python objects governed by Rift policies"""
    
    def __init__(self):
        # Memory-first declaration
        self._memory = RiftMemorySpan(
            span_type="fixed",
            bytes=4096,
            alignment=RIFT_CLASSICAL_ALIGNMENT
        )
        self._token = RiftToken("PyObject", self._memory)
        self._token.validate()
    
    def __enter__(self):
        self._token.lock()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self._token.unlock()
        return False


def rift_quantum_decorator(func):
    """Decorator for quantum mode functions"""
    def wrapper(*args, **kwargs):
        # Switch to quantum mode
        result = func(*args, **kwargs)
        # Auto-collapse if needed
        return result
    return wrapper


def rift_superpose(states: List[Any]) -> RiftToken:
    """Create a superposed token from multiple states"""
    memory = RiftMemorySpan(
        span_type="superposed",
        bytes=64,
        alignment=RIFT_QUANTUM_ALIGNMENT
    )
    token = RiftToken("QPyObject", memory)

    # Create child tokens for each state
    state_tokens = []
    for state in states:
        state_memory = RiftMemorySpan(span_type="fixed", bytes=64)
        state_token = RiftToken("PyObject", state_memory)
        state_token.value = state
        state_tokens.append(state_token)

    token.superpose(state_tokens)
    return token


def validate(value: Any) -> None:
    """Validate and display a governed value.

    Called from generated .py output as: rift.validate(varname)
    """
    print(f"rift.validate: {value}")
