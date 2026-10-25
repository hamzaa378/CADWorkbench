#include "core/SceneDocument.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void closeTo(double actual, double expected, double tolerance = 1e-5) {
    require(std::isfinite(actual) && std::abs(actual-expected) <= tolerance*std::max(1.,std::abs(expected)),
            ("Numeric mismatch: actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected)).c_str());
}
double volume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape,props);
    return props.Mass();
}
void validate(const cad::Part& part) {
    require(!part.shape.IsNull() && BRepCheck_Analyzer(part.shape).IsValid(),"Topology invalid");
    require(volume(part.shape)>0,"Non-positive volume");
}
QByteArray readBytes(const QString& filename) {
    QFile file(filename); require(file.open(QIODevice::ReadOnly),"Cannot open file");
    return file.readAll();
}
void writeBytes(const QString& filename,const QByteArray& bytes) {
    QFile file(filename); require(file.open(QIODevice::WriteOnly),"Cannot create file");
    require(file.write(bytes)==bytes.size(),"Write mismatch");
}
void testWingRib() {
    QString error;
    const auto spec=cad::productionPartSpec(cad::ProductionPart::Wingrib);
    require(spec.name=="Wing rib"&&spec.defaults.size()==5,"Wing rib specification is incomplete");
    require(spec.labels==QStringList({"Chord length (mm)","Maximum airfoil depth (mm)","Rib thickness (mm)","Spar opening radius (mm)","Lightening hole radius (mm)"}),
            "Wing rib labels do not describe airfoil parameters");
    cad::SceneDocument doc;
    require(doc.addProductionPart(cad::ProductionPart::Wingrib,spec.defaults,&error),
            ("Default Wing rib failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Wing rib","Wing rib was not inserted");
    const auto& part=doc.parts().first();
    validate(part);
    int solids=0,faces=0;
    for(TopExp_Explorer it(part.shape,TopAbs_SOLID);it.More();it.Next())++solids;
    for(TopExp_Explorer it(part.shape,TopAbs_FACE);it.More();it.Next())++faces;
    require(solids==1,"Wing rib should be one continuous solid");
    require(faces>20,"Wing rib is missing the airfoil profile or through-hole detail");
    Bnd_Box bounds;
    BRepBndLib::Add(part.shape,bounds);
    double xmin,ymin,zmin,xmax,ymax,zmax;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    closeTo(xmax-xmin,spec.defaults[0],1e-4);
    closeTo(ymax-ymin,spec.defaults[1],0.01);
    closeTo(zmax-zmin,spec.defaults[2],1e-4);
    require(doc.undo()&&doc.parts().isEmpty(),"Wing rib undo failed");

    auto invalid=spec.defaults;
    invalid[4]=20.0;
    require(!doc.addProductionPart(cad::ProductionPart::Wingrib,invalid,&error),"Oversized Wing rib lightening hole was accepted");
    require(doc.parts().isEmpty()&&!doc.canUndo(),"Rejected Wing rib dimensions mutated the document");
}
void testModelingTools() {
    QString error;
    for(int index=0;index<static_cast<int>(cad::ModelingTool::Count);++index){
        const auto kind=static_cast<cad::ModelingTool>(index);
        const auto spec=cad::modelingToolSpec(kind);
        require(!spec.name.isEmpty()&&!spec.defaults.isEmpty()&&spec.defaults.size()==spec.labels.size(),
                "Modeling-tool specification is incomplete");
        cad::SceneDocument doc;
        auto invalid=spec.defaults; invalid[0]=0;
        require(!doc.addModelingTool(kind,invalid,&error),"Modeling tool accepted a zero dimension");
        require(doc.parts().isEmpty()&&!doc.canUndo(),"Rejected modeling tool changed the document");
        const bool created=doc.addModelingTool(kind,spec.defaults,&error);
        if(!created){std::string dimensions;for(double value:spec.defaults)dimensions+=" "+std::to_string(value);throw std::runtime_error("Modeling tool failed: "+spec.name.toStdString()+"; "+error.toStdString()+"; dimensions:"+dimensions);}
        require(doc.parts().size()==1&&doc.parts().first().name==spec.name,"Modeling tool inserted the wrong body");
        validate(doc.parts().first());
        require(doc.undo()&&doc.parts().isEmpty(),"Modeling-tool undo failed");
    }
}
void execute() {
    QTemporaryDir dir;
    require(dir.isValid(),"Temporary directory unavailable");
    QString error;
    cad::SceneDocument doc;
    require(!doc.dirty(),"Empty document dirty");
    require(!doc.addPrimitive(cad::Primitive::Box,{0,10,10},&error),"Zero width accepted");
    require(!doc.addPrimitive(cad::Primitive::Sphere,{std::numeric_limits<double>::quiet_NaN()},&error),"NaN accepted");
    require(!doc.addPrimitive(cad::Primitive::Torus,{5,6},&error),"Self-overlapping torus accepted");
    require(!doc.addPrimitive(cad::Primitive::Cone,{10,3},&error),"Incomplete dimensions accepted");
    require(doc.parts().isEmpty() && !doc.canUndo(),"Invalid primitive mutated document");

    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Box creation failed");
    const QString box = doc.parts().last().id;
    closeTo(volume(doc.parts().last().shape),6000);
    require(doc.addPrimitive(cad::Primitive::Cylinder,{3,10},&error),"Cylinder creation failed");
    const QString cylinder = doc.parts().last().id;
    closeTo(volume(doc.parts().last().shape),std::numbers::pi*90);
    require(doc.addPrimitive(cad::Primitive::Sphere,{4},&error),"Sphere creation failed");
    closeTo(volume(doc.parts().last().shape),4./3.*std::numbers::pi*64);
    require(doc.addPrimitive(cad::Primitive::Cone,{6,2,10},&error),"Cone creation failed");
    closeTo(volume(doc.parts().last().shape),std::numbers::pi*10*(36+12+4)/3);
    require(doc.addPrimitive(cad::Primitive::Torus,{12,2},&error),"Torus creation failed");
    closeTo(volume(doc.parts().last().shape),2*std::numbers::pi*std::numbers::pi*12*4,1e-3);
    for (const auto& part : doc.parts()) validate(part);
    require(doc.parts().size()==5,"Primitive count mismatch");

    require(doc.translate(cylinder,4,4,0,&error),"Translation failed");
    require(doc.rotate(box,cad::Axis::Z,90,&error),"Rotation failed");
    closeTo(volume(doc.find(box)->shape),6000);
    require(doc.scale(box,2,&error),"Uniform scaling failed");
    closeTo(volume(doc.find(box)->shape),48000,1e-4);
    require(!doc.scale(box,0,&error),"Zero scale accepted");
    require(!doc.rotate(box,cad::Axis::X,std::numeric_limits<double>::infinity(),&error),"Infinity angle accepted");
    require(!doc.translate("not-a-body",1,0,0,&error),"Unknown body accepted");
    require(doc.duplicate(box,100,0,0,&error),"Duplicate failed");
    require(doc.parts().size()==6 && doc.parts().last().id != box,"Duplicate ID conflict");
    closeTo(volume(doc.parts().last().shape),48000,1e-4);

    require(doc.rename(box,"Machine Base",&error),"Rename failed");
    require(!doc.rename(cylinder,"machine base",&error),"Case-insensitive duplicate name accepted");
    require(doc.setVisibility(box,false,&error),"Hide failed");
    require(doc.setColor(box,"#ff781a",&error),"Color change failed");
    require(!doc.setColor(box,"#bad",&error),"Invalid color accepted");
    require(!doc.find(box)->visible && doc.find(box)->color=="#ff781a","Appearance not applied");

    const QString scene = dir.filePath("project_v2.cadscene");
    require(doc.saveAs(scene,&error),"v2 save failed");
    require(!doc.dirty(),"Saved project dirty");
    const auto savedCount=doc.parts().size();
    cad::SceneDocument reopened;
    require(reopened.load(scene,&error),"v2 load failed");
    require(reopened.parts().size()==savedCount,"Round-trip body count mismatch");
    require(!reopened.find(box)->visible && reopened.find(box)->color=="#ff781a","Appearance not persisted");
    closeTo(volume(reopened.find(box)->shape),48000,1e-4);

    // Old v0.1 files do not contain visible/color attributes; migration defaults are applied.
    QJsonObject oldRoot=QJsonDocument::fromJson(readBytes(scene)).object();
    oldRoot.insert("schema","cadworkbench.scene.v1");
    QJsonArray oldParts=oldRoot.value("parts").toArray();
    for (int i=0;i<oldParts.size();++i) {
        auto item=oldParts[i].toObject();
        item.remove("visible"); item.remove("color");
        oldParts[i]=item;
    }
    oldRoot.insert("parts",oldParts);
    const QString legacy = dir.filePath("legacy.cadscene");
    writeBytes(legacy,QJsonDocument(oldRoot).toJson());
    cad::SceneDocument migrated;
    require(migrated.load(legacy,&error),"v0.1 migration failed");
    require(migrated.find(box)->visible && migrated.find(box)->color=="#6c9dc2","Legacy defaults wrong");

    // Every rejected load must retain the current document and its save path.
    auto corrupted=oldRoot;
    auto malformedParts=corrupted.value("parts").toArray();
    auto invalid=malformedParts[0].toObject(); invalid.insert("brep","!!!not-base64!!!");
    malformedParts[0]=invalid; corrupted.insert("parts",malformedParts);
    const QString bad = dir.filePath("corrupt.cadscene");
    writeBytes(bad,QJsonDocument(corrupted).toJson());
    const QString beforePath = reopened.path();
    require(!reopened.load(bad,&error),"Corrupted shape accepted");
    require(reopened.path()==beforePath && reopened.parts().size()==savedCount,"Rejected load mutated document");

    // One multi-body delete creates exactly one undo step.
    const QString d1=reopened.parts()[1].id, d2=reopened.parts()[2].id;
    require(reopened.eraseMany({d1,d2},&error),"Batch delete failed");
    require(reopened.parts().size()==savedCount-2,"Batch delete count mismatch");
    require(reopened.undo() && reopened.parts().size()==savedCount,"Batch undo not atomic");
    require(!reopened.dirty(),"Undo to saved state should be clean");
    require(reopened.eraseMany({d1,d2},&error),"Branched batch delete failed");
    require(reopened.dirty(),"Branch after undo treated as clean");
    require(!reopened.canRedo(),"Redo should clear on branch");
    require(!reopened.eraseMany({d1,"missing"},&error),"Partial invalid batch accepted");
    require(reopened.parts().size()==savedCount-2,"Invalid batch modified document");

    // Boolean cut: a hole entirely enclosed in a rectangular solid.
    cad::SceneDocument booleanDoc;
    require(booleanDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Boolean box failed");
    const QString a=booleanDoc.parts()[0].id;
    require(booleanDoc.addPrimitive(cad::Primitive::Cylinder,{3,10},&error),"Boolean cylinder failed");
    const QString b=booleanDoc.parts()[1].id;
    require(booleanDoc.translate(b,4,4,0,&error),"Boolean cylinder offset failed");
    require(!booleanDoc.booleanOperation(cad::BooleanOp::Cut,a,a,&error),"Self-Boolean accepted");
    require(booleanDoc.booleanOperation(cad::BooleanOp::Cut,a,b,&error),"Cut failed");
    require(booleanDoc.parts().size()==1,"Cut should replace both operands");
    closeTo(volume(booleanDoc.parts().first().shape),6000.-std::numbers::pi*90.,1e-3);
    require(booleanDoc.undo() && booleanDoc.parts().size()==2,"Boolean undo failed");
    require(booleanDoc.redo() && booleanDoc.parts().size()==1,"Boolean redo failed");
    cad::SceneDocument disjoint;
    require(disjoint.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Disjoint box creation failed");
    require(disjoint.addPrimitive(cad::Primitive::Sphere,{2},&error),"Disjoint sphere creation failed");
    const QString u=disjoint.parts()[0].id, v=disjoint.parts()[1].id;
    require(disjoint.translate(v,100,0,0,&error),"Sphere translation failed");
    require(!disjoint.booleanOperation(cad::BooleanOp::Common,u,v,&error),"Empty intersection accepted");
    require(disjoint.parts().size()==2,"Empty intersection mutated document");

    // Rectangle and circle sketch profiles extrude into validated B-rep solids.
    cad::SceneDocument extrusionDoc;
    cad::ExtrusionCommand rectangleCommand;
    require(extrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,5,-4,40,30,20,
                                          &rectangleCommand,&error),"Rectangle extrusion preparation failed");
    const auto rectanglePreview = cad::SceneDocument::buildExtrusionPreview(rectangleCommand);
    require(!rectanglePreview.shape.IsNull(),"Rectangle extrusion preview failed");
    require(extrusionDoc.parts().isEmpty() && !extrusionDoc.canUndo(),"Rectangle preview mutated document");
    require(BRepCheck_Analyzer(rectanglePreview.shape).IsValid(),"Rectangle extrusion topology invalid");
    closeTo(volume(rectanglePreview.shape),24000,1e-5);
    require(extrusionDoc.commitExtrusion(rectangleCommand,rectanglePreview.shape,&error),"Rectangle extrusion commit failed");
    require(extrusionDoc.parts().size()==1 && extrusionDoc.canUndo(),"Rectangle extrusion was not a single undoable body");
    require(extrusionDoc.undo() && extrusionDoc.parts().isEmpty(),"Rectangle extrusion undo failed");
    require(extrusionDoc.redo() && extrusionDoc.parts().size()==1,"Rectangle extrusion redo failed");

    cad::SceneDocument circleExtrusionDoc;
    cad::ExtrusionCommand circleCommand;
    require(circleExtrusionDoc.prepareExtrusion(cad::SketchProfile::Circle,2,3,20,0,25,
                                                &circleCommand,&error),"Circle extrusion preparation failed");
    const auto circlePreview = cad::SceneDocument::buildExtrusionPreview(circleCommand);
    require(!circlePreview.shape.IsNull() && BRepCheck_Analyzer(circlePreview.shape).IsValid(),"Circle extrusion preview invalid");
    closeTo(volume(circlePreview.shape),std::numbers::pi*100*25,1e-5);
    require(circleExtrusionDoc.commitExtrusion(circleCommand,circlePreview.shape,&error),"Circle extrusion commit failed");
    require(circleExtrusionDoc.parts().size()==1,"Circle extrusion body missing");

    cad::SceneDocument joinExtrusionDoc;
    require(joinExtrusionDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Join extrusion fixture failed");
    const QString joinTarget=joinExtrusionDoc.parts().first().id;
    cad::ExtrusionCommand joinCommand;
    require(joinExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,5,5,5,5,15,
                                              cad::ExtrusionMode::Join,joinTarget,&joinCommand,&error),
            "Join extrusion preparation failed");
    const auto joinPreview=cad::SceneDocument::buildExtrusionPreview(joinCommand);
    require(!joinPreview.shape.IsNull() && BRepCheck_Analyzer(joinPreview.shape).IsValid(),"Join extrusion preview invalid");
    closeTo(volume(joinPreview.shape),4125,1e-5);
    require(joinExtrusionDoc.parts().size()==1 && std::abs(volume(joinExtrusionDoc.parts().first().shape)-4000)<1e-5,
            "Join extrusion preview mutated the target");
    require(joinExtrusionDoc.commitExtrusion(joinCommand,joinPreview.shape,&error),"Join extrusion commit failed");
    require(joinExtrusionDoc.parts().size()==1,"Join extrusion created an extra body");
    closeTo(volume(joinExtrusionDoc.parts().first().shape),4125,1e-5);
    require(joinExtrusionDoc.undo(),"Join extrusion undo failed");
    closeTo(volume(joinExtrusionDoc.parts().first().shape),4000,1e-5);

    cad::SceneDocument cutExtrusionDoc;
    require(cutExtrusionDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cut extrusion fixture failed");
    const QString cutTarget=cutExtrusionDoc.parts().first().id;
    cad::ExtrusionCommand cutCommand;
    require(cutExtrusionDoc.prepareExtrusion(cad::SketchProfile::Circle,10,10,4,0,10,
                                             cad::ExtrusionMode::Cut,cutTarget,&cutCommand,&error),
            "Cut extrusion preparation failed");
    const auto cutPreview=cad::SceneDocument::buildExtrusionPreview(cutCommand);
    require(!cutPreview.shape.IsNull() && BRepCheck_Analyzer(cutPreview.shape).IsValid(),"Cut extrusion preview invalid");
    closeTo(volume(cutPreview.shape),4000-40*std::numbers::pi,1e-5);
    require(cutExtrusionDoc.commitExtrusion(cutCommand,cutPreview.shape,&error),"Cut extrusion commit failed");
    require(cutExtrusionDoc.parts().size()==1,"Cut extrusion created an extra body");
    closeTo(volume(cutExtrusionDoc.parts().first().shape),4000-40*std::numbers::pi,1e-5);
    require(cutExtrusionDoc.undo(),"Cut extrusion undo failed");
    closeTo(volume(cutExtrusionDoc.parts().first().shape),4000,1e-5);

    cad::ExtrusionCommand missingTargetCommand;
    require(!cutExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,5,5,5,
                                              cad::ExtrusionMode::Join,QStringLiteral("missing"),
                                              &missingTargetCommand,&error),"Join extrusion accepted a missing target");

    cad::SceneDocument invalidExtrusionDoc;
    cad::ExtrusionCommand invalidExtrusion;
    require(!invalidExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,0,20,10,
                                                 &invalidExtrusion,&error),"Zero-width sketch profile accepted");
    require(!invalidExtrusionDoc.prepareExtrusion(cad::SketchProfile::Circle,0,0,20,0,-10,
                                                 &invalidExtrusion,&error),"Negative extrusion height accepted");
    require(invalidExtrusionDoc.parts().isEmpty() && !invalidExtrusionDoc.canUndo(),"Rejected extrusion changed document");

    cad::SceneDocument staleExtrusionDoc;
    cad::ExtrusionCommand staleExtrusion;
    require(staleExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,10,10,10,
                                               &staleExtrusion,&error),"Stale extrusion preparation failed");
    const auto staleExtrusionPreview = cad::SceneDocument::buildExtrusionPreview(staleExtrusion);
    require(!staleExtrusionPreview.shape.IsNull(),"Stale extrusion fixture preview failed");
    require(staleExtrusionDoc.addPrimitive(cad::Primitive::Box,{5,5,5},&error),"Stale extrusion mutation setup failed");
    require(!staleExtrusionDoc.commitExtrusion(staleExtrusion,staleExtrusionPreview.shape,&error),"Stale extrusion preview committed");
    require(staleExtrusionDoc.parts().size()==1,"Rejected stale extrusion changed document");

    // A chamfer preview leaves the model unchanged until commit and commits as one undo step.
    cad::SceneDocument chamferDoc;
    require(chamferDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Chamfer fixture creation failed");
    const cad::Part* chamferPart = &chamferDoc.parts().first();
    cad::ChamferCommand chamferCommand;
    require(chamferDoc.prepareChamfer({chamferPart->id,chamferPart->geometryToken,1},0.5,
                                      &chamferCommand,&error),"Chamfer preparation failed");
    const auto chamferPreview = cad::SceneDocument::buildChamferPreview(chamferCommand);
    require(!chamferPreview.shape.IsNull() && BRepCheck_Analyzer(chamferPreview.shape).IsValid(),"Chamfer preview invalid");
    require(chamferDoc.parts().size()==1 && volume(chamferDoc.parts().first().shape)==6000,
            "Chamfer preview mutated the document");
    require(chamferDoc.commitChamfer(chamferCommand,chamferPreview.shape,&error),"Chamfer commit failed");
    require(chamferDoc.undo() && std::abs(volume(chamferDoc.parts().first().shape)-6000)<1e-5,
            "Chamfer undo failed");

    cad::SceneDocument allEdgeFilletDoc;
    require(allEdgeFilletDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"All-edge fillet fixture failed");
    const QString allFilletId=allEdgeFilletDoc.parts().first().id;
    require(!allEdgeFilletDoc.filletAllEdges(allFilletId,100,&error),"Impossible all-edge fillet was accepted");
    require(allEdgeFilletDoc.parts().size()==1,"Rejected all-edge fillet mutated the document");
    require(allEdgeFilletDoc.filletAllEdges(allFilletId,1,&error),"All-edge fillet failed");
    validate(allEdgeFilletDoc.parts().first());
    require(volume(allEdgeFilletDoc.parts().first().shape)<1000,"All-edge fillet did not remove corner volume");
    require(allEdgeFilletDoc.undo()&&std::abs(volume(allEdgeFilletDoc.parts().first().shape)-1000)<1e-5,
            "All-edge fillet undo failed");
    cad::SceneDocument allEdgeChamferDoc;
    require(allEdgeChamferDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"All-edge chamfer fixture failed");
    require(allEdgeChamferDoc.chamferAllEdges(allEdgeChamferDoc.parts().first().id,1,&error),"All-edge chamfer failed");
    validate(allEdgeChamferDoc.parts().first());
    require(volume(allEdgeChamferDoc.parts().first().shape)<1000,"All-edge chamfer did not remove corner volume");
    require(allEdgeChamferDoc.undo()&&std::abs(volume(allEdgeChamferDoc.parts().first().shape)-1000)<1e-5,
            "All-edge chamfer undo failed");
    cad::SceneDocument draftDoc;
    require(draftDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Draft fixture failed");
    const QString draftTarget=draftDoc.parts().first().id;
    require(!draftDoc.draftFace(draftTarget,99,cad::Axis::Z,0,5,&error),"Draft accepted a nonexistent face");
    bool draftApplied=false;
    for(int faceIndex=1;faceIndex<=6&&!draftApplied;++faceIndex)
        draftApplied=draftDoc.draftFace(draftTarget,faceIndex,cad::Axis::Z,0,5,&error);
    require(draftApplied,"No box side face could be drafted against the base plane");
    validate(draftDoc.parts().first());
    require(std::abs(volume(draftDoc.parts().first().shape)-1000)>1e-4,"Face draft did not change the solid");
    require(draftDoc.undo()&&std::abs(volume(draftDoc.parts().first().shape)-1000)<1e-5,"Draft undo failed");

    // Selected-edge APIs: preview/commit and one-shot paths must preserve geometry and undo.
    cad::SceneDocument selectedFilletDoc;
    require(selectedFilletDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Selected fillet fixture failed");
    const auto filletPart=selectedFilletDoc.parts().first();
    const cad::EdgeReference filletRef{filletPart.id,filletPart.geometryToken,1};
    cad::FilletCommand filletCommand;
    require(!selectedFilletDoc.prepareFillet(filletRef,0,&filletCommand,&error),"Zero selected-edge fillet radius accepted");
    require(!selectedFilletDoc.prepareFillet({filletPart.id,QStringLiteral("stale"),1},1,&filletCommand,&error),"Stale fillet edge accepted");
    require(!selectedFilletDoc.prepareFillet({filletPart.id,filletPart.geometryToken,999},1,&filletCommand,&error),"Missing fillet edge accepted");
    require(selectedFilletDoc.prepareFillet(filletRef,1,&filletCommand,&error),"Selected fillet preparation failed");
    const auto filletPreview=cad::SceneDocument::buildFilletPreview(filletCommand);
    require(!filletPreview.shape.IsNull()&&BRepCheck_Analyzer(filletPreview.shape).IsValid(),"Selected fillet preview invalid");
    require(selectedFilletDoc.parts().size()==1&&std::abs(volume(selectedFilletDoc.parts().first().shape)-1000)<1e-5,
            "Selected fillet preview mutated source");
    require(selectedFilletDoc.commitFillet(filletCommand,filletPreview.shape,&error),"Selected fillet commit failed");
    validate(selectedFilletDoc.parts().first());
    require(volume(selectedFilletDoc.parts().first().shape)<1000,"Selected fillet did not change solid");
    require(selectedFilletDoc.undo()&&std::abs(volume(selectedFilletDoc.parts().first().shape)-1000)<1e-5,
            "Selected fillet undo failed");
    cad::SceneDocument oneShotFilletDoc;
    require(oneShotFilletDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"One-shot fillet fixture failed");
    const auto oneShotFilletPart=oneShotFilletDoc.parts().first();
    require(oneShotFilletDoc.filletEdge({oneShotFilletPart.id,oneShotFilletPart.geometryToken,1},1,&error),
            "One-shot selected-edge fillet failed");
    require(oneShotFilletDoc.undo()&&std::abs(volume(oneShotFilletDoc.parts().first().shape)-1000)<1e-5,
            "One-shot selected-edge fillet undo failed");

    cad::SceneDocument oneShotChamferDoc;
    require(oneShotChamferDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"One-shot chamfer fixture failed");
    const auto oneShotChamferPart=oneShotChamferDoc.parts().first();
    require(!oneShotChamferDoc.chamferEdge({oneShotChamferPart.id,oneShotChamferPart.geometryToken,1},0,&error),
            "Zero selected-edge chamfer distance accepted");
    require(oneShotChamferDoc.chamferEdge({oneShotChamferPart.id,oneShotChamferPart.geometryToken,1},1,&error),
            "One-shot selected-edge chamfer failed");
    validate(oneShotChamferDoc.parts().first());
    require(oneShotChamferDoc.undo()&&std::abs(volume(oneShotChamferDoc.parts().first().shape)-1000)<1e-5,
            "One-shot selected-edge chamfer undo failed");

    // Single delete, clear, and New document have independent and predictable history semantics.
    cad::SceneDocument lifecycleDoc;
    require(lifecycleDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Lifecycle fixture creation failed");
    const QString lifecycleId=lifecycleDoc.parts().first().id;
    require(lifecycleDoc.erase(lifecycleId,&error)&&lifecycleDoc.parts().isEmpty(),"Single-body erase failed");
    require(lifecycleDoc.undo()&&lifecycleDoc.parts().size()==1,"Single-body erase undo failed");
    lifecycleDoc.clear();
    require(lifecycleDoc.parts().isEmpty()&&lifecycleDoc.canUndo(),"Clear did not checkpoint removal");
    require(lifecycleDoc.undo()&&lifecycleDoc.parts().size()==1,"Clear undo failed");
    require(lifecycleDoc.saveAs(dir.filePath("lifecycle.cadscene"),&error),"Lifecycle document save failed");
    lifecycleDoc.newDocument();
    require(lifecycleDoc.parts().isEmpty()&&lifecycleDoc.path().isEmpty()&&!lifecycleDoc.dirty()&&
            !lifecycleDoc.canUndo()&&!lifecycleDoc.canRedo(),"New document did not reset document state");

    cad::SceneDocument staleChamferDoc;
    require(staleChamferDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Stale chamfer fixture failed");
    const cad::Part* staleChamferPart = &staleChamferDoc.parts().first();
    cad::ChamferCommand staleChamfer;
    require(staleChamferDoc.prepareChamfer({staleChamferPart->id,staleChamferPart->geometryToken,1},0.5,
                                            &staleChamfer,&error),"Stale chamfer preparation failed");
    const auto staleChamferPreview = cad::SceneDocument::buildChamferPreview(staleChamfer);
    require(!staleChamferPreview.shape.IsNull(),"Stale chamfer fixture preview failed");
    require(staleChamferDoc.translate(staleChamferPart->id,1,0,0,&error),"Stale chamfer mutation setup failed");
    require(!staleChamferDoc.commitChamfer(staleChamfer,staleChamferPreview.shape,&error),"Stale chamfer preview committed");

    // Revolve profiles support hollow full turns and partial angular sweeps.
    cad::SceneDocument revolveDoc;
    cad::RevolveCommand revolveCommand;
    require(revolveDoc.prepareRevolve(5,2,5,20,360,&revolveCommand,&error),"Revolve preparation failed");
    const auto revolvePreview=cad::SceneDocument::buildRevolvePreview(revolveCommand);
    require(!revolvePreview.shape.IsNull() && BRepCheck_Analyzer(revolvePreview.shape).IsValid(),"Full revolve preview invalid");
    require(revolveDoc.parts().isEmpty() && !revolveDoc.canUndo(),"Revolve preview mutated document");
    closeTo(volume(revolvePreview.shape),1500*std::numbers::pi,1e-4);
    require(revolveDoc.commitRevolve(revolveCommand,revolvePreview.shape,&error),"Revolve commit failed");
    require(revolveDoc.undo() && revolveDoc.parts().isEmpty(),"Revolve undo failed");
    require(revolveDoc.redo() && revolveDoc.parts().size()==1,"Revolve redo failed");
    cad::RevolveCommand partialRevolveCommand;
    require(revolveDoc.prepareRevolve(0,0,10,20,180,&partialRevolveCommand,&error),"Partial revolve preparation failed");
    const auto partialRevolve=cad::SceneDocument::buildRevolvePreview(partialRevolveCommand);
    require(!partialRevolve.shape.IsNull() && BRepCheck_Analyzer(partialRevolve.shape).IsValid(),"Partial revolve preview invalid");
    closeTo(volume(partialRevolve.shape),1000*std::numbers::pi,1e-4);
    require(!revolveDoc.prepareRevolve(-1,0,10,20,360,&partialRevolveCommand,&error),"Negative revolve radius accepted");
    require(!revolveDoc.prepareRevolve(0,0,10,20,361,&partialRevolveCommand,&error),"Revolve angle above 360 accepted");
    cad::RevolveCommand axisRevolveCommand;
    require(!revolveDoc.prepareRevolveAboutAxis(0,0,10,20,360,3,0,0,0,&axisRevolveCommand,&error),
            "Revolve accepted an invalid axis index");
    require(revolveDoc.prepareRevolveAboutAxis(0,0,10,20,360,0,5,0,0,&axisRevolveCommand,&error),
            "X-axis revolve preparation failed");
    const auto axisRevolvePreview=cad::SceneDocument::buildRevolvePreview(axisRevolveCommand);
    require(!axisRevolvePreview.shape.IsNull()&&BRepCheck_Analyzer(axisRevolvePreview.shape).IsValid(),"X-axis revolve invalid");
    closeTo(volume(axisRevolvePreview.shape),2000*std::numbers::pi,1e-4);
    Bnd_Box axisRevolveBounds;BRepBndLib::AddOptimal(axisRevolvePreview.shape,axisRevolveBounds,false,false);
    double arx0=0,ary0=0,arz0=0,arx1=0,ary1=0,arz1=0;axisRevolveBounds.Get(arx0,ary0,arz0,arx1,ary1,arz1);
    closeTo(arx0,5);closeTo(arx1,25);closeTo(ary0,-10);closeTo(ary1,10);
    cad::RevolveCommand staleRevolveCommand;
    require(revolveDoc.prepareRevolve(0,0,10,20,360,&staleRevolveCommand,&error),"Stale revolve preparation failed");
    const auto staleRevolvePreview=cad::SceneDocument::buildRevolvePreview(staleRevolveCommand);
    require(!staleRevolvePreview.shape.IsNull(),"Stale revolve preview fixture failed");
    require(revolveDoc.addPrimitive(cad::Primitive::Box,{2,2,2},&error),"Stale revolve mutation setup failed");
    require(!revolveDoc.commitRevolve(staleRevolveCommand,staleRevolvePreview.shape,&error),"Stale revolve preview committed");

    // Mirror creates an independent body; linear pattern creates all copies in one undo step.
    cad::SceneDocument mirrorDoc;
    require(mirrorDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Mirror source creation failed");
    const QString mirrorSource=mirrorDoc.parts().first().id;
    require(mirrorDoc.mirror(mirrorSource,cad::Axis::X,0,&error),"Mirror operation failed");
    require(mirrorDoc.parts().size()==2 && mirrorDoc.parts().last().id!=mirrorSource,"Mirror did not create an independent body");
    validate(mirrorDoc.parts().last());
    closeTo(volume(mirrorDoc.parts().last().shape),6000);
    Bnd_Box mirroredBounds;
    BRepBndLib::Add(mirrorDoc.parts().last().shape,mirroredBounds);
    double mirrorXmin=0,mirrorYmin=0,mirrorZmin=0,mirrorXmax=0,mirrorYmax=0,mirrorZmax=0;
    mirroredBounds.Get(mirrorXmin,mirrorYmin,mirrorZmin,mirrorXmax,mirrorYmax,mirrorZmax);
    closeTo(mirrorXmin,-10); closeTo(mirrorXmax,0);
    require(mirrorDoc.undo() && mirrorDoc.parts().size()==1,"Mirror undo failed");
    require(!mirrorDoc.mirror("missing",cad::Axis::Y,0,&error),"Mirror accepted unknown body");

    cad::SceneDocument advancedTransformDoc;
    require(advancedTransformDoc.addPrimitive(cad::Primitive::Box,{2,2,2},&error),"Custom transform fixture failed");
    const QString advancedTransformId=advancedTransformDoc.parts().first().id;
    require(!advancedTransformDoc.mirrorAcrossPlane(advancedTransformId,0,0,0,0,&error),"Zero-normal mirror plane accepted");
    require(advancedTransformDoc.mirrorAcrossPlane(advancedTransformId,1,1,0,0,&error),"Arbitrary-plane mirror failed");
    require(advancedTransformDoc.parts().size()==2,"Arbitrary-plane mirror did not preserve the source");
    Bnd_Box arbitraryMirrorBounds;
    BRepBndLib::AddOptimal(advancedTransformDoc.parts().last().shape,arbitraryMirrorBounds,false,false);
    double amx0=0,amy0=0,amz0=0,amx1=0,amy1=0,amz1=0;
    arbitraryMirrorBounds.Get(amx0,amy0,amz0,amx1,amy1,amz1);
    closeTo(amx0,-2);closeTo(amx1,0);closeTo(amy0,-2);closeTo(amy1,0);
    require(advancedTransformDoc.undo()&&advancedTransformDoc.parts().size()==1,"Arbitrary-plane mirror undo failed");
    require(!advancedTransformDoc.rotateAboutAxis(advancedTransformId,0,0,0,0,0,0,90,&error),"Zero rotation axis accepted");
    require(advancedTransformDoc.rotateAboutAxis(advancedTransformId,1,1,0,0,0,1,90,&error),"Custom-axis rotation failed");
    closeTo(volume(advancedTransformDoc.find(advancedTransformId)->shape),8);
    require(advancedTransformDoc.undo(),"Custom-axis rotation undo failed");

    cad::SceneDocument rectangularPatternDoc;
    require(rectangularPatternDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Rectangular pattern fixture failed");
    const QString rectangularSource=rectangularPatternDoc.parts().first().id;
    require(!rectangularPatternDoc.rectangularPattern(rectangularSource,cad::Axis::X,2,10,cad::Axis::X,2,20,&error),
            "Rectangular pattern accepted a repeated axis");
    require(rectangularPatternDoc.rectangularPattern(rectangularSource,cad::Axis::X,2,10,cad::Axis::Y,3,20,&error),
            "Two-direction rectangular pattern failed");
    require(rectangularPatternDoc.parts().size()==6,"Rectangular pattern instance count mismatch");
    for(const auto& part:rectangularPatternDoc.parts()){validate(part);closeTo(volume(part.shape),24);}
    require(rectangularPatternDoc.undo()&&rectangularPatternDoc.parts().size()==1,"Rectangular pattern was not atomic");
    require(rectangularPatternDoc.redo()&&rectangularPatternDoc.parts().size()==6,"Rectangular pattern redo failed");

    cad::SceneDocument patternDoc;
    require(patternDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Pattern source creation failed");
    const QString patternSource=patternDoc.parts().first().id;
    require(!patternDoc.linearPattern(patternSource,cad::Axis::Y,1,10,&error),"One-instance pattern accepted");
    require(!patternDoc.linearPattern(patternSource,cad::Axis::Y,4,0,&error),"Zero-spacing pattern accepted");
    require(patternDoc.parts().size()==1 && patternDoc.canUndo(),"Invalid pattern changed document unexpectedly");
    require(patternDoc.linearPattern(patternSource,cad::Axis::Y,4,-10,&error),"Linear pattern failed");
    require(patternDoc.parts().size()==4,"Linear pattern instance count mismatch");
    QSet<QString> patternNames;
    for (const auto& part : patternDoc.parts()) {
        validate(part);
        closeTo(volume(part.shape),24);
        require(!patternNames.contains(part.name.toCaseFolded()),"Pattern produced duplicate names");
        patternNames.insert(part.name.toCaseFolded());
    }
    require(patternDoc.undo() && patternDoc.parts().size()==1,"Pattern should undo as one operation");
    require(patternDoc.redo() && patternDoc.parts().size()==4,"Pattern redo failed");

    // Circular patterns include the source as instance one and commit as one undo step.
    cad::SceneDocument circularDoc;
    require(circularDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Circular pattern source creation failed");
    const QString circularSource=circularDoc.parts().first().id;
    require(!circularDoc.circularPattern(circularSource,cad::Axis::Z,0,0,1,360,&error),"One-instance circular pattern accepted");
    require(!circularDoc.circularPattern(circularSource,cad::Axis::Z,0,0,3,0,&error),"Zero-angle circular pattern accepted");
    require(circularDoc.parts().size()==1,"Invalid circular pattern changed the document");
    require(circularDoc.circularPattern(circularSource,cad::Axis::Z,0,0,4,360,&error),"Full circular pattern failed");
    require(circularDoc.parts().size()==4,"Circular pattern instance count mismatch");
    QSet<QString> circularNames;
    for (const auto& part : circularDoc.parts()) {
        validate(part);
        closeTo(volume(part.shape),24);
        require(!circularNames.contains(part.name.toCaseFolded()),"Circular pattern produced duplicate names");
        circularNames.insert(part.name.toCaseFolded());
    }
    require(circularDoc.undo() && circularDoc.parts().size()==1,"Circular pattern undo failed");
    require(circularDoc.redo() && circularDoc.parts().size()==4,"Circular pattern redo failed");
    cad::SceneDocument partialCircularDoc;
    require(partialCircularDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Partial circular source creation failed");
    require(partialCircularDoc.circularPattern(partialCircularDoc.parts().first().id,cad::Axis::Y,0,0,3,180,&error),"Partial circular pattern failed");
    require(partialCircularDoc.parts().size()==3,"Partial circular pattern instance count mismatch");

    // Vertical blind and through-all holes preview without mutation, then commit and undo atomically.
    cad::SceneDocument holeDoc;
    require(holeDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Hole source creation failed");
    const QString holeSource=holeDoc.parts().first().id;
    cad::HoleCommand blindHoleCommand;
    require(holeDoc.prepareHole(holeSource,10,10,20,4,5,false,&blindHoleCommand,&error),"Blind hole preparation failed");
    const double originalHoleVolume=volume(holeDoc.parts().first().shape);
    const auto blindHole=cad::SceneDocument::buildHolePreview(blindHoleCommand);
    require(!blindHole.shape.IsNull() && BRepCheck_Analyzer(blindHole.shape).IsValid(),"Blind hole preview invalid");
    closeTo(originalHoleVolume-volume(blindHole.shape),20*std::numbers::pi,1e-4);
    closeTo(volume(holeDoc.parts().first().shape),originalHoleVolume);
    require(holeDoc.commitHole(blindHoleCommand,blindHole.shape,&error),"Blind hole commit failed");
    require(holeDoc.undo() && std::abs(volume(holeDoc.parts().first().shape)-originalHoleVolume)<1e-5,"Blind hole undo failed");
    cad::HoleCommand throughHoleCommand;
    require(holeDoc.prepareHole(holeSource,10,10,20,4,1,true,&throughHoleCommand,&error),"Through-hole preparation failed");
    const auto throughHole=cad::SceneDocument::buildHolePreview(throughHoleCommand);
    require(!throughHole.shape.IsNull() && BRepCheck_Analyzer(throughHole.shape).IsValid(),"Through-hole preview invalid");
    closeTo(originalHoleVolume-volume(throughHole.shape),80*std::numbers::pi,1e-4);
    cad::HoleCommand missedHoleCommand;
    require(holeDoc.prepareHole(holeSource,100,100,20,4,5,false,&missedHoleCommand,&error),"Missed hole preparation rejected before preview");
    require(cad::SceneDocument::buildHolePreview(missedHoleCommand).shape.IsNull(),"Hole outside the body produced a preview");
    cad::HoleCommand staleHoleCommand;
    require(holeDoc.prepareHole(holeSource,10,10,20,4,5,false,&staleHoleCommand,&error),"Stale hole preparation failed");
    const auto staleHole=cad::SceneDocument::buildHolePreview(staleHoleCommand);
    require(!staleHole.shape.IsNull(),"Stale hole fixture preview failed");
    require(holeDoc.translate(holeSource,1,0,0,&error),"Stale hole mutation setup failed");
    require(!holeDoc.commitHole(staleHoleCommand,staleHole.shape,&error),"Stale hole preview committed");

    cad::SceneDocument screwHoleDoc;
    require(screwHoleDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Screw-hole fixture failed");
    const QString screwHoleTarget=screwHoleDoc.parts().first().id;
    cad::HoleCommand counterboreCommand;
    require(!screwHoleDoc.prepareCounterbore(screwHoleTarget,10,10,20,4,1,false,8,2,&counterboreCommand,&error),
            "Counterbore deeper than its blind drill accepted");
    require(screwHoleDoc.prepareCounterbore(screwHoleTarget,10,10,20,4,5,false,8,2,&counterboreCommand,&error),
            "Counterbore preparation failed");
    const auto counterborePreview=cad::SceneDocument::buildHolePreview(counterboreCommand);
    require(!counterborePreview.shape.IsNull()&&BRepCheck_Analyzer(counterborePreview.shape).IsValid(),"Counterbore preview invalid");
    closeTo(8000-volume(counterborePreview.shape),44*std::numbers::pi,1e-4);
    require(screwHoleDoc.commitHole(counterboreCommand,counterborePreview.shape,&error),"Counterbore commit failed");
    require(screwHoleDoc.undo(),"Counterbore undo failed");
    cad::HoleCommand countersinkCommand;
    require(!screwHoleDoc.prepareCountersink(screwHoleTarget,10,10,20,4,5,false,3,90,&countersinkCommand,&error),
            "Countersink head smaller than drill accepted");
    require(screwHoleDoc.prepareCountersink(screwHoleTarget,10,10,20,4,5,false,8,90,&countersinkCommand,&error),
            "Countersink preparation failed");
    const auto countersinkPreview=cad::SceneDocument::buildHolePreview(countersinkCommand);
    require(!countersinkPreview.shape.IsNull()&&BRepCheck_Analyzer(countersinkPreview.shape).IsValid(),"Countersink preview invalid");
    closeTo(8000-volume(countersinkPreview.shape),92*std::numbers::pi/3.0,1e-4);

    cad::SceneDocument holeGridDoc;
    require(holeGridDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Hole-grid fixture failed");
    const QString holeGridTarget=holeGridDoc.parts().first().id;
    require(!holeGridDoc.holePattern(holeGridTarget,19,6,10,2,10,true,2,4,2,4,&error),
            "Hole grid accepted a cutter outside the body");
    require(holeGridDoc.parts().size()==1&&std::abs(volume(holeGridDoc.parts().first().shape)-4000)<1e-5,
            "Failed hole grid partially modified the target");
    require(holeGridDoc.holePattern(holeGridTarget,6,6,10,2,10,true,3,4,2,4,&error),"Six-hole grid failed");
    closeTo(4000-volume(holeGridDoc.parts().first().shape),60*std::numbers::pi,1e-4);
    require(holeGridDoc.undo()&&std::abs(volume(holeGridDoc.parts().first().shape)-4000)<1e-5,"Hole-grid undo failed");

    // Split body replaces one source with two valid pieces in one undoable operation.
    cad::SceneDocument splitDoc;
    require(splitDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Split source creation failed");
    const QString splitSource=splitDoc.parts().first().id;
    require(!splitDoc.splitBody(splitSource,cad::Axis::X,0,&error),"Outside split plane accepted");
    require(splitDoc.parts().size()==1,"Rejected split changed the document");
    require(splitDoc.splitBody(splitSource,cad::Axis::X,5,&error),"Split body operation failed");
    require(splitDoc.parts().size()==2,"Split body did not produce two pieces");
    require(splitDoc.parts().first().id==splitSource && splitDoc.parts().last().id!=splitSource,"Split body identity handling failed");
    for (const auto& part:splitDoc.parts()) { validate(part); closeTo(volume(part.shape),3000); }
    require(splitDoc.undo() && splitDoc.parts().size()==1,"Split body undo failed");
    require(splitDoc.redo() && splitDoc.parts().size()==2,"Split body redo failed");

    // Shell removes a planar opening face and hollows the body inward.
    cad::SceneDocument shellDoc;
    require(shellDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Shell source creation failed");
    const QString shellSource=shellDoc.parts().first().id;
    require(!shellDoc.shellBody(shellSource,cad::Axis::Z,true,0,&error),"Zero wall thickness accepted");
    require(shellDoc.shellBody(shellSource,cad::Axis::Z,true,2,&error),"Shell operation failed");
    validate(shellDoc.parts().first());
    closeTo(volume(shellDoc.parts().first().shape),3392,1e-3);
    require(shellDoc.undo() && std::abs(volume(shellDoc.parts().first().shape)-8000)<1e-5,"Shell undo failed");
    cad::SceneDocument nonPlanarShellDoc;
    require(nonPlanarShellDoc.addPrimitive(cad::Primitive::Sphere,{10},&error),"Nonplanar shell fixture failed");
    require(!nonPlanarShellDoc.shellBody(nonPlanarShellDoc.parts().first().id,cad::Axis::Z,true,1,&error),"Shell found a nonexistent planar opening face");

    // New profile-driven solids: regular prism, capsule, ellipsoid, tube and wedge.
    cad::SceneDocument advancedPrimitives;
    require(!advancedPrimitives.addPrimitive(cad::Primitive::RegularPrism,{5,10,2},&error),
            "Regular prism accepted fewer than three sides");
    require(!advancedPrimitives.addPrimitive(cad::Primitive::RegularPrism,{5,10,5.5},&error),
            "Regular prism accepted fractional side count");
    require(advancedPrimitives.addPrimitive(cad::Primitive::RegularPrism,{5,10,6},&error),
            "Hexagonal prism creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),375*std::sqrt(3.0),1e-4);
    require(advancedPrimitives.addPrimitive(cad::Primitive::Capsule,{3,10},&error),
            "Capsule creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),90*std::numbers::pi+36*std::numbers::pi,1e-4);
    require(advancedPrimitives.addPrimitive(cad::Primitive::Ellipsoid,{3,4,5},&error),
            "Ellipsoid creation failed");
    validate(advancedPrimitives.parts().last());
    Bnd_Box ellipsoidBounds;
    BRepBndLib::AddOptimal(advancedPrimitives.parts().last().shape,ellipsoidBounds,false,false);
    double ex0=0,ey0=0,ez0=0,ex1=0,ey1=0,ez1=0;
    ellipsoidBounds.Get(ex0,ey0,ez0,ex1,ey1,ez1);
    // OCCT represents a general triaxial ellipsoid as a transformed spline surface;
    // its bounds and integrated volume stay within 0.5% of the analytic dimensions.
    closeTo(ex1-ex0,6,5e-3); closeTo(ey1-ey0,8,5e-3); closeTo(ez1-ez0,10,1e-5);
    closeTo(volume(advancedPrimitives.parts().last().shape),80*std::numbers::pi,5e-4);
    require(!advancedPrimitives.addPrimitive(cad::Primitive::Tube,{5,5,10},&error),
            "Tube accepted equal inner and outer radii");
    require(advancedPrimitives.addPrimitive(cad::Primitive::Tube,{5,2,10},&error),
            "Tube creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),210*std::numbers::pi,1e-4);
    require(!advancedPrimitives.addPrimitive(cad::Primitive::Wedge,{10,10,10,11},&error),
            "Wedge accepted a top wider than its base");
    require(advancedPrimitives.addPrimitive(cad::Primitive::Wedge,{10,8,6,4},&error),
            "Wedge creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),336,1e-4);
    require(advancedPrimitives.parts().size()==5,"Advanced primitive operation count mismatch");
    require(advancedPrimitives.undo() && advancedPrimitives.parts().size()==4,
            "Wedge creation was not undoable");
    require(advancedPrimitives.redo() && advancedPrimitives.parts().size()==5,
            "Wedge redo failed");
    require(!advancedPrimitives.addPrimitive(cad::Primitive::PyramidFrustum,{30,20,31,10,25},&error),
            "Pyramid frustum accepted an oversized top");
    require(advancedPrimitives.addPrimitive(cad::Primitive::PyramidFrustum,{30,20,12,10,25},&error),
            "Pyramid frustum creation failed");
    validate(advancedPrimitives.parts().last());
    // Each rectangular side tapers linearly; integrate the interpolated width*depth.
    closeTo(volume(advancedPrimitives.parts().last().shape),8250,1e-4);
    require(!advancedPrimitives.addPrimitive(cad::Primitive::HexNut,{10,9,8},&error),
            "Hex nut accepted a bore that breaks through a flat");
    require(advancedPrimitives.addPrimitive(cad::Primitive::HexNut,{10,4,8},&error),
            "Hex nut creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),(150*std::sqrt(3.0)-16*std::numbers::pi)*8,1e-4);
    require(advancedPrimitives.parts().size()==7,"New engineering solid count mismatch");

    cad::SceneDocument orientedPrimitiveDoc;
    require(!orientedPrimitiveDoc.addOrientedCylinder(2,10,1,2,3,0,0,0,&error),"Oriented cylinder accepted a zero axis");
    require(orientedPrimitiveDoc.addOrientedCylinder(2,10,1,2,3,1,0,0,&error),"Oriented cylinder creation failed");
    validate(orientedPrimitiveDoc.parts().last());closeTo(volume(orientedPrimitiveDoc.parts().last().shape),40*std::numbers::pi);
    Bnd_Box orientedCylinderBounds;BRepBndLib::AddOptimal(orientedPrimitiveDoc.parts().last().shape,orientedCylinderBounds,false,false);
    double ocx0=0,ocy0=0,ocz0=0,ocx1=0,ocy1=0,ocz1=0;orientedCylinderBounds.Get(ocx0,ocy0,ocz0,ocx1,ocy1,ocz1);
    closeTo(ocx0,1);closeTo(ocx1,11);closeTo(ocy0,0);closeTo(ocy1,4);closeTo(ocz0,1);closeTo(ocz1,5);
    require(orientedPrimitiveDoc.addOrientedCone(3,1,6,0,0,0,0,0,1,&error),"Oriented cone creation failed");
    validate(orientedPrimitiveDoc.parts().last());closeTo(volume(orientedPrimitiveDoc.parts().last().shape),26*std::numbers::pi);
    require(orientedPrimitiveDoc.addTorusSegment(10,2,90,&error),"Torus segment creation failed");
    validate(orientedPrimitiveDoc.parts().last());closeTo(volume(orientedPrimitiveDoc.parts().last().shape),20*std::numbers::pi*std::numbers::pi,1e-4);
    require(!orientedPrimitiveDoc.addTorusSegment(10,2,361,&error),"Torus sweep above 360 degrees accepted");

    // Additional closed sketch profiles extrude into solids with analytic volumes.
    const double profileHeight=12.0;
    cad::ExtrusionCommand ellipseCommand;
    require(!advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ellipse,0,0,20,25,profileHeight,
                                                 &ellipseCommand,&error),"Ellipse accepted minor diameter larger than major");
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ellipse,1,2,20,10,profileHeight,
                                                &ellipseCommand,&error),"Ellipse extrusion preparation failed");
    const auto ellipsePreview=cad::SceneDocument::buildExtrusionPreview(ellipseCommand);
    require(!ellipsePreview.shape.IsNull() && BRepCheck_Analyzer(ellipsePreview.shape).IsValid(),"Ellipse extrusion invalid");
    closeTo(volume(ellipsePreview.shape),600*std::numbers::pi,1e-4);
    require(advancedPrimitives.commitExtrusion(ellipseCommand,ellipsePreview.shape,&error),"Ellipse extrusion commit failed");

    cad::ExtrusionCommand slotCommand;
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Slot,0,0,30,10,profileHeight,
                                                 &slotCommand,&error),"Slot extrusion preparation failed");
    const auto slotPreview=cad::SceneDocument::buildExtrusionPreview(slotCommand);
    require(!slotPreview.shape.IsNull() && BRepCheck_Analyzer(slotPreview.shape).IsValid(),"Slot extrusion invalid");
    closeTo(volume(slotPreview.shape),(200+25*std::numbers::pi)*profileHeight,1e-4);

    cad::ExtrusionCommand triangleCommand;
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Triangle,0,0,20,0,profileHeight,
                                                &triangleCommand,&error),"Triangle extrusion preparation failed");
    const auto trianglePreview=cad::SceneDocument::buildExtrusionPreview(triangleCommand);
    require(!trianglePreview.shape.IsNull() && BRepCheck_Analyzer(trianglePreview.shape).IsValid(),"Triangle extrusion invalid");
    closeTo(volume(trianglePreview.shape),900*std::sqrt(3.0),1e-4);

    cad::ExtrusionCommand polygonCommand;
    require(!advancedPrimitives.prepareExtrusion(cad::SketchProfile::RegularPolygon,0,0,20,2.5,profileHeight,
                                                 &polygonCommand,&error),"Polygon accepted fractional side count");
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::RegularPolygon,0,0,20,8,profileHeight,
                                                 &polygonCommand,&error),"Octagon extrusion preparation failed");
    const auto polygonPreview=cad::SceneDocument::buildExtrusionPreview(polygonCommand);
    require(!polygonPreview.shape.IsNull() && BRepCheck_Analyzer(polygonPreview.shape).IsValid(),"Regular polygon extrusion invalid");
    closeTo(volume(polygonPreview.shape),2400*std::sqrt(2.0),1e-4);

    cad::ExtrusionCommand symmetricCommand;
    require(advancedPrimitives.prepareSymmetricExtrusion(cad::SketchProfile::Rectangle,0,0,10,6,8,
        cad::ExtrusionMode::NewBody,QString(),&symmetricCommand,&error),"Symmetric extrusion preparation failed");
    const auto symmetricPreview=cad::SceneDocument::buildExtrusionPreview(symmetricCommand);
    require(!symmetricPreview.shape.IsNull()&&BRepCheck_Analyzer(symmetricPreview.shape).IsValid(),"Symmetric extrusion invalid");
    closeTo(volume(symmetricPreview.shape),480);
    Bnd_Box symmetricBounds;BRepBndLib::AddOptimal(symmetricPreview.shape,symmetricBounds,false,false);
    double sx0=0,sy0=0,sz0=0,sx1=0,sy1=0,sz1=0;symmetricBounds.Get(sx0,sy0,sz0,sx1,sy1,sz1);
    closeTo(sz0,-4);closeTo(sz1,4);
    require(advancedPrimitives.commitExtrusion(symmetricCommand,symmetricPreview.shape,&error),"Symmetric extrusion commit failed");
    require(advancedPrimitives.undo(),"Symmetric extrusion undo failed");

    cad::ExtrusionCommand taperedCommand;
    require(!advancedPrimitives.prepareTaperedExtrusion(cad::SketchProfile::Rectangle,0,0,10,6,20,45,
        cad::ExtrusionMode::NewBody,QString(),&taperedCommand,&error),"Collapsing taper accepted");
    require(advancedPrimitives.prepareTaperedExtrusion(cad::SketchProfile::Rectangle,0,0,10,8,5,10,
        cad::ExtrusionMode::NewBody,QString(),&taperedCommand,&error),"Tapered extrusion preparation failed");
    const auto taperedPreview=cad::SceneDocument::buildExtrusionPreview(taperedCommand);
    require(!taperedPreview.shape.IsNull()&&BRepCheck_Analyzer(taperedPreview.shape).IsValid(),"Tapered extrusion invalid");
    const double tw=10-10*std::tan(std::numbers::pi/18.0),td=8-10*std::tan(std::numbers::pi/18.0);
    const double expectedTaperVolume=5.0/6.0*(2*80+10*td+tw*8+2*tw*td);
    closeTo(volume(taperedPreview.shape),expectedTaperVolume,1e-4);

    cad::ExtrusionCommand roundedCommand;
    require(!advancedPrimitives.prepareRoundedRectangleExtrusion(0,0,20,10,5,5,cad::ExtrusionMode::NewBody,QString(),&roundedCommand,&error),
            "Rounded rectangle accepted a corner radius too large for the profile");
    require(advancedPrimitives.prepareRoundedRectangleExtrusion(0,0,20,10,2,5,cad::ExtrusionMode::NewBody,QString(),&roundedCommand,&error),
            "Rounded rectangle extrusion preparation failed");
    const auto roundedPreview=cad::SceneDocument::buildExtrusionPreview(roundedCommand);
    require(!roundedPreview.shape.IsNull()&&BRepCheck_Analyzer(roundedPreview.shape).IsValid(),"Rounded rectangle extrusion invalid");
    closeTo(volume(roundedPreview.shape),(200-(4-std::numbers::pi)*4)*5,1e-4);

    cad::ExtrusionCommand ringCommand;
    require(!advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ring,0,0,20,20,4,&ringCommand,&error),
            "Ring accepted equal inner and outer diameters");
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ring,0,0,20,10,4,&ringCommand,&error),
            "Ring extrusion preparation failed");
    const auto ringPreview=cad::SceneDocument::buildExtrusionPreview(ringCommand);
    require(!ringPreview.shape.IsNull()&&BRepCheck_Analyzer(ringPreview.shape).IsValid(),"Ring extrusion invalid");
    closeTo(volume(ringPreview.shape),300*std::numbers::pi,1e-4);

    cad::SceneDocument loftDoc;
    require(!loftDoc.loftRectangles(0,0,10,8,1,1,6,4,0,&error),"Zero-height loft accepted");
    require(loftDoc.loftRectangles(0,0,10,8,2,2,6,4,20,&error),"Rectangular-section loft failed");
    require(loftDoc.parts().size()==1,"Loft did not create one body");
    validate(loftDoc.parts().first());
    closeTo(volume(loftDoc.parts().first().shape),20.0/6.0*(2*80+10*4+6*8+2*24),1e-4);
    require(loftDoc.undo()&&loftDoc.parts().isEmpty(),"Loft undo failed");

    // v1.3 adds thirty engineering components. Exercise every default build,
    // topology/volume guard, scene transaction, and undo path.
    for(int index=0;index<static_cast<int>(cad::EngineeringPart::Count);++index){
        const auto kind=static_cast<cad::EngineeringPart>(index);
        const auto spec=cad::engineeringPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Engineering component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addEngineeringPart(kind,invalidDimensions,&error),"Engineering component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid engineering component mutated its document");
        const bool componentCreated=componentDoc.addEngineeringPart(kind,spec.defaults,&error);
        const std::string componentError="Engineering component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Engineering component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Engineering component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Engineering component undo failed");
    }

    // v1.4 adds thirty advanced production and machine components. Validate every
    // default model, malformed dimension handling, a single document commit, and undo.
    for(int index=0;index<static_cast<int>(cad::AdvancedPart::Count);++index){
        const auto kind=static_cast<cad::AdvancedPart>(index);
        const auto spec=cad::advancedPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Advanced component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addAdvancedPart(kind,invalidDimensions,&error),"Advanced component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid advanced component mutated its document");
        const bool componentCreated=componentDoc.addAdvancedPart(kind,spec.defaults,&error);
        const std::string componentError="Advanced component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Advanced component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Advanced component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Advanced component undo failed");
    }

    // v1.5 adds thirty-five specialized drivetrain, bearing, actuator, engine,
    // coupling and piping models; exercise defaults, rejection, validity and undo.
    for(int index=0;index<static_cast<int>(cad::SpecializedPart::Count);++index){
        const auto kind=static_cast<cad::SpecializedPart>(index);
        const auto spec=cad::specializedPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Specialized component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addSpecializedPart(kind,invalidDimensions,&error),"Specialized component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid specialized component mutated its document");
        const bool componentCreated=componentDoc.addSpecializedPart(kind,spec.defaults,&error);
        const std::string componentError="Specialized component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Specialized component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Specialized component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Specialized component undo failed");
    }

    // v1.6 adds seventy industrial components. Exercise all default builds,
    // malformed dimensions, topology/volume guards and transactional undo.
    for(int index=0;index<static_cast<int>(cad::IndustrialPart::Count);++index){
        const auto kind=static_cast<cad::IndustrialPart>(index);
        const auto spec=cad::industrialPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Industrial component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addIndustrialPart(kind,invalidDimensions,&error),"Industrial component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid industrial component mutated its document");
        const bool componentCreated=componentDoc.addIndustrialPart(kind,spec.defaults,&error);
        const std::string componentError="Industrial component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Industrial component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Industrial component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Industrial component undo failed");
    }

    // v1.7 contributes seventy more extended component templates. Check every
    // default build, zero-dimension rejection, valid topology/volume and undo.
    for(int index=0;index<static_cast<int>(cad::ExtendedPart::Count);++index){
        const auto kind=static_cast<cad::ExtendedPart>(index);
        const auto spec=cad::extendedPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Extended component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addExtendedPart(kind,invalidDimensions,&error),"Extended component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid extended component mutated its document");
        const bool componentCreated=componentDoc.addExtendedPart(kind,spec.defaults,&error);
        const std::string componentError="Extended component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Extended component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Extended component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Extended component undo failed");
    }

    // v1.8 adds seventy precision and utility component templates. Exercise every
    // default, zero-dimension rejection, valid topology/volume, and undo.
    for(int index=0;index<static_cast<int>(cad::PrecisionPart::Count);++index){
        const auto kind=static_cast<cad::PrecisionPart>(index);
        const auto spec=cad::precisionPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Precision component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addPrecisionPart(kind,invalidDimensions,&error),"Precision component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid precision component mutated its document");
        const bool componentCreated=componentDoc.addPrecisionPart(kind,spec.defaults,&error);
        const std::string componentError="Precision component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Precision component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Precision component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Precision component undo failed");
    }

    // v1.9 adds one hundred production-component templates. Exercise every default,
    // zero-dimension rejection, valid topology/volume and undo.
    for(int index=0;index<static_cast<int>(cad::ProductionPart::Count);++index){
        const auto kind=static_cast<cad::ProductionPart>(index);
        const auto spec=cad::productionPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Production component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addProductionPart(kind,invalidDimensions,&error),"Production component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid production component mutated its document");
        const bool componentCreated=componentDoc.addProductionPart(kind,spec.defaults,&error);
        const std::string componentError="Production component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Production component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Production component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Production component undo failed");
    }

    const QString stl=dir.filePath("solid.stl"),step=dir.filePath("solid.step");
    require(booleanDoc.exportSTEP(step,&error) && QFileInfo(step).size()>100,"STEP export failed");
    require(booleanDoc.exportSTL(stl,&error) && QFileInfo(stl).size()>84,"STL export failed");
    require(!cad::SceneDocument{}.exportSTL(stl,&error),"Empty STL export accepted");
    std::cout << "PASS: " << checks << " C++ assertions; primitives, 30 v1.3 engineering components, 30 v1.4 advanced components, 35 v1.5 specialized components, 70 v1.6 industrial components, 70 v1.7 extended components, 70 v1.8 precision components, 100 v1.9 production components, transforms, duplicates, metadata, "
              << "undo branches, atomic batch deletion, v1 migration, v2 save/load, "
              << "Boolean validity, fillet/chamfer transactions, profile extrusion/revolve preview and staleness, "
              << "mirror, linear/circular patterns, hole previews, split body, and shell with undo, "
              << "corrupt rollback, STEP/STL output\n";
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string(argv[1])=="--wing-rib") { testWingRib(); std::cout<<"PASS: "<<checks<<" Wing rib assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--modeling-tools") { testModelingTools(); std::cout<<"PASS: "<<checks<<" assertions across "<<static_cast<int>(cad::ModelingTool::Count)<<" modeling tools\n"; return 0; }
        execute(); return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1; }
}
