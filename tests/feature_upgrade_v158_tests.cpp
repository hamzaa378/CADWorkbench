#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void expect(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
double volume(const TopoDS_Shape& shape){GProp_GProps properties;BRepGProp::VolumeProperties(shape,properties);return properties.Mass();}
cad::ShapeFeatureNode node(const QString& id,const QString& type,const QMap<QString,double>& fields){
    return cad::ShapeFeatureNode{id,type,fields,false};
}
void bounds(const TopoDS_Shape& shape,double p[6]){
    Bnd_Box box;BRepBndLib::AddOptimal(shape,box,false,false);
    box.Get(p[0],p[1],p[2],p[3],p[4],p[5]);
}
int topFace(const TopoDS_Shape& shape,double z){
    int index=0;
    for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next()){
        ++index;const auto face=TopoDS::Face(e.Current());
        const BRepAdaptor_Surface adaptor(face);
        if(adaptor.GetType()!=GeomAbs_Plane)continue;
        GProp_GProps props;BRepGProp::SurfaceProperties(face,props);
        if(std::abs(props.CentreOfMass().Z()-z)<1e-6)return index;
    }
    return 0;
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        QString error;
        cad::SceneDocument doc;
        expect(doc.addPrimitive(cad::Primitive::Box,{10,12,8},&error),"Could not create box fixture");
        const QString id=doc.parts().first().id;
        const double initial=volume(doc.find(id)->shape);
        expect(std::abs(initial-960)<0.01,"Unexpected source volume");

        expect(doc.setShapeFeatureTree(id,{node("pattern","Linear pattern X",{{"count",3},{"spacing",20}})},&error),
               "Linear X feature pattern failed");
        expect(std::abs(volume(doc.find(id)->shape)-3*initial)<0.01,"Linear feature pattern volume mismatch");
        double xyz[6];bounds(doc.find(id)->shape,xyz);
        expect(xyz[3]>49.9&&xyz[3]<50.1,"Linear feature pattern extent mismatch");
        expect(doc.shapeFeatureTreeCurrent(id),"Pattern feature tree should be current");
        expect(doc.undo()&&std::abs(volume(doc.find(id)->shape)-initial)<0.1,"Pattern undo failed");
        expect(doc.redo()&&std::abs(volume(doc.find(id)->shape)-3*initial)<0.1,"Pattern redo failed");

        expect(doc.setShapeFeatureTree(id,{node("mirror","Mirror XY",{})},&error),
               "Mirroring through XY plane failed");
        bounds(doc.find(id)->shape,xyz);
        expect(xyz[2]<-7.99&&std::abs(xyz[5])<0.01,"Mirror axis geometry mismatch");
        expect(std::abs(volume(doc.find(id)->shape)-initial)<0.01,"Mirror changed solid mass");

        expect(doc.setShapeFeatureTree(id,{node("moved","Translate",{{"x",30},{"y",0},{"z",0}}),
            node("circular","Circular pattern Z",{{"count",4},{"sweep_degrees",360}})},&error),
            "Circular feature pattern failed");
        expect(std::abs(volume(doc.find(id)->shape)-4*initial)<0.01,"Circular pattern volume mismatch");
        bounds(doc.find(id)->shape,xyz);
        expect(xyz[0]<-29.9&&xyz[3]>39.9,"Circular pattern did not rotate around global Z");

        auto invalid=node("bad","Linear pattern Y",{{"count",2.5},{"spacing",5}});
        const auto token=doc.find(id)->geometryToken;
        expect(!doc.setShapeFeatureTree(id,{invalid},&error),"Fractional pattern count was accepted");
        expect(doc.find(id)->geometryToken==token,"Invalid count mutated geometry");
        invalid.parameters.insert("count",25);
        expect(!doc.setShapeFeatureTree(id,{invalid},&error),"Excessive pattern count was accepted");
        invalid.parameters.insert("count",3);invalid.parameters.insert("spacing",-1);
        expect(!doc.setShapeFeatureTree(id,{invalid},&error),"Negative pattern spacing was accepted");
        expect(doc.find(id)->geometryToken==token,"Failed pattern left partial changes");

        expect(doc.setShapeFeatureTree(id,{node("hole","Cylinder cut X",{{"x",-1},{"y",6},{"z",4},
                {"radius",2},{"depth",12}})},&error),"X axis pocket failed");
        expect(volume(doc.find(id)->shape)<initial-100,"X axis bore did not remove material");
        expect(doc.setShapeFeatureTree(id,{node("hole","Cylinder cut Y",{{"x",5},{"y",-1},{"z",4},
                {"radius",2},{"depth",14}})},&error),"Y axis pocket failed");
        expect(volume(doc.find(id)->shape)<initial-110,"Y axis bore did not remove material");

        const int face=topFace(doc.find(id)->shapeFeatureBase,8);
        expect(face>0,"Top planar face is missing");
        expect(doc.setShapeFeatureTree(id,{node("pocket","Face rectangle pocket",{{"face_index",double(face)},
                {"u",0},{"v",0},{"width",4},{"height",4},{"depth",3}})},&error),
               "Face rectangular pocket failed");
        expect(volume(doc.find(id)->shape)<initial-45,"Face rectangle pocket has no volume change");
        expect(doc.setShapeFeatureTree(id,{node("boss","Face rectangle boss",{{"face_index",double(face)},
                {"u",0},{"v",0},{"width",4},{"height",4},{"depth",3}})},&error),
               "Face rectangular boss failed");
        expect(volume(doc.find(id)->shape)>initial+45,"Face rectangle boss has no volume change");

        // Adding a sequential node should use the cached previous B-rep while
        // retaining the frozen source and complete serializable node history.
        cad::SceneDocument incremental;
        expect(incremental.addPrimitive(cad::Primitive::Box,{10,12,8},&error),
               "Incremental fixture could not be created");
        const QString incrementalId=incremental.parts().first().id;
        auto translation=node("translate","Translate",{{"x",30},{"y",0},{"z",0}});
        auto repetitions=node("repeat","Linear pattern Y",{{"count",3},{"spacing",20}});
        expect(incremental.setShapeFeatureTree(incrementalId,{translation},&error),
               "First incremental modifier failed");
        expect(incremental.setShapeFeatureTree(incrementalId,{translation,repetitions},&error),
               "Appending a modifier to existing geometry failed");
        expect(std::abs(volume(incremental.find(incrementalId)->shape)-3*initial)<0.01&&
               incremental.find(incrementalId)->shapeFeatureNodes.size()==2,
               "Incremental append did not retain graph history");
        expect(incremental.undo()&&incremental.find(incrementalId)->shapeFeatureNodes.size()==1,
               "Incremental append should be one undo step");
        expect(incremental.redo()&&incremental.find(incrementalId)->shapeFeatureNodes.size()==2,
               "Incremental append redo failed");

        // Smoke-test shell and draft parameter validation without depending on
        // kernel-specific draft/shell success for every OCCT patch release.
        expect(!cad::validateShapeFeatureNodes({node("shell","Open-face shell",{{"face_index",1},
            {"thickness",-1}})},&error),"Negative shell thickness was accepted");
        expect(cad::validateShapeFeatureNodes({node("shell","Open-face shell",{{"face_index",1},
            {"thickness",1}})},&error),"Valid shell parameters were rejected");
        expect(!cad::validateShapeFeatureNodes({node("draft","Draft face Z",{{"face_index",1},
            {"angle_degrees",50},{"neutral_z",0}})},&error),"Excessive draft angle accepted");
        expect(cad::validateShapeFeatureNodes({node("draft","Draft face Z",{{"face_index",1},
            {"angle_degrees",5},{"neutral_z",0}})},&error),"Valid draft settings rejected");

        QTemporaryDir folder;expect(folder.isValid(),"Could not allocate temporary project directory");
        const QString filename=folder.path()+"/v158-project.cadscene";
        expect(doc.saveAs(filename,&error),"Could not persist v1.58 feature tree");
        cad::SceneDocument reopened;
        expect(reopened.load(filename,&error),"Could not load v1.58 feature tree");
        expect(reopened.find(id)!=nullptr&&reopened.find(id)->shapeFeatureNodes.size()==1&&
            reopened.find(id)->shapeFeatureNodes[0].type=="Face rectangle boss"&&
            std::abs(volume(reopened.find(id)->shape)-volume(doc.find(id)->shape))<0.01,
            "New face-based feature did not persist");
        std::cout<<"PASS "<<checks<<" feature-tree regression checks\n";
        return 0;
    }catch(const std::exception& exception){
        std::cerr<<"FAIL check "<<checks<<": "<<exception.what()<<'\n';return 1;
    }
}
