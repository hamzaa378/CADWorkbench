#include "SketchSolver.h"
#include "SketchConstraintDiagnostics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <QHash>
#include <QSet>
#include <QJsonArray>

namespace cad {
namespace {

using Vector = QVector<double>;
using Matrix = QVector<Vector>;
constexpr double kGeometryEpsilon = 1e-12;

struct SegmentVector { double x = 0.0; double y = 0.0; };

bool finite(double value) { return std::isfinite(value); }

bool solveLinearSystem(Matrix matrix, Vector rhs, Vector* solution)
{
    const int n = rhs.size();
    if (!solution || matrix.size() != n) return false;
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int row = col + 1; row < n; ++row)
            if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col])) pivot = row;
        if (std::abs(matrix[pivot][col]) < 1e-18) return false;
        if (pivot != col) { std::swap(matrix[pivot], matrix[col]); std::swap(rhs[pivot], rhs[col]); }
        const double divisor = matrix[col][col];
        for (int j = col; j < n; ++j) matrix[col][j] /= divisor;
        rhs[col] /= divisor;
        for (int row = 0; row < n; ++row) {
            if (row == col) continue;
            const double factor = matrix[row][col];
            if (factor == 0.0) continue;
            for (int j = col; j < n; ++j) matrix[row][j] -= factor * matrix[col][j];
            rhs[row] -= factor * rhs[col];
        }
    }
    *solution = std::move(rhs);
    return true;
}

int matrixRank(Matrix matrix, double tolerance = 1e-8)
{
    if (matrix.isEmpty()) return 0;
    const int rows = matrix.size(), cols = matrix.first().size();
    double largest = 0.0;
    for (const Vector& row : matrix) for (double value : row) largest = std::max(largest, std::abs(value));
    if (largest == 0.0) return 0;
    const double threshold = largest * tolerance;
    int rank = 0;
    for (int col = 0; col < cols && rank < rows; ++col) {
        int pivot = rank;
        for (int row = rank + 1; row < rows; ++row)
            if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col])) pivot = row;
        if (std::abs(matrix[pivot][col]) <= threshold) continue;
        std::swap(matrix[pivot], matrix[rank]);
        const double divisor = matrix[rank][col];
        for (int j = col; j < cols; ++j) matrix[rank][j] /= divisor;
        for (int row = rank + 1; row < rows; ++row) {
            const double factor = matrix[row][col];
            for (int j = col; j < cols; ++j) matrix[row][j] -= factor * matrix[rank][j];
        }
        ++rank;
    }
    return rank;
}

double squaredNorm(const Vector& values)
{
    double sum = 0.0;
    for (double value : values) sum += value * value;
    return sum;
}

double wrapAngle(double radians)
{
    return std::remainder(radians, 2.0 * std::acos(-1.0));
}

} // namespace

