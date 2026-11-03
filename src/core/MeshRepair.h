#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cad::mesh {

struct Point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Triangle {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
};

struct RepairStats {
    std::size_t inputVertices = 0;
    std::size_t inputTriangles = 0;
    std::size_t outputVertices = 0;
    std::size_t outputTriangles = 0;
    std::size_t weldedVertices = 0;
    std::size_t removedDegenerateTriangles = 0;
    std::size_t removedDuplicateTriangles = 0;
    std::size_t flippedTriangles = 0;
    std::size_t connectedComponents = 0;
    std::size_t boundaryEdges = 0;
    std::size_t nonManifoldEdges = 0;
    std::size_t filledHoles = 0;
    std::size_t filledHoleTriangles = 0;
};

struct RepairResult {
    std::vector<Point3> vertices;
    std::vector<Triangle> triangles;
    RepairStats stats;
};

// Welds coincident vertices, removes collapsed/duplicate triangles, compacts
// unused vertices, and consistently orients each manifold connected surface.
// Open boundaries and edges shared by more than two triangles are reported in
// stats; this routine does not invent faces or claim the mesh is a solid.
// On invalid input or a non-orientable manifold component, output is unchanged.
bool repairTriangleMesh(const std::vector<Point3>& vertices,
                        const std::vector<Triangle>& triangles,
                        double weldTolerance,
                        RepairResult* output,
                        std::string* error = nullptr);

// Fills every simple, strictly convex, planar boundary loop with a reversed
// triangle fan. The operation is all-or-nothing: an invalid, non-planar,
// branched, non-manifold, or oversized loop leaves the mesh unchanged. This
// adds surface facets only; callers must not assume the result is a solid.
// planarityTolerance is in the same length units as the mesh coordinates.
bool fillPlanarBoundaryHoles(RepairResult* mesh,
                             double planarityTolerance,
                             std::size_t maxLoopEdges,
                             std::string* error = nullptr);

} // namespace cad::mesh
