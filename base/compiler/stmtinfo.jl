struct MethodMatchInfo
    applicable::Any
end

struct UnionSplitInfo
    matches::Vector{MethodMatchInfo}
end
