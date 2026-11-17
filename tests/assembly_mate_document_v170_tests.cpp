#include "core/SceneDocument.h"
#include "core/PlanarMateSelection.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <stdexcept>
#include <cmath>
#include <iostream>
namespace {
int checks=0;
void must(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
void run(){
    cad::SceneDocument doc;QString issue;
    must(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&issue),"parent box failed");
    const QString parent=doc.parts().last().id;
    must(doc.makeComponent(parent,"Parent component",&issue),"parent component failed");
    must(doc.addPrimitive(cad::Primitive::Box,{5,5,5},&issue),"child box failed");
    const QString child=doc.parts().last().id;
    must(doc.makeComponent(child,"Child component",&issue),"child component failed");
    using namespace cad::mates;
    // F05: real B-rep planar observation extraction is read-only. A cylinder
    // must NOT be silently treated as a plane, and a single face is not a
    // complete mate (the GUI obtains explicit locks for the remaining DOFs).
    TopoDS_Face sampleFace;
    for(TopExp_Explorer it(doc.find(parent)->shape,TopAbs_FACE);it.More();it.Next()){
        sampleFace=TopoDS::Face(it.Current());break;
    }
    PlanarFaceObservation measured;
    must(describeSelectedPlanarFace(sampleFace,&measured,&issue),"planar mate face observation");
    must(std::abs(measured.normal.x*measured.worldGuide.x+
        measured.normal.y*measured.worldGuide.y+
        measured.normal.z*measured.worldGuide.z)<1e-8,"guide must be tangent");
    const auto cylinder=BRepPrimAPI_MakeCylinder(3.0,10.0).Shape();
    TopoDS_Face curved;
    for(TopExp_Explorer it(cylinder,TopAbs_FACE);it.More();it.Next()){
        const TopoDS_Face candidate=TopoDS::Face(it.Current());
        if(BRepAdaptor_Surface(candidate).GetType()==GeomAbs_Cylinder){curved=candidate;break;}
    }
    const auto previouslyMeasured=measured;
    must(!describeSelectedPlanarFace(curved,&measured,&issue),
         "cylinder must not be misidentified as a planar mate");
    must(measured.center.x==previouslyMeasured.center.x&&
         measured.normal.z==previouslyMeasured.normal.z,
         "failed face observation changed output");
    // F05: real cylindrical B-rep observation prefill, not synthetic axes.
    CylindricalFaceObservation boreObservation;
    must(describeSelectedCylindricalFace(curved,&boreObservation,&issue),
         "cylindrical mate observation must accept a trimmed cylinder");
    must(std::abs(boreObservation.radiusMm-3.0)<1e-7,
         "cylindrical mate observation lost B-rep radius");
    const auto dot=[&](Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    must(std::abs(dot(boreObservation.axisDirection,boreObservation.worldGuide))<1e-8,
         "cylinder guide must be perpendicular to physical axis");
    must(std::abs(dot(boreObservation.axisDirection,boreObservation.axisDirection)-1.0)<1e-8,
         "cylindrical mate axis must be unit length");
    const auto keptBore=boreObservation;
    must(!describeSelectedCylindricalFace(sampleFace,&boreObservation,&issue),
         "plane must not be misidentified as a cylindrical mating surface");
    must(boreObservation.radiusMm==keptBore.radiusMm&&
         boreObservation.axisDirection.z==keptBore.axisDirection.z,
         "failed cylindrical observation changed existing measurement");
    std::vector<Constraint> constraints;
    for(Vec3 p:{Vec3{0,0,0},Vec3{10,0,0},Vec3{0,10,0}})
        constraints.push_back({Kind::Point,{p.x+20,p.y,p.z},p,{},{}});
    must(doc.solveAssemblyMates(parent,child,constraints,true,&issue),"frame mate placement failed");
    const auto* moved=doc.find(child);
    must(moved->assemblyJointType=="Rigid"&&moved->assemblyJointParentBodyId==parent,"mate joint not persisted");
    must(std::abs(moved->placementX-20)<1e-4,"mate translation incorrect");
    Bnd_Box bounds;BRepBndLib::AddOptimal(moved->shape,bounds,false,false);
    double xmin,ymin,zmin,xmax,ymax,zmax;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    must(std::abs(xmin-20)<1e-4&&std::abs(xmax-25)<1e-4,"world shape does not match placement");
    QTemporaryDir tmp;must(tmp.isValid(),"temp dir failed");
    const QString file=tmp.path()+"/mate.cadscene";
    must(doc.saveAs(file,&issue),"save of rigid mate failed");
    cad::SceneDocument reopened;must(reopened.load(file,&issue),"mate reopen failed");
    must(std::abs(reopened.find(child)->placementX-20)<1e-4,"mate placement not saved");
    must(reopened.find(child)->assemblyJointParentBodyId==parent,"mate hierarchy lost");
    // The expanded GUI's point + two oriented axes mode must replay the pose
    // through the same document transaction and save/restore path.
    must(doc.addPrimitive(cad::Primitive::Box,{3,3,3},&issue),"second child box failed");
    const QString second=doc.parts().last().id;
    must(doc.makeComponent(second,"Second child component",&issue),"second child component failed");
    std::vector<Constraint> axes={
        {Kind::Point,{30,0,0},{0,0,0},{},{}},
        {Kind::CoaxialAxis,{30,0,0},{0,0,0},{1,0,0},{1,0,0}},
        {Kind::CoaxialAxis,{30,0,0},{0,0,0},{0,1,0},{0,1,0}}};
    must(doc.solveAssemblyMates(parent,second,axes,true,&issue),"two-axis mate document failed");
    must(std::abs(doc.find(second)->placementX-30)<1e-4,"two-axis mate placement wrong");
    must(doc.saveAs(file,&issue),"save two-axis mate document failed");
    cad::SceneDocument secondOpened;
    must(secondOpened.load(file,&issue),"reopen two-axis mate document failed");
    must(std::abs(secondOpened.find(second)->placementX-30)<1e-4,"two-axis save/load lost placement");
    must(doc.undo(),"undo two-axis mate failed");
    must(doc.find(second)->assemblyJointType.isEmpty(),"two-axis undo retained rigid joint");
    must(doc.undo(),"undo second-child component setup failed");
    must(doc.undo(),"undo second-child primitive setup failed");
    must(doc.undo(),"mate undo failed");
    must(doc.find(child)->assemblyJointType.isEmpty(),"mate undo still attached");

    // Local-frame definitions survive round trips and can be re-solved.
    cad::SceneDocument live;
    must(live.addPrimitive(cad::Primitive::Box,{10,10,10},&issue),"live fixed box");
    const QString fixed=live.parts().last().id;
    must(live.makeComponent(fixed,"Live fixed",&issue),"live fixed component");
    must(live.addPrimitive(cad::Primitive::Box,{3,3,3},&issue),"live free box");
    const QString free=live.parts().last().id;
    must(live.makeComponent(free,"Live free",&issue),"live free component");
    cad::SceneDocument::AssemblyNetworkMate linked;
    linked.parentBodyId=fixed;linked.childBodyId=free;
    for(Vec3 p:{Vec3{0,0,0},Vec3{10,0,0},Vec3{0,10,0}})
        linked.constraints.push_back({Kind::Point,{p.x+18,p.y,p.z},p,{},{}});
    must(live.setLiveRigidMateNetwork(fixed,{linked},&issue),"live rigid network authoring");
    must(live.hasLiveRigidMateNetwork(),"live network not retained");
    must(live.liveRigidMateDefinitions().size()==1,"incorrect stored mate count");
    must(std::abs(live.find(free)->placementX-18)<1e-4,"live mate initial placement");
    const auto token=live.find(free)->geometryToken;
    must(live.refreshLiveRigidMateNetwork(&issue),"refresh of solved live mates");
    must(live.find(free)->geometryToken==token,"no-op refresh invalidated geometry");
    const QString liveFile=tmp.path()+"/live_mates.cadscene";
    must(live.saveAs(liveFile,&issue),"save live mate definitions");
    cad::SceneDocument loadedLive;
    must(loadedLive.load(liveFile,&issue),"reload live mate definitions");
    must(loadedLive.hasLiveRigidMateNetwork(),"live definitions lost after reopen");
    // F05: a signed planar distance is part of the SAVED rigid network schema.
    // Legacy five-field mate observations remain readable with zero offset.
    cad::SceneDocument offsetScene;
    must(offsetScene.addPrimitive(cad::Primitive::Box,{8,8,8},&issue),"offset parent box");
    const QString offsetParent=offsetScene.parts().last().id;
    must(offsetScene.makeComponent(offsetParent,"Offset parent",&issue),"offset parent component");
    must(offsetScene.addPrimitive(cad::Primitive::Box,{3,3,3},&issue),"offset child box");
    const QString offsetChild=offsetScene.parts().last().id;
    must(offsetScene.makeComponent(offsetChild,"Offset child",&issue),"offset child component");
    cad::SceneDocument::AssemblyNetworkMate offsetMate;
    offsetMate.parentBodyId=offsetParent;offsetMate.childBodyId=offsetChild;
    offsetMate.constraints={
        {Kind::Point,{0,6,0},{0,0,0},{},{}},
        {Kind::CoaxialAxis,{0,6,0},{0,0,0},{1,0,0},{1,0,0}},
        {Kind::CoincidentPlane,{0,0,0},{0,0,0},{0,1,0},{0,1,0},6.0}};
    must(offsetScene.setLiveRigidMateNetwork(offsetParent,{offsetMate},&issue),
         "saved signed plane offset network must solve");
    must(std::abs(offsetScene.find(offsetChild)->placementY-6.0)<1e-4,
         "signed plane offset did not place occurrence");
    const QString offsetFile=tmp.path()+"/offset_mates.cadscene";
    must(offsetScene.saveAs(offsetFile,&issue),"save offset mates");
    cad::SceneDocument loadedOffset;
    must(loadedOffset.load(offsetFile,&issue),"reopen signed plane offset mates");
    must(loadedOffset.liveRigidMateDefinitions().size()==1&&
         std::abs(loadedOffset.liveRigidMateDefinitions()[0].constraints[2].offsetMm-6.0)<1e-9,
         "signed plane offset lost in saved scene");
    must(loadedOffset.refreshLiveRigidMateNetwork(&issue),"refresh signed offset from local frames");
    must(std::abs(loadedOffset.find(offsetChild)->placementY-6.0)<1e-4,
         "saved signed offset changed after refresh");
    QVector<cad::SceneDocument::AssemblyNetworkMate> offsetWorld;
    must(loadedOffset.liveRigidMateWorldDefinitions(&offsetWorld,&issue),
         "project signed offset world observations");
    const auto oldOffsetGeometry=loadedOffset.find(offsetChild)->geometryToken;
    offsetWorld[0].constraints[2].offsetMm=7.0;
    must(!loadedOffset.replaceLiveRigidMateConnection(0,offsetWorld[0],&issue),
         "contradictory signed plane offset edit must roll back");
    must(loadedOffset.find(offsetChild)->geometryToken==oldOffsetGeometry,
         "rejected plane offset edit changed geometry");

    must(loadedLive.liveRigidMateFixedBodyId()==fixed,"fixed ID not preserved");
    must(loadedLive.liveRigidMateDefinitions().first().constraints.size()==3,
         "constraint set not preserved");
    must(loadedLive.refreshLiveRigidMateNetwork(&issue),"reloaded live refresh");
    const auto reopenedToken=loadedLive.find(free)->geometryToken;
    must(loadedLive.refreshLiveRigidMateNetwork(&issue),"repeat live refresh");
    must(loadedLive.find(free)->geometryToken==reopenedToken,
         "repeat refresh must preserve B-rep revision");
    must(!loadedLive.erase(fixed,&issue),"deleting constrained fixed component accepted");
    must(loadedLive.removeLiveRigidMateNetwork(),"removing live network failed");
    must(!loadedLive.hasLiveRigidMateNetwork(),"live removal retained definitions");
    must(loadedLive.undo(),"undo mate removal failed");
    must(loadedLive.hasLiveRigidMateNetwork(),"undo did not restore live mates");
    must(loadedLive.redo(),"redo mate removal failed");
    must(!loadedLive.hasLiveRigidMateNetwork(),"redo did not remove live mates");

    // Corrupt a length-three anchor, and prove load does not replace the open scene.
    QFile serialized(liveFile);
    must(serialized.open(QIODevice::ReadOnly),"open live scene JSON");
    const auto data=serialized.readAll();serialized.close();
    QJsonDocument document=QJsonDocument::fromJson(data);
    QJsonObject root=document.object();
    QJsonObject archive=root.value("live_rigid_mates").toObject();
    QJsonArray links=archive.value("connections").toArray();
    QJsonObject first=links[0].toObject();
    QJsonArray observations=first.value("constraints").toArray();
    QJsonObject invalid=observations[0].toObject();
    invalid.insert("parent_point",QJsonArray{2.0,5.0});
    observations.replace(0,invalid);
    first.insert("constraints",observations);links.replace(0,first);
    archive.insert("connections",links);root.insert("live_rigid_mates",archive);
    const QString corrupt=tmp.path()+"/invalid_live.cadscene";
    QFile corruptFile(corrupt);
    must(corruptFile.open(QIODevice::WriteOnly),"create corrupted project");
    must(corruptFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact))>0,
         "write corrupted project");
    corruptFile.close();
    cad::SceneDocument protect;
    must(protect.addPrimitive(cad::Primitive::Sphere,{3},&issue),"protected scene seed");
    const QString intact=protect.parts().last().id;
    must(!protect.load(corrupt,&issue),"malformed stored mate accepted");
    must(protect.parts().size()==1&&protect.find(intact)!=nullptr,
         "failed load mutated open document");
    // A multi-connection saved graph can be extended without overwriting the
    // existing links, and deletion must preserve graph connectivity.
    cad::SceneDocument multi;
    QStringList bodies;
    for(int i=0;i<3;++i){
        must(multi.addPrimitive(cad::Primitive::Box,{5,5,5},&issue),"multi-mate source box");
        const QString id=multi.parts().last().id;
        must(multi.makeComponent(id,QStringLiteral("Multi %1").arg(i),&issue),"multi-mate component");
        bodies.append(id);
    }
    const auto makeTranslationMate=[&](QString a,QString b,double parentX,double childX){
        cad::SceneDocument::AssemblyNetworkMate result;
        result.parentBodyId=a;result.childBodyId=b;
        for(Vec3 p:{Vec3{0,0,0},Vec3{10,0,0},Vec3{0,10,0}})
            result.constraints.push_back({Kind::Point,{p.x+parentX,p.y,p.z},
                                              {p.x+childX,p.y,p.z},{},{}});
        return result;
    };
    must(multi.setLiveRigidMateNetwork(bodies[0],
        {makeTranslationMate(bodies[0],bodies[1],18,0)},&issue),"first saved connection");
    must(multi.appendLiveRigidMateConnection(
        makeTranslationMate(bodies[0],bodies[2],35,0),&issue),"append second saved connection");
    must(multi.liveRigidMateDefinitions().size()==2,"append replaced existing saved connection");
    must(std::abs(multi.find(bodies[1])->placementX-18)<1e-4&&
         std::abs(multi.find(bodies[2])->placementX-35)<1e-4,"multi-link placements wrong");
    must(!multi.removeLiveRigidMateConnection(1,&issue),
         "removing a bridge connection silently orphaned a saved member");
    must(!multi.appendLiveRigidMateConnection(
        makeTranslationMate(bodies[0],bodies[1],18,0),&issue),
         "duplicate saved component pair was accepted");
    const QString closureToken=multi.find(bodies[2])->geometryToken;
    must(!multi.appendLiveRigidMateConnection(
        makeTranslationMate(bodies[1],bodies[2],19,35),&issue),
        "contradictory rigid loop must be rejected");
    must(multi.find(bodies[2])->geometryToken==closureToken&&
         multi.liveRigidMateDefinitions().size()==2,
         "rejected inconsistent loop must not mutate the assembly");
    must(multi.appendLiveRigidMateConnection(
        makeTranslationMate(bodies[1],bodies[2],35,35),&issue),"close consistent rigid loop");
    must(multi.liveRigidMateDefinitions().size()==3,"loop connection not stored");
    // A contradictory third edge is never accepted just to close the graph.
    const QString consistentToken=multi.find(bodies[2])->geometryToken;
    must(!multi.appendLiveRigidMateConnection(
        makeTranslationMate(bodies[1],bodies[2],19,35),&issue),
        "duplicate rigid pair must not be silently reauthored");
    must(multi.find(bodies[2])->geometryToken==consistentToken,
        "rejected rigid mate changed an occurrence");
    must(multi.removeLiveRigidMateConnection(1,&issue),"remove redundant graph connection");
    must(multi.liveRigidMateDefinitions().size()==2,"wrong retained mate count");
    const auto beforeRefresh=multi.find(bodies[2])->geometryToken;
    must(multi.refreshLiveRigidMateNetwork(&issue),"refresh pruned rigid graph");
    must(multi.find(bodies[2])->geometryToken==beforeRefresh,
         "unchanged multi-mate network invalidated geometry");
    must(multi.saveAs(tmp.path()+"/multi-mates.cadscene",&issue),"save multi-link graph");
    cad::SceneDocument reopenedMulti;
    must(reopenedMulti.load(tmp.path()+"/multi-mates.cadscene",&issue),"reopen multi-link graph");
    must(reopenedMulti.liveRigidMateDefinitions().size()==2,"saved multi-link graph lost links");
    // The editable saved mate must preserve its endpoints and ID/index, and
    // update dependent placements as one undoable graph transaction.
    QVector<cad::SceneDocument::AssemblyNetworkMate> beforeEdit;
    must(reopenedMulti.liveRigidMateWorldDefinitions(&beforeEdit,&issue),"mate world projection failed");
    must(beforeEdit.size()==2,"world projection changed connection inventory");
    const QString idOne=bodies[1],idTwo=bodies[2];
    const double originalOne=reopenedMulti.find(idOne)->placementX;
    const double originalTwo=reopenedMulti.find(idTwo)->placementX;
    auto revised=beforeEdit[0];
    for(auto& c:revised.constraints)c.parentPoint.x+=2.0;
    const QString oldToken=reopenedMulti.find(idTwo)->geometryToken;
    QString unchangedPreview;
    must(reopenedMulti.previewLiveRigidMateConnectionEdit(0,beforeEdit[0],
         &unchangedPreview,&issue),"read-only no-change mate preview failed");
    must(unchangedPreview.contains("Predicted moving occurrences: 0 / 2"),
         "preview of stored mate should not propose a movement");
    must(!reopenedMulti.dirty(),"reopening a saved mate scene should start clean");
    must(reopenedMulti.replaceLiveRigidMateConnection(0,beforeEdit[0],&issue),
         "no-op mate editing should succeed");
    must(!reopenedMulti.dirty()&&reopenedMulti.find(idTwo)->geometryToken==oldToken,
         "no-op mate editing changed scene revision or B-rep token");
    QString proposedPreview;
    must(reopenedMulti.previewLiveRigidMateConnectionEdit(0,revised,&proposedPreview,&issue),
         "valid read-only mate preview failed");
    must(proposedPreview.contains("Predicted moving occurrences: 2 / 2")&&
         proposedPreview.contains("READ-ONLY KINEMATIC PREVIEW"),
         "mate preview failed to report dependent motion and limitations");
    must(proposedPreview.contains("GEOMETRY PREFLIGHT")&&
         proposedPreview.contains("NOMINAL FIT PREVIEW: 3 exact pair checks")&&
         proposedPreview.contains("0 interference pairs"),
         "mate preview failed to validate solved B-reps and nominal fit");
    must(!reopenedMulti.dirty()&&reopenedMulti.find(idTwo)->geometryToken==oldToken,
         "read-only preview mutated document state");
    QString marginReport;
    // An 18 mm threshold yields two genuinely undersized gaps in the
    // three-box test fixture. Expose exact pairwise measurements as evidence.
    must(reopenedMulti.previewLiveRigidMateConnectionEdit(0,revised,
         &marginReport,&issue,18.0),"clearance-margin mate preview failed");
    must(marginReport.contains("2 below-required-clearance pairs")&&
         marginReport.contains("required 18 mm")&&
         marginReport.contains("PAIR:"),
         "predicted gap shortfalls were not shown with requested clearance");
    QString invalidMarginReport="must remain unchanged";
    must(!reopenedMulti.previewLiveRigidMateConnectionEdit(0,revised,
         &invalidMarginReport,&issue,-1.0)&&
         invalidMarginReport=="must remain unchanged",
         "negative clearance preview should fail without altering prior report");

