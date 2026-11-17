#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool ok,const char* message){
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
void approx(double actual,double expected,double tolerance,const char* message){
    require(std::abs(actual-expected)<=tolerance,message);
}
cad::ShapeFeatureNode feature(const QString& id,const QString& type,const QMap<QString,double>& parameters,
                             const QStringList& sources={}){
    cad::ShapeFeatureNode node;
    node.id=id;node.type=type;node.parameters=parameters;node.inputs=sources;
    return node;
}
double volume(const TopoDS_Shape& shape){
    GProp_GProps props;BRepGProp::VolumeProperties(shape,props);return props.Mass();
}
void checkIncrementalBranches(){
    cad::SceneDocument scene;QString error;
    require(scene.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Box fixture creation failed");
    const QString id=scene.parts().last().id;
    QVector<cad::ShapeFeatureNode> nodes={
        feature("step_x","Translate",{{"x",5},{"y",0},{"z",0}}),
        feature("step_y","Translate",{{"x",0},{"y",10},{"z",0}},{"@base"}),
        feature("overlap","Intersect branches",{}, {"step_x","step_y"})
    };
    require(scene.setShapeFeatureTree(id,nodes,&error),"Branch intersection failed");
    approx(volume(scene.find(id)->shape),3000.0,1e-4,"Unexpected branch-intersection volume");
    require(scene.find(id)->shapeFeatureCachedOutputs.size()==3,"Missing first-build feature cache");
    const TopoDS_Shape unchanged=scene.find(id)->shapeFeatureCachedOutputs[0];
    const QString previousToken=scene.find(id)->geometryToken;
    nodes[1].parameters["y"]=12;
    require(scene.setShapeFeatureTree(id,nodes,&error),"Late feature edit failed");
    approx(volume(scene.find(id)->shape),2400.0,1e-4,"Edited branch did not regenerate intersection");
    require(scene.find(id)->shapeFeatureCachedOutputs[0].IsSame(unchanged),
            "Unchanged feature prefix was not reused");
    require(scene.find(id)->geometryToken!=previousToken,"Successful edit retained geometry revision token");
    const QString stableToken=scene.find(id)->geometryToken;
    nodes[1].parameters["y"]=100; // no overlap
    require(!scene.setShapeFeatureTree(id,nodes,&error),"Empty intersection was accepted");
    require(scene.find(id)->geometryToken==stableToken,"Invalid edit committed a token");
    approx(volume(scene.find(id)->shape),2400.0,1e-4,"Invalid edit mutated the geometry");
    require(scene.find(id)->shapeFeatureCachedOutputs[0].IsSame(unchanged),
            "Invalid edit invalidated a good prefix");
    require(scene.undo(),"Undo failed");
    approx(volume(scene.find(id)->shape),3000.0,1e-4,"Undo failed to restore earlier graph");
    require(scene.redo(),"Redo failed");
    approx(volume(scene.find(id)->shape),2400.0,1e-4,"Redo failed to restore edited graph");
    QTemporaryDir temporary;
    require(temporary.isValid(),"Cannot create temporary folder");
    const QString path=temporary.path()+"/rebuild.cadscene";
    require(scene.saveAs(path,&error),"Saving edited graph failed");
    cad::SceneDocument reopened;
    require(reopened.load(path,&error),"Reopening edited graph failed");
    require(reopened.find(id)->shapeFeatureCachedOutputs.size()==3,
            "Saved graph did not reconstruct its transient caches");
    nodes[1].parameters["y"]=14;
    require(reopened.setShapeFeatureTree(id,nodes,&error),"Edit after reopening failed");
    approx(volume(reopened.find(id)->shape),1800.0,1e-4,"Reopened graph produced wrong volume");
}
void checkOrientationAndPatternFeatures(){
    cad::SceneDocument scene;QString error;
    require(scene.addPrimitive(cad::Primitive::Box,{10,10,5},&error),"Box fixture failed");
    const QString id=scene.parts().last().id;
    const QVector<cad::ShapeFeatureNode> nodes={
        feature("rotate","Rotate arbitrary axis",{{"degrees",90},{"axis_x",0},{"axis_y",0},{"axis_z",2},
                     {"pivot_x",0},{"pivot_y",0},{"pivot_z",0}}),
        feature("reflect","Mirror arbitrary plane",{{"normal_x",1},{"normal_y",0},{"normal_z",0},
                     {"origin_x",0},{"origin_y",0},{"origin_z",0}}),
        feature("pattern","Linear pattern vector",{{"count",3},{"dx",30},{"dy",5},{"dz",0}})
    };
    require(scene.setShapeFeatureTree(id,nodes,&error),"Custom transform and vector pattern failed");
    approx(volume(scene.find(id)->shape),1500.0,1e-3,"Vector pattern volume incorrect");
    Bnd_Box bounds;BRepBndLib::AddOptimal(scene.find(id)->shape,bounds,false,false);
    double x0,y0,z0,x1,y1,z1;bounds.Get(x0,y0,z0,x1,y1,z1);
    require(x1-x0>60.0&&y1-y0>15.0,"Pattern did not offset all three copies");
    auto invalid=nodes;
    invalid[2].parameters["dx"]=0;invalid[2].parameters["dy"]=0;
    require(!cad::validateShapeFeatureNodes(invalid,&error),"Zero vector pattern accepted");
    invalid=nodes;invalid[0].parameters["axis_z"]=0;
    require(!cad::validateShapeFeatureNodes(invalid,&error),"Zero rotation axis accepted");
    invalid=nodes;invalid[1].parameters["normal_x"]=0;
    require(!cad::validateShapeFeatureNodes(invalid,&error),"Zero mirror normal accepted");
    QVector<cad::ShapeFeatureNode> circular={feature("circle","Circular pattern arbitrary axis",
        {{"count",3},{"sweep_degrees",360},{"axis_x",0},{"axis_y",0},{"axis_z",1},
         {"pivot_x",50},{"pivot_y",50},{"pivot_z",0}})};
    require(scene.setShapeFeatureTree(id,circular,&error),"Arbitrary-axis circular pattern failed");
    approx(volume(scene.find(id)->shape),1500.0,1e-3,"Circular pattern volume incorrect");
    // A corrupted/incompatible cache is rejected without replacing either output.
    const TopoDS_Shape original=scene.find(id)->shape;
    TopoDS_Shape preview=original;
    QVector<TopoDS_Shape> invalidPrefix={original,original};
    require(!cad::rebuildShapeFeatureTree(scene.find(id)->shapeFeatureBase,false,circular,
        &preview,&error,&invalidPrefix),"Oversized feature cache accepted");
    require(preview.IsSame(original),"Cache failure overwrote preview output");
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        checkIncrementalBranches();
        checkOrientationAndPatternFeatures();
        std::cout<<"PASS: "<<checks<<" v1.65 incremental graph/transform assertions\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"FAIL after check "<<checks<<": "<<ex.what()<<'\n';
        return 1;
    }
}
