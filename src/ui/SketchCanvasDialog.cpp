#include "SketchCanvasDialog.h"
#include "core/SceneDocument.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QPainterPath>
#include <QLabel>
#include <QListWidget>
#include <QKeyEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

SketchCanvasWidget::SketchCanvasWidget(QWidget* parent):QWidget(parent){
    setMinimumSize(620,440);setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
}
void SketchCanvasWidget::setSketch(const cad::SketchModel& sketch,const QStringList& boundary){
    model_=sketch;boundary_=boundary;closed_=boundary.size()>=3;
    tool_=closed_?Tool::Select:Tool::Polyline;relation_=Relation::None;
    firstRelationPoint_.clear();firstRelationSegment_.clear();firstRelationCircle_.clear();
    dragging_=-1;circlePending_=false;undo_.clear();zoomToFit();update();
}
void SketchCanvasWidget::setTool(Tool tool){
    relation_=Relation::None;firstRelationPoint_.clear();firstRelationSegment_.clear();firstRelationCircle_.clear();
    tool_=tool;circlePending_=false;rectanglePending_=false;slotStep_=0;
    curveEdgeIndex_=-1;pendingControls_.clear();update();
}
void SketchCanvasWidget::setRelation(Relation relation){
    relation_=relation;firstRelationPoint_.clear();firstRelationSegment_.clear();firstRelationCircle_.clear();
    tool_=Tool::Select;dragging_=-1;
    if(status)status(relation==Relation::None?"Select vertices and handles to move them.":
        "Constraint tool ready: click a vertex or straight edge. Two-item constraints need two clicks.");
    update();
}
bool SketchCanvasWidget::tryRelation(cad::SketchModel trial,const QString& description){
    const cad::SketchSolveResult solved=cad::SketchSolver::solve(trial);
    if(!solved.success||solved.solvedPoints.size()!=trial.points.size()||
       solved.solvedCircleRadii.size()!=trial.circles.size()){
        if(status)status("Constraint rejected: "+solved.message);return false;
    }
    for(int i=0;i<trial.points.size();++i){
        trial.points[i].x=solved.solvedPoints[i].x();
        trial.points[i].y=solved.solvedPoints[i].y();
    }
    // Persist solver-computed radii too. Otherwise editing a Radius or EqualRadius
    // constraint would show a valid numerical solution but leave the drawn hole
    // at the old radius, silently disagreeing with the saved dimension.
    for(int i=0;i<trial.circles.size();++i)
        trial.circles[i].radius=solved.solvedCircleRadii[i];
    if(closed_&&boundary_.size()>=3){
        QString reason;TopoDS_Shape result;
        if(!cad::buildSketchExtrusion(trial,boundary_,1.0,{},&result,&reason)){
            if(status)status("Constraint would invalidate the profile: "+reason);return false;
        }
    }
    checkpoint();model_=std::move(trial);update();
    if(status){
        QString diagnostics=description+QStringLiteral(" · remaining DOF: %1").arg(solved.degreesOfFreedom);
        if(solved.degreesOfFreedom>0 && !solved.locallyMobileCoordinateIndices.isEmpty()){
            QStringList labels;
            const int pointCoordinates=2*trial.points.size();
            for(int index:solved.locallyMobileCoordinateIndices){
                if(labels.size()>=4)break;
                if(index<pointCoordinates){
                    const int point=index/2;
                    labels<<QStringLiteral("point %1 %2").arg(point+1).arg(index%2==0?"X":"Y");
                }else{
                    labels<<QStringLiteral("circle %1 radius").arg(index-pointCoordinates+1);
                }
            }
            diagnostics+=QStringLiteral(" · locally mobile: %1").arg(labels.join(", "));
            if(solved.locallyMobileCoordinateIndices.size()>labels.size())
                diagnostics+=QStringLiteral(" (+%1 more)")
                    .arg(solved.locallyMobileCoordinateIndices.size()-labels.size());
        }
        if(!solved.redundantConstraintIndices.isEmpty()){
            QStringList indices;
            for(int index:solved.redundantConstraintIndices){
                if(indices.size()==6)break;
                indices.append(QString::number(index+1));
            }
            QString summary=indices.join(QStringLiteral(", "));
            if(solved.redundantConstraintIndices.size()>indices.size())
                summary+=QStringLiteral(" +%1 more")
                    .arg(solved.redundantConstraintIndices.size()-indices.size());
            diagnostics+=QStringLiteral(" · redundant constraints: %1 (review before adding more)")
                .arg(summary);
        }
        status(diagnostics);
    }
    return true;
}
bool SketchCanvasWidget::constrainSegmentPair(const QString& firstId,const QString& secondId,
        cad::SketchConstraintType type,double angleDegrees){
    using T=cad::SketchConstraintType;
    if(firstId==secondId||firstId.isEmpty()||secondId.isEmpty()||
       (type!=T::Parallel&&type!=T::Perpendicular&&type!=T::EqualLength&&type!=T::Angle)||
       !std::isfinite(angleDegrees)||std::abs(angleDegrees)>180.0){
        if(status)status("Select two distinct straight segments and a valid signed angle.");return false;
    }
    const auto findSegment=[&](const QString& id)->const cad::SketchSegment*{
        for(const auto& segment:model_.segments)if(segment.id==id)return &segment;
        return nullptr;
    };
    const auto* first=findSegment(firstId),*second=findSegment(secondId);
    if(!first||!second){if(status)status("A selected segment is missing from the sketch.");return false;}
    const auto curved=[&](const cad::SketchSegment& segment){
        for(const auto& arc:model_.arcs)
            if(arc.startPointId==segment.startPointId&&arc.endPointId==segment.endPointId)return true;
        for(const auto& spline:model_.splines)
            if(spline.startPointId==segment.startPointId&&spline.endPointId==segment.endPointId)return true;
        return false;
    };
    if(curved(*first)||curved(*second)){
        if(status)status("A straight-segment relation cannot use an arc or spline.");return false;
    }
    cad::SketchModel trial=model_;
    // The hand-drawn profile initially fixes every vertex. Release only the
    // second segment's endpoint so a driven relation can adjust it; preserve
    // every unrelated dimension and the selected first segment's anchors.
    trial.constraints.erase(std::remove_if(trial.constraints.begin(),trial.constraints.end(),
        [&](const auto& c){return c.type==T::FixedPoint&&c.pointA==second->endPointId;}),
        trial.constraints.end());
    cad::SketchConstraint constraint;constraint.type=type;
    constraint.segmentA=firstId;constraint.segmentB=secondId;
    if(type==T::Angle)constraint.value=angleDegrees*std::acos(-1.0)/180.0;
    trial.constraints.append(constraint);
    return tryRelation(std::move(trial),"Two-edge sketch constraint added");
}
bool SketchCanvasWidget::constrainCirclePair(const QString& firstId,const QString& secondId,
        cad::SketchConstraintType type){
    using T=cad::SketchConstraintType;
    if(firstId==secondId||firstId.isEmpty()||secondId.isEmpty()||
       (type!=T::EqualRadius&&type!=T::Concentric)){
        if(status)status("Select two distinct circles.");return false;
    }
    const auto findCircle=[&](const QString& id)->const cad::SketchCircle*{
        for(const auto& circle:model_.circles)if(circle.id==id)return &circle;
        return nullptr;
    };
    const auto* first=findCircle(firstId),*second=findCircle(secondId);
    if(!first||!second){if(status)status("A selected circle is missing from the sketch.");return false;}
    if(type==T::Concentric&&first->centerPointId==second->centerPointId){
        if(status)status("These circles already share the same center point.");return false;
    }
    cad::SketchModel trial=model_;
    // Replace only the second circle's conflicting driving measurement. The
    // first circle stays the reference; all edits are atomic and undoable.
    trial.constraints.erase(std::remove_if(trial.constraints.begin(),trial.constraints.end(),
        [&](const auto& c){return (type==T::EqualRadius&&c.type==T::Radius&&c.circleA==secondId)||
            (type==T::Concentric&&c.type==T::FixedPoint&&c.pointA==second->centerPointId);}),
        trial.constraints.end());
    cad::SketchConstraint constraint;constraint.type=type;
    constraint.circleA=firstId;constraint.circleB=secondId;
    trial.constraints.append(constraint);
    return tryRelation(std::move(trial),"Two-circle sketch constraint added");
}
void SketchCanvasWidget::handleRelationClick(const QPointF& cursor){
    using T=cad::SketchConstraintType;
    auto release=[&](cad::SketchModel* sketch,const QString& pointId){
        sketch->constraints.erase(std::remove_if(sketch->constraints.begin(),sketch->constraints.end(),
            [&](const cad::SketchConstraint& c){return c.type==T::FixedPoint&&c.pointA==pointId;}),
            sketch->constraints.end());
    };
    const bool circleRelation=relation_==Relation::EqualRadius||relation_==Relation::Concentric;
    if(circleRelation){
        const int index=hitCircle(cursor);
        if(index<0){if(status)status("Click near the circumference of a sketch circle.");return;}
        const QString circleId=model_.circles[index].id;
        if(firstRelationCircle_.isEmpty()){
            firstRelationCircle_=circleId;
            if(status)status("First circle selected. Click the second circle's circumference.");
            return;
        }
        if(constrainCirclePair(firstRelationCircle_,circleId,
                relation_==Relation::EqualRadius?T::EqualRadius:T::Concentric))
            firstRelationCircle_.clear();
        return;
    }
    const bool edgeRelation=relation_==Relation::Horizontal||relation_==Relation::Vertical||
        relation_==Relation::Length||relation_==Relation::Parallel||relation_==Relation::Perpendicular||
        relation_==Relation::EqualLength||relation_==Relation::Angle;
    cad::SketchModel trial=model_;cad::SketchConstraint c;
    if(edgeRelation){
        const int index=nearestBoundaryEdge(cursor);
        if(index<0||index>=boundary_.size()){
            if(status)status("Click near a straight boundary segment.");return;
        }
        const auto a=boundary_[index],b=boundary_[(index+1)%boundary_.size()];
        QString segment;
        for(const auto& s:trial.segments)if(s.startPointId==a&&s.endPointId==b){segment=s.id;break;}
        if(segment.isEmpty()){
            if(status)status("This boundary edge has no segment identity.");return;
        }
        const bool curved=std::any_of(trial.arcs.cbegin(),trial.arcs.cend(),[&](const auto& v){return v.startPointId==a&&v.endPointId==b;})||
            std::any_of(trial.splines.cbegin(),trial.splines.cend(),[&](const auto& v){return v.startPointId==a&&v.endPointId==b;});
        if(curved){if(status)status("Choose a straight segment rather than an arc or spline.");return;}
        if(relation_==Relation::Parallel||relation_==Relation::Perpendicular||
           relation_==Relation::EqualLength||relation_==Relation::Angle){
            if(firstRelationSegment_.isEmpty()){
                firstRelationSegment_=segment;if(status)status("First edge selected. Click the second straight edge.");return;
            }
            if(segment==firstRelationSegment_){
                if(status)status("Select two distinct straight edges.");return;
            }
            double degrees=0.0;
            if(relation_==Relation::Angle){
                const auto get=[&](const QString& id){
                    for(const auto& pt:trial.points)if(pt.id==id)return QPointF(pt.x,pt.y);
                    return QPointF();};
                const auto first=std::find_if(trial.segments.cbegin(),trial.segments.cend(),
                    [&](const auto& item){return item.id==firstRelationSegment_;});
                if(first==trial.segments.cend()){firstRelationSegment_.clear();return;}
                const QPointF v=get(first->endPointId)-get(first->startPointId);
                const QPointF w=get(b)-get(a);
                const double initial=std::atan2(v.x()*w.y()-v.y()*w.x(),
                    v.x()*w.x()+v.y()*w.y())*180.0/std::acos(-1.0);
                bool accepted=false;
                degrees=QInputDialog::getDouble(this,"Sketch angle",
                    "Signed angle from first to second segment (degrees):",
                    initial,-180.0,180.0,3,&accepted);
                if(!accepted)return;
            }
            const T type=relation_==Relation::Parallel?T::Parallel:
                relation_==Relation::Perpendicular?T::Perpendicular:
                relation_==Relation::EqualLength?T::EqualLength:T::Angle;
            if(constrainSegmentPair(firstRelationSegment_,segment,type,degrees))
                firstRelationSegment_.clear();
            return;
        }else{
            c.segmentA=segment;c.type=relation_==Relation::Horizontal?T::Horizontal:
                relation_==Relation::Vertical?T::Vertical:T::SegmentLength;
            if(relation_==Relation::Length){
                const auto get=[&](const QString& id){
                    for(const auto& pt:trial.points)if(pt.id==id)return QPointF(pt.x,pt.y);
                    return QPointF();};
                const double current=std::hypot((get(a)-get(b)).x(),(get(a)-get(b)).y());
                bool accepted=false;
                c.value=QInputDialog::getDouble(this,"Sketch length","Length (mm):",current,0.00001,1e6,4,&accepted);
                if(!accepted)return;
            }
            release(&trial,b);
        }
    }else{
        const int index=hitPoint(cursor);
        if(index<0){if(status)status("Click a sketch vertex or control point.");return;}
        const auto& point=trial.points[index];
        if(relation_==Relation::Fixed){
            checkpoint();fixPoint(point.id,QPointF(point.x,point.y));if(status)status("Point fixed at its current location.");update();return;
        }
        if(relation_==Relation::Unfix){
            release(&trial,point.id);tryRelation(std::move(trial),"Point released");return;
        }
        if(firstRelationPoint_.isEmpty()){
            firstRelationPoint_=point.id;if(status)status("First point selected. Click the second vertex.");return;
        }
        if(point.id==firstRelationPoint_){if(status)status("Choose two different points.");return;}
        c.pointA=firstRelationPoint_;c.pointB=point.id;
        c.type=relation_==Relation::Coincident?T::Coincident:T::Distance;
        firstRelationPoint_.clear();
        if(relation_==Relation::Distance){
            const auto get=[&](const QString& id){
                for(const auto& pt:trial.points)if(pt.id==id)return QPointF(pt.x,pt.y);
                return QPointF();};
            const auto delta=get(c.pointA)-get(c.pointB);
            bool accepted=false;
            c.value=QInputDialog::getDouble(this,"Sketch distance","Distance (mm):",
                std::hypot(delta.x(),delta.y()),0.00001,1e6,4,&accepted);
            if(!accepted)return;
        }
        release(&trial,point.id);
    }
    trial.constraints.append(c);
    tryRelation(std::move(trial),"Sketch relation added");
}
namespace {
QString sketchConstraintKind(cad::SketchConstraintType kind) {
    using T=cad::SketchConstraintType;
    switch(kind) {
    case T::Coincident:return "Coincident points";
    case T::Horizontal:return "Horizontal edge";
    case T::Vertical:return "Vertical edge";
    case T::Distance:return "Point distance";
    case T::FixedPoint:return "Fixed point";
    case T::EqualLength:return "Equal lengths";
    case T::Parallel:return "Parallel edges";
    case T::Perpendicular:return "Perpendicular edges";
    case T::Angle:return "Angle between edges";
    case T::HorizontalDistance:return "Horizontal distance";
    case T::VerticalDistance:return "Vertical distance";
    case T::PointOnLine:return "Point on line";
    case T::Midpoint:return "Midpoint";
    case T::Collinear:return "Collinear edges";
    case T::Radius:return "Circle radius";
    case T::EqualRadius:return "Equal circle radii";
    case T::Concentric:return "Concentric circles";
    case T::PointOnCircle:return "Point on circle";
    case T::TangentLineCircle:return "Line-circle tangent";
    case T::SymmetricAboutLine:return "Symmetric points";
    case T::TangentCirclesExternal:return "External circle tangent";
    case T::TangentCirclesInternal:return "Internal circle tangent";
    case T::PointOnSegment:return "Point on segment";
    case T::SegmentLength:return "Segment length";
    case T::SignedPointLineDistance:return "Signed point-to-line offset";
    case T::SegmentAngleFromX:return "Segment angle from X";
    case T::SignedPointCircleDistance:return "Signed point-to-circle offset";
    }
    return "Unknown constraint";
}
bool editableSketchDimension(cad::SketchConstraintType type){
    using T=cad::SketchConstraintType;
    switch(type){
    case T::Distance:case T::SegmentLength:case T::Radius:
    case T::HorizontalDistance:case T::VerticalDistance:
    case T::SignedPointLineDistance:case T::SignedPointCircleDistance:
    case T::Angle:case T::SegmentAngleFromX:return true;
    default:return false;
    }
}
bool validSketchDimension(cad::SketchConstraintType type,double value){
    if(!std::isfinite(value))return false;
    using T=cad::SketchConstraintType;
    if(type==T::Distance||type==T::SegmentLength||type==T::Radius)
        return value>1e-5 && value<=1e6;
    if(type==T::Angle||type==T::SegmentAngleFromX)
        return std::abs(value)<=2*std::acos(-1.0);
    return std::abs(value)<=1e6;
}
}
QStringList SketchCanvasWidget::constraintDescriptions() const {
    QStringList lines;
    for(int i=0;i<model_.constraints.size();++i){
        const auto& c=model_.constraints[i];
        QStringList references;
        for(const QString& id:{c.pointA,c.pointB,c.segmentA,c.segmentB,c.circleA,c.circleB})
            if(!id.isEmpty())references.append(id);
        QString line=QStringLiteral("%1. %2 [%3]")
            .arg(i+1).arg(sketchConstraintKind(c.type),references.join(", "));
        if(editableSketchDimension(c.type)){
            const bool angular=c.type==cad::SketchConstraintType::Angle ||
                c.type==cad::SketchConstraintType::SegmentAngleFromX;
            line+=QStringLiteral(" = %1 %2").arg(angular?c.value*180.0/std::acos(-1.0):c.value,0,'g',10)
                .arg(angular?QStringLiteral("deg"):QStringLiteral("mm"));
        }else if(c.type==cad::SketchConstraintType::FixedPoint){
            line+=QStringLiteral(" (%1, %2) mm").arg(c.fixedX,0,'g',8).arg(c.fixedY,0,'g',8);
        }
        lines.append(line);
    }
    return lines;
}
bool SketchCanvasWidget::changeConstraintDimension(int index,double value) {
    if(index<0||index>=model_.constraints.size()||
       !editableSketchDimension(model_.constraints[index].type)||
       !validSketchDimension(model_.constraints[index].type,value)){
        if(status)status("Invalid constraint selection or dimension; no sketch changes were made.");
        return false;
    }
    if(model_.constraints[index].value==value)return true;
    cad::SketchModel trial=model_;
    trial.constraints[index].value=value;
    return tryRelation(std::move(trial),"Sketch dimension edited");
}
bool SketchCanvasWidget::changeFixedPointAnchor(int index,double xMm,double yMm){
    using T=cad::SketchConstraintType;
    if(index<0||index>=model_.constraints.size()||
       model_.constraints[index].type!=T::FixedPoint||
       !std::isfinite(xMm)||!std::isfinite(yMm)||
       std::abs(xMm)>1e6||std::abs(yMm)>1e6){
        if(status)status("Invalid fixed-point anchor or coordinates; sketch unchanged.");
        return false;
    }
    const auto& current=model_.constraints[index];
    if(current.fixedX==xMm&&current.fixedY==yMm)return true;
    cad::SketchModel trial=model_;
    trial.constraints[index].fixedX=xMm;
    trial.constraints[index].fixedY=yMm;
    return tryRelation(std::move(trial),"Fixed-point anchor edited");
}
bool SketchCanvasWidget::removeConstraint(int index) {
    if(index<0||index>=model_.constraints.size()){
        if(status)status("Select an existing constraint to remove.");
        return false;
    }
    cad::SketchModel trial=model_;
    trial.constraints.removeAt(index);
    return tryRelation(std::move(trial),"Sketch constraint removed");
}
void SketchCanvasWidget::checkpoint(){
    if(undo_.size()>=50)undo_.removeFirst();
    undo_.append({model_,boundary_,closed_});
}
void SketchCanvasWidget::undoEdit(){
    if(undo_.isEmpty()){if(status)status("Nothing to undo in this sketch.");return;}
    const auto previous=undo_.takeLast();model_=previous.sketch;boundary_=previous.boundary;
    closed_=previous.closed;dragging_=-1;circlePending_=false;
    firstRelationPoint_.clear();firstRelationSegment_.clear();firstRelationCircle_.clear();
    curveEdgeIndex_=-1;pendingControls_.clear();update();
}
QPointF SketchCanvasWidget::screen(const QPointF& point) const {
    return QPointF(width()/2.0+(point.x()-center_.x())*pixelsPerMm_,
                   height()/2.0-(point.y()-center_.y())*pixelsPerMm_);
}
QPointF SketchCanvasWidget::world(const QPointF& point) const {
    return QPointF(center_.x()+(point.x()-width()/2.0)/pixelsPerMm_,
                   center_.y()-(point.y()-height()/2.0)/pixelsPerMm_);
}
QPointF SketchCanvasWidget::snapped(const QPointF& mm,const QString& exclude) const {
    if(snapPoints_){
        double best=10.0/pixelsPerMm_;QPointF hit;
        bool found=false;
        for(const auto& point:model_.points){
            if(point.id==exclude)continue;
            const double distance=std::hypot(mm.x()-point.x,mm.y()-point.y);
            if(distance<best){best=distance;hit=QPointF(point.x,point.y);found=true;}
        }
        if(found)return hit;
    }
    if(snapGrid_)return QPointF(std::round(mm.x()),std::round(mm.y()));
    return mm;
}
int SketchCanvasWidget::hitPoint(const QPointF& cursor) const {
    int chosen=-1;double best=11;
    for(int i=0;i<model_.points.size();++i){
        const auto& p=model_.points[i];const QPointF at=screen(QPointF(p.x,p.y));
        const double d=std::hypot(at.x()-cursor.x(),at.y()-cursor.y());
        if(d<best){chosen=i;best=d;}
    }
    return chosen;
}
int SketchCanvasWidget::hitCircle(const QPointF& cursor) const {
    int closest=-1;double distancePixels=11.0;
    for(int i=0;i<model_.circles.size();++i){
        const auto& circle=model_.circles[i];
        const auto center=std::find_if(model_.points.cbegin(),model_.points.cend(),
            [&](const auto& p){return p.id==circle.centerPointId;});
        if(center==model_.points.cend()||!std::isfinite(circle.radius)||circle.radius<=0)continue;
        const QPointF at=screen(QPointF(center->x,center->y));
        const double ring=std::abs(std::hypot(at.x()-cursor.x(),at.y()-cursor.y())-
                                   circle.radius*pixelsPerMm_);
        if(ring<distancePixels){distancePixels=ring;closest=i;}
    }
    return closest;
}
QString SketchCanvasWidget::newPointId() const {
    int suffix=1;
    for(;;++suffix){
        const QString id=QStringLiteral("point_%1").arg(suffix);
        const auto found=std::find_if(model_.points.cbegin(),model_.points.cend(),[&](const auto& point){return point.id==id;});
        if(found==model_.points.cend())return id;
    }
}
void SketchCanvasWidget::fixPoint(const QString& id,const QPointF& point){
    for(auto& p:model_.points)if(p.id==id){p.x=point.x();p.y=point.y();break;}
    bool found=false;
    for(auto& constraint:model_.constraints)if(constraint.type==cad::SketchConstraintType::FixedPoint&&constraint.pointA==id){
        constraint.fixedX=point.x();constraint.fixedY=point.y();found=true;
    }
    if(!found){
        cad::SketchConstraint fix;fix.type=cad::SketchConstraintType::FixedPoint;
        fix.pointA=id;fix.fixedX=point.x();fix.fixedY=point.y();model_.constraints.append(fix);
    }
}
void SketchCanvasWidget::addPoint(const QPointF& point){
    if(closed_){if(status)status("Use New sketch to start another outline, or Select to move points.");return;}
    if(boundary_.size()>=64){if(status)status("Sketch boundary limit is 64 vertices.");return;}
    checkpoint();const QString id=newPointId();
    model_.points.append({id,point.x(),point.y()});
    boundary_.append(id);fixPoint(id,point);
    if(status)status(QStringLiteral("Vertex %1 placed; click first vertex to close, or press Close outline.").arg(boundary_.size()));
    update();
}
void SketchCanvasWidget::regenerateBoundarySegments(){
    // Newly authored freehand profiles own only fixed-point constraints.
    // Editing old sketches retains their original segment identities.
    model_.segments.clear();
    for(int i=0;i<boundary_.size();++i){
        model_.segments.append({QStringLiteral("boundary_%1").arg(i+1),boundary_[i],boundary_[(i+1)%boundary_.size()]});
    }
}
void SketchCanvasWidget::closeBoundary(){
    if(closed_)return;
    if(boundary_.size()<3){if(status)status("An extrusion requires at least three boundary vertices.");return;}
    checkpoint();closed_=true;regenerateBoundarySegments();tool_=Tool::Select;
    if(status)status("Outline closed. Move points or add circle cutouts, then create the extrusion.");
    update();
}
void SketchCanvasWidget::clearSketch(){
    checkpoint();model_={};boundary_.clear();closed_=false;circlePending_=false;
    relation_=Relation::None;firstRelationPoint_.clear();firstRelationSegment_.clear();firstRelationCircle_.clear();
    curveEdgeIndex_=-1;pendingControls_.clear();rectanglePending_=false;slotStep_=0;
    tool_=Tool::Polyline;update();
    if(status)status("Sketch cleared. Click to draw the new polygon outline.");
}
void SketchCanvasWidget::alignEdgeUnderCursor(bool horizontal){
    const int edge=nearestBoundaryEdge(lastCursor_);
    if(edge<0){if(status)status("Point to a straight boundary edge before aligning it.");return;}
    const QString from=boundary_[edge],to=boundary_[(edge+1)%boundary_.size()];
    for(const auto& arc:model_.arcs)if(arc.startPointId==from&&arc.endPointId==to){
        if(status)status("Remove the circular arc before aligning this edge.");return;
    }
    for(const auto& spline:model_.splines)if(spline.startPointId==from&&spline.endPointId==to){
        if(status)status("Remove the Bézier edge before aligning it.");return;
    }
    int first=-1,second=-1;
    for(int i=0;i<model_.points.size();++i){
        if(model_.points[i].id==from)first=i;
        if(model_.points[i].id==to)second=i;
    }
    if(first<0||second<0)return;
    cad::SketchModel trial=model_;
    auto& destination=trial.points[second];
    if(horizontal)destination.y=trial.points[first].y;
    else destination.x=trial.points[first].x;
    for(auto& constraint:trial.constraints)if(constraint.type==cad::SketchConstraintType::FixedPoint&&
        constraint.pointA==to){constraint.fixedX=destination.x;constraint.fixedY=destination.y;}
    QString segment;
    for(const auto& item:trial.segments)if(item.startPointId==from&&item.endPointId==to){segment=item.id;break;}
    if(segment.isEmpty()){if(status)status("Cannot find the selected edge's constraint identity.");return;}
    trial.constraints.erase(std::remove_if(trial.constraints.begin(),trial.constraints.end(),[&](const auto& c){
        return c.segmentA==segment&&(c.type==cad::SketchConstraintType::Horizontal||
            c.type==cad::SketchConstraintType::Vertical);
    }),trial.constraints.end());
    cad::SketchConstraint relation;relation.type=horizontal?cad::SketchConstraintType::Horizontal:
        cad::SketchConstraintType::Vertical;relation.segmentA=segment;trial.constraints.append(relation);
    TopoDS_Shape checked;QString error;
    if(!cad::buildSketchExtrusion(trial,boundary_,1.0,{},&checked,&error)){
        if(status)status("Alignment rejected: "+error);return;
    }
    checkpoint();model_=trial;update();
    if(status)status(horizontal?"Aligned edge horizontally and saved its constraint.":
                                 "Aligned edge vertically and saved its constraint.");
}
void SketchCanvasWidget::removeCurveUnderCursor(){
    const int edge=nearestBoundaryEdge(lastCursor_);
    if(edge<0){if(status)status("Hover a curved boundary edge first.");return;}
    const QString a=boundary_[edge],b=boundary_[(edge+1)%boundary_.size()];
    const auto curved=[&](const auto& c){return c.startPointId==a&&c.endPointId==b;};
    const auto arcs=std::count_if(model_.arcs.cbegin(),model_.arcs.cend(),curved);
    const auto splines=std::count_if(model_.splines.cbegin(),model_.splines.cend(),curved);
    if(!arcs&&!splines){if(status)status("The hovered edge is already straight.");return;}
    checkpoint();
    QStringList controls;
    for(const auto& arc:model_.arcs)if(curved(arc))controls.append(arc.midpointId);
    for(const auto& spline:model_.splines)if(curved(spline)){
        controls.append(spline.control1PointId);controls.append(spline.control2PointId);
    }
    model_.arcs.erase(std::remove_if(model_.arcs.begin(),model_.arcs.end(),curved),model_.arcs.end());
    model_.splines.erase(std::remove_if(model_.splines.begin(),model_.splines.end(),curved),model_.splines.end());
    // Trim the orphaned construction handles, not only the curve objects.
    for(const auto& id:controls){
        if(boundary_.contains(id))continue;
        bool referenced=false;
        for(const auto& arc:model_.arcs)if(arc.midpointId==id)referenced=true;
        for(const auto& spline:model_.splines)if(spline.control1PointId==id||spline.control2PointId==id)referenced=true;
        for(const auto& circle:model_.circles)if(circle.centerPointId==id)referenced=true;
        if(referenced)continue;
        model_.points.erase(std::remove_if(model_.points.begin(),model_.points.end(),
            [&](const auto& point){return point.id==id;}),model_.points.end());
        model_.constraints.erase(std::remove_if(model_.constraints.begin(),model_.constraints.end(),
            [&](const auto& c){return c.type==cad::SketchConstraintType::FixedPoint&&c.pointA==id;}),
            model_.constraints.end());
    }
    if(status)status("Curved edge changed to a straight segment (undo is available).");
    update();
}
void SketchCanvasWidget::offsetStraightOutline(double distanceMm){
    if(!closed_||boundary_.size()<3||!model_.arcs.isEmpty()||!model_.splines.isEmpty()){
        if(status)status("Offset supports a closed straight-edge profile only.");return;
    }
    QVector<QPointF> positions;
    for(const auto& id:boundary_){
        const auto it=std::find_if(model_.points.cbegin(),model_.points.cend(),
            [&](const auto& p){return p.id==id;});
        if(it==model_.points.cend())return;
        positions.append(QPointF(it->x,it->y));
    }
    const int n=positions.size();double doubledArea=0;
    for(int i=0;i<n;++i)doubledArea+=positions[i].x()*positions[(i+1)%n].y()-
        positions[(i+1)%n].x()*positions[i].y();
    if(std::abs(doubledArea)<1e-6){if(status)status("Degenerate boundary; offset cancelled.");return;}
    const double direction=doubledArea>0?1.0:-1.0;
    QVector<QPointF> normals;normals.reserve(n);
    for(int i=0;i<n;++i){
        const QPointF delta=positions[(i+1)%n]-positions[i];
        const double length=std::hypot(delta.x(),delta.y());
        if(length<1e-6){if(status)status("Zero-length edge prevents offset.");return;}
        normals.append(direction*QPointF(delta.y()/length,-delta.x()/length));
        const QPointF next=positions[(i+2)%n]-positions[(i+1)%n];
        if(direction*(delta.x()*next.y()-delta.y()*next.x())<1e-7){
            if(status)status("Offset requires a strictly convex polygon.");return;
        }
    }
    QVector<QPointF> shifted;shifted.reserve(n);
    for(int i=0;i<n;++i){
        const int prev=(i+n-1)%n;
        const QPointF a=positions[prev]+distanceMm*normals[prev];
        const QPointF b=positions[i]+distanceMm*normals[i];
        const QPointF u=positions[i]-positions[prev];
        const QPointF v=positions[(i+1)%n]-positions[i];
        const double cross=u.x()*v.y()-u.y()*v.x();
        if(std::abs(cross)<1e-8){if(status)status("Nearly parallel edges prevent a stable offset.");return;}
        const QPointF gap=b-a;
        const double t=(gap.x()*v.y()-gap.y()*v.x())/cross;
        const QPointF intersection=a+t*u;
        if(!std::isfinite(intersection.x())||!std::isfinite(intersection.y())||
            std::abs(intersection.x())>1e6||std::abs(intersection.y())>1e6){
            if(status)status("Offset exceeds the coordinate limit.");return;
        }
        shifted.append(intersection);
    }
    cad::SketchModel trial=model_;
    for(int i=0;i<n;++i){
        for(auto& point:trial.points)if(point.id==boundary_[i]){
            point.x=shifted[i].x();point.y=shifted[i].y();break;
        }
        for(auto& constraint:trial.constraints)if(constraint.pointA==boundary_[i]&&
           constraint.type==cad::SketchConstraintType::FixedPoint){
            constraint.fixedX=shifted[i].x();constraint.fixedY=shifted[i].y();
        }
    }
    TopoDS_Shape checked;QString error;
    if(!cad::buildSketchExtrusion(trial,boundary_,1.0,{},&checked,&error)){
        if(status)status("Offset rejected: "+error);return;
    }
    checkpoint();model_=trial;update();
    if(status)status(QStringLiteral("Applied %1 mm polygon offset.").arg(distanceMm,0,'g',8));
}
int SketchCanvasWidget::nearestBoundaryEdge(const QPointF& cursor) const {
    if(!closed_||boundary_.size()<3)return -1;
    QMap<QString,QPointF> points;
    for(const auto& p:model_.points)points.insert(p.id,screen(QPointF(p.x,p.y)));
    int best=-1;double nearest=18.0;
    for(int i=0;i<boundary_.size();++i){
        const QPointF a=points.value(boundary_[i]),b=points.value(boundary_[(i+1)%boundary_.size()]);
        const QPointF delta=b-a;
        const double length=delta.x()*delta.x()+delta.y()*delta.y();
        if(length<1e-8)continue;
        const double t=std::clamp(QPointF::dotProduct(cursor-a,delta)/length,0.0,1.0);
        const QPointF closest=a+t*delta;
        const double distance=std::hypot(closest.x()-cursor.x(),closest.y()-cursor.y());
        if(distance<nearest){nearest=distance;best=i;}
    }
    return best;
}
void SketchCanvasWidget::completeCurvePoint(const QPointF& point){
    if(curveEdgeIndex_<0||curveEdgeIndex_>=boundary_.size())return;
    pendingControls_.append(point);
    if(tool_==Tool::Spline&&pendingControls_.size()<2){
        if(status)status("Bezier: click the second control handle.");return;
    }
    if(model_.points.size()+pendingControls_.size()>64){
        if(status)status("Maximum 64 sketch points, including curve handles.");
        curveEdgeIndex_=-1;pendingControls_.clear();return;
    }
    checkpoint();
    QStringList ids;
    for(const auto& control:pendingControls_){
        const QString id=newPointId();model_.points.append({id,control.x(),control.y()});
        fixPoint(id,control);ids.append(id);
    }
    const QString first=boundary_[curveEdgeIndex_],last=boundary_[(curveEdgeIndex_+1)%boundary_.size()];
    model_.arcs.erase(std::remove_if(model_.arcs.begin(),model_.arcs.end(),[&](const auto& a){
        return a.startPointId==first&&a.endPointId==last;
    }),model_.arcs.end());
    model_.splines.erase(std::remove_if(model_.splines.begin(),model_.splines.end(),[&](const auto& a){
        return a.startPointId==first&&a.endPointId==last;
    }),model_.splines.end());
    int suffix=1;QString id;
    for(;;++suffix){
        id=QStringLiteral("curve_%1").arg(suffix);
        bool exists=false;
        for(const auto& arc:model_.arcs)if(arc.id==id)exists=true;
        for(const auto& spline:model_.splines)if(spline.id==id)exists=true;
        if(!exists)break;
    }
    if(tool_==Tool::Arc)model_.arcs.append({id,first,last,ids.first()});
    else model_.splines.append({id,first,last,ids[0],ids[1]});
    curveEdgeIndex_=-1;pendingControls_.clear();
    if(status)status("Curved edge added. Drag its white control handles to adjust curvature.");
    update();
}
void SketchCanvasWidget::zoomToFit(){
    if(model_.points.isEmpty()){center_=QPointF(0,0);pixelsPerMm_=15;update();return;}
    double minX=std::numeric_limits<double>::max(),minY=minX;
    double maxX=-minX,maxY=-minX;
    for(const auto& p:model_.points){minX=std::min(minX,p.x);minY=std::min(minY,p.y);maxX=std::max(maxX,p.x);maxY=std::max(maxY,p.y);}
    center_=QPointF((minX+maxX)/2,(minY+maxY)/2);
    pixelsPerMm_=std::clamp(std::min((width()-100)/std::max(15.,maxX-minX),(height()-100)/std::max(15.,maxY-minY)),2.,80.);
    update();
}
void SketchCanvasWidget::mousePressEvent(QMouseEvent* event){
    lastCursor_=event->position();
    if(event->button()==Qt::MiddleButton){panning_=true;return;}
    if(event->button()!=Qt::LeftButton)return;
    const QPointF mm=snapped(world(event->position()));
    if(relation_!=Relation::None){handleRelationClick(event->position());return;}
    if(tool_==Tool::Select){
        dragging_=hitPoint(event->position());
        if(dragging_>=0)checkpoint();
    }else if(tool_==Tool::Polyline){
        if(boundary_.size()>=3){
            const auto firstIt=std::find_if(model_.points.cbegin(),model_.points.cend(),
                [&](const auto& p){return p.id==boundary_.first();});
            if(firstIt==model_.points.cend())return;
            const auto& first=*firstIt;
            if(std::hypot(screen(QPointF(first.x,first.y)).x()-event->position().x(),
                          screen(QPointF(first.x,first.y)).y()-event->position().y())<=11){closeBoundary();return;}
        }
        addPoint(mm);
    }else if(tool_==Tool::Arc||tool_==Tool::Spline){
        if(curveEdgeIndex_<0){
            curveEdgeIndex_=nearestBoundaryEdge(event->position());pendingControls_.clear();
            if(status)status(curveEdgeIndex_<0?
                "Close the outline, then click near an edge to select it.":
                (tool_==Tool::Arc?"Arc: click a point on the desired circular arc.":
                                    "Bezier: click the first control handle."));
        }else completeCurvePoint(mm);
    }else if(tool_==Tool::Rectangle){
        if(!boundary_.isEmpty()){
            if(status)status("Select New sketch to replace the existing outline.");return;
        }
        if(!rectanglePending_){rectangleStart_=mm;rectanglePending_=true;
            if(status)status("Rectangle: click the opposite corner.");
        }else{
            const double x1=rectangleStart_.x(),y1=rectangleStart_.y();
            if(std::abs(mm.x()-x1)<0.0001||std::abs(mm.y()-y1)<0.0001){
                if(status)status("The rectangle must have nonzero width and height.");return;
            }
            checkpoint();model_={};boundary_.clear();
            const QVector<QPointF> corners={QPointF(x1,y1),QPointF(mm.x(),y1),mm,QPointF(x1,mm.y())};
            for(const auto& corner:corners){const QString id=newPointId();
                model_.points.append({id,corner.x(),corner.y()});boundary_.append(id);fixPoint(id,corner);}
            regenerateBoundarySegments();closed_=true;rectanglePending_=false;tool_=Tool::Select;
            if(status)status("Outline closed: rectangle created. Use Select to adjust vertices.");
        }
    }else if(tool_==Tool::Slot){
        if(!boundary_.isEmpty()){
            if(status)status("Select New sketch before creating a slot outline.");return;
        }
        if(slotStep_==0){slotStart_=mm;slotStep_=1;
            if(status)status("Slot: click the other centerline endpoint.");
        }else if(slotStep_==1){
            if(std::hypot(mm.x()-slotStart_.x(),mm.y()-slotStart_.y())<0.001){
                if(status)status("Slot centerline must have positive length.");return;
            }
            slotEnd_=mm;slotStep_=2;
            if(status)status("Slot: click to set the radius from the centerline.");
        }else{
            const QPointF d=slotEnd_-slotStart_;
            const double length=std::hypot(d.x(),d.y());
            const QPointF unit(d.x()/length,d.y()/length),normal(-unit.y(),unit.x());
            const double radius=std::abs((mm.x()-slotStart_.x())*normal.x()+
                                         (mm.y()-slotStart_.y())*normal.y());
            if(radius<0.0001||radius>1e5){
                if(status)status("Slot radius must be positive and finite.");return;
            }
            const QVector<QPointF> corners={slotStart_+radius*normal,
                slotEnd_+radius*normal,slotEnd_-radius*normal,slotStart_-radius*normal};
            const QVector<QPointF> midpoints={slotEnd_+radius*unit,slotStart_-radius*unit};
            checkpoint();model_={};boundary_.clear();
            for(const auto& point:corners){const QString id=newPointId();
                model_.points.append({id,point.x(),point.y()});boundary_.append(id);fixPoint(id,point);}
            QStringList mids;
            for(const auto& point:midpoints){const QString id=newPointId();
                model_.points.append({id,point.x(),point.y()});fixPoint(id,point);mids.append(id);}
            regenerateBoundarySegments();
            model_.arcs.append({"slot_end",boundary_[1],boundary_[2],mids[0]});
            model_.arcs.append({"slot_start",boundary_[3],boundary_[0],mids[1]});
            closed_=true;slotStep_=0;tool_=Tool::Select;
            if(status)status("Outline closed: tangent-end slot created. Select and drag control points.");
        }
    }else if(tool_==Tool::Circle){
        if(!circlePending_){pendingCenter_=mm;circlePending_=true;}
        else{
            const double radius=std::hypot(mm.x()-pendingCenter_.x(),mm.y()-pendingCenter_.y());
            if(radius<0.00001){if(status)status("Move the pointer to choose a circle radius.");return;}
            if(model_.circles.size()>=64||model_.points.size()>=64){if(status)status("Sketch entity limit reached.");return;}
            checkpoint();const QString centerId=newPointId();
            model_.points.append({centerId,pendingCenter_.x(),pendingCenter_.y()});
            fixPoint(centerId,pendingCenter_);
            int index=1;QString circleId;
            for(;;++index){
                circleId=QStringLiteral("circle_%1").arg(index);
                const auto existing=std::find_if(model_.circles.cbegin(),model_.circles.cend(),
                    [&](const auto& item){return item.id==circleId;});
                if(existing==model_.circles.cend())break;
            }
            model_.circles.append({circleId,centerId,radius,true});
            cad::SketchConstraint size;size.type=cad::SketchConstraintType::Radius;size.circleA=circleId;size.value=radius;
            model_.constraints.append(size);circlePending_=false;
            if(status)status("Added circle cutout. The cutout must be entirely inside the closed outline.");
        }
    }
    update();
}
void SketchCanvasWidget::mouseMoveEvent(QMouseEvent* event){
    mouseWorld_=world(event->position());
    if(panning_){
        const QPointF delta=event->position()-lastCursor_;
        center_-=QPointF(delta.x()/pixelsPerMm_,-delta.y()/pixelsPerMm_);
    }else if(dragging_>=0&&(event->buttons()&Qt::LeftButton)){
        const QPointF chosen=snapped(mouseWorld_,model_.points[dragging_].id);
        cad::SketchModel trial=model_;
        const QString selected=trial.points[dragging_].id;
        trial.points[dragging_].x=chosen.x();trial.points[dragging_].y=chosen.y();
        bool hadAnchor=false;
        for(auto& c:trial.constraints)if(c.type==cad::SketchConstraintType::FixedPoint&&c.pointA==selected){
            c.fixedX=chosen.x();c.fixedY=chosen.y();hadAnchor=true;
        }
        if(!hadAnchor){
            cad::SketchConstraint dragAnchor;dragAnchor.type=cad::SketchConstraintType::FixedPoint;
            dragAnchor.pointA=selected;dragAnchor.fixedX=chosen.x();dragAnchor.fixedY=chosen.y();
            trial.constraints.append(dragAnchor);
        }
        const auto solution=cad::SketchSolver::solve(trial);
        if(solution.success&&solution.solvedPoints.size()==trial.points.size()){
            for(int i=0;i<trial.points.size();++i){
                trial.points[i].x=solution.solvedPoints[i].x();
                trial.points[i].y=solution.solvedPoints[i].y();
            }
            if(!hadAnchor)trial.constraints.removeLast();
            TopoDS_Shape checked;QString reason;
            if(!closed_||cad::buildSketchExtrusion(trial,boundary_,1.0,{},&checked,&reason))
                model_=std::move(trial);
        }
    }
    lastCursor_=event->position();update();
}
void SketchCanvasWidget::mouseReleaseEvent(QMouseEvent* event){
    if(event->button()==Qt::MiddleButton)panning_=false;
    if(event->button()==Qt::LeftButton)dragging_=-1;
}
void SketchCanvasWidget::wheelEvent(QWheelEvent* event){
    const QPointF anchor=world(event->position());
    pixelsPerMm_=std::clamp(pixelsPerMm_*(event->angleDelta().y()>0?1.20:1/1.20),1.0,200.0);
    const QPointF after=world(event->position());center_+=anchor-after;
    update();event->accept();
}
void SketchCanvasWidget::keyPressEvent(QKeyEvent* event){
    if(event->key()==Qt::Key_Backspace||(event->modifiers()==Qt::ControlModifier&&
                                          event->key()==Qt::Key_Z)){
        undoEdit();event->accept();return;
    }
    if(event->key()==Qt::Key_Escape){
        setTool(Tool::Select);
        if(status)status("Selection tool active. Pending creation was cancelled.");
        event->accept();return;
    }
    if(event->key()==Qt::Key_F){zoomToFit();event->accept();return;}
    QWidget::keyPressEvent(event);
}
void SketchCanvasWidget::paintEvent(QPaintEvent*){
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(),QColor(24,32,43));
    const QPointF tl=world(QPointF(0,0)),br=world(QPointF(width(),height()));
    painter.setPen(QPen(QColor(46,58,72),1));
    const int gridStep=std::max(1,int(std::ceil(9/pixelsPerMm_)));
    int drawn=0;
    for(int x=int(std::ceil(tl.x()/gridStep))*gridStep;x<=br.x()&&drawn<500;x+=gridStep,++drawn){
        const double px=screen(QPointF(x,0)).x();painter.drawLine(QPointF(px,0),QPointF(px,height()));
    }
    drawn=0;
    for(int y=int(std::ceil(br.y()/gridStep))*gridStep;y<=tl.y()&&drawn<500;y+=gridStep,++drawn){
        const double py=screen(QPointF(0,y)).y();painter.drawLine(QPointF(0,py),QPointF(width(),py));
    }
    painter.setPen(QPen(QColor(130,93,93),1.2));
    painter.drawLine(screen(QPointF(tl.x(),0)),screen(QPointF(br.x(),0)));
    painter.setPen(QPen(QColor(88,145,121),1.2));
    painter.drawLine(screen(QPointF(0,tl.y())),screen(QPointF(0,br.y())));
    QMap<QString,QPointF> points;
    for(const auto& p:model_.points)points.insert(p.id,QPointF(p.x,p.y));
    if(!boundary_.isEmpty()){
        QPainterPath outline;outline.moveTo(screen(points.value(boundary_.first())));
        for(int i=0;i<(closed_?boundary_.size():boundary_.size()-1);++i){
            const QString from=boundary_[i],to=boundary_[(i+1)%boundary_.size()];
            const QPointF a=points.value(from),b=points.value(to);
            bool curved=false;
            for(const auto& arc:model_.arcs)if(arc.startPointId==from&&arc.endPointId==to){
                const QPointF mid=points.value(arc.midpointId);
                const double det=2*(a.x()*(mid.y()-b.y())+mid.x()*(b.y()-a.y())+b.x()*(a.y()-mid.y()));
                if(std::abs(det)>1e-8){
                    const double aa=a.x()*a.x()+a.y()*a.y(),mm=mid.x()*mid.x()+mid.y()*mid.y();
                    const double bb=b.x()*b.x()+b.y()*b.y();
                    const QPointF c((aa*(mid.y()-b.y())+mm*(b.y()-a.y())+bb*(a.y()-mid.y()))/det,
                                    (aa*(b.x()-mid.x())+mm*(a.x()-b.x())+bb*(mid.x()-a.x()))/det);
                    const auto angle=[&](const QPointF& p){return std::atan2(p.y()-c.y(),p.x()-c.x());};
                    const auto ccw=[](double x){const double tau=2*std::acos(-1.0);x=std::fmod(x,tau);return x<0?x+tau:x;};
                    const double startAngle=angle(a);
                    const double arcSpan=ccw(angle(b)-startAngle),midSpan=ccw(angle(mid)-startAngle);
                    const double sweep=midSpan<=arcSpan?arcSpan:arcSpan-2*std::acos(-1.0);
                    const double radius=std::hypot(a.x()-c.x(),a.y()-c.y());
                    for(int j=1;j<=32;++j){const double t=startAngle+sweep*j/32.0;
                        outline.lineTo(screen(QPointF(c.x()+radius*std::cos(t),c.y()+radius*std::sin(t))));}
                    curved=true;
                }
                break;
            }
            if(!curved)for(const auto& spline:model_.splines)if(spline.startPointId==from&&spline.endPointId==to){
                outline.cubicTo(screen(points.value(spline.control1PointId)),
                                screen(points.value(spline.control2PointId)),screen(b));
                curved=true;break;
            }
            if(!curved)outline.lineTo(screen(b));
        }
        if(closed_)outline.closeSubpath();
        painter.setPen(QPen(QColor(86,210,245),2.5));
        painter.setBrush(closed_?QBrush(QColor(57,177,215,36)):QBrush(Qt::NoBrush));
        painter.drawPath(outline);
        if(tool_==Tool::Polyline&&!closed_){
            painter.setPen(QPen(QColor(86,210,245),1.0,Qt::DashLine));
            painter.drawLine(screen(points.value(boundary_.last())),screen(snapped(mouseWorld_)));
        }
        if(curveEdgeIndex_>=0){
            const auto first=screen(points.value(boundary_[curveEdgeIndex_]));
            const auto last=screen(points.value(boundary_[(curveEdgeIndex_+1)%boundary_.size()]));
            painter.setPen(QPen(QColor(255,209,110),3,Qt::DashLine));painter.drawLine(first,last);
            painter.setPen(QPen(QColor(255,209,110),1,Qt::DashLine));
            for(const auto& p:pendingControls_)painter.drawLine(first,screen(p));
        }
    }
    painter.setPen(QPen(QColor(255,172,111),2));painter.setBrush(QColor(245,153,83,30));
    for(int i=0;i<model_.circles.size();++i){
        const auto& circle=model_.circles[i];const QPointF c=screen(points.value(circle.centerPointId));
        painter.drawEllipse(c,circle.radius*pixelsPerMm_,circle.radius*pixelsPerMm_);
    }
    if(rectanglePending_){
        painter.setPen(QPen(QColor(255,206,114),1.5,Qt::DashLine));
        painter.setBrush(QColor(255,206,114,25));
        painter.drawRect(QRectF(screen(rectangleStart_),screen(snapped(mouseWorld_))).normalized());
    }
    if(slotStep_>0){
        painter.setPen(QPen(QColor(255,206,114),1.5,Qt::DashLine));
        painter.drawLine(screen(slotStart_),screen(slotStep_==1?snapped(mouseWorld_):slotEnd_));
    }
    if(circlePending_){
        painter.setPen(QPen(QColor(255,184,114),1,Qt::DashLine));
        const double r=std::hypot(mouseWorld_.x()-pendingCenter_.x(),mouseWorld_.y()-pendingCenter_.y())*pixelsPerMm_;
        painter.drawEllipse(screen(pendingCenter_),r,r);
    }
    for(int i=0;i<model_.points.size();++i){
        const auto& p=model_.points[i];const QPointF at=screen(QPointF(p.x,p.y));
        painter.setPen(QPen(QColor(20,36,53),1));
        painter.setBrush(i==dragging_?QColor(255,210,112):QColor(227,243,253));
        painter.drawEllipse(at,5,5);
    }
    painter.setPen(QColor(186,204,223));
    painter.drawText(12,21,QStringLiteral("Sketch local plane • mm | %1 vertices • %2 circular cutouts | %3")
        .arg(boundary_.size()).arg(model_.circles.size()).arg(closed_?"closed":"open"));
    painter.drawText(12,height()-12,"Wheel: zoom   Middle drag: pan   Select: drag points and curve handles   Arc/Bezier: choose an edge first");
}

