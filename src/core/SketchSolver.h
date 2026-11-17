#pragma once

#include <QPointF>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace cad {

struct SketchPoint {
    QString id;
    double x = 0.0;
    double y = 0.0;
};

struct SketchSegment {
    QString id;
    QString startPointId;
    QString endPointId;
};

struct SketchCircle {
    QString id;
    QString centerPointId;
    double radius = 1.0;
    bool cutout = false;
};

struct SketchArc {
    QString id;
    QString startPointId, endPointId, midpointId;
};

// Cubic Bezier: the two control points are editable sketch points.
struct SketchSpline {
    QString id;
    QString startPointId, endPointId, control1PointId, control2PointId;
};

enum class SketchConstraintType {
    Coincident,
    Horizontal,
    Vertical,
    Distance,
    FixedPoint,
    EqualLength,
    Parallel,
    Perpendicular,
    Angle,
    HorizontalDistance,
    VerticalDistance,
    PointOnLine,
    Midpoint,
    Collinear,
    Radius,
    EqualRadius,
    Concentric,
    PointOnCircle,
    TangentLineCircle,
    SymmetricAboutLine,
    TangentCirclesExternal,
    TangentCirclesInternal,
    PointOnSegment,
    SegmentLength,
    SignedPointLineDistance,
    SegmentAngleFromX,
    SignedPointCircleDistance
};

// Constraint references use point IDs for pointA/pointB and segment IDs for
// segmentA/segmentB. Distance is in model units; angle is in radians.
// SignedPointLineDistance is positive to the left of a directed segment;
// SignedPointCircleDistance is positive outside the circle. SegmentAngleFromX
// is counterclockwise from the positive sketch X axis.
struct SketchConstraint {
    SketchConstraintType type = SketchConstraintType::Coincident;
    QString pointA;
    QString pointB;
    QString segmentA;
    QString segmentB;
    QString circleA;
    QString circleB;
    double value = 0.0;
    double fixedX = 0.0;
    double fixedY = 0.0;
};

struct SketchModel {
    QVector<SketchPoint> points;
    QVector<SketchSegment> segments;
    QVector<SketchCircle> circles;
    QVector<SketchArc> arcs;
    QVector<SketchSpline> splines;
    QVector<SketchConstraint> constraints;
};

struct SketchSolveOptions {
    int maxIterations = 100;
    double residualTolerance = 1e-7;
    double stepTolerance = 1e-9;
    double finiteDifferenceStep = 1e-6;
};

struct SketchSolveResult {
    bool success = false;
    bool converged = false;
    QString message;
    int iterations = 0;
    int numericalRank = 0;
    int degreesOfFreedom = 0;
    double maximumResidual = 0.0;
    QVector<double> constraintMaximumResiduals;
    int largestResidualConstraintIndex = -1; // zero-based; -1 for unconstrained sketches
    // Successful sketches may contain constraints that add no independent
    // equations at the solved configuration. Indices are zero-based and are
    // advisory (not automatic deletions or proof of a conflicting constraint).
    QVector<int> redundantConstraintIndices;
    // Local Jacobian nullspace advisory, in variable order: x,y for each
    // point followed by circle radii. Coupled coordinates can share one DOF.
    QVector<int> locallyMobileCoordinateIndices;
    QVector<QPointF> solvedPoints;
    QVector<double> solvedCircleRadii;
};

// A small, general 2D geometric constraint solver. Solving is transactional:
// input geometry is never changed, and solvedPoints is populated only when all
// constraints are satisfied within tolerance.
class SketchSolver {
public:
    static SketchSolveResult solve(const SketchModel& model,
                                   const SketchSolveOptions& options = {});
};

QJsonObject sketchModelToJson(const SketchModel& model);
bool sketchModelFromJson(const QJsonObject& object,SketchModel* model,QString* error = nullptr);

} // namespace cad