SketchSolveResult SketchSolver::solve(const SketchModel& model, const SketchSolveOptions& options)
{
    SketchSolveResult result;
    auto fail = [&result](const QString& message) { result.message = message; return result; };
    if (model.points.isEmpty()) return fail(QStringLiteral("A sketch needs at least one point."));
    if (options.maxIterations < 1 || options.maxIterations > 10000 ||
        !finite(options.residualTolerance) || options.residualTolerance <= 0.0 ||
        !finite(options.stepTolerance) || options.stepTolerance <= 0.0 ||
        !finite(options.finiteDifferenceStep) || options.finiteDifferenceStep <= 0.0)
        return fail(QStringLiteral("Sketch solver options are invalid."));

    QHash<QString, int> pointIndex, segmentIndex, circleIndex;
    for (int i = 0; i < model.points.size(); ++i) {
        const SketchPoint& point = model.points[i];
        if (point.id.isEmpty() || pointIndex.contains(point.id) || !finite(point.x) || !finite(point.y) ||
            std::abs(point.x)>1e9 || std::abs(point.y)>1e9)
            return fail(QStringLiteral("Sketch point IDs must be unique and coordinates finite within the supported range."));
        pointIndex.insert(point.id, i);
    }
    for (int i = 0; i < model.segments.size(); ++i) {
        const SketchSegment& segment = model.segments[i];
        if (segment.id.isEmpty() || segmentIndex.contains(segment.id) ||
            !pointIndex.contains(segment.startPointId) || !pointIndex.contains(segment.endPointId) ||
            segment.startPointId == segment.endPointId)
            return fail(QStringLiteral("Sketch segments need unique IDs and two distinct existing points."));
        segmentIndex.insert(segment.id, i);
    }
    if(model.circles.size()>64)return fail(QStringLiteral("A sketch supports at most 64 circles."));
    for(int i=0;i<model.circles.size();++i){
        const SketchCircle& circle=model.circles[i];
        if(circle.id.isEmpty()||circleIndex.contains(circle.id)||!pointIndex.contains(circle.centerPointId)||
           !finite(circle.radius)||circle.radius<=kGeometryEpsilon||circle.radius>1e9)
            return fail(QStringLiteral("Sketch circles need unique IDs, existing center points, and positive finite radii."));
        circleIndex.insert(circle.id,i);
    }

    auto pointExists = [&pointIndex](const QString& id) { return pointIndex.contains(id); };
    auto segmentExists = [&segmentIndex](const QString& id) { return segmentIndex.contains(id); };
    auto circleExists = [&circleIndex](const QString& id) { return circleIndex.contains(id); };
    for (const SketchConstraint& constraint : model.constraints) {
        switch (constraint.type) {
        case SketchConstraintType::Coincident:
            if (!pointExists(constraint.pointA) || !pointExists(constraint.pointB) || constraint.pointA == constraint.pointB)
                return fail(QStringLiteral("Coincident constraint references invalid points."));
            break;
        case SketchConstraintType::Horizontal:
        case SketchConstraintType::Vertical:
            if (!segmentExists(constraint.segmentA)) return fail(QStringLiteral("Horizontal or vertical constraint references an invalid segment."));
            break;
        case SketchConstraintType::SegmentLength:
            if(!segmentExists(constraint.segmentA)||!finite(constraint.value)||constraint.value<=kGeometryEpsilon)
                return fail(QStringLiteral("Segment length needs an existing segment and a positive finite value."));
            break;
        case SketchConstraintType::SignedPointLineDistance:
            if(!pointExists(constraint.pointA)||!segmentExists(constraint.segmentA)||!finite(constraint.value))
                return fail(QStringLiteral("Signed point-to-line distance needs a point, segment, and finite offset."));
            break;
        case SketchConstraintType::SegmentAngleFromX:
            if(!segmentExists(constraint.segmentA)||!finite(constraint.value)||
               std::abs(constraint.value)>2.0*std::acos(-1.0))
                return fail(QStringLiteral("Segment angle needs a segment and a finite angle in radians."));
            break;
        case SketchConstraintType::SignedPointCircleDistance:
            if(!pointExists(constraint.pointA)||!circleExists(constraint.circleA)||!finite(constraint.value))
                return fail(QStringLiteral("Signed point-to-circle distance needs a point, circle, and finite offset."));
            break;
        case SketchConstraintType::Distance:
            if (!pointExists(constraint.pointA) || !pointExists(constraint.pointB) || constraint.pointA == constraint.pointB ||
                !finite(constraint.value) || constraint.value < 0.0)
                return fail(QStringLiteral("Distance constraint needs distinct points and a finite non-negative length."));
            break;
        case SketchConstraintType::HorizontalDistance:
        case SketchConstraintType::VerticalDistance:
            if (!pointExists(constraint.pointA) || !pointExists(constraint.pointB) || constraint.pointA == constraint.pointB ||
                !finite(constraint.value))
                return fail(QStringLiteral("Directional distance needs distinct points and a finite signed value."));
            break;
        case SketchConstraintType::PointOnLine:
        case SketchConstraintType::PointOnSegment:
        case SketchConstraintType::Midpoint:
            if (!pointExists(constraint.pointA) || !segmentExists(constraint.segmentA))
                return fail(QStringLiteral("Point-on-line or midpoint constraint references missing geometry."));
            break;
        case SketchConstraintType::FixedPoint:
            if (!pointExists(constraint.pointA) || !finite(constraint.fixedX) || !finite(constraint.fixedY))
                return fail(QStringLiteral("Fixed-point constraint references an invalid point or coordinate."));
            break;
        case SketchConstraintType::EqualLength:
        case SketchConstraintType::Parallel:
        case SketchConstraintType::Perpendicular:
        case SketchConstraintType::Collinear:
            if (!segmentExists(constraint.segmentA) || !segmentExists(constraint.segmentB) || constraint.segmentA == constraint.segmentB)
                return fail(QStringLiteral("Segment constraint references invalid or identical segments."));
            break;
        case SketchConstraintType::Angle:
            if (!segmentExists(constraint.segmentA) || !segmentExists(constraint.segmentB) || constraint.segmentA == constraint.segmentB ||
                !finite(constraint.value) || std::abs(constraint.value) > 2.0 * std::acos(-1.0))
                return fail(QStringLiteral("Angle constraint needs two distinct segments and an angle in radians."));
            break;
        case SketchConstraintType::Radius:
            if(!circleExists(constraint.circleA)||!finite(constraint.value)||constraint.value<=kGeometryEpsilon)
                return fail(QStringLiteral("Radius constraint needs a circle and a positive finite value."));
            break;
        case SketchConstraintType::EqualRadius:
        case SketchConstraintType::Concentric:
            if(!circleExists(constraint.circleA)||!circleExists(constraint.circleB)||constraint.circleA==constraint.circleB)
                return fail(QStringLiteral("Circle-pair constraint needs two distinct existing circles."));
            break;
        case SketchConstraintType::PointOnCircle:
            if(!pointExists(constraint.pointA)||!circleExists(constraint.circleA))
                return fail(QStringLiteral("Point-on-circle constraint needs a point and a circle."));
            break;
        case SketchConstraintType::TangentLineCircle:
            if(!segmentExists(constraint.segmentA)||!circleExists(constraint.circleA))
                return fail(QStringLiteral("Line-circle tangent constraint needs a segment and a circle."));
            break;
        case SketchConstraintType::SymmetricAboutLine:
            if(!pointExists(constraint.pointA)||!pointExists(constraint.pointB)||
               constraint.pointA==constraint.pointB||!segmentExists(constraint.segmentA))
                return fail(QStringLiteral("Symmetry constraint needs two distinct points and an axis segment."));
            break;
        case SketchConstraintType::TangentCirclesExternal:
        case SketchConstraintType::TangentCirclesInternal:
            if(!circleExists(constraint.circleA)||!circleExists(constraint.circleB)||
               constraint.circleA==constraint.circleB)
                return fail(QStringLiteral("Circle tangency needs two distinct circles."));
            break;
        default:
            return fail(QStringLiteral("Unknown sketch constraint type."));
        }
    }

    Vector values(model.points.size() * 2+model.circles.size());
    for (int i = 0; i < model.points.size(); ++i) { values[2*i] = model.points[i].x; values[2*i+1] = model.points[i].y; }
    const int radiusOffset=model.points.size()*2;
    for(int i=0;i<model.circles.size();++i)values[radiusOffset+i]=model.circles[i].radius;
    const int variableCount = values.size();
    auto point = [&pointIndex](const Vector& v, const QString& id) {
        const int i = pointIndex.value(id);
        return QPointF(v[2*i], v[2*i+1]);
    };
    auto segmentVector = [&pointIndex, &segmentIndex, &model](const Vector& v, const QString& id) {
        const SketchSegment& s = model.segments[segmentIndex.value(id)];
        const int a = pointIndex.value(s.startPointId), b = pointIndex.value(s.endPointId);
        return SegmentVector{v[2*b]-v[2*a], v[2*b+1]-v[2*a+1]};
    };
    auto radius=[&circleIndex,radiusOffset](const Vector& v,const QString& id){return v[radiusOffset+circleIndex.value(id)];};
    auto circleCenter=[&model,&circleIndex,&point](const Vector& v,const QString& id){
        return point(v,model.circles[circleIndex.value(id)].centerPointId);
    };
    auto evaluate = [&](const Vector& v) {
        Vector residuals;
        residuals.reserve(model.constraints.size() * 2);
        for (const SketchConstraint& c : model.constraints) {
            if (c.type == SketchConstraintType::Coincident) {
                const QPointF a = point(v,c.pointA), b = point(v,c.pointB);
                residuals << a.x()-b.x() << a.y()-b.y();
            } else if (c.type == SketchConstraintType::Horizontal) {
                const SegmentVector s=segmentVector(v,c.segmentA); residuals << s.y;
            } else if (c.type == SketchConstraintType::Vertical) {
                const SegmentVector s=segmentVector(v,c.segmentA); residuals << s.x;
            } else if(c.type==SketchConstraintType::SegmentLength){
                const SegmentVector s=segmentVector(v,c.segmentA);
                const double length=std::hypot(s.x,s.y);
                residuals << (length<kGeometryEpsilon?s.x-c.value:length-c.value);
            } else if(c.type==SketchConstraintType::SegmentAngleFromX){
                const SegmentVector s=segmentVector(v,c.segmentA);
                residuals << wrapAngle(std::atan2(s.y,s.x)-c.value);
            } else if(c.type==SketchConstraintType::SignedPointLineDistance){
                const SketchSegment& segment=model.segments[segmentIndex.value(c.segmentA)];
                const QPointF start=point(v,segment.startPointId),target=point(v,c.pointA);
                const SegmentVector direction=segmentVector(v,c.segmentA);
                const double length=std::max(std::hypot(direction.x,direction.y),kGeometryEpsilon);
                residuals << (direction.x*(target.y()-start.y())-direction.y*(target.x()-start.x()))/length-c.value;
            } else if (c.type == SketchConstraintType::Distance) {
                const QPointF a=point(v,c.pointA), b=point(v,c.pointB);
                const double dx=b.x()-a.x(),dy=b.y()-a.y(),length=std::hypot(dx,dy);
                // Give a coincident pair a deterministic escape direction so a
                // positive distance can resolve from a zero-length initial guess.
                residuals << (length < kGeometryEpsilon ? dx-c.value : length-c.value);
            } else if (c.type == SketchConstraintType::FixedPoint) {
                const QPointF a=point(v,c.pointA); residuals << a.x()-c.fixedX << a.y()-c.fixedY;
            } else if (c.type == SketchConstraintType::HorizontalDistance || c.type == SketchConstraintType::VerticalDistance) {
                const QPointF a=point(v,c.pointA),b=point(v,c.pointB);
                residuals << (c.type==SketchConstraintType::HorizontalDistance?b.x()-a.x()-c.value:b.y()-a.y()-c.value);
            } else if (c.type == SketchConstraintType::PointOnLine || c.type == SketchConstraintType::PointOnSegment ||
                       c.type == SketchConstraintType::Midpoint) {
                const SketchSegment& s=model.segments[segmentIndex.value(c.segmentA)];
                const QPointF a=point(v,s.startPointId),b=point(v,s.endPointId),p=point(v,c.pointA);
                if(c.type==SketchConstraintType::Midpoint)
                    residuals << p.x()-(a.x()+b.x())/2.0 << p.y()-(a.y()+b.y())/2.0;
                else {
                    const double dx=b.x()-a.x(),dy=b.y()-a.y();
                    const double length=std::max(std::hypot(dx,dy),kGeometryEpsilon);
                    residuals << ((p.x()-a.x())*dy-(p.y()-a.y())*dx)/length;
                    if(c.type==SketchConstraintType::PointOnSegment){
                        const double fraction=((p.x()-a.x())*dx+(p.y()-a.y())*dy)/(length*length);
                        residuals << (fraction-std::clamp(fraction,0.0,1.0))*length;
                    }
                }
            } else if(c.type==SketchConstraintType::Radius){
                residuals << radius(v,c.circleA)-c.value;
            } else if(c.type==SketchConstraintType::EqualRadius){
                residuals << radius(v,c.circleA)-radius(v,c.circleB);
            } else if(c.type==SketchConstraintType::Concentric){
                const QPointF a=circleCenter(v,c.circleA),b=circleCenter(v,c.circleB);
                residuals << a.x()-b.x() << a.y()-b.y();
            } else if(c.type==SketchConstraintType::PointOnCircle){
                const QPointF p=point(v,c.pointA),center=circleCenter(v,c.circleA);
                const double dx=p.x()-center.x(),dy=p.y()-center.y(),distance=std::hypot(dx,dy);
                residuals << (distance<kGeometryEpsilon?dx-radius(v,c.circleA):distance-radius(v,c.circleA));
            } else if(c.type==SketchConstraintType::SignedPointCircleDistance){
                const QPointF p=point(v,c.pointA),center=circleCenter(v,c.circleA);
                const double dx=p.x()-center.x(),dy=p.y()-center.y(),distance=std::hypot(dx,dy);
                const double target=radius(v,c.circleA)+c.value;
                residuals << (distance<kGeometryEpsilon?dx-target:distance-target);
            } else if(c.type==SketchConstraintType::TangentLineCircle){
                const SketchSegment& segment=model.segments[segmentIndex.value(c.segmentA)];
                const QPointF a=point(v,segment.startPointId),center=circleCenter(v,c.circleA);
                const SegmentVector direction=segmentVector(v,c.segmentA);
                const double length=std::hypot(direction.x,direction.y);
                residuals << std::abs((center.x()-a.x())*direction.y-(center.y()-a.y())*direction.x)/std::max(length,kGeometryEpsilon)-radius(v,c.circleA);
            } else if(c.type==SketchConstraintType::SymmetricAboutLine){
                const SketchSegment& segment=model.segments[segmentIndex.value(c.segmentA)];
                const QPointF axisStart=point(v,segment.startPointId);
                const QPointF a=point(v,c.pointA),b=point(v,c.pointB);
                const SegmentVector direction=segmentVector(v,c.segmentA);
                const double length=std::max(std::hypot(direction.x,direction.y),kGeometryEpsilon);
                const QPointF midpoint=(a+b)/2.0;
                residuals << ((midpoint.x()-axisStart.x())*direction.y-(midpoint.y()-axisStart.y())*direction.x)/length;
                residuals << ((b.x()-a.x())*direction.x+(b.y()-a.y())*direction.y)/length;
            } else if(c.type==SketchConstraintType::TangentCirclesExternal||
                      c.type==SketchConstraintType::TangentCirclesInternal){
                const QPointF a=circleCenter(v,c.circleA),b=circleCenter(v,c.circleB);
                const double dx=b.x()-a.x(),dy=b.y()-a.y(),distance=std::hypot(dx,dy);
                const double target=c.type==SketchConstraintType::TangentCirclesExternal?
                    radius(v,c.circleA)+radius(v,c.circleB):
                    std::abs(radius(v,c.circleA)-radius(v,c.circleB));
                residuals << (distance<kGeometryEpsilon?dx-target:distance-target);
            } else {
                const SegmentVector a=segmentVector(v,c.segmentA), b=segmentVector(v,c.segmentB);
                const double la=std::hypot(a.x,a.y),lb=std::hypot(b.x,b.y);
                if (c.type == SketchConstraintType::EqualLength) residuals << la-lb;
                else if (c.type == SketchConstraintType::Parallel)
                    residuals << (a.x*b.y-a.y*b.x)/std::max(la*lb,kGeometryEpsilon);
                else if (c.type == SketchConstraintType::Perpendicular)
                    residuals << (a.x*b.x+a.y*b.y)/std::max(la*lb,kGeometryEpsilon);
                else if (c.type == SketchConstraintType::Collinear) {
                    const SketchSegment& second=model.segments[segmentIndex.value(c.segmentB)];
                    const SketchSegment& first=model.segments[segmentIndex.value(c.segmentA)];
                    const QPointF start=point(v,first.startPointId),other=point(v,second.startPointId);
                    residuals << (a.x*b.y-a.y*b.x)/std::max(la*lb,kGeometryEpsilon)
                              << ((other.x()-start.x())*a.y-(other.y()-start.y())*a.x)/std::max(la,kGeometryEpsilon);
                }
                else {
                    const double angle=std::atan2(a.x*b.y-a.y*b.x,a.x*b.x+a.y*b.y);
                    residuals << wrapAngle(angle-c.value);
                }
            }
        }
        return residuals;
    };

    auto jacobianAt = [&](const Vector& v, const Vector& baseline) {
        Matrix jacobian(baseline.size(), Vector(variableCount,0.0));
        for (int col=0;col<variableCount;++col) {
            Vector plus=v,minus=v;
            const double step=options.finiteDifferenceStep*std::max(1.0,std::abs(v[col]));
            plus[col]+=step; minus[col]-=step;
            const Vector rp=evaluate(plus),rm=evaluate(minus);
            for (int row=0;row<baseline.size();++row) jacobian[row][col]=(rp[row]-rm[row])/(2.0*step);
        }
        return jacobian;
    };

    if (model.constraints.isEmpty()) {
        result.success=true;result.converged=true;result.message=QStringLiteral("Sketch has no constraints.");
        result.degreesOfFreedom=variableCount;
        for(int i=0;i<variableCount;++i)result.locallyMobileCoordinateIndices.append(i);
        for(int i=0;i<model.points.size();++i)result.solvedPoints.append(QPointF(values[2*i],values[2*i+1]));
        for(int i=0;i<model.circles.size();++i)result.solvedCircleRadii.append(values[radiusOffset+i]);
        return result;
    }

    double lambda=1e-3;
    Vector residuals=evaluate(values);
    double cost=squaredNorm(residuals);
    if(!finite(cost))return fail(QStringLiteral("Sketch residuals overflowed the numeric range."));
    const double toleranceSquared=options.residualTolerance*options.residualTolerance;
    for(int iteration=0;iteration<options.maxIterations && cost>toleranceSquared;++iteration) {
        result.iterations=iteration+1;
        Matrix jacobian=jacobianAt(values,residuals);
        Matrix normal(variableCount,Vector(variableCount,0.0));
        Vector gradient(variableCount,0.0);
        for(int row=0;row<residuals.size();++row)
            for(int i=0;i<variableCount;++i) {
                gradient[i]+=jacobian[row][i]*residuals[row];
                for(int j=0;j<variableCount;++j)normal[i][j]+=jacobian[row][i]*jacobian[row][j];
            }
        Matrix damped=normal;
        for(int i=0;i<variableCount;++i)damped[i][i]+=lambda*std::max(normal[i][i],1.0);
        Vector rhs(variableCount);for(int i=0;i<variableCount;++i)rhs[i]=-gradient[i];
        Vector step;
        if(!solveLinearSystem(damped,rhs,&step)) break;
        double stepNorm=0.0;for(double d:step)stepNorm=std::max(stepNorm,std::abs(d));
        Vector candidate=values;for(int i=0;i<variableCount;++i)candidate[i]+=step[i];
        bool validCandidate=true;
        for(double coordinate:candidate)if(!finite(coordinate)||std::abs(coordinate)>1e9){validCandidate=false;break;}
        for(int i=0;i<model.circles.size();++i)if(candidate[radiusOffset+i]<=kGeometryEpsilon)validCandidate=false;
        if(!validCandidate){lambda=std::min(lambda*10.0,1e18);continue;}
        const Vector candidateResiduals=evaluate(candidate);
        const double candidateCost=squaredNorm(candidateResiduals);
        if(finite(candidateCost)&&candidateCost<cost) {
            values=std::move(candidate);residuals=candidateResiduals;cost=candidateCost;lambda=std::max(lambda*0.3,1e-12);
            if(stepNorm<=options.stepTolerance)break;
        } else {
            lambda=std::min(lambda*10.0,1e18);
            if(lambda>=1e18)break;
        }
    }

    int residualIndex=0;
    for(int constraintIndex=0;constraintIndex<model.constraints.size();++constraintIndex){
        const SketchConstraintType type=model.constraints[constraintIndex].type;
        const int count=(type==SketchConstraintType::Coincident||type==SketchConstraintType::FixedPoint||
                         type==SketchConstraintType::Midpoint||type==SketchConstraintType::Concentric||
                         type==SketchConstraintType::SymmetricAboutLine||type==SketchConstraintType::Collinear||
                         type==SketchConstraintType::PointOnSegment)?2:1;
        if(residualIndex+count>residuals.size())
            return fail(QStringLiteral("Sketch solver residual mapping is inconsistent."));
        double constraintMaximum=0.0;
        for(int component=0;component<count;++component){
            const double residual=residuals[residualIndex++];
            if(!finite(residual))return fail(QStringLiteral("Sketch solution is not finite."));
            constraintMaximum=std::max(constraintMaximum,std::abs(residual));
        }
        result.constraintMaximumResiduals.append(constraintMaximum);
        if(constraintMaximum>result.maximumResidual){
            result.maximumResidual=constraintMaximum;
            result.largestResidualConstraintIndex=constraintIndex;
        }
    }
    if(residualIndex!=residuals.size())return fail(QStringLiteral("Sketch solver residual mapping is incomplete."));
    const Matrix finalJacobian=jacobianAt(values,residuals);
    result.numericalRank=matrixRank(finalJacobian);
    result.degreesOfFreedom=std::max(0,variableCount-result.numericalRank);
    if(result.maximumResidual>options.residualTolerance) {
        result.message=QStringLiteral("Sketch constraints are inconsistent or did not converge (largest residual at constraint %1: %2).")
                           .arg(result.largestResidualConstraintIndex+1).arg(result.maximumResidual,0,'g',8);
        return result;
    }
    for(int i=0;i<model.circles.size();++i)if(values[radiusOffset+i]<=kGeometryEpsilon){
        result.message=QStringLiteral("A solved circle radius collapsed or became negative.");return result;
    }
    for(const SketchConstraint& constraint:model.constraints) {
        if(constraint.type!=SketchConstraintType::PointOnLine &&
           constraint.type!=SketchConstraintType::PointOnSegment &&
           constraint.type!=SketchConstraintType::SignedPointLineDistance &&
           constraint.type!=SketchConstraintType::SegmentAngleFromX &&
           constraint.type!=SketchConstraintType::Parallel &&
           constraint.type!=SketchConstraintType::Perpendicular &&
           constraint.type!=SketchConstraintType::Angle &&
           constraint.type!=SketchConstraintType::Collinear &&
           constraint.type!=SketchConstraintType::TangentLineCircle &&
           constraint.type!=SketchConstraintType::SymmetricAboutLine)continue;
        const SegmentVector first=segmentVector(values,constraint.segmentA);
        if(std::hypot(first.x,first.y)<1e-8){result.message=QStringLiteral("An angular or point-on-line constraint has a collapsed segment.");return result;}
        if(constraint.type==SketchConstraintType::PointOnLine||constraint.type==SketchConstraintType::PointOnSegment||
           constraint.type==SketchConstraintType::SignedPointLineDistance||
           constraint.type==SketchConstraintType::SegmentAngleFromX||
           constraint.type==SketchConstraintType::TangentLineCircle||
           constraint.type==SketchConstraintType::SymmetricAboutLine)continue;
        const SegmentVector second=segmentVector(values,constraint.segmentB);
        if(std::hypot(second.x,second.y)<1e-8){result.message=QStringLiteral("An angular or collinear constraint has a collapsed segment.");return result;}
    }
    for(const SketchConstraint& constraint:model.constraints){
        if(constraint.type!=SketchConstraintType::PointOnSegment)continue;
        const SketchSegment& segment=model.segments[segmentIndex.value(constraint.segmentA)];
        const QPointF start=point(values,segment.startPointId),end=point(values,segment.endPointId);
        const QPointF position=point(values,constraint.pointA);
        const double dx=end.x()-start.x(),dy=end.y()-start.y();
        const double fraction=((position.x()-start.x())*dx+(position.y()-start.y())*dy)/(dx*dx+dy*dy);
        if(fraction<-options.residualTolerance||fraction>1.0+options.residualTolerance){
            result.message=QStringLiteral("Point-on-segment solution lies beyond a segment endpoint.");return result;
        }
    }
    for(const SketchConstraint& constraint:model.constraints){
        if(constraint.type==SketchConstraintType::TangentCirclesInternal&&
           std::abs(radius(values,constraint.circleA)-radius(values,constraint.circleB))<=options.residualTolerance){
            result.message=QStringLiteral("Equal-radius coincident circles are not internally tangent.");return result;
        }
        if(constraint.type==SketchConstraintType::SymmetricAboutLine){
            const QPointF a=point(values,constraint.pointA),b=point(values,constraint.pointB);
            if(std::hypot(a.x()-b.x(),a.y()-b.y())<=options.residualTolerance){
                result.message=QStringLiteral("Mirrored points collapsed onto the symmetry axis.");return result;
            }
        }
    }
    // Determine which *whole* constraints add no independent local equation.
    // The portable helper validates its Jacobian and never mutates the sketch.
    std::vector<std::vector<double>> numericalRows;
    numericalRows.reserve(static_cast<std::size_t>(finalJacobian.size()));
    for(const auto& row:finalJacobian){
        numericalRows.emplace_back(row.cbegin(),row.cend());
    }
    std::vector<int> equationCounts;
    equationCounts.reserve(static_cast<std::size_t>(model.constraints.size()));
    for(const SketchConstraint& c:model.constraints){
        const auto type=c.type;
        equationCounts.push_back((type==SketchConstraintType::Coincident||
            type==SketchConstraintType::FixedPoint||type==SketchConstraintType::Midpoint||
            type==SketchConstraintType::Concentric||type==SketchConstraintType::SymmetricAboutLine||
            type==SketchConstraintType::Collinear||type==SketchConstraintType::PointOnSegment)?2:1);
    }
    std::vector<int> redundant,mobile;
    std::string diagnosticsError;
    if(!sketchdiagnostics::findRedundantConstraints(numericalRows,equationCounts,
                                                    &redundant,&diagnosticsError,&mobile))
        return fail(QStringLiteral("Sketch constraint diagnostics failed: ")+
                    QString::fromStdString(diagnosticsError));
    for(int index:redundant)result.redundantConstraintIndices.append(index);
    for(int index:mobile)result.locallyMobileCoordinateIndices.append(index);
    result.success=true;result.converged=true;
    result.message=result.degreesOfFreedom==0?QStringLiteral("Sketch is fully constrained."):
                   QStringLiteral("Sketch solved with %1 remaining degree(s) of freedom.").arg(result.degreesOfFreedom);
    for(int i=0;i<model.points.size();++i)result.solvedPoints.append(QPointF(values[2*i],values[2*i+1]));
    for(int i=0;i<model.circles.size();++i)result.solvedCircleRadii.append(values[radiusOffset+i]);
    return result;
}

