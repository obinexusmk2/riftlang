-- luarift/targets/pattern_engine.lua
-- Polar Bipartite Pattern Matching Engine - Lua Implementation

local pattern_engine = {}

-- Pattern polarity
pattern_engine.POLARITY = {
    LEFT = 0,   -- Input/matcher
    RIGHT = 1   -- Output/generator
}

-- ============================================================================
-- RiftPattern Class
-- ============================================================================

local RiftPattern = {}
RiftPattern.__index = RiftPattern

function RiftPattern.new(pattern_str, polarity, priority, anchored, is_literal)
    local self = setmetatable({}, RiftPattern)
    self.pattern_str = pattern_str
    self.polarity = polarity
    self.priority = priority
    self.anchored = anchored
    self.is_literal = is_literal
    self.compiled_pattern = nil
    return self
end

pattern_engine.RiftPattern = RiftPattern

-- ============================================================================
-- BipartitePair Class
-- ============================================================================

local BipartitePair = {}
BipartitePair.__index = BipartitePair

function BipartitePair.new(left, right, transform_fn, is_governed, transform_id)
    local self = setmetatable({}, BipartitePair)
    self.left = left
    self.right = right
    self.transform_fn = transform_fn
    self.is_governed = is_governed or false
    self.transform_id = transform_id or 0
    return self
end

pattern_engine.BipartitePair = BipartitePair

-- ============================================================================
-- MatchResult Class
-- ============================================================================

local MatchResult = {}
MatchResult.__index = MatchResult

function MatchResult.new(matched, output, priority, transform_id, groups)
    local self = setmetatable({}, MatchResult)
    self.matched = matched
    self.output = output
    self.priority = priority or 0
    self.transform_id = transform_id or 0
    self.groups = groups or {}
    return self
end

pattern_engine.MatchResult = MatchResult

-- ============================================================================
-- PatternEngine Class
-- ============================================================================

local PatternEngine = {}
PatternEngine.__index = PatternEngine

function PatternEngine.new(mode)
    local self = setmetatable({}, PatternEngine)
    self.pairs = {}
    self.mode = mode or "classical"
    
    -- Metrics
    self.total_matches = 0
    self.total_failures = 0
    self.average_match_time_ms = 0.0
    
    return self
end

function PatternEngine:add_pair(left_pattern, right_pattern, priority, right_is_literal)
    priority = priority or 100
    right_is_literal = right_is_literal or false
    
    -- Create left pattern (input matcher)
    local left = RiftPattern.new(
        left_pattern,
        pattern_engine.POLARITY.LEFT,
        priority,
        left_pattern:sub(1, 1) == "^",
        false
    )
    
    -- Create right pattern (output generator)
    local right = RiftPattern.new(
        right_pattern,
        pattern_engine.POLARITY.RIGHT,
        priority,
        false,
        right_is_literal
    )
    
    -- Create pair
    local pair = BipartitePair.new(
        left,
        right,
        nil,
        false,
        #self.pairs + 1
    )
    
    table.insert(self.pairs, pair)
    return true
end

function PatternEngine:match(input_str)
    local start_time = os.clock()
    
    local best_pair = nil
    local best_priority = math.huge
    local best_match = nil
    local best_groups = {}
    
    -- Search for matching pattern (respecting priority)
    for _, pair in ipairs(self.pairs) do
        -- Try to match input against left pattern
        local captures = {input_str:match(pair.left.pattern_str)}
        if #captures > 0 then
            -- Lower priority number = higher priority
            if pair.left.priority < best_priority then
                best_pair = pair
                best_priority = pair.left.priority
                best_match = captures
            end
        end
    end
    
    -- Generate output
    if best_pair then
        local template = best_pair.right.pattern_str
        local output = template
        
        -- Simple substitution for capture groups
        if best_match then
            for i, capture in ipairs(best_match) do
                output = output:gsub("$" .. i, capture)
            end
        end
        
        -- Update metrics
        local elapsed = (os.clock() - start_time) * 1000
        self.total_matches = self.total_matches + 1
        self:_update_metrics(elapsed)
        
        return MatchResult.new(
            true,
            output,
            best_priority,
            best_pair.transform_id,
            best_groups
        )
    end
    
    -- No match found
    local elapsed = (os.clock() - start_time) * 1000
    self.total_failures = self.total_failures + 1
    self:_update_metrics(elapsed)
    
    return MatchResult.new(false)
end

function PatternEngine:_update_metrics(elapsed_ms)
    local total = self.total_matches + self.total_failures
    if total > 0 then
        self.average_match_time_ms = (
            (self.average_match_time_ms * (total - 1)) + elapsed_ms
        ) / total
    end
end

function PatternEngine:get_metrics()
    return {
        total_matches = self.total_matches,
        total_failures = self.total_failures,
        average_match_time_ms = self.average_match_time_ms,
        pair_count = #self.pairs
    }
end

pattern_engine.PatternEngine = PatternEngine

-- ============================================================================
-- Default Lua Patterns
-- ============================================================================

pattern_engine.DEFAULT_LUA_PATTERNS = {
    -- Function definition transformation
    {"function%s+(%w+)%s*%(%)%s*", "function %1(self) end", 100},
    -- Local variable annotation
    {"local%s+(%w+)%s*=%s*(.+)", "rift_local('%1', %2)", 90},
    -- Table creation
    {"(%w+)%s*=%s*{%s*}", "rift_table('%1', {})", 80},
    -- Quantum decorator
    {"@quantum", "@rift_quantum_decorator", 200},
}

function pattern_engine.create_default_engine()
    local engine = PatternEngine.new()
    for _, pattern in ipairs(pattern_engine.DEFAULT_LUA_PATTERNS) do
        engine:add_pair(pattern[1], pattern[2], pattern[3], true)
    end
    return engine
end

return pattern_engine
