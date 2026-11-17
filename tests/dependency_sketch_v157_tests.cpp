#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include "core/SketchProfile.h"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <stdexcept>
#include <cmath>
#include <iostream>

namespace {
int checks=0;
void expect(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
double volume(const TopoDS_Shape& shape){GProp_GProps props;BRepGProp::VolumeProperties(shape,props);return props.Mass();}
void bounds(const TopoDS_Shape& shape,double values[6]){
    Bnd_Box box;BRepBndLib::AddOptimal(shape,box,false,false);
    box.Get(values[0],values[1],values[2],values[3],values[4],values[5]);
}
cad::ShapeFeatureNode node(const QString& id,const QString& type,const QMap<QString,double>& parameters,
                            const QStringList& inputs={}){
    cad::ShapeFeatureNode result{id,type,parameters,false};result.inputs=inputs;return result;
}
cad::SketchModel profile(bool curved){
    cad::SketchModel s;
    s.points={{"a",0,0},{"b",40,0},{"c",40,30},{"d",0,30}};
    if(curved){
        s.points.append({"arc_mid",20,-8});
        s.points.append({"handle1",33,42});s.points.append({"handle2",7,42});
        s.arcs.append({"arc_1","a","b","arc_mid"});
        s.splines.append({"spline_1","c","d","handle1","handle2"});
    }
    for(const auto& p:s.points){
        cad::SketchConstraint fixed;fixed.type=cad::SketchConstraintType::FixedPoint;
        fixed.pointA=p.id;fixed.fixedX=p.x;fixed.fixedY=p.y;
        s.constraints.append(fixed);
    }
    s.segments={{"ab","a","b"},{"bc","b","c"},{"cd","c","d"},{"da","d","a"}};
    return s;
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        QString error;
        const auto curved=profile(true);
        cad::SketchPlane plane;plane.originX=12;plane.originY=8;plane.originZ=30;
        plane.normalX=1;plane.normalY=0;plane.normalZ=0;
        cad::SceneDocument sketches;
        expect(sketches.addConstrainedPolygonExtrusion(curved,{"a","b","c","d"},12,&error,plane),
               "Could not extrude a curved profile on an arbitrary plane");
        const QString sketchId=sketches.parts().first().id;
        double xyz[6];bounds(sketches.find(sketchId)->shape,xyz);
        expect(std::abs(xyz[0]-12)<0.01&&std::abs(xyz[3]-24)<0.01,
               "Sketch datum normal was not used as extrusion direction");
        expect(volume(sketches.find(sketchId)->shape)>14000,
               "Curved sketch extrusion has unexpected volume");
        const auto serialized=cad::sketchModelToJson(curved);
        cad::SketchModel recovered;
        expect(cad::sketchModelFromJson(serialized,&recovered,&error)&&
               recovered.arcs.size()==1&&recovered.splines.size()==1,
               "Arc/spline identities were not roundtripped");
        auto badPlane=plane;badPlane.normalX=badPlane.normalY=badPlane.normalZ=0;
        const double before=volume(sketches.find(sketchId)->shape);
        const QString oldToken=sketches.find(sketchId)->geometryToken;
        expect(!sketches.updateConstrainedPolygonExtrusion(sketchId,curved,{"a","b","c","d"},12,&error,badPlane),
               "Zero sketch normal was accepted");
        expect(sketches.find(sketchId)->geometryToken==oldToken&&
               std::abs(volume(sketches.find(sketchId)->shape)-before)<1e-6,
               "Invalid sketch plane mutated the open scene");

        cad::SceneDocument graph;
        expect(graph.addPrimitive(cad::Primitive::Box,{30,30,12},&error),"Unable to add graph source box");
        const QString body=graph.parts().first().id;
        QVector<cad::ShapeFeatureNode> dag={
            node("left","Translate",{{"x",0},{"y",0},{"z",0}},{"@base"}),
            node("right","Translate",{{"x",15},{"y",0},{"z",0}},{"@base"}),
            node("union","Fuse branches",{}, {"left","right"})
        };
        expect(graph.setShapeFeatureTree(body,dag,&error),"Feature branch fusion rejected");
        expect(std::abs(volume(graph.find(body)->shape)-30.0*45.0*12.0)<0.1,
               "Feature branch fusion produced incorrect volume");
        dag.append(node("cut","Subtract branches",{}, {"union","right"}));
        expect(graph.setShapeFeatureTree(body,dag,&error),"Feature branch subtraction rejected");
        expect(std::abs(volume(graph.find(body)->shape)-15.0*30.0*12.0)<0.1,
               "Feature graph did not route branches to Boolean subtraction");
        const auto stable=graph.find(body)->geometryToken;
        dag[1].inputs={"cut"};
        expect(!graph.setShapeFeatureTree(body,dag,&error),"Cycle or forward input was accepted");
        expect(graph.find(body)->geometryToken==stable,"Invalid feature graph damaged body");
        dag[1].inputs={"@base"};
        const double graphVolume=volume(graph.find(body)->shape);
        expect(graph.undo(),"Undo of feature graph failed");
        expect(graph.redo(),"Redo of feature graph failed");
        expect(std::abs(volume(graph.find(body)->shape)-graphVolume)<1e-4,
               "Redo did not restore feature graph result");

        // A face pocket attaches to an upstream planar face of a primitive.
        cad::SceneDocument faceDoc;
        expect(faceDoc.addPrimitive(cad::Primitive::Box,{30,30,12},&error),"Box fixture failed");
        const QString faceId=faceDoc.parts().first().id;
        int topIndex=0,faceIndex=0;
        for(TopExp_Explorer e(faceDoc.find(faceId)->shape,TopAbs_FACE);e.More();e.Next()){
            ++faceIndex;const TopoDS_Face face=TopoDS::Face(e.Current());
            BRepAdaptor_Surface surface(face);
            if(surface.GetType()!=GeomAbs_Plane)continue;
            GProp_GProps area;BRepGProp::SurfaceProperties(face,area);
            if(std::abs(area.CentreOfMass().Z()-12)<0.001){topIndex=faceIndex;break;}
        }
        expect(topIndex>0,"Could not locate top planar face");
        const double faceBefore=volume(faceDoc.find(faceId)->shape);
        expect(faceDoc.setShapeFeatureTree(faceId,{
            node("pocket","Face normal pocket",{{"face_index",double(topIndex)},
                {"u",0},{"v",0},{"radius",2},{"depth",8}})},&error),
            "Face-normal pocket did not build");
        expect(volume(faceDoc.find(faceId)->shape)<faceBefore-90,
            "Face pocket did not remove expected material");

        // This is the critical upstream/downstream regression: updating the
        // saved sketch replays existing feature nodes instead of making them stale.
        cad::SceneDocument history;
        auto rectangle=profile(false);
        expect(history.addConstrainedPolygonExtrusion(rectangle,{"a","b","c","d"},12,&error),
               "Initial parametric sketch failed");
        const QString id=history.parts().first().id;
        const auto cutter=node("cut","Cylinder cut Z",{{"x",15},{"y",12},
            {"z",0},{"radius",3},{"depth",12}});
        expect(history.setShapeFeatureTree(id,{cutter},&error),
               "Unable to attach downstream cylinder cut to sketch");
        const double initial=volume(history.find(id)->shape);
        rectangle.points[1].x=50;rectangle.points[2].x=50;
        for(auto& fixed:rectangle.constraints)if(fixed.pointA=="b"||fixed.pointA=="c")fixed.fixedX=50;
        expect(history.updateConstrainedPolygonExtrusion(id,rectangle,{"a","b","c","d"},12,&error),
               "Editing source sketch did not regenerate downstream features");
        expect(history.shapeFeatureTreeCurrent(id)&&
               history.find(id)->constrainedPolygonGeometryToken==history.find(id)->geometryToken,
               "Upstream edit broke persistent feature history");
        expect(volume(history.find(id)->shape)>initial+3500,
               "Sketch expansion did not propagate to downstream solid");
        const double expanded=volume(history.find(id)->shape);
        expect(history.undo()&&std::abs(volume(history.find(id)->shape)-initial)<0.1,
               "Undo of upstream sketch edit did not restore downstream model");
        expect(history.redo()&&std::abs(volume(history.find(id)->shape)-expanded)<0.1,
               "Redo of upstream sketch edit did not restore downstream model");
        cad::SketchPlane relocated;relocated.originZ=50;
        const auto safeToken=history.find(id)->geometryToken;
        expect(!history.updateConstrainedPolygonExtrusion(id,rectangle,{"a","b","c","d"},12,&error,relocated),
               "Invalid downstream cut after moving source plane was not detected");
        expect(history.find(id)->geometryToken==safeToken&&
               std::abs(volume(history.find(id)->shape)-expanded)<1e-4,
               "Failed dependency graph regeneration changed the solid");

        QTemporaryDir folder;expect(folder.isValid(),"Missing temporary test folder");
        const QString file=folder.path()+"/feature-graph-v24.cadscene";
        expect(history.saveAs(file,&error),"Could not persist edited feature graph");
        cad::SceneDocument reopened;
        expect(reopened.load(file,&error),"Could not reopen feature graph and sketch plane");
        expect(reopened.find(id)!=nullptr&&reopened.shapeFeatureTreeCurrent(id)&&
               std::abs(volume(reopened.find(id)->shape)-expanded)<0.1,
               "Saved feature graph did not preserve regenerated shape");
        const QString curvedFile=folder.path()+"/curved.cadscene";
        expect(sketches.saveAs(curvedFile,&error),"Could not save curved sketch document");
        cad::SceneDocument curvedLoaded;
        expect(curvedLoaded.load(curvedFile,&error)&&
               curvedLoaded.parts().first().constrainedPolygonSketch.arcs.size()==1&&
               curvedLoaded.parts().first().constrainedPolygonSketch.splines.size()==1,
               "Curved sketch plane did not survive save/reopen");
        const QString graphFile=folder.path()+"/branches.cadscene";
        expect(graph.saveAs(graphFile,&error),"Could not save branch graph");
        cad::SceneDocument branchLoaded;
        expect(branchLoaded.load(graphFile,&error)&&
               branchLoaded.find(body)->shapeFeatureNodes[2].inputs==QStringList({"left","right"}),
               "Graph source connections did not survive save/reopen");
        // Tamper persisted graph reference; loader must reject without replacing document.
        QFile raw(graphFile);expect(raw.open(QIODevice::ReadOnly),"Could not inspect saved graph");
        auto json=QJsonDocument::fromJson(raw.readAll()).object();raw.close();
        auto bodies=json.value("parts").toArray();auto saved=bodies[0].toObject();
        auto tree=saved.value("shape_feature_tree").toObject();auto sequence=tree.value("nodes").toArray();
        auto third=sequence[2].toObject();third.insert("inputs",QJsonArray{QStringLiteral("unknown"),QStringLiteral("right")});
        sequence.replace(2,third);tree.insert("nodes",sequence);saved.insert("shape_feature_tree",tree);
        bodies.replace(0,saved);json.insert("parts",bodies);
        const QString invalid=folder.path()+"/bad-graph.cadscene";
        QFile out(invalid);expect(out.open(QIODevice::WriteOnly),"Could not write invalid graph fixture");
        out.write(QJsonDocument(json).toJson());out.close();
        const QString goodToken=branchLoaded.find(body)->geometryToken;
        expect(!branchLoaded.load(invalid,&error),"Invalid persisted graph was accepted");
        expect(branchLoaded.find(body)->geometryToken==goodToken,
               "Corrupt graph replaced the open document");

        std::cout<<"PASS: "<<checks<<" sketch plane, curve, dependency graph and face feature checks\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL at check "<<checks<<": "<<e.what()<<'\n';return 1;
    }
}
