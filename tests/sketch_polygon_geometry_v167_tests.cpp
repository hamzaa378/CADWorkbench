#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
double volume(const TopoDS_Shape& shape){GProp_GProps p;BRepGProp::VolumeProperties(shape,p);return p.Mass();}
using cad::ShapeFeatureNode;
ShapeFeatureNode polygonNode(const QString& type,const QString& id,
        const QVector<QPointF>& lower,const QVector<QPointF>& upper={},
        double base=8,double height=12){
    ShapeFeatureNode result;result.id=id;result.type=type;
    result.parameters.insert("profile_count",lower.size());
    for(int i=0;i<lower.size();++i){const QString key="p"+QString::number(i);
        result.parameters.insert(key+"_x",lower[i].x());result.parameters.insert(key+"_y",lower[i].y());}
    for(int i=0;i<upper.size();++i){const QString key="q"+QString::number(i);
        result.parameters.insert(key+"_x",upper[i].x());result.parameters.insert(key+"_y",upper[i].y());}
    if(type!="Sketch polygon revolve Z"){
        result.parameters.insert("base_z",base);result.parameters.insert("height",height);
    }
    return result;
}
void testOperation(const QString& type,const QVector<QPointF>& lower,
                   const QVector<QPointF>& upper,double base,double height,bool pocket){
    cad::SceneDocument doc;QString issue;
    check(doc.addPrimitive(cad::Primitive::Box,{30,30,10},&issue),"Base box creation failed");
    const QString id=doc.parts().last().id;
    const double initial=volume(doc.find(id)->shape);
    ShapeFeatureNode node=polygonNode(type,"profile1",lower,upper,base,height);
    QVector<ShapeFeatureNode> nodes={node};
    check(cad::validateShapeFeatureNodes(nodes,&issue),"Valid polygon feature rejected by schema");
    check(doc.setShapeFeatureTree(id,nodes,&issue),"Polygon geometry feature rebuild failed");
    const double changed=volume(doc.find(id)->shape);
    check(pocket?changed<initial-1:changed>initial+1,"Boolean did not change the body as expected");
    const TopoDS_Shape committed=doc.find(id)->shape;
    TopoDS_Shape fresh;
    check(cad::rebuildShapeFeatureTree(doc.find(id)->shapeFeatureBase,false,nodes,&fresh,&issue),
          "Fresh feature rebuild failed");
    check(std::abs(volume(fresh)-changed)<1e-4,"Cached and uncached volumes differ");
    const QString acceptedToken=doc.find(id)->geometryToken;
    auto invalid=nodes;invalid[0].parameters["p1_x"]=invalid[0].parameters.value("p0_x");
    invalid[0].parameters["p1_y"]=invalid[0].parameters.value("p0_y");
    check(!doc.setShapeFeatureTree(id,invalid,&issue),"Degenerate polygon was accepted");
    check(doc.find(id)->geometryToken==acceptedToken,"Invalid polygon changed the geometry token");
    check(std::abs(volume(doc.find(id)->shape)-changed)<1e-4,"Invalid polygon changed the body");
    check(doc.undo(),"Undo polygon feature failed");
    check(doc.redo(),"Redo polygon feature failed");
    check(std::abs(volume(doc.find(id)->shape)-changed)<1e-4,"Redo did not restore polygon geometry");
    QTemporaryDir dir;
    check(dir.isValid(),"Temp directory failed");
    const QString file=dir.path()+"/polygon.cadscene";
    check(doc.saveAs(file,&issue),"Save polygon scene failed");
    cad::SceneDocument restored;
    check(restored.load(file,&issue),"Reload polygon scene failed");
    check(restored.find(id)->shapeFeatureNodes.size()==1,"Reload lost feature node");
    check(restored.find(id)->shapeFeatureNodes.first().parameters==node.parameters,
          "Reload lost polygon coordinates");
    check(std::abs(volume(restored.find(id)->shape)-changed)<1e-4,
          "Reload changed the reconstructed polygon geometry");
    nodes[0].parameters["p0_x"]+=0.25;
    check(restored.setShapeFeatureTree(id,nodes,&issue),"Edit polygon after reopening failed");
    check(!restored.find(id)->shape.IsSame(committed),"Edited polygon did not rebuild geometry");
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        const QVector<QPointF> rectangle={{5,5},{12,5},{12,12},{5,12}};
        const QVector<QPointF> upper={{3,3},{14,3},{14,14},{3,14}};
        const QVector<QPointF> radial={{4,4},{8,4},{8,16},{4,16}};
        testOperation("Sketch polygon extrude Z",rectangle,{},8,12,false);
        testOperation("Sketch polygon pocket Z",rectangle,{},1,10,true);
        testOperation("Sketch polygon revolve Z",radial,{},0,0,false);
        testOperation("Sketch polygons loft Z",rectangle,upper,8,12,false);
        std::cout<<"PASS: "<<checks<<" native sketch-polygon geometry assertions\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL check "<<checks<<": "<<e.what()<<'\n';return 1;}
}
