-- luarift/targets/rift_binding.lua
-- Lua binding for RiftLang - Generated from .rift policies
-- Governance: classic mode with memory-first enforcement
-- Compatible with Lua 5.1/5.2/5.3/5.4

local rift = {}

-- ============================================================================
-- Bitwise Compatibility Layer (works on Lua 5.1/5.2/5.3/5.4 and LuaJIT)
-- ============================================================================
-- These functions avoid the '&' / '|' / '~' tokens which are not valid syntax
-- in Lua < 5.3.  They are implemented using pure-integer arithmetic and are
-- exact for the 8-bit validation-bit flags used by RiftLang.

local function _band(a, b)
    -- Bitwise AND of non-negative integers
    local r, m = 0, 1
    a, b = math.floor(a), math.floor(b)
    while a > 0 and b > 0 do
        if a % 2 == 1 and b % 2 == 1 then r = r + m end
        a, b, m = math.floor(a / 2), math.floor(b / 2), m * 2
    end
    return r
end

local function _bor(a, b)
    -- Bitwise OR of non-negative integers
    local r, m = 0, 1
    a, b = math.floor(a), math.floor(b)
    while a > 0 or b > 0 do
        if a % 2 == 1 or b % 2 == 1 then r = r + m end
        a, b, m = math.floor(a / 2), math.floor(b / 2), m * 2
    end
    return r
end

local function _band_not(a, mask)
    -- Equivalent to (a & ~mask): clear bits in 'a' that are set in 'mask'.
    -- Identity: a - (a & mask) clears exactly those bits.
    return a - _band(a, mask)
end

-- ============================================================================

-- Rift validation bits
rift.TOKEN_BITS = {
    ALLOCATED   = 0x01,
    INITIALIZED = 0x02,
    LOCKED      = 0x04,
    GOVERNED    = 0x08,
    SUPERPOSED  = 0x10,
    ENTANGLED   = 0x20,
    PERSISTENT  = 0x40,
    SHADOW      = 0x80
}

-- Memory alignment constants
rift.CLASSICAL_ALIGNMENT = 4096
rift.QUANTUM_ALIGNMENT = 8
rift.DEFAULT_THRESHOLD = 0.85

-- ============================================================================
-- Memory Governance
-- ============================================================================

---@class RiftMemorySpan
--- Memory span - declared BEFORE type or value per Rift spec
local RiftMemorySpan = {}
RiftMemorySpan.__index = RiftMemorySpan

function RiftMemorySpan.new(span_type, bytes)
    local self = setmetatable({}, RiftMemorySpan)
    self.span_type = span_type or "fixed"
    self.bytes = bytes or 4096
    self.alignment = rift.CLASSICAL_ALIGNMENT
    self.open = true
    self.direction = true  -- true = right->left
    self.access_mask = 0x0F  -- CREATE|READ|UPDATE|DELETE
    return self
end

function RiftMemorySpan:validate_alignment()
    -- Validate alignment is power of 2.
    -- A power-of-2 value N satisfies: N > 0 and (N AND (N-1)) == 0.
    local align = self.alignment
    return align > 0 and _band(align, align - 1) == 0
end

rift.RiftMemorySpan = RiftMemorySpan

-- ============================================================================
-- Token Structure - The Triplet Model
-- ============================================================================

---@class RiftToken
--- The Token Triplet: (type, value, memory) with governance
local RiftToken = {}
RiftToken.__index = RiftToken

function RiftToken.new(token_type, memory)
    local self = setmetatable({}, RiftToken)

    -- Core triplet - memory declared first per Rift spec
    self.type = token_type
    self.memory = memory
    self._value = nil

    -- Governance fields
    self.validation_bits = rift.TOKEN_BITS.ALLOCATED
    self._lock = false
    self._lock_owner = nil

    -- Quantum fields (initialized when needed)
    self._superposed_states = nil
    self._amplitudes = nil
    self._phase = 0.0

    -- Entanglement fields
    self._entangled_with = nil
    self._entanglement_id = 0

    -- Source location
    self.source_line = 0
    self.source_column = 0
    self.source_file = nil

    return self
end

function RiftToken:get_value()
    -- Get value with validation check
    if _band(self.validation_bits, rift.TOKEN_BITS.INITIALIZED) == 0 then
        error("Token value not initialized")
    end
    return self._value
end

function RiftToken:set_value(val)
    -- Set value with immediate binding (classic mode)
    self._value = val
    self.validation_bits = _bor(self.validation_bits, rift.TOKEN_BITS.INITIALIZED)
end

function RiftToken:lock()
    -- Acquire token lock for thread safety (coroutine-safe in Lua)
    if not self._lock then
        self._lock = true
        self._lock_owner = coroutine.running()
        self.validation_bits = _bor(self.validation_bits, rift.TOKEN_BITS.LOCKED)
        return true
    end
    return false
end

function RiftToken:unlock()
    -- Release token lock
    if self._lock and self._lock_owner == coroutine.running() then
        self._lock = false
        self._lock_owner = nil
        self.validation_bits = _band_not(self.validation_bits, rift.TOKEN_BITS.LOCKED)
        return true
    end
    return false
end

