#include "core/SketchSolver.h"
#include "core/SceneDocument.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a-b) < 1e-5; }
cad::SketchConstraint fixed(const QString& point, double x, double y) {
    cad::SketchConstraint c;c.type=cad::SketchConstraintType::FixedPoint;c.pointA=point;c.fixedX=x;c.fixedY=y;return c;
}
cad::SketchConstraint segmentConstraint(cad::SketchConstraintType type, const QString& a, const QString& b={}) {
    cad::SketchConstraint c;c.type=type;c.segmentA=a;c.segmentB=b;return c;
}
void run() {
    using namespace cad;
    SketchModel line;
    line.points={{"a",0,0},{"b",8,2}};line.segments={{"ab","a","b"}};
    line.constraints={fixed("a",0,0),segmentConstraint(SketchConstraintType::Horizontal,"ab")};
    SketchConstraint length;length.type=SketchConstraintType::Distance;length.pointA="a";length.pointB="b";length.value=10;
    line.constraints.append(length);
    auto result=SketchSolver::solve(line);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[1].x(),10)&&near(result.solvedPoints[1].y(),0),
            "Fully constrained line did not solve");
    require(near(line.points[1].x,8)&&near(line.points[1].y,2),"Solving changed input geometry");

    line.constraints.removeLast();result=SketchSolver::solve(line);
    require(result.success&&result.degreesOfFreedom==1,"Underconstrained line did not report one degree of freedom");
    line.constraints.append(length);line.constraints.append(fixed("b",5,0));result=SketchSolver::solve(line);
    require(!result.success&&result.solvedPoints.isEmpty(),"Incompatible constraints returned a solved sketch");

    SketchModel quadrilateral;
    quadrilateral.points={{"a",0,0},{"b",10,1},{"c",10,9},{"d",0,10}};
    quadrilateral.segments={{"ab","a","b"},{"bc","b","c"},{"cd","c","d"},{"da","d","a"}};
    quadrilateral.constraints={fixed("a",0,0),segmentConstraint(SketchConstraintType::Horizontal,"ab"),
                               segmentConstraint(SketchConstraintType::Vertical,"bc"),
                               segmentConstraint(SketchConstraintType::Parallel,"ab","cd"),
                               segmentConstraint(SketchConstraintType::Perpendicular,"ab","da"),
                               segmentConstraint(SketchConstraintType::EqualLength,"ab","cd")};
    SketchConstraint width=length;width.pointA="a";width.pointB="b";width.value=10;
    SketchConstraint height=length;height.pointA="b";height.pointB="c";height.value=10;
    quadrilateral.constraints.append(width);quadrilateral.constraints.append(height);
    result=SketchSolver::solve(quadrilateral);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[2].x(),10)&&near(result.solvedPoints[2].y(),10),
            "Mixed quadrilateral constraints did not produce a fully constrained profile");

    SketchModel angleSketch;
    angleSketch.points={{"o",0,0},{"x",10,0},{"y",1,8}};
    angleSketch.segments={{"ox","o","x"},{"oy","o","y"}};
    angleSketch.constraints={fixed("o",0,0),fixed("x",10,0)};
    SketchConstraint angle=segmentConstraint(SketchConstraintType::Angle,"ox","oy");angle.value=std::acos(-1.0)/2.0;
    angleSketch.constraints.append(angle);
    SketchConstraint radial=length;radial.pointA="o";radial.pointB="y";radial.value=8;
    angleSketch.constraints.append(radial);
    result=SketchSolver::solve(angleSketch);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[2].x(),0)&&near(result.solvedPoints[2].y(),8),
            "Signed-angle constraint failed");

    SketchModel coincident;coincident.points={{"a",1,2},{"b",3,4}};
    SketchConstraint coincidence;coincidence.type=SketchConstraintType::Coincident;coincidence.pointA="a";coincidence.pointB="b";
    coincident.constraints={fixed("a",1,2),coincidence};
    result=SketchSolver::solve(coincident);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[1].x(),1)&&near(result.solvedPoints[1].y(),2),
            "Coincident constraint failed");
    coincident.points[1].id="a";
    result=SketchSolver::solve(coincident);
    require(!result.success&&result.solvedPoints.isEmpty(),"Duplicate sketch point ID was accepted");

    SketchModel circles;
    circles.points={{"center",0,5},{"on",0,1},{"line-a",10,0},{"line-b",-10,0},{"center-2",2,5}};
    circles.segments={{"baseline","line-a","line-b"}};
    circles.circles={{"outer","center",4},{"matching","center-2",6}};
    circles.constraints={fixed("center",0,5),fixed("line-a",10,0),fixed("line-b",-10,0)};
    SketchConstraint fixedRadius;fixedRadius.type=SketchConstraintType::Radius;fixedRadius.circleA="outer";fixedRadius.value=5;
    SketchConstraint onCircle;onCircle.type=SketchConstraintType::PointOnCircle;onCircle.pointA="on";onCircle.circleA="outer";
    SketchConstraint centerline;centerline.type=SketchConstraintType::HorizontalDistance;centerline.pointA="center";centerline.pointB="on";
    SketchConstraint tangent;tangent.type=SketchConstraintType::TangentLineCircle;tangent.segmentA="baseline";tangent.circleA="outer";
    SketchConstraint concentric;concentric.type=SketchConstraintType::Concentric;concentric.circleA="outer";concentric.circleB="matching";
    SketchConstraint equalRadius;equalRadius.type=SketchConstraintType::EqualRadius;equalRadius.circleA="outer";equalRadius.circleB="matching";
    circles.constraints.append(fixedRadius);circles.constraints.append(onCircle);circles.constraints.append(centerline);
    circles.constraints.append(tangent);circles.constraints.append(concentric);circles.constraints.append(equalRadius);
    result=SketchSolver::solve(circles);
    require(result.success&&result.degreesOfFreedom==0&&result.solvedCircleRadii.size()==2&&
            near(result.solvedCircleRadii[0],5)&&near(result.solvedCircleRadii[1],5)&&
            near(result.solvedPoints[1].y(),0)&&near(result.solvedPoints[4].x(),0),
            "Circle radius, point-on-circle, tangent, concentric, or equal-radius constraint failed");
    std::swap(circles.segments[0].startPointId,circles.segments[0].endPointId);
    require(SketchSolver::solve(circles).success,"Line-circle tangency changed when the segment direction was reversed");
    SketchModel reopenedCircles;QString circleError;
    require(sketchModelFromJson(sketchModelToJson(circles),&reopenedCircles,&circleError)&&
            reopenedCircles.circles.size()==2&&reopenedCircles.constraints.size()==circles.constraints.size(),
            "Circle sketch JSON did not round trip");
    circles.circles[0].radius=-1;
    require(!SketchSolver::solve(circles).success,"Negative circle radius was accepted");

    SketchModel symmetric;
    symmetric.points={{"axis-low",0,-10},{"axis-high",0,10},{"left",-3,4},{"right",2,2}};
    symmetric.segments={{"mirror-axis","axis-low","axis-high"}};
    SketchConstraint symmetry;symmetry.type=SketchConstraintType::SymmetricAboutLine;
    symmetry.pointA="left";symmetry.pointB="right";symmetry.segmentA="mirror-axis";
    symmetric.constraints={fixed("axis-low",0,-10),fixed("axis-high",0,10),fixed("left",-3,4),symmetry};
    result=SketchSolver::solve(symmetric);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[3].x(),3)&&
            near(result.solvedPoints[3].y(),4),"Point symmetry about a line did not solve the mirrored coordinates");
    SketchModel reopenedSymmetric;QString symmetryError;
    require(sketchModelFromJson(sketchModelToJson(symmetric),&reopenedSymmetric,&symmetryError)&&
            SketchSolver::solve(reopenedSymmetric).success,"Symmetry constraint did not round-trip through sketch JSON");
    SketchModel mirroredPlate;
    mirroredPlate.points={{"a",0,0},{"b",9,1},{"c",9,9},{"d",0,10},
                          {"axis-low",5,-10},{"axis-high",5,20}};
    mirroredPlate.segments={{"ab","a","b"},{"bc","b","c"},{"cd","c","d"},
                            {"da","d","a"},{"axis","axis-low","axis-high"}};
    SketchConstraint lowerSymmetry=symmetry;lowerSymmetry.pointA="a";lowerSymmetry.pointB="b";
    lowerSymmetry.segmentA="axis";
    SketchConstraint upperSymmetry=lowerSymmetry;upperSymmetry.pointA="d";upperSymmetry.pointB="c";
    mirroredPlate.constraints={fixed("axis-low",5,-10),fixed("axis-high",5,20),
                               fixed("a",0,0),fixed("d",0,10),lowerSymmetry,upperSymmetry};
    SceneDocument mirroredDocument;
    require(mirroredDocument.addConstrainedPolygonExtrusion(mirroredPlate,{"a","b","c","d"},2,&symmetryError),
            "Symmetric sketch could not generate a persistent solid");
    GProp_GProps mirroredMass;BRepGProp::VolumeProperties(mirroredDocument.parts().first().shape,mirroredMass);
    require(near(mirroredMass.Mass(),200),"Symmetric sketch generated an incorrect solid volume");
    QTemporaryDir mirroredFolder;require(mirroredFolder.isValid(),"Symmetric sketch folder unavailable");
    const QString mirroredPath=mirroredFolder.filePath("mirrored-plate.cadscene");
    require(mirroredDocument.saveAs(mirroredPath,&symmetryError),"Symmetric sketch scene could not be saved");
    SceneDocument reopenedMirrored;
    require(reopenedMirrored.load(mirroredPath,&symmetryError)&&reopenedMirrored.parts().size()==1&&
            reopenedMirrored.parts().first().constrainedPolygonSketch.constraints.size()==mirroredPlate.constraints.size(),
            "Symmetric sketch scene did not reopen with its editable constraints");
    SketchModel collapsedSymmetry=symmetric;
    collapsedSymmetry.points[2].x=0;collapsedSymmetry.points[3].x=0;
    collapsedSymmetry.constraints[2].fixedX=0;
    require(!SketchSolver::solve(collapsedSymmetry).success,
            "Two mirrored points collapsed onto the axis were accepted as a distinct pair");
    symmetric.constraints.last().segmentA="missing-axis";
    require(!SketchSolver::solve(symmetric).success,"Symmetry accepted a missing mirror axis");

    SketchModel touchingCircles;
    touchingCircles.points={{"first",0,0},{"second",7,1}};
    touchingCircles.circles={{"first-circle","first",5},{"second-circle","second",3}};
    SketchConstraint firstRadius;firstRadius.type=SketchConstraintType::Radius;
    firstRadius.circleA="first-circle";firstRadius.value=5;
    SketchConstraint secondRadius=firstRadius;secondRadius.circleA="second-circle";secondRadius.value=3;
    SketchConstraint sameHeight;sameHeight.type=SketchConstraintType::VerticalDistance;
    sameHeight.pointA="first";sameHeight.pointB="second";sameHeight.value=0;
    SketchConstraint externalTangent;externalTangent.type=SketchConstraintType::TangentCirclesExternal;
    externalTangent.circleA="first-circle";externalTangent.circleB="second-circle";
    touchingCircles.constraints={fixed("first",0,0),firstRadius,secondRadius,sameHeight,externalTangent};
    result=SketchSolver::solve(touchingCircles);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[1].x(),8)&&
            near(result.solvedPoints[1].y(),0),"External circle tangency did not solve center spacing");
    SketchModel reopenedTangent;QString tangentError;
    require(sketchModelFromJson(sketchModelToJson(touchingCircles),&reopenedTangent,&tangentError)&&
            SketchSolver::solve(reopenedTangent).success,"Circle tangency did not round-trip through sketch JSON");
    touchingCircles.constraints.last().circleB="first-circle";
    require(!SketchSolver::solve(touchingCircles).success,"A circle was accepted as tangent to itself");
    touchingCircles.constraints.last().circleB="second-circle";
    touchingCircles.constraints.last().type=SketchConstraintType::TangentCirclesInternal;
    touchingCircles.circles[1].radius=2;
    touchingCircles.constraints[2].value=2;
    touchingCircles.points[1].x=2;touchingCircles.points[1].y=1;
    result=SketchSolver::solve(touchingCircles);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[1].x(),3)&&
            near(result.solvedPoints[1].y(),0),"Internal circle tangency did not solve center spacing");
    require(sketchModelFromJson(sketchModelToJson(touchingCircles),&reopenedTangent,&tangentError)&&
            SketchSolver::solve(reopenedTangent).success,"Internal circle tangency did not survive sketch JSON");
    SketchModel coincidentTangents=touchingCircles;
    coincidentTangents.circles[1].radius=5;
    coincidentTangents.constraints[2].value=5;
    coincidentTangents.points[1].x=0;coincidentTangents.points[1].y=0;
    require(!SketchSolver::solve(coincidentTangents).success,
            "Coincident equal-radius circles were misclassified as internally tangent");

    SketchModel perforated=quadrilateral;
    perforated.points.append({"hole-center",5,5});
    perforated.circles.append({"hole","hole-center",2,true});
    perforated.constraints.append(fixed("hole-center",5,5));
    SketchConstraint holeRadius;holeRadius.type=SketchConstraintType::Radius;holeRadius.circleA="hole";holeRadius.value=2;
    perforated.constraints.append(holeRadius);
    SceneDocument perforatedDocument;QString cutoutError;
    require(perforatedDocument.addConstrainedPolygonExtrusion(perforated,{"a","b","c","d"},2,&cutoutError),
            ("Constrained circular cutout failed: "+cutoutError.toStdString()).c_str());
    GProp_GProps cutoutMass;BRepGProp::VolumeProperties(perforatedDocument.parts().first().shape,cutoutMass);
    require(near(cutoutMass.Mass(),200-8*std::acos(-1.0)),"Circular sketch cutout has the wrong extruded volume");
    SketchModel outside=perforated;outside.constraints[outside.constraints.size()-2].fixedX=10;
    require(!perforatedDocument.addConstrainedPolygonExtrusion(outside,{"a","b","c","d"},2,&cutoutError)&&
            perforatedDocument.parts().size()==1,"Cutout crossing a polygon boundary changed the scene");
    SceneDocument clockwiseDocument;
    require(clockwiseDocument.addConstrainedPolygonExtrusion(perforated,{"a","d","c","b"},2,&cutoutError),
            "Clockwise circular cutout could not be extruded");
    BRepGProp::VolumeProperties(clockwiseDocument.parts().first().shape,cutoutMass);
    require(near(cutoutMass.Mass(),200-8*std::acos(-1.0)),"Clockwise cutout has incorrect volume");

    SceneDocument document;QString error;
    require(document.addConstrainedPolygonExtrusion(quadrilateral,{"a","b","c","d"},2,&error),
            "Constrained quadrilateral could not create an extruded solid");
    GProp_GProps props;BRepGProp::VolumeProperties(document.parts().first().shape,props);
    require(near(props.Mass(),200)&&document.canUndo(),"Constrained extrusion has wrong volume or missing undo checkpoint");
    QTemporaryDir folder;require(folder.isValid(),"Temporary scene folder unavailable");
    const QString path=folder.filePath("constrained-polygon.cadscene");
    require(document.saveAs(path,&error),"Constrained solid could not be saved");
    SceneDocument reopened;require(reopened.load(path,&error)&&reopened.parts().size()==1,
            "Constrained solid could not be reopened");
    require(reopened.parts().first().hasConstrainedPolygonDefinition&&
            reopened.parts().first().constrainedPolygonSketch.constraints.size()==quadrilateral.constraints.size(),
            "Saved constrained sketch did not reopen as an editable definition");
    SketchModel edited=quadrilateral;edited.points[1].x=20;edited.points[2].x=20;
    edited.constraints[6].value=20;
    const QString editedId=reopened.parts().first().id;
    require(reopened.updateConstrainedPolygonExtrusion(editedId,edited,{"a","b","c","d"},2,&error),
            "Reopened constrained sketch could not be edited");
    const QString editedToken=reopened.parts().first().geometryToken;
    require(reopened.updateConstrainedPolygonExtrusion(editedId,edited,{"a","b","c","d"},2,&error)&&
            reopened.parts().first().geometryToken==editedToken,
            "Unchanged constrained sketch edit rebuilt the solid");
    require(!reopened.updateConstrainedPolygonExtrusion(editedId,edited,{"a","c","b","d"},2,&error)&&
            reopened.parts().first().geometryToken==editedToken,
            "Invalid constrained sketch edit changed the existing solid");
    GProp_GProps editedVolume;BRepGProp::VolumeProperties(reopened.parts().first().shape,editedVolume);
    require(near(editedVolume.Mass(),400),"Constraint edit did not rebuild the solid volume");
    require(reopened.undo()&&reopened.redo(),"Constraint edit did not undo and redo");
    const QString editedPath=folder.filePath("edited-polygon.cadscene");
    require(reopened.saveAs(editedPath,&error),"Edited constrained sketch could not be saved");
    SceneDocument editedReopen;require(editedReopen.load(editedPath,&error)&&
            near(editedReopen.parts().first().constrainedPolygonSketch.constraints[6].value,20),
            "Edited constrained sketch dimension did not survive save/reopen");
    QFile editedFile(editedPath);require(editedFile.open(QIODevice::ReadOnly),"Edited scene could not be read for tamper check");
    QJsonObject root=QJsonDocument::fromJson(editedFile.readAll()).object();editedFile.close();
    QJsonArray parts=root.value("parts").toArray();QJsonObject body=parts[0].toObject();
    QJsonObject definition=body.value("parametric_definition").toObject();
    QJsonObject savedSketch=definition.value("sketch").toObject();QJsonArray savedConstraints=savedSketch.value("constraints").toArray();
    QJsonObject changedWidth=savedConstraints[6].toObject();changedWidth.insert("value",15);
    savedConstraints[6]=changedWidth;savedSketch.insert("constraints",savedConstraints);
    definition.insert("sketch",savedSketch);body.insert("parametric_definition",definition);parts[0]=body;root.insert("parts",parts);
    const QString tamperedPath=folder.filePath("tampered-polygon.cadscene");QFile tampered(tamperedPath);
    require(tampered.open(QIODevice::WriteOnly)&&tampered.write(QJsonDocument(root).toJson())>0,
            "Tampered scene fixture could not be written");tampered.close();
    SceneDocument rejected;require(!rejected.load(tamperedPath,&error),
            "Saved solid with a mismatched editable sketch was accepted");
    require(editedReopen.translate(editedId,1,0,0,&error),"Direct geometry edit fixture failed");
    require(!editedReopen.updateConstrainedPolygonExtrusion(editedId,edited,{"a","b","c","d"},3,&error),
            "Stale constrained sketch history overwrote a later geometry edit");
    require(document.undo()&&document.parts().isEmpty()&&document.redo()&&document.parts().size()==1,
            "Constrained extrusion did not undo and redo as one operation");
    require(!document.addConstrainedPolygonExtrusion(quadrilateral,{"a","c","b","d"},2,&error),
            "Self-intersecting constrained boundary was accepted");

    SketchModel midpointSketch;midpointSketch.points={{"a",0,0},{"b",10,0},{"m",4,2}};
    midpointSketch.segments={{"ab","a","b"}};
    SketchConstraint midpoint;midpoint.type=SketchConstraintType::Midpoint;midpoint.pointA="m";midpoint.segmentA="ab";
    midpointSketch.constraints={fixed("a",0,0),fixed("b",10,0),midpoint};
    result=SketchSolver::solve(midpointSketch);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[2].x(),5)&&near(result.solvedPoints[2].y(),0),
            "Midpoint constraint failed");
    midpointSketch.constraints.removeLast();
    SketchConstraint onLine=midpoint;onLine.type=SketchConstraintType::PointOnLine;
    SketchConstraint horizontalDistance;horizontalDistance.type=SketchConstraintType::HorizontalDistance;
    horizontalDistance.pointA="a";horizontalDistance.pointB="m";horizontalDistance.value=6;
    midpointSketch.constraints.append(onLine);midpointSketch.constraints.append(horizontalDistance);
    result=SketchSolver::solve(midpointSketch);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[2].x(),6)&&near(result.solvedPoints[2].y(),0),
            "Point-on-line or signed horizontal distance constraint failed");
    midpointSketch.constraints[2].type=SketchConstraintType::PointOnSegment;
    result=SketchSolver::solve(midpointSketch);
    require(result.success&&near(result.solvedPoints[2].x(),6),"Point-on-segment rejected an interior solution");
    SketchModel projectedSegment=midpointSketch;
    projectedSegment.constraints.removeLast();
    projectedSegment.points[2].x=12;projectedSegment.points[2].y=2;
    result=SketchSolver::solve(projectedSegment);
    require(result.success&&near(result.solvedPoints[2].x(),10)&&near(result.solvedPoints[2].y(),0),
            "A free point beyond the segment did not project onto its endpoint");
    SketchModel reopenedSegment;QString segmentError;
    require(sketchModelFromJson(sketchModelToJson(midpointSketch),&reopenedSegment,&segmentError)&&
            SketchSolver::solve(reopenedSegment).success,"Point-on-segment constraint did not survive sketch JSON");
    midpointSketch.constraints[3].value=12;
    require(!SketchSolver::solve(midpointSketch).success,"Point-on-segment accepted a solution past the endpoint");
    midpointSketch.constraints[2].type=SketchConstraintType::PointOnLine;
    require(SketchSolver::solve(midpointSketch).success,"Infinite point-on-line constraint was incorrectly bounded");
    midpointSketch.constraints[3].value=6;
    SketchConstraint verticalDistance=horizontalDistance;verticalDistance.type=SketchConstraintType::VerticalDistance;
    verticalDistance.value=3;midpointSketch.constraints.append(verticalDistance);
    result=SketchSolver::solve(midpointSketch);
    require(!result.success,"Conflicting point-on-line and vertical distance constraints were accepted");
    midpointSketch.points[0].x=1e300;
    result=SketchSolver::solve(midpointSketch);
    require(!result.success&&result.solvedPoints.isEmpty(),"Unsafe sketch coordinates were accepted");

    SketchModel triangle;triangle.points={{"a",0,0},{"b",4,0},{"c",0,3}};
    triangle.segments={{"base","a","b"},{"left","a","c"},{"right","b","c"}};
    triangle.constraints={fixed("a",0,0),segmentConstraint(SketchConstraintType::Horizontal,"base")};
    auto side=[&](const QString& a,const QString& b,double value){SketchConstraint c;c.type=SketchConstraintType::Distance;c.pointA=a;c.pointB=b;c.value=value;return c;};
    triangle.constraints.append(side("a","b",4));triangle.constraints.append(side("a","c",3));triangle.constraints.append(side("b","c",5));
    require(document.addConstrainedPolygonExtrusion(triangle,{"a","b","c"},2,&error),
            "Triangle constrained by side lengths could not create a solid");
    GProp_GProps triangleVolume;BRepGProp::VolumeProperties(document.parts().last().shape,triangleVolume);
    require(near(triangleVolume.Mass(),12),"Triangle extrusion volume is wrong");

    SketchModel collinear;collinear.points={{"a",0,0},{"b",2,0},{"c",3,0},{"d",6,1}};
    collinear.segments={{"ab","a","b"},{"cd","c","d"}};
    collinear.constraints={fixed("a",0,0),fixed("b",2,0),fixed("c",3,0),
                           segmentConstraint(SketchConstraintType::Collinear,"ab","cd")};
    SketchConstraint gap;gap.type=SketchConstraintType::HorizontalDistance;gap.pointA="c";gap.pointB="d";gap.value=3;
    collinear.constraints.append(gap);
    result=SketchSolver::solve(collinear);
    require(result.success&&result.degreesOfFreedom==0&&near(result.solvedPoints[3].x(),6)&&near(result.solvedPoints[3].y(),0),
            "Collinear constraint failed");

    SketchModel arbitrary;
    arbitrary.points={{"p1",0,0},{"p2",4,0},{"p3",5,2},{"p4",2,4},{"p5",0,2}};
    for(const SketchPoint& p:arbitrary.points)arbitrary.constraints.append(fixed(p.id,p.x,p.y));
    const QStringList outline={"p1","p2","p3","p4","p5"};
    for(int i=0;i<outline.size();++i)
        arbitrary.segments.append({QStringLiteral("edge%1").arg(i),outline[i],outline[(i+1)%outline.size()]});
    SceneDocument arbitraryBody;
    require(arbitraryBody.addConstrainedPolygonExtrusion(arbitrary,outline,3,&error),
            "Five-vertex constrained polygon could not extrude");
    GProp_GProps arbitraryVolume;BRepGProp::VolumeProperties(arbitraryBody.parts().first().shape,arbitraryVolume);
    require(near(arbitraryVolume.Mass(),42),"Five-vertex polygon extrusion volume is wrong");
    SketchModel relational=arbitrary;
    relational.constraints.removeAt(2); // Free p3 so dimensions can drive its position.
    SketchConstraint dx;dx.type=SketchConstraintType::HorizontalDistance;
    dx.pointA="p2";dx.pointB="p3";dx.value=2;
    SketchConstraint dy;dy.type=SketchConstraintType::VerticalDistance;
    dy.pointA="p2";dy.pointB="p3";dy.value=2;
    relational.constraints.append(dx);relational.constraints.append(dy);
    const QString arbitraryId=arbitraryBody.parts().first().id;
    require(arbitraryBody.updateConstrainedPolygonExtrusion(arbitraryId,relational,outline,3,&error),
            "Geometric constraint edit could not rebuild the five-vertex polygon");
    GProp_GProps relationVolume;BRepGProp::VolumeProperties(arbitraryBody.parts().first().shape,relationVolume);
    require(near(relationVolume.Mass(),48),"Dimension-driven vertex did not change the polygon volume");
    const QString relationPath=folder.filePath("relation-polygon.cadscene");
    require(arbitraryBody.saveAs(relationPath,&error),"Geometric relation edit could not be saved");
    SceneDocument relationReopen;
    require(relationReopen.load(relationPath,&error)&&
            relationReopen.parts().first().constrainedPolygonSketch.constraints.size()==6,
            "Geometric relation edit did not survive save/reopen");
}
}

int main() {
    try { run();std::cout<<"PASS: general 2D sketch solver\n";return 0; }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
