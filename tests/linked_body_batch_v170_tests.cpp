#include "core/SceneDocument.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
double volume(const cad::SceneDocument& scene,const QString& id){
    const auto* part=scene.find(id);
    check(part!=nullptr,"Missing part");
    GProp_GProps props;
    BRepGProp::VolumeProperties(part->shape,props);
    return props.Mass();
}
void near(double actual,double expected,const char* message){
    if(std::abs(actual-expected)>0.1)throw std::runtime_error(message);
}
struct Fixture {
    cad::SceneDocument scene;
    QString a,b,c,d,x,y,z;
    void create(){
        QString error;
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cannot create A");
        a=scene.parts().last().id;
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cannot create B");
        b=scene.parts().last().id;
        check(scene.translate(b,10,0,0,&error),"Cannot move B");
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cannot create C");
        c=scene.parts().last().id;
        check(scene.translate(c,15,0,0,&error),"Cannot move C");
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cannot create D");
        d=scene.parts().last().id;
        check(scene.translate(d,200,0,0,&error),"Cannot move D");
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Fuse,a,b,&error),"Cannot create X");
        x=scene.parts().last().id;
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Fuse,x,c,&error),"Cannot create Y");
        y=scene.parts().last().id;
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Fuse,x,c,&error),"Cannot create Z");
        z=scene.parts().last().id;
        near(volume(scene,x),6000,"Wrong starting X volume");
        near(volume(scene,y),7000,"Wrong starting Y volume");
    }
    QVector<cad::LinkedBooleanEdit> coordinated()const{
        return {
            {x,cad::BooleanOp::Common,a,y},  // Would be cyclic until Y is also changed.
            {y,cad::BooleanOp::Fuse,a,b}      // Removes Y -> X, making X -> Y valid.
        };
    }
    void apply(){
        QString error;
        check(!scene.updateLinkedBooleanBody(x,cad::BooleanOp::Common,a,y,&error),
              "Single-step edit unexpectedly allowed temporary cycle");
        check(scene.updateLinkedBooleanBatch(coordinated(),&error),"Atomic coordinated retarget failed");
        near(volume(scene,x),4000,"Retargeted X volume wrong");
        near(volume(scene,y),6000,"Retargeted Y volume wrong");
        near(volume(scene,z),7000,"Descendant Z volume wrong");
        check(scene.linkedBooleanCurrent(x)&&scene.linkedBooleanCurrent(y)&&scene.linkedBooleanCurrent(z),
              "A dependent result is stale after batch");
    }
};
void batchTest(){
    Fixture f;f.create();QString error;
    const auto oldX=f.scene.find(f.x)->geometryToken;
    const auto oldY=f.scene.find(f.y)->geometryToken;
    const auto oldZ=f.scene.find(f.z)->geometryToken;
    f.apply();
    check(f.scene.find(f.x)->geometryToken!=oldX,"X token unchanged after rewrite");
    check(f.scene.find(f.y)->geometryToken!=oldY,"Y token unchanged after rewrite");
    check(f.scene.find(f.z)->geometryToken!=oldZ,"Dependent token unchanged after rewrite");
    const auto currentX=f.scene.find(f.x)->geometryToken;
    const auto currentY=f.scene.find(f.y)->geometryToken;
    // A no-op batch does not create undo history or regenerate geometry.
    check(f.scene.updateLinkedBooleanBatch(f.coordinated(),&error),"No-op batch failed");
    check(f.scene.find(f.x)->geometryToken==currentX&&f.scene.find(f.y)->geometryToken==currentY,
          "No-op unnecessarily rebuilt geometry");
    check(f.scene.undo(),"Undo batch failed");
    near(volume(f.scene,f.x),6000,"Undo did not restore X");
    near(volume(f.scene,f.y),7000,"Undo did not restore Y");
    check(f.scene.redo(),"Redo batch failed");
    near(volume(f.scene,f.x),4000,"Redo did not restore X");
    near(volume(f.scene,f.y),6000,"Redo did not restore Y");
    const auto savedX=f.scene.find(f.x)->geometryToken;
    const auto savedY=f.scene.find(f.y)->geometryToken;
    check(!f.scene.updateLinkedBooleanBatch({{f.x,cad::BooleanOp::Fuse,f.y,f.a},
                                             {f.y,cad::BooleanOp::Fuse,f.x,f.b}},&error),
          "Cyclic batch was accepted");
    check(f.scene.find(f.x)->geometryToken==savedX&&f.scene.find(f.y)->geometryToken==savedY,
          "Rejected cycle modified committed graph");
    check(!f.scene.updateLinkedBooleanBatch({{f.x,cad::BooleanOp::Common,f.a,f.d}},&error),
          "Empty Boolean was committed");
    check(f.scene.find(f.x)->geometryToken==savedX,"Failed geometry changed committed model");
    check(!f.scene.updateLinkedBooleanBatch({{f.x,cad::BooleanOp::Cut,f.a,f.b},
                                             {f.x,cad::BooleanOp::Fuse,f.a,f.b}},&error),
          "Duplicate batch result accepted");
    check(!f.scene.updateLinkedBooleanBatch({{f.x,static_cast<cad::BooleanOp>(100),f.a,f.b}},&error),
          "Invalid Boolean operation accepted");
    check(!f.scene.updateLinkedBooleanBatch({{f.x,cad::BooleanOp::Fuse,f.a,f.x}},&error),
          "Self source accepted");
    near(volume(f.scene,f.x),4000,"A rejected batch changed X volume");
}
void previewTest(){
    Fixture f;f.create();QString error;QStringList impacted;
    const auto oldX=f.scene.find(f.x)->geometryToken;
    const auto oldY=f.scene.find(f.y)->geometryToken;
    check(f.scene.previewLinkedBooleanBatch(f.coordinated(),&impacted,&error),"Valid preview rejected");
    check(impacted.size()==3,"Preview should include X, Y, Z");
    check(impacted.indexOf(f.y)<impacted.indexOf(f.x),"Preview ignores final dependency order");
    check(impacted.indexOf(f.x)<impacted.indexOf(f.z),"Preview omitted downstream dependency order");
    check(!impacted.contains(f.a)&&!impacted.contains(f.d),"Preview included unrelated body");
    check(f.scene.find(f.x)->geometryToken==oldX&&f.scene.find(f.y)->geometryToken==oldY,
          "Read-only preview mutated committed document");
    const QStringList saved=impacted;
    check(!f.scene.previewLinkedBooleanBatch({{f.x,cad::BooleanOp::Fuse,f.y,f.a}},&impacted,&error),
          "Invalid preview cycle was accepted");
    check(impacted==saved,"Failed preview changed caller's output");
    check(f.scene.previewLinkedBooleanBatch({},&impacted,&error),"Empty preview failed");
    check(impacted.isEmpty(),"Empty preview has affected bodies");
    check(!f.scene.previewLinkedBooleanBatch(f.coordinated(),nullptr,&error),
          "Missing preview output accepted");
}
void persistenceTest(){
    Fixture f;f.create();f.apply();QString error;
    QTemporaryDir directory;check(directory.isValid(),"Cannot create scratch folder");
    const auto path=directory.path()+QStringLiteral("/atomic-batch.cadscene");
    check(f.scene.saveAs(path,&error),"Cannot save coordinated graph");
    cad::SceneDocument loaded;
    check(loaded.load(path,&error),"Cannot load coordinated graph");
    near(volume(loaded,f.x),4000,"Loaded batch X volume wrong");
    near(volume(loaded,f.y),6000,"Loaded batch Y volume wrong");
    check(loaded.linkedBooleanCurrent(f.x)&&loaded.linkedBooleanCurrent(f.y)&&loaded.linkedBooleanCurrent(f.z),
          "Reloaded dependent graph is stale");
    QStringList impacted;
    check(loaded.linkedBodyImpact(f.y,&impacted,&error),"Loaded impact planning failed");
    check(impacted==QStringList{f.y,f.x,f.z},"Loaded graph has incorrect downstream impacts");
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        const auto mode=argc>1?QString::fromUtf8(argv[1]):QStringLiteral("batch");
        if(mode==QStringLiteral("batch"))batchTest();
        else if(mode==QStringLiteral("preview"))previewTest();
        else if(mode==QStringLiteral("persistence"))persistenceTest();
        else throw std::runtime_error("Unrecognized test mode");
        std::cout<<"PASS: atomic linked Boolean batch "<<mode.toStdString()<<"\n";
    }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<"\n";return 1;}
    return 0;
}
