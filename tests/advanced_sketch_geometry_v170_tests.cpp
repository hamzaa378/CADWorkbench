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
void test(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
double volume(const TopoDS_Shape& s){GProp_GProps props;BRepGProp::VolumeProperties(s,props);return props.Mass();}
using cad::ShapeFeatureNode;
void outline(ShapeFeatureNode* node,const QString& prefix,const QVector<QPointF>& points){
    for(int i=0;i<points.size();++i){const QString k=prefix+QString::number(i);
        node->parameters.insert(k+"_x",points[i].x());node->parameters.insert(k+"_y",points[i].y());
    }
}
ShapeFeatureNode makeNode(const QString& type){
    ShapeFeatureNode node;node.id="advanced_1";node.type=type;
    const bool revolve=type=="Sketch polygon revolve axis";
    const QVector<QPointF> outline2=revolve?QVector<QPointF>{{4,4},{8,4},{8,16},{4,16}}:
       QVector<QPointF>{{-2,-2},{2,-2},{2,2},{-2,2}};
    node.parameters.insert("profile_count",outline2.size());
    if(type=="Sketch multi-section loft Z"){
        node.parameters.insert("section_count",3);
        const QVector<QPointF> bottom={{5,5},{12,5},{12,12},{5,12}};
        const QVector<QPointF> middle={{3,3},{14,3},{14,14},{3,14}};
        outline(&node,"s0_p",bottom);outline(&node,"s1_p",middle);outline(&node,"s2_p",bottom);
        node.parameters.insert("s0_z",8);node.parameters.insert("s1_z",15);node.parameters.insert("s2_z",24);
    }else{
        outline(&node,"p",outline2);
        if(type=="Sketch polygon sweep 3D"||type=="Sketch polygon sweep spline 3D"){
            node.parameters.insert("path_count",2);
            node.parameters.insert("t0_x",10);node.parameters.insert("t0_y",10);node.parameters.insert("t0_z",8);
            node.parameters.insert("t1_x",10);node.parameters.insert("t1_y",10);node.parameters.insert("t1_z",14);
            if(type=="Sketch polygon sweep spline 3D"){
                node.parameters.insert("path_count",4);
                node.parameters.insert("t2_x",10);node.parameters.insert("t2_y",12);node.parameters.insert("t2_z",19);
                node.parameters.insert("t3_x",12);node.parameters.insert("t3_y",15);node.parameters.insert("t3_z",24);
            }
        }else{
            node.parameters.insert("origin_x",10);node.parameters.insert("origin_y",10);node.parameters.insert("origin_z",0);
            node.parameters.insert("axis_x",0);node.parameters.insert("axis_y",0);node.parameters.insert("axis_z",1);
            node.parameters.insert("sweep_degrees",360);
        }
    }
    return node;
}
void exercise(const QString& type){
    cad::SceneDocument doc;QString error;
    test(doc.addPrimitive(cad::Primitive::Box,{30,30,10},&error),"base box failed");
    const QString id=doc.parts().last().id;
    const double original=volume(doc.find(id)->shape);
    ShapeFeatureNode node=makeNode(type);
    test(cad::validateShapeFeatureNodes({node},&error),"feature validation failed");
    if(type=="Sketch polygon sweep spline 3D"){
        auto insufficient=node;
        insufficient.parameters["path_count"]=2;
        insufficient.parameters.remove("t2_x");insufficient.parameters.remove("t2_y");insufficient.parameters.remove("t2_z");
        insufficient.parameters.remove("t3_x");insufficient.parameters.remove("t3_y");insufficient.parameters.remove("t3_z");
        test(!cad::validateShapeFeatureNodes({insufficient},&error),"2-station spline accepted");
    }
    test(doc.setShapeFeatureTree(id,{node},&error),"feature rebuild failed");
    const double updated=volume(doc.find(id)->shape);
    test(updated>original+1,"advanced operation did not add material");
    TopoDS_Shape cold;
    test(cad::rebuildShapeFeatureTree(doc.find(id)->shapeFeatureBase,false,{node},&cold,&error),
         "cold rebuilding advanced profile failed");
    test(std::abs(volume(cold)-updated)<1e-4,"cold rebuild different volume");
    const QString token=doc.find(id)->geometryToken;
    auto invalid=node;
    if(type=="Sketch polygon sweep 3D"||type=="Sketch polygon sweep spline 3D")invalid.parameters["t1_z"]=8;
    else if(type=="Sketch multi-section loft Z")invalid.parameters["s1_z"]=7;
    else invalid.parameters["axis_z"]=0;
    test(!doc.setShapeFeatureTree(id,{invalid},&error),"bad profile accepted");
    test(doc.find(id)->geometryToken==token,"rollback changed committed token");
    test(std::abs(volume(doc.find(id)->shape)-updated)<1e-4,"rollback changed solid");
    test(doc.undo(),"undo failed");test(doc.redo(),"redo failed");
    test(std::abs(volume(doc.find(id)->shape)-updated)<1e-4,"redo changed geometry");
    QTemporaryDir folder;test(folder.isValid(),"temp directory failed");
    const QString file=folder.path()+"/advanced.cadscene";
    test(doc.saveAs(file,&error),"save failed");
    cad::SceneDocument loaded;test(loaded.load(file,&error),"reopen failed");
    test(loaded.find(id)->shapeFeatureNodes.size()==1,"saved feature lost");
    test(loaded.find(id)->shapeFeatureNodes.first().parameters==node.parameters,"saved parameters lost");
    test(std::abs(volume(loaded.find(id)->shape)-updated)<1e-4,"reopened volume mismatch");
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        exercise("Sketch polygon sweep 3D");
        exercise("Sketch polygon sweep spline 3D");
        exercise("Sketch multi-section loft Z");
        exercise("Sketch polygon revolve axis");
        std::cout<<"PASS: "<<checks<<" advanced sketch geometry/persistence assertions\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL #"<<checks<<": "<<e.what()<<"\n";return 1;}
}
