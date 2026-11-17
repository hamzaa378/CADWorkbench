#pragma once

#include "SceneDocument.h"
#include <BRepBuilderAPI_MakeShape.hxx>
#include <gp_Pnt.hxx>

namespace cad {
// Retarget a face- or edge-based operation through ONE completed Boolean using
// OpenCascade's actual Modified/Generated history. Both the source geometry and
// the Boolean builder must belong to the CURRENT rebuild, never to an old cache.
// Missing, deleted and non-unique descendants are errors, not guesses.
// Retarget 1-40 independent witnessed references through the SAME fresh
// Boolean kernel transaction. Never overwrite the destination when any
// element is deleted, ambiguous, invalid or names a duplicate feature ID.
// An untrusted edge list must not lead to a partially rebound feature tree.
bool rebaseFeaturesThroughBoolean(const TopoDS_Shape& source,
                                 const TopoDS_Shape& result,
                                 BRepBuilderAPI_MakeShape& boolean,
                                 const QVector<ShapeFeatureNode>& features,
                                 QVector<ShapeFeatureNode>* rebound,
                                 QString* error=nullptr);

bool rebaseFeatureThroughBoolean(const TopoDS_Shape& source,
                                const TopoDS_Shape& result,
                                BRepBuilderAPI_MakeShape& boolean,
                                const ShapeFeatureNode& feature,
                                ShapeFeatureNode* rebound,
                                QString* error=nullptr);
// Positive uniform scale is a one-to-one similarity transformation.  Unlike
// a rigid move, it changes face area, edge length, circle curvature and analytic
// surface radii.  It preserves physical *relative* locations, not feature-tool
// dimensions (a 4 mm downstream drill stays 4 mm).  Nonuniform scaling,
// mirrors and procedural replacements remain explicit lineage boundaries.
bool supportsSimilarityReferenceLineage(const ShapeFeatureNode& operation);
bool transportDesignPointThroughSimilarity(const ShapeFeatureNode& operation,
                                           gp_Pnt* designPoint,QString* error=nullptr);
bool rebaseFeatureThroughSimilarity(const TopoDS_Shape& source,
                                    const TopoDS_Shape& result,
                                    const ShapeFeatureNode& operation,
                                    const ShapeFeatureNode& feature,
                                    ShapeFeatureNode* rebound,QString* error=nullptr);
// A rigid transformation has an exact, one-to-one image of every source
// subshape. This resolver transports the saved witness through that transform,
// then requires a unique matching edge/planar face in the actual output.
// Only rotations and translations are tracked (not scale, reflection, pattern).
bool supportsRigidReferenceLineage(const ShapeFeatureNode& operation);
// Transport an immutable, physical design witness independently of the edge/
// face descriptor. Boolean re-capture must not overwrite this position.
bool transportDesignPointThroughRigid(const ShapeFeatureNode& operation,
                                      gp_Pnt* designPoint,QString* error=nullptr);

bool rebaseFeatureThroughRigid(const TopoDS_Shape& source,
                              const TopoDS_Shape& result,
                              const ShapeFeatureNode& operation,
                              const ShapeFeatureNode& feature,
                              ShapeFeatureNode* rebound,
                              QString* error=nullptr);
}

namespace cad {
// Pattern occurrences have exact rigid transforms, but the result is a
// compound containing multiple instances. The caller must name the occurrence;
// only a unique witnessed successor is accepted. The seed is occurrence zero.
bool supportsPatternReferenceLineage(const ShapeFeatureNode& operation);
// Apply the selected pattern occurrence's actual rigid transform to a design
// witness. Uses the same construction as rebaseFeatureThroughPattern.
bool transportDesignPointThroughPattern(const ShapeFeatureNode& operation,
                                       int occurrence,gp_Pnt* designPoint,
                                       QString* error=nullptr);

bool rebaseFeatureThroughPattern(const TopoDS_Shape& source,
                                const TopoDS_Shape& result,
                                const ShapeFeatureNode& operation,
                                int occurrence,
                                const ShapeFeatureNode& feature,
                                ShapeFeatureNode* rebound,
                                QString* error=nullptr);
}

namespace cad {
// Only exact unchanged subshapes may cross a destructive edge modifier.
// Fillet/chamfer-generated edges and modified faces do not acquire invented
// ancestry. A reference whose source TopoDS identity is not retained is
// rejected, even if a similar-looking edge exists in the output.
bool supportsSurvivingModifierLineage(const ShapeFeatureNode& operation);
bool rebaseFeatureThroughSurvivingModifier(const TopoDS_Shape& source,
                                         const TopoDS_Shape& result,
                                         const ShapeFeatureNode& feature,
                                         ShapeFeatureNode* rebound,
                                         QString* error=nullptr);
}

namespace cad {
// Return true only for operators whose kernel builder is retained by the
// one-node ShapeFeatureTree replay. This does not imply generated geometry
// has a persistent identity: it tracks CURRENT source descendants only.
bool supportsRecordedModifierLineage(const ShapeFeatureNode& operation);
// Directly witnessed intermediate output (e.g. a loft-generated face). No
// upstream ancestry is implied. Uses strictly unique, bounded anchors.
bool rebaseFeatureOnCurrentStage(const TopoDS_Shape& current,
                                 const ShapeFeatureNode& feature,
                                 ShapeFeatureNode* rebound,
                                 QString* error=nullptr);
}