function RiftToken:validate()
    -- Validate token against governance policy
    -- Check ALLOCATED bit
    if _band(self.validation_bits, rift.TOKEN_BITS.ALLOCATED) == 0 then
        return false
    end

    -- Memory span must exist and be valid
    if not self.memory or self.memory.alignment == 0 then
        return false
    end

    -- Validate alignment
    if not self.memory:validate_alignment() then
        return false
    end

    -- Mark as governed
    self.validation_bits = _bor(self.validation_bits, rift.TOKEN_BITS.GOVERNED)
    return true
end

function RiftToken:superpose(states, amplitudes)
    -- Put token into quantum superposition
    if not states or #states == 0 then
        return false
    end

    self._superposed_states = states
    self._amplitudes = amplitudes or {}
    if #self._amplitudes == 0 then
        local prob = 1.0 / #states
        for _ = 1, #states do
            table.insert(self._amplitudes, math.sqrt(prob))
        end
    end

    self.validation_bits = _bor(self.validation_bits, rift.TOKEN_BITS.SUPERPOSED)
    return true
end

function RiftToken:entangle_with(other, entanglement_id)
    -- Create entanglement with another token
    if not self._entangled_with then
        self._entangled_with = {}
    end
    table.insert(self._entangled_with, other)
    self._entanglement_id = entanglement_id
    self.validation_bits = _bor(self.validation_bits, rift.TOKEN_BITS.ENTANGLED)
    return true
end

function RiftToken:collapse(selected_index)
    -- Collapse superposition to single state
    selected_index = selected_index or 1

    if _band(self.validation_bits, rift.TOKEN_BITS.SUPERPOSED) == 0 then
        return false
    end

    if self._superposed_states and selected_index <= #self._superposed_states then
        local collapsed = self._superposed_states[selected_index]
        self._value = collapsed._value
        self.type = collapsed.type
        self._superposed_states = nil
        self._amplitudes = nil
        self.validation_bits = _band_not(self.validation_bits, rift.TOKEN_BITS.SUPERPOSED)
        return true
    end
    return false
end

function RiftToken:is_valid()
    -- Check if token is valid and governed
    return _band(self.validation_bits, rift.TOKEN_BITS.INITIALIZED) ~= 0 and
           _band(self.validation_bits, rift.TOKEN_BITS.GOVERNED) ~= 0
end

function RiftToken:__tostring()
    return string.format("RiftToken(type=%s, governed=%s)",
                         tostring(self.type),
                         tostring(self:is_valid()))
end

rift.RiftToken = RiftToken

-- ============================================================================
-- Rift Object Base Class
-- ============================================================================

---@class RiftObject
--- Base class for Lua objects governed by Rift policies
local RiftObject = {}
RiftObject.__index = RiftObject

function RiftObject.new()
    local self = setmetatable({}, RiftObject)

    -- Memory-first declaration
    self._memory = RiftMemorySpan.new("fixed", 4096)
    self._token = RiftToken.new("LuaTable", self._memory)
    self._token:validate()

    return self
end

function RiftObject:__enter()
    self._token:lock()
    return self
end

function RiftObject:__exit()
    self._token:unlock()
end

rift.RiftObject = RiftObject

-- ============================================================================
-- Quantum Functions
-- ============================================================================

function rift.superpose(...)
    -- Create a superposed token from multiple states
    local states = {...}
    local memory = RiftMemorySpan.new("superposed", 64)
    memory.alignment = rift.QUANTUM_ALIGNMENT

    local token = RiftToken.new("QLuaValue", memory)

    -- Create child tokens for each state
    local state_tokens = {}
    for _, state in ipairs(states) do
        local state_memory = RiftMemorySpan.new("fixed", 64)
        local state_token = RiftToken.new("LuaValue", state_memory)
        state_token:set_value(state)
        table.insert(state_tokens, state_token)
    end

    token:superpose(state_tokens)
    return token
end

function rift.entangle(a, b)
    -- Entangle two tokens
    local entanglement_id = math.random(1, 1000000)
    a:entangle_with(b, entanglement_id)
    b:entangle_with(a, entanglement_id)
    return entanglement_id
end

function rift.collapse(token, selected_index)
    -- Collapse a superposed token
    return token:collapse(selected_index)
end

-- ============================================================================
-- Utility Functions
-- ============================================================================

function rift.with_token(token, fn)
    -- Safe token access pattern
    if token:lock() then
        local success, result = pcall(fn, token)
        token:unlock()
        if success then
            return result
        else
            error(result)
        end
    end
end

function rift.local_(name, value)
    -- Rift-governed local variable declaration
    local memory = RiftMemorySpan.new("fixed", 64)
    local token = RiftToken.new("LuaValue", memory)
    token:set_value(value)
    token:validate()
    return token
end

function rift.function_(name, fn)
    -- Rift-governed function declaration
    local memory = RiftMemorySpan.new("row", 4096)
    local token = RiftToken.new("LuaFunction", memory)
    token:set_value(fn)
    token:validate()
    return token
end

-- ============================================================================
-- Governance Validation
-- ============================================================================

function rift.validate(value)
    -- Validate and display a governed value.
    -- Called from generated .lua output as: rift.validate(varname)
    io.write(string.format("rift.validate: %s\n", tostring(value)))
end

return rift
