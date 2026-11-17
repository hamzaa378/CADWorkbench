#include "core/SceneDocument.h"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <stdexcept>
#include <iostream>
namespace {
int checks=0;
void must(bool condition,const char* why){++checks;if(!condition)throw std::runtime_error(why);}
void near(double actual,double expected,const char* why){must(std::abs(actual-expected)<1e-3,why);}
double volume(const cad::SceneDocument& doc,const QString& id){
    const auto* p=doc.find(id);must(p!=nullptr,"missing body");
    GProp_GProps props;BRepGProp::VolumeProperties(p->shape,props);return props.Mass();
}
cad::SketchModel sketch(double width){
    cad::SketchModel s;
    s.points={{"a",0,0},{"b",width,0},{"c",width,10},{"d",0,10}};
    s.segments={{"e1","a","b"},{"e2","b","c"},{"e3","c","d"},{"e4","d","a"}};
    return s;
}
QStringList boundary(){return {"a","b","c","d"};}
struct Fixture {
    cad::SceneDocument doc;QString source,target,descendant;
    void prepare(){
        QString error;
        must(doc.addConstrainedPolygonExtrusion(sketch(10),boundary(),10,&error),"source creation failed");
        source=doc.parts().last().id;
        must(doc.addPrimitive(cad::Primitive::Box,{12,12,10},&error),"target creation failed");
        target=doc.parts().last().id;
        near(volume(doc,target),1440,"target volume wrong");
    }
};
void operation(){
    Fixture f;f.prepare();QString issue;
    must(f.doc.setInPlaceSketchFeature(f.target,f.source,cad::BooleanOp::Fuse,5,8,&issue),
         "in-place additive feature failed");
    near(volume(f.doc,f.target),1740,"in-place boss volume incorrect");
    must(f.doc.inPlaceSketchFeatureCurrent(f.target),"new in-place feature stale");
    const auto token=f.doc.find(f.target)->geometryToken;
    must(f.doc.updateConstrainedPolygonExtrusion(f.source,sketch(8),boundary(),10,&issue),
         "source edit failed");
    near(volume(f.doc,f.target),1680,"in-place feature did not track sketch edits");
    must(f.doc.find(f.target)->geometryToken!=token,"geometry token not regenerated");
    must(!f.doc.erase(f.source,&issue),"deleted source with live in-place dependent");
    const auto before=f.doc.find(f.target)->geometryToken;
    must(!f.doc.setInPlaceSketchFeature(f.target,f.source,cad::BooleanOp::Fuse,5,50,&issue),
         "detached in-place boss was accepted");
    must(before==f.doc.find(f.target)->geometryToken,"failed change committed geometry");
    must(f.doc.setInPlaceSketchFeature(f.target,f.source,cad::BooleanOp::Cut,5,8,&issue),
         "in-place pocket edit failed");
    near(volume(f.doc,f.target),1280,"in-place pocket volume incorrect");
    must(f.doc.undo(),"undo pocket edit failed");
    near(volume(f.doc,f.target),1680,"undo did not restore boss");
    must(f.doc.redo(),"redo pocket edit failed");
    near(volume(f.doc,f.target),1280,"redo did not restore pocket");
    must(f.doc.removeInPlaceSketchFeature(f.target,&issue),"cannot remove in-place modifier");
    near(volume(f.doc,f.target),1440,"remove did not restore frozen target");
}
void twoStage(bool persist,bool corrupted){
    Fixture f;f.prepare();QString issue;
    must(f.doc.addConstrainedPolygonExtrusion(sketch(5),boundary(),10,&issue),
         "second source creation failed");
    const QString second=f.doc.parts().last().id;
    must(f.doc.setInPlaceSketchFeature(f.target,f.source,cad::BooleanOp::Fuse,5,8,&issue),
         "first in-place modifier failed");
    must(f.doc.appendInPlaceSketchFeature(f.target,second,cad::BooleanOp::Cut,5,8,&issue),
         "append second in-place modifier failed");
    near(volume(f.doc,f.target),1490,"two-stage boss/pocket volume incorrect");
    must(f.doc.inPlaceSketchFeatureCurrent(f.target),"two-stage feature stale");
    QStringList impacted;
    must(f.doc.linkedBodyImpact(second,&impacted,&issue),"second-source impact plan failed");
    must(impacted.contains(f.target),"second stage absent from impact planner");
    must(!f.doc.erase(second,&issue),"second source deleted despite live dependent");
    const QString token=f.doc.find(f.target)->geometryToken;
    must(!f.doc.appendInPlaceSketchFeature(f.target,second,cad::BooleanOp::Cut,5,8,&issue),
         "third modifier incorrectly accepted");
    must(f.doc.find(f.target)->geometryToken==token,"failed third stage changed target");
    must(!f.doc.appendInPlaceSketchFeature(f.target,f.source,cad::BooleanOp::Cut,5,8,&issue),
         "duplicate source incorrectly accepted");
    if(!persist){
        must(f.doc.updateInPlaceSketchFollowUp(f.target,second,cad::BooleanOp::Cut,4,9,&issue),
             "edit second stage failed");
        near(volume(f.doc,f.target),1540,"second stage parametric edit volume incorrect");
        must(f.doc.undo(),"undo second stage edit failed");
        near(volume(f.doc,f.target),1490,"undo second stage edit changed result");
        const QString stable=f.doc.find(f.target)->geometryToken;
        must(!f.doc.updateInPlaceSketchFollowUp(f.target,f.source,cad::BooleanOp::Fuse,3,9,&issue),
             "duplicate edited second sketch source accepted");
        must(stable==f.doc.find(f.target)->geometryToken,"failed follow-up edit committed geometry");
        must(f.doc.updateConstrainedPolygonExtrusion(second,sketch(4),boundary(),10,&issue),
             "edit to second source failed");
        near(volume(f.doc,f.target),1540,"second-source edit failed to propagate");
        must(f.doc.updateConstrainedPolygonExtrusion(f.source,sketch(8),boundary(),10,&issue),
             "first-source edit failed");
        near(volume(f.doc,f.target),1480,"first-source edit lost downstream modifier");
        must(f.doc.undo(),"undo first-source edit failed");
        near(volume(f.doc,f.target),1540,"undo did not restore two-stage target");
        must(f.doc.redo(),"redo first-source edit failed");
        near(volume(f.doc,f.target),1480,"redo did not regenerate follow-up");
        must(f.doc.removeInPlaceSketchFollowUp(f.target,&issue),"remove second stage failed");
        near(volume(f.doc,f.target),1680,"remove second did not restore first stage");
        must(f.doc.undo(),"undo removal of second failed");
        near(volume(f.doc,f.target),1480,"undo stage removal failed");
        return;
    }
    QTemporaryDir temp;must(temp.isValid(),"no temp directory");
    const QString path=temp.path()+"/two-stage.cadscene";
    must(f.doc.saveAs(path,&issue),"save two-stage scene failed");
    cad::SceneDocument reopened;
    must(reopened.load(path,&issue),"reopen two-stage scene failed");
    near(volume(reopened,f.target),1490,"two-stage persisted volume changed");
    must(reopened.inPlaceSketchFeatureCurrent(f.target),"saved second sketch token stale");
    must(reopened.updateConstrainedPolygonExtrusion(second,sketch(4),boundary(),10,&issue),
         "saved second source no longer linked");
    near(volume(reopened,f.target),1540,"saved second source did not propagate");
    if(corrupted){
        QFile io(path);must(io.open(QIODevice::ReadOnly),"open saved scene failed");
        auto j=QJsonDocument::fromJson(io.readAll());io.close();
        auto r=j.object();
        auto parts=r.value("parts").toArray();
        for(int i=0;i<parts.size();++i){
            auto part=parts[i].toObject();
            if(!part.contains("in_place_sketch_feature"))continue;
            auto feature=part.value("in_place_sketch_feature").toObject();
            auto secondRecord=feature.value("follow_up").toObject();
            secondRecord.insert("depth_mm",99999.0);
            feature.insert("follow_up",secondRecord);
            part.insert("in_place_sketch_feature",feature);parts[i]=part;
        }
        r.insert("parts",parts);
        must(io.open(QIODevice::WriteOnly|QIODevice::Truncate),"rewrite test scene failed");
        io.write(QJsonDocument(r).toJson());io.close();
        const QString untouched=reopened.find(f.target)->geometryToken;
        must(!reopened.load(path,&issue),"tampered second modifier was accepted");
        must(reopened.find(f.target)->geometryToken==untouched,
             "failed saved load changed committed geometry");
    }
}
// 4-stage regression with distinct source sketches and explicit last-stage rollback.
// First: +300. Second: -250. Third: +320 (restore the cut and extend 3mm).
// Fourth: -60. All tools are positioned in the same offset sketch frame.
void orderedStack(bool persist,bool corrupted,bool rejected){
    Fixture f;f.prepare();QString issue;
    must(f.doc.addConstrainedPolygonExtrusion(sketch(5),boundary(),10,&issue),"second source creation failed");
    const QString second=f.doc.parts().last().id;
    must(f.doc.addConstrainedPolygonExtrusion(sketch(4),boundary(),10,&issue),"third source creation failed");
    const QString third=f.doc.parts().last().id;
    must(f.doc.addConstrainedPolygonExtrusion(sketch(2),boundary(),10,&issue),"fourth source creation failed");
    const QString fourth=f.doc.parts().last().id;
    must(f.doc.setInPlaceSketchFeature(f.target,f.source,cad::BooleanOp::Fuse,5,8,&issue),"stage one failed");
    must(f.doc.appendInPlaceSketchFeature(f.target,second,cad::BooleanOp::Cut,5,8,&issue),"stage two failed");
    must(f.doc.appendInPlaceSketchStage(f.target,third,cad::BooleanOp::Fuse,8,8,&issue),"stage three failed");
    near(volume(f.doc,f.target),1810,"third stage expected volume incorrect");
    must(f.doc.appendInPlaceSketchStage(f.target,fourth,cad::BooleanOp::Cut,3,11,&issue),"stage four failed");
    near(volume(f.doc,f.target),1750,"fourth stage expected volume incorrect");
    must(f.doc.inPlaceSketchFeatureCurrent(f.target),"ordered stack immediately stale");
    const auto* body=f.doc.find(f.target);
    must(body->extraInPlaceSketchStages.size()==2,"additional stage records not stored");
    QStringList impact;
    must(f.doc.linkedBodyImpact(third,&impact,&issue)&&impact.contains(f.target),"stage three impact missing");
    must(f.doc.linkedBodyImpact(fourth,&impact,&issue)&&impact.contains(f.target),"stage four impact missing");
    must(!f.doc.erase(third,&issue),"deleted live third source");
    if(rejected){
        const QString token=f.doc.find(f.target)->geometryToken;
        must(!f.doc.appendInPlaceSketchStage(f.target,third,cad::BooleanOp::Fuse,1,8,&issue),
             "duplicate third source accepted");
        must(!f.doc.updateInPlaceSketchStage(f.target,1,third,cad::BooleanOp::Cut,3,11,&issue),
             "duplicate fourth source accepted");
        must(!f.doc.updateInPlaceSketchStage(f.target,0,third,cad::BooleanOp::Fuse,8,100,&issue),
             "detached third stage accepted");
        must(!f.doc.removeInPlaceSketchFollowUp(f.target,&issue),"removed stage two below stages three and four");
        must(f.doc.find(f.target)->geometryToken==token,"failed edit changed committed geometry");
        return;
    }
    if(!persist){
        must(f.doc.updateConstrainedPolygonExtrusion(third,sketch(3),boundary(),10,&issue),
             "editing third stage sketch failed");
        near(volume(f.doc,f.target),1670,"third stage sketch edit did not rebuild stage four");
        must(f.doc.undo(),"undo third source edit failed");
        near(volume(f.doc,f.target),1750,"undo third source edit did not restore stack");
        must(f.doc.redo(),"redo third source edit failed");
        near(volume(f.doc,f.target),1670,"redo third source edit did not rebuild stack");
        must(f.doc.removeLastInPlaceSketchStage(f.target,&issue),"remove last modifier failed");
        near(volume(f.doc,f.target),1730,"remove fourth modifier did not preserve third");
        must(f.doc.undo(),"undo removal failed");
        near(volume(f.doc,f.target),1670,"undo removal lost fourth stage");
        return;
    }
    QTemporaryDir temp;must(temp.isValid(),"temp folder unavailable");
    const QString file=temp.path()+"/four-stage.cadscene";
    must(f.doc.saveAs(file,&issue),"cannot save ordered stack");
    cad::SceneDocument reopened;
    must(reopened.load(file,&issue),"cannot reopen ordered stack");
    near(volume(reopened,f.target),1750,"reopened stack volume incorrect");
    must(reopened.inPlaceSketchFeatureCurrent(f.target),"reopened ordered stack stale");
    if(!corrupted){
        must(reopened.updateConstrainedPolygonExtrusion(third,sketch(3),boundary(),10,&issue),
             "reopened third source edit failed");
        near(volume(reopened,f.target),1670,"reopened stack failed to propagate");
        return;
    }
    QFile io(file);must(io.open(QIODevice::ReadOnly),"cannot read scene for corruption test");
    QJsonDocument saved=QJsonDocument::fromJson(io.readAll());io.close();
    auto root=saved.object();auto bodies=root.value("parts").toArray();
    for(int i=0;i<bodies.size();++i){
        auto b=bodies[i].toObject();if(!b.contains("in_place_sketch_feature"))continue;
        auto feature=b.value("in_place_sketch_feature").toObject();
        auto extra=feature.value("extra_stages").toArray();
        auto tampered=extra[0].toObject();tampered.insert("depth_mm",100.0);
        extra[0]=tampered;feature.insert("extra_stages",extra);
        b.insert("in_place_sketch_feature",feature);bodies[i]=b;
    }
    root.insert("parts",bodies);
    must(io.open(QIODevice::WriteOnly|QIODevice::Truncate),"cannot rewrite tampered scene");
    io.write(QJsonDocument(root).toJson());io.close();
    const QString token=reopened.find(f.target)->geometryToken;
    must(!reopened.load(file,&issue),"tampered ordered stage accepted");
    must(reopened.find(f.target)->geometryToken==token,"failed load replaced committed geometry");
}
void persistence(){
    Fixture f;f.prepare();QString issue;
    must(f.doc.setInPlaceSketchFeature(f.target,f.source,cad::BooleanOp::Cut,5,8,&issue),
         "create pocket failed");
    QTemporaryDir tmp;must(tmp.isValid(),"temp dir failed");
    const QString file=tmp.path()+"/in-place.cadscene";
    must(f.doc.saveAs(file,&issue),"save failed");
    cad::SceneDocument reopened;
    must(reopened.load(file,&issue),"reopen failed");
    must(reopened.inPlaceSketchFeatureCurrent(f.target),"restored in-place feature stale");
    near(volume(reopened,f.target),1240,"restored pocket changed geometry");
    must(reopened.updateConstrainedPolygonExtrusion(f.source,sketch(8),boundary(),10,&issue),
         "reopened upstream sketch edit failed");
    near(volume(reopened,f.target),1280,"reopened derived body did not regenerate");
    // Saved B-rep consistency is checked before a corrupt scene can replace the document.
    QFile io(file);must(io.open(QIODevice::ReadOnly),"read persisted scene failed");
    auto doc=QJsonDocument::fromJson(io.readAll());io.close();
    auto root=doc.object();auto parts=root.value("parts").toArray();
    for(int i=0;i<parts.size();++i){
        auto record=parts[i].toObject();if(!record.contains("in_place_sketch_feature"))continue;
        auto link=record.value("in_place_sketch_feature").toObject();link.insert("depth_mm",50.0);
        record.insert("in_place_sketch_feature",link);parts[i]=record;
    }
    root.insert("parts",parts);must(io.open(QIODevice::WriteOnly|QIODevice::Truncate),"write failed");
    io.write(QJsonDocument(root).toJson());io.close();
    const QString savedToken=reopened.find(f.target)->geometryToken;
    must(!reopened.load(file,&issue),"tampered in-place link was accepted");
    must(reopened.find(f.target)->geometryToken==savedToken,"corrupt load replaced committed scene");
}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);
    try{const QString mode=argc>1?QString::fromUtf8(argv[1]):"operations";
        if(mode=="operations")operation();else if(mode=="persistence")persistence();
        else if(mode=="two-stage")twoStage(false,false);
        else if(mode=="two-stage-persistence")twoStage(true,false);
        else if(mode=="two-stage-corruption")twoStage(true,true);
        else if(mode=="ordered-stack")orderedStack(false,false,false);
        else if(mode=="ordered-stack-persistence")orderedStack(true,false,false);
        else if(mode=="ordered-stack-corruption")orderedStack(true,true,false);
        else if(mode=="ordered-stack-rollback")orderedStack(false,false,true);
        else throw std::runtime_error("unknown mode");
        std::cout<<"in-place associative sketch: "<<checks<<" passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
