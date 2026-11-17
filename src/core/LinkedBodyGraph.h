#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cad::linkedgraph {

// A pure C++ dependency description. Boolean derivatives have two sources;
// single-source sketch derivatives set unary=true and leave tool empty.
// Ordinary bodies have no inputs. Node order is UI/document order.
struct Node {
    std::string id;
    bool derived = false;
    std::string primary;
    std::string tool;
    bool unary = false;
    std::vector<std::string> additionalSources = {}; // Optional independent sources for ordered sketch stacks.
};

// Returns all node indices in stable topological order (source before user).
// Rejects missing/duplicate IDs, duplicate sources, self-dependencies, and cycles.
// On failure, output remains unchanged.
bool plan(const std::vector<Node>& nodes, std::vector<std::size_t>* order,
          std::string* error = nullptr);

// Union of downstream dependency cones in topological order. The input IDs
// must be unique and present. On failure, output is not modified.
bool affectedMany(const std::vector<Node>& nodes,
                  const std::vector<std::string>& changedIds,
                  std::vector<std::size_t>* indices,
                  std::string* error = nullptr);

// Explains each affected result using one shortest source-to-result dependency
// chain, in deterministic rebuild order. For multiple changes, ties are broken
// lexicographically, never by unordered-map iteration. This reports *why* a
// rebuild is required; it does not imply that geometry recomputation succeeds.
struct ImpactCause {
    std::size_t nodeIndex=0;
    std::string changedSourceId;
    std::vector<std::string> pathIds; // changed source ... affected node
};
bool explainAffectedMany(const std::vector<Node>& nodes,
                         const std::vector<std::string>& changedIds,
                         std::vector<ImpactCause>* causes,
                         std::string* error=nullptr);

// An incremental regeneration plan grouped by causal dependency depth.
// All inputs of an affected node that are also changing occur in an earlier
// wave, so batches can stage intermediate results with explicit barriers.
// Independent nodes in a wave MAY be evaluated concurrently by a future
// engine, but this function does not launch threads or commit any geometry.
// A directly edited descendant is still scheduled AFTER its edited ancestors.
struct RebuildWave {
    std::size_t dependencyDepth=0;
    std::vector<std::size_t> nodeIndices; // stable document order within wave
};
bool planAffectedWaves(const std::vector<Node>& nodes,
                       const std::vector<std::string>& changedIds,
                       std::vector<RebuildWave>* waves,
                       std::string* error=nullptr);

// Returns the affected descendant nodes, including the specified changed node,
// in dependency order. A failed request does not mutate the output.
bool affected(const std::vector<Node>& nodes, const std::string& changedId,
              std::vector<std::size_t>* indices, std::string* error = nullptr);

}  // namespace cad::linkedgraph
