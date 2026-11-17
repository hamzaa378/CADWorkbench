#pragma once

// Portable, Qt-independent engineering design study kernel.
// Internal base units: mm, N, s, K. Angles are dimensionless radians.
// This module plans rebuilds but never mutates CAD geometry on its own.
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace cad::study {
struct Dimension {
    int length=0, force=0, time=0, temperature=0;
    friend bool operator==(const Dimension&,const Dimension&) = default;
};
constexpr Dimension scalar{};
constexpr Dimension length{1,0,0,0};
constexpr Dimension force{0,1,0,0};
constexpr Dimension stress{-2,1,0,0};
constexpr Dimension mass{ -1,1,2,0};
struct Quantity { double value=0; Dimension unit{}; friend bool operator==(const Quantity&,const Quantity&) = default; };
struct Evaluation {bool valid=false; Quantity quantity{}; std::string error;};
using Resolver = std::function<Evaluation(const std::string&)>;
Evaluation evaluateExpression(const std::string& expression,const Resolver& resolver);
// Each edited parameter resolves against other edited entries first, otherwise
// the supplied baseline. Cycles, unknown names and dimension mismatches fail
// without modifying output. Variable names are case sensitive.
bool evaluateParameterExpressions(const std::map<std::string,Quantity>& baseline,
    const std::map<std::string,std::string>& edits,
    const std::map<std::string,Dimension>& expected,
    std::map<std::string,Quantity>* result,std::string* error=nullptr);

struct FeatureDependency {
    std::string id;
    std::vector<std::string> inputs;
};
struct RebuildPlan {
    bool valid=false;
    std::vector<std::string> allTopological;
    std::vector<std::string> affected;
    std::string error;
};
// Includes all descendants of changed IDs, topologically sorted; no caching
// claims are made here. An actual CAD executor must use this order and rollback
// on any geometry failure.
RebuildPlan makeRebuildPlan(const std::vector<FeatureDependency>& nodes,
                            const std::vector<std::string>& changed);

struct Configuration {
    std::string name;
    std::map<std::string,std::string> parameters;
};
struct ConfigurationResult {
    std::string name;
    bool valid=false;
    std::map<std::string,Quantity> values;
    std::string error;
};
std::vector<ConfigurationResult> evaluateConfigurations(
    const std::map<std::string,Quantity>& baseline,
    const std::map<std::string,Dimension>& expected,
    const std::vector<Configuration>& cases);

struct MaterialRecord {
    std::string id, name, grade, source, revision;
    double densityKgPerM3=0, youngsModulusGPa=0, yieldStrengthMPa=0;
};
bool validateMaterial(const MaterialRecord& material,std::string* error=nullptr);
// On invalid or duplicate records, return empty and set error. Never silently discard an engineering material.
std::string materialCatalogCsv(const std::vector<MaterialRecord>& records,std::string* error=nullptr);
}