    auto wrong=revised;wrong.childBodyId=idTwo;
    must(!reopenedMulti.replaceLiveRigidMateConnection(0,wrong,&issue),
         "editing a mate must not silently retarget its occurrence");
    must(!reopenedMulti.replaceLiveRigidMateConnection(99,revised,&issue),
         "editing an unknown mate index should fail");
    must(reopenedMulti.find(idTwo)->geometryToken==oldToken,
         "rejected mate edit changed geometry");
    must(reopenedMulti.replaceLiveRigidMateConnection(0,revised,&issue),
         "valid mate translation edit rejected");
    must(std::abs(reopenedMulti.find(idOne)->placementX-(originalOne+2.0))<1e-4,
         "edited mate did not move direct child");
    must(std::abs(reopenedMulti.find(idTwo)->placementX-(originalTwo+2.0))<1e-4,
         "edited mate did not move downstream child");
    must(reopenedMulti.liveRigidMateDefinitions().size()==2,
         "mate edit duplicated or deleted connection");
    const QString editToken=reopenedMulti.find(idTwo)->geometryToken;
    must(reopenedMulti.refreshLiveRigidMateNetwork(&issue),"mate refresh after edit failed");
    must(reopenedMulti.find(idTwo)->geometryToken==editToken,
         "edit changed local frame incorrectly; refresh should be no-op");
    must(reopenedMulti.undo(),"undo mate edit failed");
    must(std::abs(reopenedMulti.find(idOne)->placementX-originalOne)<1e-4&&
         std::abs(reopenedMulti.find(idTwo)->placementX-originalTwo)<1e-4,
         "undo mate edit failed to restore all dependent placements");
    must(reopenedMulti.redo(),"redo mate edit failed");
    must(std::abs(reopenedMulti.find(idOne)->placementX-(originalOne+2.0))<1e-4&&
         std::abs(reopenedMulti.find(idTwo)->placementX-(originalTwo+2.0))<1e-4,
         "redo mate edit failed to replay placements");
    const QString editedFile=tmp.path()+"/revised-mates.cadscene";
    must(reopenedMulti.saveAs(editedFile,&issue),"save edited rigid mate network");
    cad::SceneDocument reloadedEdited;
    must(reloadedEdited.load(editedFile,&issue),"reopen edited rigid mate network");
    must(reloadedEdited.liveRigidMateDefinitions().size()==2,
         "edited mate definition lost on reopen");
    must(reloadedEdited.refreshLiveRigidMateNetwork(&issue),
         "saved edited rigid mate cannot refresh");
    must(std::abs(reloadedEdited.find(idTwo)->placementX-(originalTwo+2.0))<1e-4,
         "saved edited mate placement changed after refresh");
    // A changed mate can be kinematically valid while creating solid overlap.
    // The read-only preview must WARN, not mutate the assembly or silently
    // declare continuous collision freedom. Intentional interference is legal.
    cad::SceneDocument collisionScene;
    QStringList collisionIds;
    for(int k=0;k<2;++k){
        must(collisionScene.addPrimitive(cad::Primitive::Box,{5,5,5},&issue),
             "collision preview box");
        const QString id=collisionScene.parts().last().id;
        must(collisionScene.makeComponent(id,QStringLiteral("Overlap %1").arg(k),&issue),
             "collision preview occurrence");
        collisionIds << id;
    }
    must(collisionScene.setLiveRigidMateNetwork(collisionIds[0],{
         makeTranslationMate(collisionIds[0],collisionIds[1],10,0)},&issue),
         "collision preview initial separated mate");
    QVector<cad::SceneDocument::AssemblyNetworkMate> collisionWorld;
    must(collisionScene.liveRigidMateWorldDefinitions(&collisionWorld,&issue),
         "collision preview world observations");
    const QString collisionOldToken=collisionScene.find(collisionIds[1])->geometryToken;
    const double collisionOldX=collisionScene.find(collisionIds[1])->placementX;
    for(auto& constraint:collisionWorld[0].constraints)constraint.parentPoint.x-=8.0;
    QString collisionReport;
    must(collisionScene.previewLiveRigidMateConnectionEdit(0,collisionWorld[0],
         &collisionReport,&issue),"overlap preview should succeed with warning");
    must(collisionReport.contains("1 interference pairs")&&collisionReport.contains("OVERLAP:"),
         "new predicted overlapping solids not reported");
    must(collisionScene.find(collisionIds[1])->geometryToken==collisionOldToken&&
         std::abs(collisionScene.find(collisionIds[1])->placementX-collisionOldX)<1e-7,
         "overlap preview must not move components or alter geometry tokens");
    // Consistent loops must reject a changed link that invalidates closure.
    cad::SceneDocument cyclic;
    QStringList loopIds;
    for(int i=0;i<3;++i){
        must(cyclic.addPrimitive(cad::Primitive::Box,{4,4,4},&issue),"edit-loop box");
        const QString id=cyclic.parts().last().id;
        must(cyclic.makeComponent(id,QStringLiteral("EditLoop %1").arg(i),&issue),"edit-loop component");
        loopIds<<id;
    }
    must(cyclic.setLiveRigidMateNetwork(loopIds[0],{
         makeTranslationMate(loopIds[0],loopIds[1],10,0),
         makeTranslationMate(loopIds[1],loopIds[2],10,0),
         makeTranslationMate(loopIds[0],loopIds[2],20,0)},&issue),
         "setup persistent closed-loop mates for edit");
    QVector<cad::SceneDocument::AssemblyNetworkMate> closedWorld;
    must(cyclic.liveRigidMateWorldDefinitions(&closedWorld,&issue),"project closed-loop frames");
    auto conflicting=closedWorld[2];
    for(auto& c:conflicting.constraints)c.parentPoint.x+=1.0;
    const QString savedToken=cyclic.find(loopIds[2])->geometryToken;
    QString guardPreview="preserved";
    must(!cyclic.previewLiveRigidMateConnectionEdit(2,conflicting,&guardPreview,&issue),
         "inconsistent loop mate preview accepted");
    must(guardPreview=="preserved"&&cyclic.find(loopIds[2])->geometryToken==savedToken,
         "failed mate preview changed caller report or scene");
    must(!cyclic.replaceLiveRigidMateConnection(2,conflicting,&issue),
         "contradictory loop mate edit accepted");
    must(cyclic.find(loopIds[2])->geometryToken==savedToken&&
         cyclic.liveRigidMateDefinitions().size()==3,
         "invalid closed-loop edit must not modify committed graph");

}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);
    try{run();std::cout<<"assembly mate document: "<<checks<<" checks passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