QJsonObject sketchModelToJson(const SketchModel& model)
{
    QJsonArray points,segments,circles,arcs,splines,constraints;
    for(const SketchPoint& point:model.points){
        QJsonObject item;item.insert("id",point.id);item.insert("x",point.x);item.insert("y",point.y);points.append(item);
    }
    for(const SketchSegment& segment:model.segments){
        QJsonObject item;item.insert("id",segment.id);item.insert("start",segment.startPointId);
        item.insert("end",segment.endPointId);segments.append(item);
    }
    for(const SketchCircle& circle:model.circles){
        QJsonObject item;item.insert("id",circle.id);item.insert("center",circle.centerPointId);item.insert("radius",circle.radius);
        if(circle.cutout)item.insert("cutout",true);
        circles.append(item);
    }
    for(const SketchArc& arc:model.arcs){
        QJsonObject item;item.insert("id",arc.id);item.insert("start",arc.startPointId);
        item.insert("end",arc.endPointId);item.insert("mid",arc.midpointId);arcs.append(item);
    }
    for(const SketchSpline& spline:model.splines){
        QJsonObject item;item.insert("id",spline.id);item.insert("start",spline.startPointId);
        item.insert("end",spline.endPointId);item.insert("control1",spline.control1PointId);
        item.insert("control2",spline.control2PointId);splines.append(item);
    }
    for(const SketchConstraint& constraint:model.constraints){
        QJsonObject item;item.insert("type",static_cast<int>(constraint.type));
        item.insert("point_a",constraint.pointA);item.insert("point_b",constraint.pointB);
        item.insert("segment_a",constraint.segmentA);item.insert("segment_b",constraint.segmentB);
        item.insert("value",constraint.value);item.insert("fixed_x",constraint.fixedX);
        item.insert("fixed_y",constraint.fixedY);
        if(!constraint.circleA.isEmpty())item.insert("circle_a",constraint.circleA);
        if(!constraint.circleB.isEmpty())item.insert("circle_b",constraint.circleB);
        constraints.append(item);
    }
    QJsonObject result;result.insert("points",points);result.insert("segments",segments);result.insert("constraints",constraints);
    if(!circles.isEmpty())result.insert("circles",circles);
    if(!arcs.isEmpty())result.insert("arcs",arcs);
    if(!splines.isEmpty())result.insert("splines",splines);
    return result;
}

