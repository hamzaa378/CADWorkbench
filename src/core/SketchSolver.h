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
    PointOnSegment
};

// Constraint references use point IDs for pointA/pointB and segment IDs for
// segmentA/segmentB. Distance is in model units; angle is in radians.
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
