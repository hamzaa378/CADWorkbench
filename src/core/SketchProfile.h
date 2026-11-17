#pragma once

#include "SketchSolver.h"
#include <TopoDS_Shape.hxx>

namespace cad {
// Persisted datum plane: origin, unit-independent normal, and twist about that normal.
// Local +Z is the extrusion direction. All positions are millimetres.
struct SketchPlane {
    double originX=0, originY=0, originZ=0;
    double normalX=0, normalY=0, normalZ=1;
    double twistDegrees=0;
};

QJsonObject sketchPlaneToJson(const SketchPlane& plane);
bool sketchPlaneFromJson(const QJsonObject& value, SketchPlane* plane, QString* error=nullptr);
bool validSketchPlane(const SketchPlane& plane, QString* error=nullptr);

// Builds an exact OCCT wire: straight segments, three-point circular arcs,
// cubic Beziers, and circular internal holes. The polygon sampling is used
// ONLY for conservative self-intersection checks, not to construct the solid.
// Callers commit the returned shape transactionally.
bool buildSketchExtrusion(const SketchModel& sketch,const QStringList& boundary,double depth,
                          const SketchPlane& plane,TopoDS_Shape* result,QString* error=nullptr);
}