bool sketchModelFromJson(const QJsonObject& object,SketchModel* model,QString* error)
{
    auto fail=[error](const QString& message){if(error)*error=message;return false;};
    if(!model||object.size()<3||object.size()>6||!object.value("points").isArray()||
       !object.value("segments").isArray()||!object.value("constraints").isArray())
        return fail(QStringLiteral("Invalid saved constrained sketch arrays."));
    for(auto it=object.begin();it!=object.end();++it){
        if(it.key()!="points"&&it.key()!="segments"&&it.key()!="constraints"&&
           it.key()!="circles"&&it.key()!="arcs"&&it.key()!="splines")
            return fail(QStringLiteral("Unknown saved sketch property."));
    }
    for(const QString& key:{"circles","arcs","splines"})
        if(object.contains(key)&&!object.value(key).isArray())
            return fail(QStringLiteral("Invalid saved sketch curve array."));
    const QJsonArray points=object.value("points").toArray(),segments=object.value("segments").toArray();
    const QJsonArray constraints=object.value("constraints").toArray();
    if(points.isEmpty()||points.size()>64||segments.size()>128||constraints.size()>128||object.value("circles").toArray().size()>64||object.value("arcs").toArray().size()>32||
       object.value("splines").toArray().size()>32)
        return fail(QStringLiteral("Saved constrained sketch exceeds its entity limits."));
    SketchModel parsed;
    for(const QJsonValue& entry:points){
        if(!entry.isObject())return fail(QStringLiteral("Invalid saved sketch point."));
        const QJsonObject p=entry.toObject();
        if(p.size()!=3||!p.value("id").isString()||!p.value("x").isDouble()||!p.value("y").isDouble()||
           p.value("id").toString().size()>64)return fail(QStringLiteral("Invalid saved sketch point fields."));
        parsed.points.append({p.value("id").toString(),p.value("x").toDouble(),p.value("y").toDouble()});
    }
    for(const QJsonValue& entry:segments){
        if(!entry.isObject())return fail(QStringLiteral("Invalid saved sketch segment."));
        const QJsonObject s=entry.toObject();
        if(s.size()!=3||!s.value("id").isString()||!s.value("start").isString()||!s.value("end").isString()||
           s.value("id").toString().size()>64||s.value("start").toString().size()>64||s.value("end").toString().size()>64)
            return fail(QStringLiteral("Invalid saved sketch segment fields."));
        parsed.segments.append({s.value("id").toString(),s.value("start").toString(),s.value("end").toString()});
    }
    for(const QJsonValue& entry:object.value("circles").toArray()){
        if(!entry.isObject())return fail(QStringLiteral("Invalid saved sketch circle."));
        const QJsonObject c=entry.toObject();
        if((c.size()!=3&&c.size()!=4)||!c.value("id").isString()||!c.value("center").isString()||!c.value("radius").isDouble()||
           (c.contains("cutout")&&!c.value("cutout").isBool())||
           c.value("id").toString().size()>64||c.value("center").toString().size()>64)
            return fail(QStringLiteral("Invalid saved sketch circle fields."));
        parsed.circles.append({c.value("id").toString(),c.value("center").toString(),c.value("radius").toDouble(),c.value("cutout").toBool()});
    }
    QSet<QString> pointIds;
    for(const auto& point:parsed.points)pointIds.insert(point.id);
    QSet<QString> curveIds;
    for(const QJsonValue& entry:object.value("arcs").toArray()){
        if(!entry.isObject())return fail(QStringLiteral("Invalid saved sketch arc."));
        const auto a=entry.toObject();
        if(a.size()!=4||!a.value("id").isString()||!a.value("start").isString()||
           !a.value("end").isString()||!a.value("mid").isString())
            return fail(QStringLiteral("Invalid saved arc fields."));
        const SketchArc arc{a.value("id").toString(),a.value("start").toString(),
            a.value("end").toString(),a.value("mid").toString()};
        if(arc.id.isEmpty()||arc.id.size()>64||curveIds.contains(arc.id)||
           !pointIds.contains(arc.startPointId)||!pointIds.contains(arc.endPointId)||
           !pointIds.contains(arc.midpointId))return fail(QStringLiteral("Invalid saved arc point references."));
        curveIds.insert(arc.id);parsed.arcs.append(arc);
    }
    for(const QJsonValue& entry:object.value("splines").toArray()){
        if(!entry.isObject())return fail(QStringLiteral("Invalid saved sketch spline."));
        const auto a=entry.toObject();
        if(a.size()!=5||!a.value("id").isString()||!a.value("start").isString()||
           !a.value("end").isString()||!a.value("control1").isString()||
           !a.value("control2").isString())return fail(QStringLiteral("Invalid saved spline fields."));
        const SketchSpline spline{a.value("id").toString(),a.value("start").toString(),
            a.value("end").toString(),a.value("control1").toString(),a.value("control2").toString()};
        if(spline.id.isEmpty()||spline.id.size()>64||curveIds.contains(spline.id)||
           !pointIds.contains(spline.startPointId)||!pointIds.contains(spline.endPointId)||
           !pointIds.contains(spline.control1PointId)||!pointIds.contains(spline.control2PointId))
            return fail(QStringLiteral("Invalid saved spline point references."));
        curveIds.insert(spline.id);parsed.splines.append(spline);
    }
    for(const QJsonValue& entry:constraints){
        if(!entry.isObject())return fail(QStringLiteral("Invalid saved sketch constraint."));
        const QJsonObject c=entry.toObject();
        if(c.size()<8||c.size()>10||!c.value("type").isDouble()||!c.value("point_a").isString()||
           !c.value("point_b").isString()||!c.value("segment_a").isString()||!c.value("segment_b").isString()||
           !c.value("value").isDouble()||!c.value("fixed_x").isDouble()||!c.value("fixed_y").isDouble())
            return fail(QStringLiteral("Invalid saved sketch constraint fields."));
        const double typeValue=c.value("type").toDouble();
        if(typeValue<0||typeValue>static_cast<int>(SketchConstraintType::SignedPointCircleDistance)||std::floor(typeValue)!=typeValue)
            return fail(QStringLiteral("Unknown saved sketch constraint type."));
        SketchConstraint constraint;constraint.type=static_cast<SketchConstraintType>(static_cast<int>(typeValue));
        constraint.pointA=c.value("point_a").toString();constraint.pointB=c.value("point_b").toString();
        constraint.segmentA=c.value("segment_a").toString();constraint.segmentB=c.value("segment_b").toString();
        constraint.value=c.value("value").toDouble();constraint.fixedX=c.value("fixed_x").toDouble();
        constraint.fixedY=c.value("fixed_y").toDouble();
        if((c.contains("circle_a")&&!c.value("circle_a").isString())||(c.contains("circle_b")&&!c.value("circle_b").isString()))
            return fail(QStringLiteral("Invalid saved sketch circle references."));
        constraint.circleA=c.value("circle_a").toString();constraint.circleB=c.value("circle_b").toString();
        if(constraint.pointA.size()>64||constraint.pointB.size()>64||
           constraint.segmentA.size()>64||constraint.segmentB.size()>64||constraint.circleA.size()>64||constraint.circleB.size()>64)
            return fail(QStringLiteral("Saved sketch reference exceeds the supported length."));
        parsed.constraints.append(constraint);
    }
    const SketchSolveResult solution=SketchSolver::solve(parsed);
    if(!solution.success)return fail(QStringLiteral("Saved constrained sketch does not solve: ")+solution.message);
    *model=std::move(parsed);return true;
}

} // namespace cad
