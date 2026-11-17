#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Pnt.hxx>
#include <TopoDS.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool success,const char* message){++checks;if(!success)throw std::runtime_error(message);}
double volume(const TopoDS_Shape& shape){GProp_GProps props;BRepGProp::VolumeProperties(shape,props);return props.Mass();}
void near(double actual,double expected,const char* message){require(std::abs(actual-expected)<1e-3,message);}
int topFaceIndex(const TopoDS_Shape& shape){
    int count=0;
    for(TopExp_Explorer it(shape,TopAbs_FACE);it.More();it.Next()){
        ++count;
        const TopoDS_Face face=TopoDS::Face(it.Current());
        BRepAdaptor_Surface surface(face);
        if(surface.GetType()!=GeomAbs_Plane)continue;
        GProp_GProps properties;BRepGProp::SurfaceProperties(face,properties);
        if(std::abs(properties.CentreOfMass().Z()-10.0)<1e-5)return count;
    }
    return -1;
}
cad::ShapeFeatureNode makeFaceNode(const QString& type,int index,const QMap<QString,double>& anchor){
    cad::ShapeFeatureNode node;node.id="attached_polygon";node.type=type;
    node.parameters={{"profile_count",4.0},{"face_index",double(index)},
        {"depth",5.0},{"u",0.0},{"v",0.0}};
    const double positions[4][2]={{-4,-4},{4,-4},{4,4},{-4,4}};
    for(int i=0;i<4;++i){const QString prefix="p"+QString::number(i);
        node.parameters.insert(prefix+"_x",positions[i][0]);
        node.parameters.insert(prefix+"_y",positions[i][1]);
    }
    for(auto it=anchor.cbegin();it!=anchor.cend();++it)node.parameters.insert(it.key(),it.value());
    return node;
}
void testSideFace(const QString& type,bool pocket,const gp_Pnt& faceCenter){
    cad::SceneDocument scene;QString issue;
    require(scene.addPrimitive(cad::Primitive::Box,{30,30,10},&issue),"side-face box creation failed");
    const QString id=scene.parts().last().id;
    int index=0;
    for(TopExp_Explorer explorer(scene.find(id)->shape,TopAbs_FACE);explorer.More();explorer.Next()){
        ++index;
        GProp_GProps surface;BRepGProp::SurfaceProperties(TopoDS::Face(explorer.Current()),surface);
        if(surface.CentreOfMass().Distance(faceCenter)>1e-6)continue;
        QMap<QString,double> anchor;
        require(cad::capturePlanarFaceAnchor(scene.find(id)->shape,index,&anchor,&issue),
            "side-face stable capture failed");
        const auto node=makeFaceNode(type,index,anchor);
        require(scene.setShapeFeatureTree(id,{node},&issue),"side-face polygon failed to build");
        near(volume(scene.find(id)->shape),9000.0+(pocket?-320.0:320.0),
            "side-face polygon changed unexpected volume");
        return;
    }
    require(false,"requested side face was not found");
}
void exercise(const QString& type,bool pocket){
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{30,30,10},&error),"base box creation failed");
    const QString id=doc.parts().last().id;
    const double original=volume(doc.find(id)->shape);
    near(original,9000.0,"unexpected base solid volume");
    const int index=topFaceIndex(doc.find(id)->shape);
    require(index>0,"could not locate upper planar face");
    QMap<QString,double> anchor;
    require(cad::capturePlanarFaceAnchor(doc.find(id)->shape,index,&anchor,&error),"face capture failed");
    require(anchor.size()==16,"face descriptor missing the second trimmed-interior witness");
    const cad::ShapeFeatureNode attached=makeFaceNode(type,index,anchor);
    require(cad::validateShapeFeatureNodes({attached},&error),"valid attached polygon rejected");
    require(doc.setShapeFeatureTree(id,{attached},&error),"face attached solid feature rebuild failed");
    const double expected=original+(pocket?-320.0:320.0);
    near(volume(doc.find(id)->shape),expected,"face profile gave wrong solid volume");
    const QString token=doc.find(id)->geometryToken;
    auto bad=attached;bad.parameters.remove("face_anchor_nz");
    require(!cad::validateShapeFeatureNodes({bad},&error),"incomplete anchor was accepted");
    bad=attached;bad.parameters["p1_x"]=bad.parameters.value("p0_x");
    bad.parameters["p1_y"]=bad.parameters.value("p0_y");
    require(!doc.setShapeFeatureTree(id,{bad},&error),"degenerate sketch was committed");
    require(doc.find(id)->geometryToken==token,"invalid sketch changed committed geometry token");
    near(volume(doc.find(id)->shape),expected,"invalid edit changed committed solid");
    bad=attached;bad.parameters["face_anchor_area"]=0.0;
    require(!cad::validateShapeFeatureNodes({bad},&error),"invalid face descriptor was accepted");
    bad=attached;bad.parameters["depth"]=-1.0;
    require(!cad::validateShapeFeatureNodes({bad},&error),"negative depth was accepted");
    bad=attached;for(auto it=anchor.cbegin();it!=anchor.cend();++it)bad.parameters.remove(it.key());
    bad.parameters["face_index"]=10000.0;
    require(!doc.setShapeFeatureTree(id,{bad},&error),"inaccessible face index was accepted");
    near(volume(doc.find(id)->shape),expected,"missing face rollback failed");
    bad=attached;bad.parameters["u"]=70.0;
    require(!doc.setShapeFeatureTree(id,{bad},&error),
            "off-face polygon should not create a detached boss or no-op pocket");
    near(volume(doc.find(id)->shape),expected,"off-face rollback failed");
    require(doc.undo(),"undo attached solid failed");
    require(doc.redo(),"redo attached solid failed");
    near(volume(doc.find(id)->shape),expected,"redo changed geometry");
    // The original face anchor is intentionally retained. Its persistent
    // descriptor must find the matching face in the *translated* input shape.
    cad::ShapeFeatureNode moved;
    moved.id="upstream_translate";moved.type="Translate";
    moved.parameters={{"x",3.0},{"y",0.0},{"z",2.0}};
    require(doc.setShapeFeatureTree(id,{moved,attached},&error),"upstream translation lost the attached face");
    // Legacy 7-field anchors still deserialize and resolve without a migration.
    auto legacy=attached;
    legacy.parameters.remove("face_anchor_body_x");
    legacy.parameters.remove("face_anchor_body_y");
    legacy.parameters.remove("face_anchor_body_z");
    require(cad::validateShapeFeatureNodes({legacy},&error),"legacy face anchor no longer validates");
    // The modern source-body reference frame must also survive translations
    // far larger than the local face dimension.
    moved.parameters["x"]=1800.0;
    moved.parameters["y"]=-1200.0;
    moved.parameters["z"]=450.0;
    require(doc.setShapeFeatureTree(id,{moved,attached},&error),"large upstream translation lost the attached face");
    auto brokenBody=attached;
    brokenBody.parameters.remove("face_anchor_body_y");
    require(!cad::validateShapeFeatureNodes({brokenBody},&error),
            "incomplete body-relative face anchor was accepted");
    near(volume(doc.find(id)->shape),expected,"upstream translation changed attached solid volume");
    const QString translatedToken=doc.find(id)->geometryToken;
    auto edited=attached;edited.parameters["depth"]=4.0;
    require(doc.setShapeFeatureTree(id,{moved,edited},&error),"editing attached sketch depth failed");
    near(volume(doc.find(id)->shape),original+(pocket?-256.0:256.0),"attached depth edit volume incorrect");
    require(doc.find(id)->geometryToken!=translatedToken,"attached feature edit did not update geometry token");
    QTemporaryDir dir;require(dir.isValid(),"could not make temporary directory");
    const QString file=dir.path()+"/attached.cadscene";
    require(doc.saveAs(file,&error),"save attached geometry failed");
    cad::SceneDocument restored;
    require(restored.load(file,&error),"load attached geometry failed");
    require(restored.find(id)!=nullptr,"loaded part missing");
    require(restored.find(id)->shapeFeatureNodes.size()==2,"loaded node count changed");
    require(restored.find(id)->shapeFeatureNodes[1].parameters==edited.parameters,
            "loaded sketch lost face anchor or polygon coordinates");
    near(volume(restored.find(id)->shape),original+(pocket?-256.0:256.0),"loaded volume changed");
    require(restored.setShapeFeatureTree(id,{attached},&error),"restored face sketch could not be edited");
    near(volume(restored.find(id)->shape),expected,"restored edit changed result");
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        exercise("Sketch polygon face boss",false);
        exercise("Sketch polygon face pocket",true);
        for(const auto& type:QStringList{"Sketch polygon face boss","Sketch polygon face pocket"}){
            const bool pocket=type.endsWith("pocket");
            testSideFace(type,pocket,gp_Pnt(15,0,5));
            testSideFace(type,pocket,gp_Pnt(30,15,5));
        }
        std::cout<<"PASS: "<<checks<<" native face-attached sketch/rollback/persistence checks\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL #"<<checks<<": "<<e.what()<<'\n';return 1;
    }
}
