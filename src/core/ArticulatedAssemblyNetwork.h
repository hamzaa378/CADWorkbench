#pragma once
#include "AssemblyConstraintNetwork.h"
#include <cstddef>
#include <string>
#include <vector>

// A bounded, deterministic positional solver for coupled revolute/prismatic
// assemblies. Local zero-pose transforms and joint axes are authored in the
// parent component's current coordinate frame. Only loops that constrain all
// undriven coordinates admit a unique result. No dynamic forces are modeled.
namespace cad::articulated {
enum class JointKind { Fixed, Revolute, Prismatic };
struct Joint {
    std::size_t parent=0, child=0;
    JointKind kind=JointKind::Fixed;
    mates::Transform childAtZero;
    mates::Vec3 axis{0,0,1};
    double initial=0, minimum=0, maximum=0;
    bool driven=true;
};
struct Solution {
    std::vector<mates::Transform> poses;
    std::vector<double> coordinates;
    double maximumClosureErrorMm=0;
    int iterations=0;
};
// Returns false without touching *output for a disconnected, inconsistent,
// over-/underconstrained, ill-conditioned or nonconvergent network.
bool solveNetwork(std::size_t count,std::size_t fixed,
                  const std::vector<Joint>& joints,
                  Solution* output,std::string* error=nullptr);
} // namespace cad::articulated
