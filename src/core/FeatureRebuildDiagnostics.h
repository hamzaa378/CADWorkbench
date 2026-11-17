#pragma once
#include "FeatureDependencyPlan.h"
#include <string>
#include <vector>

namespace cad::dependency {
// Pure planning diagnostics for rebuild decisions. Not a replacement for
// verified B-rep persistence, geometric regeneration or transaction commit.
// Each reason is deterministic; trigger points to a source or parent feature.
enum class RebuildCause {
    Reused,
    NewFeature,
    MissingCachedOutput,
    DefinitionChanged,
    DependencyChanged,
    BaseGeometryChanged,
    ExternalGeometryChanged,
    UpstreamRecomputed
};

struct RebuildDecision {
    std::string featureId;
    RebuildCause cause=RebuildCause::NewFeature;
    std::string trigger;
    int previousIndex=-1;
};

const char* rebuildCauseName(RebuildCause cause) noexcept;

// Returns a completely aligned reusable B-rep plan AND per-node causes.
// On invalid input, neither output argument is modified. Causes are computed
// using the same validated planning operation as planReuseWithSources.
bool explainReuseWithSources(
    const std::vector<FeatureRecord>& previous,
    const std::vector<FeatureRecord>& proposed,
    const std::vector<bool>& previousOutputAvailable,
    const std::string& previousBaseRevision,
    const std::string& proposedBaseRevision,
    const std::vector<SourceRevision>& previousSources,
    const std::vector<SourceRevision>& proposedSources,
    ReusePlan* plan,
    std::vector<RebuildDecision>* decisions,
    std::string* error=nullptr);
}
