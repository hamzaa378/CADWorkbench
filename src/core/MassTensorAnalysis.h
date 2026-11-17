#pragma once
// Portable CAD mass-tensor post-processing. No Qt or OpenCascade dependency.
// Inputs use mm for geometry, kg/m^3 for density, and mm^5 for the
// unit-density centroidal geometric inertia tensor from a volume integral.
#include <array>
#include <string>
#include <vector>

namespace cad::tensor {
struct Input {
    double volumeMm3=0.0;
    double densityKgPerM3=0.0;
    std::array<double,3> centerMm{};
    // Row-major 3x3 matrix about center of mass, with unit volumetric density (mm^5).
    std::array<double,9> geometricInertiaMm5{};
};
struct Report {
    double volumeMm3=0.0;
    double densityKgPerM3=0.0;
    double massKg=0.0;
    std::array<double,3> centerMm{};
    std::array<double,9> inertiaKgMm2{}; // Row-major, centroidal
    std::array<double,3> principalMomentsKgMm2{}; // Ascending order
    std::array<std::array<double,3>,3> principalAxes{}; // Corresponding unit vectors in global XYZ
    std::array<double,3> radiiOfGyrationMm{};
    bool principalAxesUnique=false; // false for nearly repeated moments
};
// Invalid inputs leave the caller's output unchanged. Results are not certified structural analyses.
bool analyze(const Input& input,Report* output,std::string* error=nullptr);
// Discrete-component mass properties. Uses the parallel-axis theorem; intended
// for non-overlapping physical components. It does NOT perform a Boolean union.
bool aggregate(const std::vector<Report>& components,Report* output,std::string* error=nullptr);
}
