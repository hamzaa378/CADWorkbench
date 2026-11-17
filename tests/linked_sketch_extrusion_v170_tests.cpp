#include "core/SceneDocument.h"
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
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
int checks=0;
void ensure(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
void closeTo(double real,double expected,const char* what){ensure(std::abs(real-expected)<1e-3,what);}
double volume(const cad::SceneDocument& scene,const QString& id){
    const auto* body=scene.find(id);ensure(body!=nullptr,"Missing body");
    GProp_GProps p;BRepGProp::VolumeProperties(body->shape,p);return p.Mass();
}
cad::SketchModel rectangle(double width){
    cad::SketchModel s;
    s.points={{"a",0,0},{"b",width,0},{"c",width,10},{"d",0,10}};
    s.segments={{"e1","a","b"},{"e2","b","c"},{"e3","c","d"},{"e4","d","a"}};
    return s;
}
QStringList boundary(){return {"a","b","c","d"};}
struct Fixture{
    cad::SceneDocument scene;
    QString source,derived,cut;
    void setup(){
        QString issue;
        ensure(scene.addConstrainedPolygonExtrusion(rectangle(10),boundary(),10,&issue),
               "Could not create constrained source sketch");
        source=scene.parts().last().id;
        closeTo(volume(scene,source),1000,"Unexpected base extrusion volume");
        ensure(scene.addLinkedSketchExtrusion(source,5,8,&issue),
               "Could not create source-linked sketch extrusion");
        derived=scene.parts().last().id;
        closeTo(volume(scene,derived),500,"Unexpected linked extrusion volume");
        ensure(scene.linkedSketchExtrusionCurrent(derived),"New link is stale");
        ensure(scene.addLinkedBooleanBody(cad::BooleanOp::Cut,source,derived,&issue),
               "Could not construct Boolean dependent");
        cut=scene.parts().last().id;
        closeTo(volume(scene,cut),800,"Initial Boolean cut volume incorrect");
        ensure(scene.linkedBooleanCurrent(cut),"Initial Boolean descendant stale");
    }
};
void operations(){
    // Saved datum normals need not be unit vectors. Offset stays physical mm.
    {
        cad::SceneDocument nonunit;
        cad::SketchPlane plane;plane.normalZ=2;
        QString err;
        ensure(nonunit.addConstrainedPolygonExtrusion(rectangle(10),boundary(),10,&err,plane),
               "Non-unit source sketch plane rejected");
        ensure(nonunit.addLinkedSketchExtrusion(nonunit.parts().last().id,5,8,&err),
               "Non-unit normal linked extrusion rejected");
        Bnd_Box bbox;
        BRepBndLib::AddOptimal(nonunit.parts().last().shape,bbox,false,false);
        double xmin,ymin,zmin,xmax,ymax,zmax;
        bbox.Get(xmin,ymin,zmin,xmax,ymax,zmax);
        closeTo(zmin,8,"Offset incorrectly scaled by non-unit normal");
        closeTo(zmax,13,"Non-unit normal linked extrusion height wrong");
    }
    Fixture f;f.setup();QString issue;
    const auto token=f.scene.find(f.derived)->geometryToken;
    ensure(f.scene.updateConstrainedPolygonExtrusion(f.source,rectangle(12),boundary(),10,&issue),
           "Upstream sketch edit did not propagate");
    closeTo(volume(f.scene,f.source),1200,"Upstream source volume incorrect");
    closeTo(volume(f.scene,f.derived),600,"Source-linked extrusion did not rebuild");
    closeTo(volume(f.scene,f.cut),960,"Boolean grandchild did not rebuild");
    ensure(f.scene.find(f.derived)->geometryToken!=token,"Linked geometry token did not change");
    ensure(f.scene.linkedBooleanCurrent(f.cut)&&f.scene.linkedSketchExtrusionCurrent(f.derived),
           "Updated dependency chain is stale");
    QStringList impact;
    ensure(f.scene.linkedBodyImpact(f.source,&impact,&issue),"Impact analysis failed");
    ensure(impact==QStringList{f.source,f.derived,f.cut},"Impact order omitted unary dependency");
    const QString rebuiltToken=f.scene.find(f.derived)->geometryToken;
    ensure(!f.scene.updateLinkedSketchExtrusion(f.derived,-2,0,&issue),
           "Invalid linked depth was accepted");
    ensure(!f.scene.updateLinkedSketchExtrusion(f.derived,5,1e7,&issue),
           "Out-of-range offset was accepted");
    ensure(f.scene.find(f.derived)->geometryToken==rebuiltToken,
           "Invalid edit committed partial geometry");
    ensure(f.scene.updateLinkedSketchExtrusion(f.derived,5,8,&issue),
           "No-op parameter edit failed");
    ensure(f.scene.find(f.derived)->geometryToken==rebuiltToken,
           "No-op edit changed token");
    ensure(f.scene.updateLinkedSketchExtrusion(f.derived,4,9,&issue),
           "Editable linked extrusion failed");
    closeTo(volume(f.scene,f.derived),480,"Linked edit volume incorrect");
    closeTo(volume(f.scene,f.cut),1080,"Linked edit Boolean result incorrect");
    ensure(f.scene.undo(),"Undo linked extrusion edit failed");
    closeTo(volume(f.scene,f.derived),600,"Undo linked extrusion volume incorrect");
    ensure(f.scene.redo(),"Redo linked extrusion edit failed");
    closeTo(volume(f.scene,f.derived),480,"Redo linked extrusion volume incorrect");
    ensure(!f.scene.erase(f.source,&issue),"Removed a sketch source with live dependents");
    ensure(!f.scene.setShapeFeatureTree(f.derived,{},&issue),
           "Modified a read-only sketch-linked derived result directly");
    ensure(f.scene.eraseMany({f.source,f.derived,f.cut},&issue),
           "Could not remove all linked descendants together");
}
void persistence(){
    Fixture f;f.setup();QString issue;
    QTemporaryDir temp;ensure(temp.isValid(),"Could not create temp directory");
    const QString file=temp.path()+"/linked-sketch.cadscene";
    ensure(f.scene.saveAs(file,&issue),"Could not save linked sketch scene");
    cad::SceneDocument opened;
    ensure(opened.load(file,&issue),"Could not reopen linked sketch scene");
    ensure(opened.linkedSketchExtrusionCurrent(f.derived),"Loaded sketch link stale");
    ensure(opened.linkedBooleanCurrent(f.cut),"Loaded Boolean descendant stale");
    closeTo(volume(opened,f.derived),500,"Loaded linked volume changed");
    ensure(opened.updateConstrainedPolygonExtrusion(f.source,rectangle(12),boundary(),10,&issue),
           "Editing loaded sketch failed");
    closeTo(volume(opened,f.derived),600,"Loaded sketch did not regenerate linked solid");
    closeTo(volume(opened,f.cut),960,"Loaded sketch did not regenerate descendant");
    // Corrupt only the explicit sketch source ID; load must reject the entire
    // file transactionally and preserve the current open document.
    QFile input(file);ensure(input.open(QIODevice::ReadOnly),"Cannot read save file");
    auto document=QJsonDocument::fromJson(input.readAll());input.close();
    ensure(document.isObject(),"Saved scene JSON not an object");
    auto root=document.object();
    auto parts=root.value("parts").toArray();
    ensure(!parts.isEmpty(),"Saved scene missing parts array");
    bool changed=false;
    for(int i=0;i<parts.size();++i){
        auto body=parts[i].toObject();
        if(body.value("id").toString()!=f.derived)continue;
        auto link=body.value("linked_sketch_extrusion").toObject();
        ensure(!link.isEmpty(),"Linked sketch record not persisted");
        link.insert("source",QStringLiteral("missing-source-0000"));
        body.insert("linked_sketch_extrusion",link);parts[i]=body;changed=true;
    }
    ensure(changed,"Could not locate linked sketch save record");
    root.insert("parts",parts);
    QFile output(file);ensure(output.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot rewrite test file");
    output.write(QJsonDocument(root).toJson());output.close();
    const auto unchangedToken=opened.find(f.derived)->geometryToken;
    ensure(!opened.load(file,&issue),"Corrupt linked sketch source accepted");
    ensure(opened.find(f.derived)->geometryToken==unchangedToken,
           "Rejected corrupt scene changed the open document");
    // A syntactically valid depth change with the old solid must also fail.
    QJsonArray altered=parts;
    for(int i=0;i<altered.size();++i){
        auto body=altered[i].toObject();
        if(body.value("id").toString()!=f.derived)continue;
        auto link=body.value("linked_sketch_extrusion").toObject();
        link.insert("source",f.source);
        link.insert("depth_mm",7.0);
        body.insert("linked_sketch_extrusion",link);altered[i]=body;
    }
    root.insert("parts",altered);
    QFile output2(file);ensure(output2.open(QIODevice::WriteOnly|QIODevice::Truncate),
                               "Cannot rewrite geometry-integrity test file");
    output2.write(QJsonDocument(root).toJson());output2.close();
    ensure(!opened.load(file,&issue),"Inconsistent saved linked extrusion geometry accepted");
    ensure(opened.find(f.derived)->geometryToken==unchangedToken,
           "Invalid saved geometry changed the open document");
}
void retarget(){
    Fixture f;f.setup();QString issue;
    ensure(f.scene.addConstrainedPolygonExtrusion(rectangle(8),boundary(),10,&issue),
           "Could not construct second sketch source");
    const QString newSource=f.scene.parts().last().id;
    const QString oldToken=f.scene.find(f.derived)->geometryToken;
    ensure(f.scene.retargetLinkedSketchExtrusion(f.derived,newSource,5,8,&issue),
           "Could not retarget live dependent to a second sketch");
    ensure(f.scene.find(f.derived)->linkedSketchSourceId==newSource,
           "New sketch source ID was not stored");
    ensure(f.scene.find(f.derived)->geometryToken!=oldToken,"Retarget did not rebuild solid");
    closeTo(volume(f.scene,f.derived),400,"Retargeted sketch volume incorrect");
    closeTo(volume(f.scene,f.cut),840,"Retargeted Boolean cut volume incorrect");
    ensure(f.scene.linkedSketchExtrusionCurrent(f.derived)&&f.scene.linkedBooleanCurrent(f.cut),
           "Retargeted dependency chain is stale");
    const QString token=f.scene.find(f.derived)->geometryToken;
    const int count=f.scene.parts().size();
    ensure(!f.scene.retargetLinkedSketchExtrusion(f.derived,QStringLiteral("missing"),5,8,&issue),
           "Nonexistent sketch source was accepted");
    ensure(!f.scene.retargetLinkedSketchExtrusion(f.derived,newSource,-5,8,&issue),
           "Invalid depth was accepted in retarget");
    ensure(!f.scene.retargetLinkedSketchExtrusion(f.derived,newSource,5,1e9,&issue),
           "Invalid normal offset was accepted in retarget");
    ensure(count==f.scene.parts().size()&&f.scene.find(f.derived)->geometryToken==token,
           "Rejected retarget changed the model");
    ensure(f.scene.retargetLinkedSketchExtrusion(f.derived,newSource,5,8,&issue),
           "No-op retarget failed");
    ensure(f.scene.find(f.derived)->geometryToken==token,"No-op retarget rebuilt geometry");
    ensure(f.scene.undo(),"Undo retarget failed");
    ensure(f.scene.find(f.derived)->linkedSketchSourceId==f.source,"Undo lost previous source");
    closeTo(volume(f.scene,f.derived),500,"Undo retarget volume incorrect");
    ensure(f.scene.redo(),"Redo retarget failed");
    ensure(f.scene.find(f.derived)->linkedSketchSourceId==newSource,"Redo lost new source");
    closeTo(volume(f.scene,f.derived),400,"Redo retarget volume incorrect");
    ensure(!f.scene.erase(newSource,&issue),"Deleted sketch used by retargeted extrusion");
    const QString derivedToken=f.scene.find(f.derived)->geometryToken;
    ensure(f.scene.updateConstrainedPolygonExtrusion(f.source,rectangle(15),boundary(),10,&issue),
           "Could not edit former sketch source");
    ensure(f.scene.find(f.derived)->geometryToken==derivedToken,
           "Former sketch source rebuilt an unrelated derived body");
    closeTo(volume(f.scene,f.cut),1340,"Boolean descendant did not follow modified primary");
    ensure(f.scene.updateConstrainedPolygonExtrusion(newSource,rectangle(9),boundary(),10,&issue),
           "Could not edit new sketch source");
    closeTo(volume(f.scene,f.derived),450,"New sketch source did not regenerate dependent");
    closeTo(volume(f.scene,f.cut),1320,"Boolean descendant did not follow new source");
}

void pocket(){
    cad::SceneDocument scene;QString issue;
    ensure(scene.addConstrainedPolygonExtrusion(rectangle(10),boundary(),10,&issue),
           "Pocket source creation failed");
    const QString source=scene.parts().last().id;
    ensure(scene.addParametricRectangularExtrusion(12,12,10,&issue),
           "Pocket target creation failed");
    const QString target=scene.parts().last().id;
    closeTo(volume(scene,target),1440,"Unexpected pocket target volume");
    ensure(!scene.addLinkedSketchPocket(source,target,5,30,&issue),
           "Disjoint pocket tool incorrectly accepted");
    ensure(scene.parts().size()==2,"Disjoint pocket created partial bodies");
    ensure(!scene.addLinkedSketchPocket(source,target,-1,8,&issue),
           "Negative pocket depth accepted");
    ensure(scene.parts().size()==2,"Invalid pocket created partial bodies");
    ensure(scene.addLinkedSketchPocket(source,target,5,8,&issue),
           "Atomic associative pocket creation failed");
    ensure(scene.parts().size()==4,"Pocket did not create exactly two derived bodies");
    const QString cutter=scene.parts()[2].id;
    const QString result=scene.parts()[3].id;
    ensure(scene.parts()[2].hasLinkedSketchExtrusion&&!scene.parts()[2].visible,
           "Pocket tool is not a hidden associative sketch extrusion");
    ensure(scene.parts()[3].hasLinkedBoolean&&
           scene.parts()[3].linkedPrimaryId==target&&scene.parts()[3].linkedToolId==cutter&&
           scene.parts()[3].linkedOperation==QStringLiteral("Cut"),
           "Pocket result has incorrect Boolean dependencies");
    closeTo(volume(scene,cutter),500,"Initial pocket tool volume incorrect");
    closeTo(volume(scene,result),1240,"Initial pocket removal incorrect");
    QStringList impacted;
    ensure(scene.linkedBodyImpact(source,&impacted,&issue),"Pocket dependency preview failed");
    ensure(impacted==QStringList{source,cutter,result},
           "Pocket did not expose source/cutter/result dependency path");
    ensure(scene.undo(),"Undo entire pocket creation failed");
    ensure(scene.parts().size()==2,"Undo left an orphaned pocket tool or result");
    ensure(scene.redo(),"Redo pocket creation failed");
    ensure(scene.parts().size()==4&&scene.parts()[2].id==cutter&&scene.parts()[3].id==result,
           "Redo failed to restore both linked bodies");
    ensure(scene.updateConstrainedPolygonExtrusion(source,rectangle(8),boundary(),10,&issue),
           "Source sketch change did not propagate through pocket");
    closeTo(volume(scene,cutter),400,"Changed sketch failed to rebuild pocket tool");
    closeTo(volume(scene,result),1280,"Changed sketch failed to rebuild pocket cut");
    ensure(scene.updateLinkedSketchExtrusion(cutter,4,7,&issue),
           "Pocket cutter dimensions could not be edited");
    closeTo(volume(scene,cutter),320,"Edited pocket tool volume incorrect");
    closeTo(volume(scene,result),1200,"Edited pocket cut volume incorrect");
    const QString lastSourceToken=scene.find(source)->geometryToken;
    const QString lastPocketToken=scene.find(result)->geometryToken;
    cad::SketchPlane farPlane;farPlane.originZ=50;
    ensure(!scene.updateConstrainedPolygonExtrusion(source,rectangle(8),boundary(),10,
                                                     &issue,farPlane),
           "Source move that invalidates a linked pocket was accepted");
    ensure(scene.find(source)->geometryToken==lastSourceToken&&
           scene.find(result)->geometryToken==lastPocketToken,
           "Invalid upstream sketch edit partially committed its geometry");
    closeTo(volume(scene,result),1200,"Invalid edit altered the committed pocket volume");
    ensure(!scene.erase(source,&issue),"Deleted sketch source of a live pocket");
    ensure(!scene.erase(cutter,&issue),"Deleted pocket cutter with a live cut result");
    ensure(!scene.erase(target,&issue),"Deleted pocket target with a live cut result");
    ensure(scene.eraseMany({source,target,cutter,result},&issue),
           "Could not remove an entire associative pocket chain together");
}

void pocketPersistence(){
    cad::SceneDocument scene;QString issue;
    ensure(scene.addConstrainedPolygonExtrusion(rectangle(10),boundary(),10,&issue),
           "Pocket persistence source creation failed");
    const QString source=scene.parts().last().id;
    ensure(scene.addParametricRectangularExtrusion(12,12,10,&issue),
           "Pocket persistence target creation failed");
    const QString target=scene.parts().last().id;
    ensure(scene.addLinkedSketchPocket(source,target,5,8,&issue),
           "Pocket persistence construction failed");
    const QString cutter=scene.parts()[2].id,result=scene.parts()[3].id;
    QTemporaryDir folder;ensure(folder.isValid(),"Could not make pocket save folder");
    const QString filename=folder.path()+QStringLiteral("/pocket.cadscene");
    ensure(scene.saveAs(filename,&issue),"Saving associative pocket failed");
    cad::SceneDocument opened;
    ensure(opened.load(filename,&issue),"Reopening associative pocket failed");
    ensure(opened.parts().size()==4&&opened.find(cutter)&&opened.find(result),
           "Saved pocket lost tool/result bodies");
    ensure(!opened.find(cutter)->visible,"Reopened pocket tool became visible");
    ensure(opened.linkedSketchExtrusionCurrent(cutter)&&opened.linkedBooleanCurrent(result),
           "Reopened associative pocket is stale");
    closeTo(volume(opened,result),1240,"Reopened pocket volume incorrect");
    ensure(opened.updateConstrainedPolygonExtrusion(source,rectangle(9),boundary(),10,&issue),
           "Reopened source sketch could not be edited");
    closeTo(volume(opened,cutter),450,"Reopened pocket tool not associative");
    closeTo(volume(opened,result),1260,"Reopened pocket cut not associative");
    ensure(opened.undo(),"Undo after reopening pocket failed");
    closeTo(volume(opened,result),1240,"Undo after reopening pocket returned wrong result");
}

void boss(){
    cad::SceneDocument scene;QString issue;
    ensure(scene.addConstrainedPolygonExtrusion(rectangle(10),boundary(),10,&issue),
           "Boss source creation failed");
    const QString source=scene.parts().last().id;
    ensure(scene.addParametricRectangularExtrusion(12,12,10,&issue),
           "Boss target creation failed");
    const QString target=scene.parts().last().id;
    closeTo(volume(scene,target),1440,"Unexpected boss target volume");
    ensure(!scene.addLinkedSketchBoss(source,target,-1,8,&issue),
           "Negative additive boss depth was accepted");
    ensure(!scene.addLinkedSketchBoss(source,target,5,30,&issue),
           "Detached additive boss was accepted");
    ensure(!scene.addLinkedSketchBoss(source,target,4,2,&issue),
           "Fully buried, ineffective additive boss was accepted");
    ensure(scene.parts().size()==2,"Invalid boss created a partial tool or result");
    ensure(scene.addLinkedSketchBoss(source,target,5,8,&issue),
           "Atomic sketch-driven additive boss construction failed");
    ensure(scene.parts().size()==4,"Boss did not create exactly two linked bodies");
    const QString tool=scene.parts()[2].id;
    const QString result=scene.parts()[3].id;
    ensure(scene.parts()[2].hasLinkedSketchExtrusion&&!scene.parts()[2].visible,
           "Additive boss tool is not hidden and associative");
    ensure(scene.parts()[3].hasLinkedBoolean&&
           scene.parts()[3].linkedOperation==QStringLiteral("Fuse")&&
           scene.parts()[3].linkedPrimaryId==target&&
           scene.parts()[3].linkedToolId==tool,
           "Additive boss result does not retain source dependencies");
    closeTo(volume(scene,tool),500,"Initial additive tool volume incorrect");
    closeTo(volume(scene,result),1740,"Initial additive boss gained the wrong volume");
    QStringList impacted;
    ensure(scene.linkedBodyImpact(source,&impacted,&issue),"Boss impact calculation failed");
    ensure(impacted==QStringList{source,tool,result},"Boss impact order incorrect");
    ensure(scene.undo(),"Undo additive boss creation failed");
    ensure(scene.parts().size()==2,"Undo left an orphaned additive tool");
    ensure(scene.redo(),"Redo additive boss creation failed");
    ensure(scene.parts().size()==4&&scene.parts()[2].id==tool&&scene.parts()[3].id==result,
           "Redo did not restore the entire additive boss");
    ensure(scene.updateConstrainedPolygonExtrusion(source,rectangle(8),boundary(),10,&issue),
           "Upstream edit did not rebuild additive boss");
    closeTo(volume(scene,tool),400,"Changed source did not rebuild additive tool");
    closeTo(volume(scene,result),1680,"Changed source did not rebuild fused result");
    ensure(scene.updateLinkedSketchExtrusion(tool,4,7,&issue),
           "Boss depth/offset editing failed");
    closeTo(volume(scene,tool),320,"Edited additive tool volume incorrect");
    closeTo(volume(scene,result),1520,"Edited additive result volume incorrect");
    const QString token=scene.find(result)->geometryToken;
    const QString sourceToken=scene.find(source)->geometryToken;
    cad::SketchPlane farPlane;farPlane.originZ=50;
    ensure(!scene.updateConstrainedPolygonExtrusion(source,rectangle(8),boundary(),10,
                                                    &issue,farPlane),
           "Upstream move detached the additive boss without rejection");
    ensure(scene.find(result)->geometryToken==token&&
           scene.find(source)->geometryToken==sourceToken,
           "Rejected upstream edit partially mutated the additive model");
    closeTo(volume(scene,result),1520,"Rejected boss edit changed committed volume");
    ensure(!scene.updateLinkedSketchExtrusion(tool,5,50,&issue),
           "Disjoint additive tool edit was accepted");
    ensure(scene.find(result)->geometryToken==token,"Disjoint tool edit changed boss geometry");
    ensure(!scene.erase(source,&issue),"Removed sketch with dependent boss");
    ensure(!scene.erase(tool,&issue),"Removed live boss construction tool");
    ensure(!scene.erase(target,&issue),"Removed target with dependent boss");
    ensure(scene.eraseMany({source,target,tool,result},&issue),
           "Could not remove the complete associative boss chain");
}

void bossPersistence(){
    cad::SceneDocument scene;QString issue;
    ensure(scene.addConstrainedPolygonExtrusion(rectangle(10),boundary(),10,&issue),
           "Boss persistence source creation failed");
    const QString source=scene.parts().last().id;
    ensure(scene.addParametricRectangularExtrusion(12,12,10,&issue),
           "Boss persistence target creation failed");
    const QString target=scene.parts().last().id;
    ensure(scene.addLinkedSketchBoss(source,target,5,8,&issue),
           "Boss persistence construction failed");
    const QString tool=scene.parts()[2].id,result=scene.parts()[3].id;
    QTemporaryDir folder;ensure(folder.isValid(),"Could not make additive boss temp folder");
    const QString file=folder.path()+QStringLiteral("/boss.cadscene");
    ensure(scene.saveAs(file,&issue),"Saving additive boss failed");
    cad::SceneDocument reopened;
    ensure(reopened.load(file,&issue),"Reopening additive boss failed");
    ensure(reopened.parts().size()==4&&reopened.find(tool)&&reopened.find(result),
           "Saved additive boss lost its dependent solids");
    ensure(!reopened.find(tool)->visible,"Reopened additive tool became visible");
    ensure(reopened.linkedSketchExtrusionCurrent(tool)&&reopened.linkedBooleanCurrent(result),
           "Loaded associative boss is stale");
    closeTo(volume(reopened,result),1740,"Reopened additive boss volume incorrect");
    ensure(reopened.updateConstrainedPolygonExtrusion(source,rectangle(9),boundary(),10,&issue),
           "Editing reopened boss source sketch failed");
    closeTo(volume(reopened,tool),450,"Reopened boss tool lost sketch associativity");
    closeTo(volume(reopened,result),1710,"Reopened boss failed to regenerate linked Fuse");
    ensure(reopened.undo(),"Undo after reopening additive boss failed");
    closeTo(volume(reopened,result),1740,"Undo did not restore saved boss geometry");
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        const QString mode=argc>1?QString::fromUtf8(argv[1]):QStringLiteral("operations");
        if(mode=="operations")operations();
        else if(mode=="persistence")persistence();
        else if(mode=="retarget")retarget();
        else if(mode=="pocket")pocket();
        else if(mode=="pocket-persistence")pocketPersistence();
        else if(mode=="boss")boss();
        else if(mode=="boss-persistence")bossPersistence();
        else throw std::runtime_error("Invalid test mode");
        std::cout<<"PASS: "<<checks<<" source-linked sketch extrusion checks ("<<mode.toStdString()<<")\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"FAIL #"<<checks<<": "<<ex.what()<<'\n';return 1;
    }
}
