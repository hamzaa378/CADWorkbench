#pragma once
#include "AssemblyMateSolver.h"
#include <cstddef>
#include <string>
#include <vector>

// Fully constrained rigid-placement graph. Each connection defines a complete
// 6-DOF relative frame using point/axis/plane constraints. Cycles are checked
// against the already accumulated placements, not silently broken into a tree.
// No geometry is modified: a caller commits *all* placements or none.
namespace cad::mates {
struct NetworkConnection {
    std::size_t parent = 0;
    std::size_t child = 0;
    std::vector<Constraint> constraints;
};
struct ConnectionClosureResidual {
    // Units: mixed 10 mm angular lever metric; linear gap; and degrees.
    double maximumMixedResidualMm=0.0;
    double maximumLinearGapMm=0.0;
    double maximumAngularErrorDegrees=0.0;
};
struct NetworkSolution {
    std::vector<Transform> worldCorrections;
    std::vector<ConnectionClosureResidual> perConnection;
    // Legacy mixed point/axis residual in mm-equivalent units (10 mm
    // reference lever for orientation). Do not interpret as a pure gap.
    double worstClosureResidualMm = 0.0;
    double worstPointOrAxisGapMm = 0.0;
    double worstAngularErrorDegrees = 0.0;
};
bool solveRigidNetwork(std::size_t nodeCount,std::size_t fixedNode,
                       const std::vector<NetworkConnection>& connections,
                       NetworkSolution* solution,std::string* error = nullptr);
Transform multiply(const Transform& a,const Transform& b);
Transform inverseRigid(const Transform& t);
} // namespace cad::mates
