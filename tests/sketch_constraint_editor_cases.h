#pragma once
// Integrated into the existing cadcore_sketch_solver CTest (130-test inventory).
// Native Qt sketch editor dimension editing, validation, radius propagation and undo.
#include "ui/SketchCanvasDialog.h"
#include <QPointF>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace sketch_editor_test {
void require(bool yes,const char* reason){if(!yes)throw std::runtime_error(reason);}
bool close(double a,double b){return std::abs(a-b)<1e-5;}
cad::SketchConstraint fixed(const QString& id,double x,double y){
    cad::SketchConstraint c;c.type=cad::SketchConstraintType::FixedPoint;
    c.pointA=id;c.fixedX=x;c.fixedY=y;return c;
}
void lineDimensionRoundtrip(){
    cad::SketchModel sketch;
    sketch.points={{"a",0,0},{"b",8,1}};sketch.segments={{"edge","a","b"}};
    sketch.constraints.append(fixed("a",0,0));
    cad::SketchConstraint horizontal;horizontal.type=cad::SketchConstraintType::Horizontal;
    horizontal.segmentA="edge";sketch.constraints.append(horizontal);
    cad::SketchConstraint length;length.type=cad::SketchConstraintType::SegmentLength;
    length.segmentA="edge";length.value=10;sketch.constraints.append(length);
    SketchCanvasWidget view;view.setSketch(sketch,{});
    require(view.constraintDescriptions().size()==3,"Expected three editable constraints");
    require(view.constraintDescriptions().last().contains("10 mm"),"Segment dimension missing from manager");
    require(view.changeConstraintDimension(2,12),"Valid segment edit failed");
    require(close(view.sketch().points[1].x,12)&&close(view.sketch().points[1].y,0),
            "Solver did not move segment endpoint after dimension edit");
    require(close(view.sketch().constraints[2].value,12),"Dimension edit was not persisted in draft");
    require(!view.changeConstraintDimension(2,-4),"Negative segment length accepted");
    require(!view.changeConstraintDimension(-1,4),"Invalid index was accepted");
    require(close(view.sketch().constraints[2].value,12),"Rejected edit changed committed dimension");
    view.undoEdit();
    require(close(view.sketch().constraints[2].value,10)&&close(view.sketch().points[1].x,8),
            "One sketch undo must restore the original dimension and points");
    require(view.removeConstraint(1),"Removing horizontal constraint failed");
    require(view.sketch().constraints.size()==2,"Constraint removal did not commit");
    view.undoEdit();
    require(view.sketch().constraints.size()==3,"Removing a constraint was not undoable");
    // A conflicting fixed endpoint must make an edit fail transactionally.
    auto constrained=sketch;constrained.points[1]={"b",10,0};
    constrained.constraints.append(fixed("b",10,0));
    view.setSketch(constrained,{});
    require(!view.changeConstraintDimension(2,13),"Overconstrained dimension edit incorrectly succeeded");
    require(close(view.sketch().constraints[2].value,10)&&close(view.sketch().points[1].x,10),
            "Invalid overconstrained edit modified sketch data");
}
void fixedPointAnchorRoundtrip(){
    cad::SketchModel sketch;
    sketch.points={{"point",1,2}};
    sketch.constraints.append(fixed("point",1,2));
    SketchCanvasWidget view;view.setSketch(sketch,{});
    require(view.constraintDescriptions().first().contains("(1, 2) mm"),"Fixed anchor not described in manager");
    require(view.changeFixedPointAnchor(0,4,-6),"Valid fixed anchor edit failed");
    require(close(view.sketch().points[0].x,4)&&close(view.sketch().points[0].y,-6)&&
            close(view.sketch().constraints[0].fixedX,4)&&close(view.sketch().constraints[0].fixedY,-6),
            "Fixed anchor solver result and draft coordinates diverged");
    require(!view.changeFixedPointAnchor(0,std::numeric_limits<double>::infinity(),0)&&
            !view.changeFixedPointAnchor(0,1e6+1,0)&&
            !view.changeFixedPointAnchor(99,0,0),"Invalid fixed anchor edit succeeded");
    require(close(view.sketch().points[0].x,4)&&close(view.sketch().constraints[0].fixedX,4),
            "Invalid fixed anchor changed the saved draft");
    view.undoEdit();
    require(close(view.sketch().points[0].x,1)&&close(view.sketch().points[0].y,2)&&
            close(view.sketch().constraints[0].fixedX,1)&&close(view.sketch().constraints[0].fixedY,2),
            "Undo did not restore fixed-point coordinates");
}
void interactivePairRelationsRoundtrip(){
    using T=cad::SketchConstraintType;
    cad::SketchModel lines;
    lines.points={{"a",0,0},{"b",10,0},{"c",20,0},{"d",24,0}};
    lines.segments={{"ab","a","b"},{"cd","c","d"}};
    lines.constraints={fixed("a",0,0),fixed("b",10,0),fixed("c",20,0),fixed("d",24,0)};
    SketchCanvasWidget view;view.setSketch(lines,{});
    require(view.constrainSegmentPair("ab","cd",T::EqualLength),"Interactive equal-length authoring failed");
    require(close(view.sketch().points[3].x,30)&&close(view.sketch().points[3].y,0),
            "Equal-length click tool failed to update the second edge");
    require(view.sketch().constraints.last().type==T::EqualLength,
            "Equal-length relation not saved as a constraint");
    require(!view.constrainSegmentPair("ab","ab",T::EqualLength)&&
            !view.constrainSegmentPair("ab","missing",T::EqualLength),
            "Invalid segment selection was accepted");
    require(close(view.sketch().points[3].x,30),"Invalid segment relation changed geometry");
    view.undoEdit();
    require(close(view.sketch().points[3].x,24)&&view.sketch().constraints.size()==4,
            "Undo did not restore second edge and original fixed anchor");
    cad::SketchConstraint locked;locked.type=T::SegmentLength;locked.segmentA="cd";locked.value=4;
    lines.constraints.append(locked);
    view.setSketch(lines,{});
    require(!view.constrainSegmentPair("ab","cd",T::EqualLength),
            "Conflicting second edge length was silently discarded");
    require(view.sketch().constraints.size()==5&&close(view.sketch().points[3].x,24),
            "Failed equal-length relation changed the sketch");
    cad::SketchModel angular=lines;
    angular.points[3].x=25;angular.points[3].y=5;
    angular.constraints.removeLast();
    view.setSketch(angular,{});
    require(view.constrainSegmentPair("ab","cd",T::Angle,45),
            "Interactive signed angle failed on a matching sketch");
    require(close(view.sketch().constraints.last().value,std::acos(-1.0)/4),
            "Angle units were not converted from degrees to radians");
    require(!view.constrainSegmentPair("ab","cd",T::Angle,181)&&
            !view.constrainSegmentPair("ab","cd",T::Angle,std::numeric_limits<double>::quiet_NaN()),
            "Invalid angle values changed the sketch");
    view.undoEdit();
    require(view.sketch().constraints.size()==4&&close(view.sketch().points[3].x,25),
            "Angle relation undo failed");

    cad::SketchModel circles;
    circles.points={{"p",0,0},{"q",9,0}};
    circles.circles={{"first","p",3.0,false},{"second","q",1.0,false}};
    circles.constraints={fixed("p",0,0),fixed("q",9,0)};
    cad::SketchConstraint firstRadius;firstRadius.type=T::Radius;
    firstRadius.circleA="first";firstRadius.value=3;
    cad::SketchConstraint secondRadius=firstRadius;
    secondRadius.circleA="second";secondRadius.value=1;
    circles.constraints.append(firstRadius);circles.constraints.append(secondRadius);
    view.setSketch(circles,{});
    require(view.constrainCirclePair("first","second",T::EqualRadius),
            "Interactive equal-radius relation failed");
    require(close(view.sketch().circles[1].radius,3)&&view.sketch().constraints.size()==4,
            "Equal radius did not replace second circle's driving radius");
    require(!view.constrainCirclePair("first","first",T::Concentric)&&
            !view.constrainCirclePair("first","missing",T::Concentric),
            "Invalid two-circle relation was accepted");
    view.undoEdit();
    require(close(view.sketch().circles[1].radius,1)&&view.sketch().constraints.size()==4,
            "Equal-radius relation undo did not restore circle radius");
    require(view.constrainCirclePair("first","second",T::Concentric),
            "Interactive concentric circles relation failed");
    require(close(view.sketch().points[1].x,0)&&close(view.sketch().points[1].y,0),
            "Concentric relation failed to move second circle's center");
    require(close(view.sketch().circles[1].radius,1),
            "Concentric relation unexpectedly changed the second circle's radius");
    view.undoEdit();
    require(close(view.sketch().points[1].x,9)&&view.sketch().constraints.size()==4,
            "Concentric relation undo failed");
}
void circleRadiusRoundtrip(){
    cad::SketchModel sketch;sketch.points={{"center",0,0}};
    sketch.circles={{"circle","center",2.0,false}};
    sketch.constraints.append(fixed("center",0,0));
    cad::SketchConstraint radius;radius.type=cad::SketchConstraintType::Radius;
    radius.circleA="circle";radius.value=2.0;sketch.constraints.append(radius);
    SketchCanvasWidget view;view.setSketch(sketch,{});
    require(view.constraintDescriptions().last().contains("Circle radius"),"Radius description missing");
    require(view.changeConstraintDimension(1,3.5),"Circle-radius edit failed");
    require(close(view.sketch().circles[0].radius,3.5),
            "Radius solver result was not propagated to persistent sketch circle");
    require(!view.changeConstraintDimension(1,0),"Zero radius was accepted");
    require(close(view.sketch().circles[0].radius,3.5),"Invalid radius edit changed sketch circle");
    view.undoEdit();
    require(close(view.sketch().circles[0].radius,2)&&close(view.sketch().constraints[1].value,2),
            "Radius edit undo did not restore circle and dimension together");
}
}