SketchCanvasDialog::SketchCanvasDialog(QWidget* parent):QDialog(parent){
    setWindowTitle("Interactive sketch editor — planes and curves");resize(1040,750);
    auto* layout=new QVBoxLayout(this);
    auto* strip=new QHBoxLayout;
    auto* tool=new QComboBox(this);tool->addItems({"Polyline", "Select / drag", "Circle cutout", "Circular arc", "Bezier spline", "Rectangle", "Slot"});
    toolPicker_=tool;
    strip->addWidget(new QLabel("Tool:",this));strip->addWidget(tool);
    auto* grid=new QCheckBox("Snap grid (1 mm)",this);grid->setChecked(true);strip->addWidget(grid);
    auto* endpoints=new QCheckBox("Snap endpoints",this);endpoints->setChecked(true);strip->addWidget(endpoints);
    auto* close=new QPushButton("Close outline",this);strip->addWidget(close);
    auto* undo=new QPushButton("Undo sketch",this);strip->addWidget(undo);
    auto* clear=new QPushButton("New sketch",this);strip->addWidget(clear);
    auto* fit=new QPushButton("Fit",this);strip->addWidget(fit);
    strip->addStretch();layout->addLayout(strip);
    auto* relationTools=new QHBoxLayout;
    auto* horizontal=new QPushButton("Horizontal edge",this);
    auto* vertical=new QPushButton("Vertical edge",this);
    auto* straighten=new QPushButton("Straighten curved edge",this);
    auto* offset=new QPushButton("Offset convex outline...",this);
    for(auto* widget:{horizontal,vertical,straighten,offset})relationTools->addWidget(widget);
    auto* help=new QLabel("Hover a boundary edge before using edge tools. Esc: cancel · Backspace: undo · F: fit",this);
    relationTools->addWidget(help,1);layout->addLayout(relationTools);
    auto* constraintRow=new QHBoxLayout;
    constraintRow->addWidget(new QLabel("Click-to-constrain:",this));
    auto* constraint=new QComboBox(this);
    constraint->addItems({"Off / select","Horizontal edge","Vertical edge","Edge length",
        "Parallel edges","Perpendicular edges","Point distance","Coincident points","Fix point","Unfix point",
        "Equal edge lengths","Angle between edges","Equal circle radii","Concentric circles"});
    constraint->setToolTip("Choose a relation, then click one or two sketch entities. The solver validates each change before applying it.");
    constraintRow->addWidget(constraint);
    auto* manageConstraints=new QPushButton("Manage constraints...",this);
    manageConstraints->setToolTip("Review, edit dimension values, and remove saved constraints with sketch undo.");
    constraintRow->addWidget(manageConstraints);
    constraintRow->addWidget(new QLabel("Constraints are solved immediately; invalid profiles are rejected.",this),1);
    layout->addLayout(constraintRow);
    auto* planeGroup=new QGroupBox("Sketch datum plane (local XY coordinates in mm)",this);
    auto* planeLayout=new QHBoxLayout(planeGroup);
    planePreset_=new QComboBox(planeGroup);
    planePreset_->addItems({"World XY","World XZ","World YZ","Custom"});
    planePreset_->setToolTip("Choose a reference plane or enter an arbitrary normal and origin.");
    planeLayout->addWidget(planePreset_);
    const QStringList labels={"Origin X","Y","Z","Normal X","Y","Z","Twist°"};
    const QVector<double> defaults={0,0,0,0,0,1,0};
    for(int i=0;i<labels.size();++i){
        planeLayout->addWidget(new QLabel(labels[i],planeGroup));
        auto* field=new QDoubleSpinBox(planeGroup);
        field->setDecimals(3);field->setRange(-1000000,1000000);field->setMinimumWidth(72);
        field->setMaximumWidth(100);field->setValue(defaults[i]);
        planeFields_.append(field);planeLayout->addWidget(field);
        connect(field,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double){
            const QSignalBlocker block(planePreset_);planePreset_->setCurrentIndex(3);
        });
    }
    connect(planePreset_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){
        if(index==3)return;
        cad::SketchPlane plane;
        if(index==1){plane.normalY=-1;plane.normalZ=0;}
        if(index==2){plane.normalX=1;plane.normalZ=0;}
        setPlane(plane);
    });
    layout->addWidget(planeGroup);
    canvas_=new SketchCanvasWidget(this);layout->addWidget(canvas_,1);
    feedback_=new QLabel("Click to trace a polygon. Close it, then add circular cutouts. No geometry is committed until Create / Update.",this);
    feedback_->setWordWrap(true);layout->addWidget(feedback_);
    auto* bottom=new QHBoxLayout;depth_=new QDoubleSpinBox(this);
    depth_->setRange(0.00001,1000000);depth_->setDecimals(4);depth_->setValue(10);depth_->setSuffix(" mm");
    bottom->addWidget(new QLabel("Extrusion depth:",this));bottom->addWidget(depth_);bottom->addStretch();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    buttons->button(QDialogButtonBox::Ok)->setText("Create / Update extrusion");bottom->addWidget(buttons);layout->addLayout(bottom);
    canvas_->status=[this](const QString& message){feedback_->setText(message);
        if(message.startsWith("Outline closed")||message.startsWith("Selection tool active"))
            toolPicker_->setCurrentIndex(1);};
    connect(tool,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,constraint](int index){
        if(constraint->currentIndex()!=0){const QSignalBlocker block(constraint);constraint->setCurrentIndex(0);}
        canvas_->setTool(index==0?SketchCanvasWidget::Tool::Polyline:
                         index==1?SketchCanvasWidget::Tool::Select:
                         index==2?SketchCanvasWidget::Tool::Circle:
                         index==3?SketchCanvasWidget::Tool::Arc:
                         index==4?SketchCanvasWidget::Tool::Spline:
                         index==5?SketchCanvasWidget::Tool::Rectangle:SketchCanvasWidget::Tool::Slot);
    });
    connect(constraint,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){
        using R=SketchCanvasWidget::Relation;
        const R choices[]={R::None,R::Horizontal,R::Vertical,R::Length,R::Parallel,
            R::Perpendicular,R::Distance,R::Coincident,R::Fixed,R::Unfix,
            R::EqualLength,R::Angle,R::EqualRadius,R::Concentric};
        if(index>0)toolPicker_->setCurrentIndex(1);
        canvas_->setRelation(choices[std::clamp(index,0,13)]);
    });
    connect(manageConstraints,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);dialog.setWindowTitle("Sketch constraints — review and edit");dialog.resize(700,460);
        auto* layout=new QVBoxLayout(&dialog);
        auto* hint=new QLabel("Select a constraint to change its dimension or remove it. "
            "All changes are validated against the complete sketch and can be undone in the sketch editor.",&dialog);
        hint->setWordWrap(true);layout->addWidget(hint);
        auto* items=new QListWidget(&dialog);layout->addWidget(items,1);
        auto* actions=new QHBoxLayout;
        auto* edit=new QPushButton("Edit dimension...",&dialog);
        auto* anchor=new QPushButton("Edit fixed anchor...",&dialog);
        auto* remove=new QPushButton("Remove constraint",&dialog);
        actions->addWidget(edit);actions->addWidget(anchor);actions->addWidget(remove);actions->addStretch(1);
        layout->addLayout(actions);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
        layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        const auto reload=[&](int selected){
            items->clear();items->addItems(canvas_->constraintDescriptions());
            if(items->count()>0)items->setCurrentRow(std::clamp(selected,0,items->count()-1));
            const int row=items->currentRow();
            const auto constraints=canvas_->sketch().constraints;
            edit->setEnabled(row>=0&&row<constraints.size()&&editableSketchDimension(constraints[row].type));
            anchor->setEnabled(row>=0&&row<constraints.size()&&constraints[row].type==cad::SketchConstraintType::FixedPoint);
            remove->setEnabled(row>=0);
        };
        connect(items,&QListWidget::currentRowChanged,&dialog,[&](int selected){
            remove->setEnabled(selected>=0);
            if(selected<0){edit->setEnabled(false);anchor->setEnabled(false);return;}
            const auto constraints=canvas_->sketch().constraints;
            edit->setEnabled(selected<constraints.size()&&editableSketchDimension(constraints[selected].type));
            anchor->setEnabled(selected<constraints.size()&&constraints[selected].type==cad::SketchConstraintType::FixedPoint);
        });
        connect(edit,&QPushButton::clicked,&dialog,[&]{
            const int index=items->currentRow();
            const auto constraints=canvas_->sketch().constraints;
            if(index<0||index>=constraints.size()||!editableSketchDimension(constraints[index].type))return;
            const auto c=constraints[index];
            const bool angular=c.type==cad::SketchConstraintType::Angle||
                c.type==cad::SketchConstraintType::SegmentAngleFromX;
            const bool positive=c.type==cad::SketchConstraintType::Distance||
                c.type==cad::SketchConstraintType::SegmentLength||
                c.type==cad::SketchConstraintType::Radius;
            const double factor=angular?180.0/std::acos(-1.0):1.0;
            bool accepted=false;
            const double entry=QInputDialog::getDouble(&dialog,"Edit sketch dimension",
                angular?"Angle (degrees):":"Dimension (mm):",c.value*factor,
                angular?-360.0:(positive?0.00001:-1e6),angular?360.0:1e6,6,&accepted);
            if(!accepted)return;
            if(!canvas_->changeConstraintDimension(index,entry/factor)){
                QMessageBox::warning(&dialog,"Dimension not applied",feedback_->text());return;
            }
            reload(index);
        });
        connect(anchor,&QPushButton::clicked,&dialog,[&]{
            const int index=items->currentRow();
            const auto constraints=canvas_->sketch().constraints;
            if(index<0||index>=constraints.size()||constraints[index].type!=cad::SketchConstraintType::FixedPoint)return;
            QDialog anchorDialog(&dialog);anchorDialog.setWindowTitle("Edit fixed-point anchor");
            auto* anchorForm=new QFormLayout(&anchorDialog);
            auto* xField=new QDoubleSpinBox(&anchorDialog);
            auto* yField=new QDoubleSpinBox(&anchorDialog);
            for(auto* field:{xField,yField}){
                field->setRange(-1e6,1e6);field->setDecimals(6);field->setSuffix(" mm");
            }
            xField->setValue(constraints[index].fixedX);
            yField->setValue(constraints[index].fixedY);
            anchorForm->addRow("X coordinate:",xField);
            anchorForm->addRow("Y coordinate:",yField);
            auto* confirm=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&anchorDialog);
            anchorForm->addRow(confirm);
            connect(confirm,&QDialogButtonBox::accepted,&anchorDialog,&QDialog::accept);
            connect(confirm,&QDialogButtonBox::rejected,&anchorDialog,&QDialog::reject);
            if(anchorDialog.exec()!=QDialog::Accepted)return;
            if(!canvas_->changeFixedPointAnchor(index,xField->value(),yField->value())){
                QMessageBox::warning(&dialog,"Anchor not applied",feedback_->text());return;
            }
            reload(index);
        });
        connect(remove,&QPushButton::clicked,&dialog,[&]{
            const int index=items->currentRow();if(index<0)return;
            if(!canvas_->removeConstraint(index)){
                QMessageBox::warning(&dialog,"Constraint not removed",feedback_->text());return;
            }
            reload(index);
        });
        reload(0);dialog.exec();
    });
    connect(grid,&QCheckBox::toggled,canvas_,&SketchCanvasWidget::setGridSnap);

    connect(endpoints,&QCheckBox::toggled,canvas_,&SketchCanvasWidget::setPointSnap);
    connect(close,&QPushButton::clicked,this,[this]{canvas_->closeBoundary();
        if(canvas_->boundary().size()>=3)toolPicker_->setCurrentIndex(1);});
    connect(undo,&QPushButton::clicked,canvas_,&SketchCanvasWidget::undoEdit);
    connect(clear,&QPushButton::clicked,this,[this]{canvas_->clearSketch();toolPicker_->setCurrentIndex(0);});
    connect(fit,&QPushButton::clicked,canvas_,&SketchCanvasWidget::zoomToFit);
    connect(horizontal,&QPushButton::clicked,this,[this]{canvas_->alignEdgeUnderCursor(true);});
    connect(vertical,&QPushButton::clicked,this,[this]{canvas_->alignEdgeUnderCursor(false);});
    connect(straighten,&QPushButton::clicked,canvas_,&SketchCanvasWidget::removeCurveUnderCursor);
    connect(offset,&QPushButton::clicked,this,[this]{
        bool accepted=false;
        const double distance=QInputDialog::getDouble(this,"Polygon offset",
            "Signed offset (mm); positive expands, negative contracts",2.0,-100000.0,100000.0,4,&accepted);
        if(accepted&&std::abs(distance)>1e-7)canvas_->offsetStraightOutline(distance);
    });
    connect(buttons,&QDialogButtonBox::accepted,this,[this]{
        if(canvas_->boundary().size()<3){feedback_->setText("At least three closed polygon vertices are required.");return;}
        if(canvas_->sketch().segments.isEmpty()){
            feedback_->setText("Close the polygon outline before committing it.");return;
        }
        const auto solved=cad::SketchSolver::solve(canvas_->sketch());
        if(!solved.success){feedback_->setText("Sketch constraints failed: "+solved.message);return;}
        cad::SceneDocument trial;
        QString error;
        if(!trial.addConstrainedPolygonExtrusion(canvas_->sketch(),canvas_->boundary(),depth_->value(),&error,plane())){
            feedback_->setText("Profile is not extrudable: "+error);
            return;
        }
        accept();
    });
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
}
void SketchCanvasDialog::setSketch(const cad::SketchModel& sketch,const QStringList& boundary,double depth,
                                   const cad::SketchPlane& plane){
    canvas_->setSketch(sketch,boundary);depth_->setValue(depth);setPlane(plane);
    toolPicker_->setCurrentIndex(boundary.size()>=3?1:0);
}
void SketchCanvasDialog::setPlane(const cad::SketchPlane& plane){
    const QVector<double> values={plane.originX,plane.originY,plane.originZ,
        plane.normalX,plane.normalY,plane.normalZ,plane.twistDegrees};
    for(int i=0;i<values.size();++i){
        const QSignalBlocker block(planeFields_[i]);planeFields_[i]->setValue(values[i]);
    }
    const QSignalBlocker block(planePreset_);
    const bool atOrigin=plane.originX==0&&plane.originY==0&&plane.originZ==0&&plane.twistDegrees==0;
    const int preset=atOrigin&&plane.normalX==0&&plane.normalY==0&&plane.normalZ==1?0:
        atOrigin&&plane.normalX==0&&plane.normalY==-1&&plane.normalZ==0?1:
        atOrigin&&plane.normalX==1&&plane.normalY==0&&plane.normalZ==0?2:3;
    planePreset_->setCurrentIndex(preset);
}
cad::SketchPlane SketchCanvasDialog::plane() const{
    cad::SketchPlane plane;
    plane.originX=planeFields_[0]->value();plane.originY=planeFields_[1]->value();
    plane.originZ=planeFields_[2]->value();plane.normalX=planeFields_[3]->value();
    plane.normalY=planeFields_[4]->value();plane.normalZ=planeFields_[5]->value();
    plane.twistDegrees=planeFields_[6]->value();return plane;
}
cad::SketchModel SketchCanvasDialog::sketch() const{return canvas_->sketch();}
QStringList SketchCanvasDialog::boundary() const{return canvas_->boundary();}
double SketchCanvasDialog::depth() const{return depth_->value();}
