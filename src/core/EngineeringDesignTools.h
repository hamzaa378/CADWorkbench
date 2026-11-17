#pragma once

#include <string>
#include <vector>

namespace cad {

// Preliminary mechanical design utilities. All calculations use the explicit
// SI-derived units in each input descriptor. Results are estimates, not
// engineering certification, FEA, or a substitute for applicable codes.
struct EngineeringInput {
    std::string key;
    std::string label;
    std::string unit;
    double example = 0.0;
    double minimum = 0.0;
    double maximum = 1e12;
};

struct EngineeringTool {
    std::string id;
    std::string group;
    std::string title;
    std::string resultUnit;
    std::string assumptions;
    std::vector<EngineeringInput> inputs;
};

struct EngineeringResult {
    bool valid = false;
    double value = 0.0;
    std::string error;
};

const std::vector<EngineeringTool>& engineeringToolCatalog();
EngineeringResult evaluateEngineeringTool(const std::string& id, const std::vector<double>& inputs);

} // namespace cad
