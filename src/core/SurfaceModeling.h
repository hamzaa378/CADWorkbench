#pragma once

#include <QString>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Vec.hxx>

#include <vector>

namespace cad {

// Builds a single ruled surface patch between two valid boundary edges.
// The output is assigned only after OCCT reports a valid, non-zero-area face.
bool makeRuledSurface(const TopoDS_Edge& firstBoundary,const TopoDS_Edge& secondBoundary,
                      TopoDS_Face* result,QString* error = nullptr);

// Builds a ruled shell between two wires. When edge counts differ, uses the
// OCCT compatible-section ruled loft instead of requiring matched topology.
// The result is an open surface shell, not a capped solid.
bool makeRuledSurfaceShell(const TopoDS_Wire& firstBoundary,const TopoDS_Wire& secondBoundary,
                           TopoDS_Shell* result,QString* error = nullptr);

// Creates a smooth, open loft shell through two or more ordered section wires.
// Sections must be valid non-degenerate wires; OCCT compatible-section
// matching handles differing edge counts when the kernel can form a valid shell.
// The result has no end caps and is not a solid.
bool makeLoftedSurfaceShell(const std::vector<TopoDS_Wire>& sectionWires,
                            TopoDS_Shell* result,QString* error = nullptr);

// Sweeps one valid edge along a finite translation vector to form a surface face.
bool makeExtrudedSurface(const TopoDS_Edge& profile,const gp_Vec& translation,
                         TopoDS_Face* result,QString* error = nullptr);

// Revolves one valid edge through an angle in radians (0, 2π] around an axis.
bool makeRevolvedSurface(const TopoDS_Edge& profile,const gp_Ax1& axis,double angleRadians,
                         TopoDS_Face* result,QString* error = nullptr);

// Offsets a bounded face. OCCT may return a face or shell depending on the
// source topology; zero offsets and invalid/self-intersecting results are rejected.
bool makeOffsetSurface(const TopoDS_Face& source,double offsetMm,double toleranceMm,
                       TopoDS_Shape* result,QString* error = nullptr);

// Extends selected U/V bounds of one face by a positive model-space distance.
// At least one side must be selected; the result is assigned only when valid.
bool extendSurfaceFace(const TopoDS_Face& source,double extensionMm,
                       bool extendUMin,bool extendUMax,bool extendVMin,bool extendVMax,
                       TopoDS_Face* result,QString* error = nullptr);

} // namespace cad
