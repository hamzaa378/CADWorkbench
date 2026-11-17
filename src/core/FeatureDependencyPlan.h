#pragma once

#include <string>
#include <utility>
#include <vector>

namespace cad::dependency {

// Platform-independent description of a feature's persisted computation.
// Parameters must be in deterministic key order. Callers promise the base
// geometry and kernel semantics are unchanged before applying this plan.
struct FeatureRecord {
    std::string id;
    std::string type;
    std::vector<std::pair<std::string,double>> parameters;
    bool suppressed=false;
    std::vector<std::string> inputs;
};

struct ReusePlan {
    // -1: recompute; otherwise index in the previously committed node list.
    // Supports moved branches, not merely the longest common prefix.
    std::vector<int> previousIndex;
    int reused=0;
    int recomputed=0;
};

// Fail closed for malformed dependency graphs. A node is reusable only if its
// persisted definition is identical, its effective inputs are identical, its
// old output is available, and every upstream source it depends on was reused.
// Strict source-aware planning for rebuilds whose upstream B-rep may change.
// An empty revision is never trusted; a different base token invalidates EVERY
// descendant of @base, including unaffected-looking parallel branches. This
// overload is recommended whenever the model source can be regenerated.
bool planReuseVersioned(const std::vector<FeatureRecord>& previous,
                        const std::vector<FeatureRecord>& proposed,
                        const std::vector<bool>& previousOutputAvailable,
                        const std::string& previousBaseRevision,
                        const std::string& proposedBaseRevision,
                        ReusePlan* plan,std::string* error=nullptr);

// A named source represents a persisted geometry provider (external sketch,
// component, or imported B-rep). Revisions must be stable geometry tokens,
// not the display name or a file timestamp. Names are ASCII letters, digits,
// underscores, hyphens, or dots. Catalogs are sorted by name and unique.
struct SourceRevision {
    std::string name;
    std::string geometryRevision;
};

// Selective invalidation for DAGs that explicitly reference external bodies
// using inputs "@source:<name>". Unlike planReuseVersioned, a changed source
// invalidates only its dependent branches and downstream joins. An implicit
// first feature still reads @base; implicit successors read the preceding node.
// Rejects unknown, malformed, or duplicate source catalogs and references.
// A successful plan does not execute geometry; its caller must verify the
// associated B-rep cache entries and atomically commit rebuilt outputs.
bool planReuseWithSources(const std::vector<FeatureRecord>& previous,
                          const std::vector<FeatureRecord>& proposed,
                          const std::vector<bool>& previousOutputAvailable,
                          const std::string& previousBaseRevision,
                          const std::string& proposedBaseRevision,
                          const std::vector<SourceRevision>& previousSources,
                          const std::vector<SourceRevision>& proposedSources,
                          ReusePlan* plan,std::string* error=nullptr);

bool planReuse(const std::vector<FeatureRecord>& previous,
               const std::vector<FeatureRecord>& proposed,
               const std::vector<bool>& previousOutputAvailable,
               ReusePlan* plan,std::string* error=nullptr);

} // namespace cad::dependency
