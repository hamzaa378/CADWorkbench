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
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double volume(const cad::Part* body){
    check(body!=nullptr,"Expected linked body missing");
    GProp_GProps props;BRepGProp::VolumeProperties(body->shape,props);return props.Mass();
}
void near(double actual,double expected,const char* message){
    if(std::abs(actual-expected)>0.01) throw std::runtime_error(message);
}
struct Fixture {
    cad::SceneDocument scene;
    QString a,b,c,linked,downstream,later;
    void create(){
        QString err;
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&err),"First box failed");
        a=scene.parts().last().id;
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&err),"Second box failed");
        b=scene.parts().last().id;
        check(scene.translate(b,10,0,0,&err),"Cannot move second body");
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&err),"Third box failed");
        c=scene.parts().last().id;
        check(scene.translate(c,15,0,0,&err),"Cannot move third body");
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Fuse,a,b,&err),"Initial fuse failed");
        linked=scene.parts().last().id;
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Fuse,linked,a,&err),"Downstream fuse failed");
        downstream=scene.parts().last().id;
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Common,b,c,&err),"Later source failed");
        later=scene.parts().last().id;
        near(volume(scene.find(linked)),6000,"Initial linked volume wrong");
        near(volume(scene.find(downstream)),6000,"Initial descendant volume wrong");
        near(volume(scene.find(later)),3000,"Later source volume wrong");
    }
    void retarget(){
        QString err;
        check(scene.updateLinkedBooleanBody(linked,cad::BooleanOp::Common,a,later,&err),
              "Retarget to a later source failed");
        check(scene.linkedBooleanCurrent(linked)&&scene.linkedBooleanCurrent(downstream),
              "Derived bodies not current after retarget");
        near(volume(scene.find(linked)),1000,"Retarget result volume wrong");
        near(volume(scene.find(downstream)),4000,"Retarget descendant volume wrong");
    }
};
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        Fixture fixture;fixture.create();
        QString error;
        if(argc==1||QString::fromUtf8(argv[1])==QStringLiteral("edit")){
            fixture.retarget();
            QStringList impacts;
            check(fixture.scene.linkedBodyImpact(fixture.c,&impacts,&error),"Impact preview failed");
            check(impacts==QStringList{fixture.c,fixture.later,fixture.linked,fixture.downstream},
                  "Impact preview order incorrect for retargeted DAG");
            check(fixture.scene.linkedBodyImpact(fixture.a,&impacts,&error),"Root impact preview failed");
            check(impacts==QStringList{fixture.a,fixture.linked,fixture.downstream},
                  "Impact preview included independent branch");
            const QStringList savedImpacts=impacts;
            check(!fixture.scene.linkedBodyImpact("missing-source",&impacts,&error),
                  "Unknown impact source accepted");
            check(impacts==savedImpacts,"Failed impact query changed output");
            const QString updatedToken=fixture.scene.find(fixture.linked)->geometryToken;
            check(!fixture.scene.updateLinkedBooleanBody(fixture.linked,cad::BooleanOp::Fuse,
                    fixture.downstream,fixture.a,&error),"Dependency cycle accepted");
            check(fixture.scene.find(fixture.linked)->geometryToken==updatedToken,
                  "Failed cycle edit mutated result");
            near(volume(fixture.scene.find(fixture.downstream)),4000,"Failed cycle edit mutated descendant");
            check(!fixture.scene.updateLinkedBooleanBody(fixture.linked,cad::BooleanOp::Fuse,
                    fixture.a,QStringLiteral("nonexistent"),&error),"Missing source accepted");
            check(!fixture.scene.updateLinkedBooleanBody(fixture.linked,cad::BooleanOp::Fuse,
                    fixture.a,fixture.a,&error),"Identical sources accepted");
            check(!fixture.scene.erase(fixture.later,&error),"A later dependency source was deleted");
            check(fixture.scene.undo(),"Undo edit failed");
            near(volume(fixture.scene.find(fixture.linked)),6000,"Undo did not restore linked source");
            check(fixture.scene.redo(),"Redo edit failed");
            near(volume(fixture.scene.find(fixture.linked)),1000,"Redo did not restore linked result");
            check(fixture.scene.translate(fixture.c,2,0,0,&error),"Translated later source failed");
            near(volume(fixture.scene.find(fixture.later)),2600,"Later independent result did not update");
            near(volume(fixture.scene.find(fixture.linked)),600,"Earlier linked result did not update");
            near(volume(fixture.scene.find(fixture.downstream)),4000,"Descendant volume wrong");
            check(!fixture.scene.translate(fixture.c,200,0,0,&error),
                  "Invalid upstream edit was committed despite empty intersection");
            near(volume(fixture.scene.find(fixture.linked)),600,"Rollback modified linked result");
        }else if(QString::fromUtf8(argv[1])==QStringLiteral("persistence")){
            fixture.retarget();
            QTemporaryDir temp;check(temp.isValid(),"Cannot create temp directory");
            const QString valid=temp.path()+QStringLiteral("/retarget.cadscene");
            check(fixture.scene.saveAs(valid,&error),"Cannot save retargeted DAG");
            cad::SceneDocument loaded;
            check(loaded.load(valid,&error),"Retargeted DAG rejected on load");
            near(volume(loaded.find(fixture.linked)),1000,"Loaded result volume differs");
            check(loaded.linkedBooleanCurrent(fixture.linked)&&loaded.linkedBooleanCurrent(fixture.downstream),
                  "Retargeted links not current after load");
            // Ensure corrupted back-edge is rejected without replacing the scene.
            QFile input(valid);check(input.open(QIODevice::ReadOnly),"Cannot read saved graph");
            auto json=QJsonDocument::fromJson(input.readAll()).object();input.close();
            QJsonArray parts=json.value(QStringLiteral("parts")).toArray();
            for(int i=0;i<parts.size();++i){
                QJsonObject part=parts[i].toObject();
                if(part.value(QStringLiteral("id")).toString()!=fixture.linked)continue;
                QJsonObject link=part.value(QStringLiteral("linked_boolean")).toObject();
                link.insert(QStringLiteral("primary"),fixture.downstream);
                part.insert(QStringLiteral("linked_boolean"),link);
                parts.replace(i,part);
            }
            json.insert(QStringLiteral("parts"),parts);
            const QString corrupt=temp.path()+QStringLiteral("/cycle.cadscene");
            QFile output(corrupt);check(output.open(QIODevice::WriteOnly),"Cannot write corrupt fixture");
            output.write(QJsonDocument(json).toJson());output.close();
            check(!loaded.load(corrupt,&error),"Saved graph cycle accepted");
            near(volume(loaded.find(fixture.linked)),1000,"Corrupt file replaced active model");
        }else throw std::runtime_error("Unknown test selection");
        std::cout<<"PASS: linked-body edit, graph, and persistence acceptance\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
    return 0;
}
