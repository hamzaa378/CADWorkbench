#pragma once
#include "SceneDocument.h"
#include <memory>
class BRepBuilderAPI_MakeShape;

namespace cad {
// Does not mutate input geometry; returns null on a failed operation. Operation
// identifiers are stable across edits/reorders, and limits protect project files.
// Persisted geometric hints resolve planar faces independently of traversal index.
// Fail closed if the face disappeared or becomes geometrically ambiguous.
bool capturePlanarFaceAnchor(const TopoDS_Shape& shape,int faceIndex,
                             QMap<QString,double>* anchor,QString* error=nullptr);
// Resolve an anchor to one planar trimmed successor face. Supports interior
// witnesses across supported coplanar splits/merges, rejects ambiguous matches.
bool resolvePlanarFaceAnchor(const TopoDS_Shape& body,const QMap<QString,double>& anchor,
                             TopoDS_Face* face,QString* error=nullptr);

// Persistent witness anchors for trimmed cylinders, spheres, cones, tori,
// B-spline and Bezier faces. Freeform faces permit only small local deformation
// (bounded 3D point projection plus a normal constraint); unrelated/ambiguous
// surfaces fail closed. This is not universal topological naming.
// Detects unique successor faces through supported splits/merges and pure
// translations. Deliberately rejects ambiguous or deleted witness regions.
// Unlike the planar-feature anchors, this API supports curved B-rep surfaces.
bool captureCurvedFaceAnchor(const TopoDS_Shape& shape,int faceIndex,
                             QMap<QString,double>* anchor,QString* error=nullptr);
bool resolveCurvedFaceAnchor(const TopoDS_Shape& shape,
                             const QMap<QString,double>& anchor,
                             TopoDS_Face* face,QString* error=nullptr);

// Persistent edge hint. New line/circle anchors include a trimmed-interior
// witness and tangent: unique successors survive supported edge splits, merges,
// and translations. Legacy centroid-length anchors remain readable. Ambiguous
// or removed successor edges fail closed rather than silently retargeting.
// edgeIndex is 1-based in the topologically unique edge map (TopExp::MapShapes),
// not the potentially repeating face/edge traversal from TopExp_Explorer.
bool captureStableEdgeAnchor(const TopoDS_Shape& body,int edgeIndex,
                             QMap<QString,double>* anchor,QString* error=nullptr);
bool resolveStableEdgeAnchor(const TopoDS_Shape& body,const QMap<QString,double>& anchor,
                             int* edgeIndex,QString* error=nullptr);

// Drawing-only recovery for full circles whose radius changes while their
// center and axis stay fixed. Opt-in and bounded to a 20% radius change.
// Rejects non-circles, incomplete arcs, moved centers/axes, and multiple
// candidates. Never use as a generic topology fallback for machining edges.
bool resolveResizedFullCircleForDrawing(const TopoDS_Shape& body,
                                       const QMap<QString,double>& anchor,
                                       int* edgeIndex,QString* error=nullptr);

// Dependency-aware reuse plan. Old outputs are indexed by the *old* node order;
// a -1 entry means rebuild. Caller must separately verify base geometry and
// cache provenance before using this plan to skip OpenCascade operations.
bool planShapeFeatureReuse(const QVector<ShapeFeatureNode>& previous,
                           const QVector<ShapeFeatureNode>& proposed,
                           const QVector<bool>& previousOutputAvailable,
                           QVector<int>* previousIndex,QString* error=nullptr);

bool validateShapeFeatureNodes(const QVector<ShapeFeatureNode>& nodes,QString* error = nullptr);
// Rebuild an ordered feature graph using an optional trusted unchanged prefix
// and optional independently reusable nodes indexed by stable feature ID.
// Cached outputs must originate from this same source geometry and verified
// committed node definitions. Branch references resolve to previously built
// *or* reused outputs. Neither output is updated until complete success.
bool rebuildShapeFeatureTree(const TopoDS_Shape& source,bool surfaceBody,
                             const QVector<ShapeFeatureNode>& nodes,TopoDS_Shape* output,
                             QString* error = nullptr,
                             const QVector<TopoDS_Shape>* cachedPrefix = nullptr,
                             QVector<TopoDS_Shape>* perNodeOutputs = nullptr,
                             const QMap<QString,TopoDS_Shape>* reusableOutputs = nullptr,
                             std::shared_ptr<BRepBuilderAPI_MakeShape>* kernelHistory = nullptr);
}
