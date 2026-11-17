#include "core/SceneDocument.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void run(){
    cad::SceneDocument d;QString error;QStringList ids;
    for(int i=0;i<4;++i){
        check(d.addPrimitive(cad::Primitive::Box,{5,5,5},&error),"create box");
        const QString id=d.parts().last().id;
        check(d.makeComponent(id,QStringLiteral("Loop %1").arg(i),&error),"make independent occurrence");
        ids<<id;
    }
    using J=cad::SceneDocument::ArticulatedNetworkJoint;
    QVector<J> joints;
    const double pi=std::acos(-1.0);
    for(int i=0;i<4;++i){
        J j;j.parentBodyId=ids[i];j.childBodyId=ids[(i+1)%4];
        j.kind=cad::articulated::JointKind::Revolute;
        j.childAtZero.translation={10,0,0};
        j.axis={0,0,1};j.initial=i==0?pi/2:1.55;
        j.minimum=i==0?pi/2:0.8;j.maximum=i==0?pi/2:2.5;j.driven=i==0;
        joints.append(j);
    }
    QVector<double> coordinates;
    check(d.solveArticulatedAssemblyNetwork(ids[0],joints,&coordinates,&error),"scene articulated closure");
    check(coordinates.size()==4,"all joint coordinates returned");
    check(std::abs(d.find(ids[1])->placementY-10)<1e-4,"revolute placement applied");
    check(std::abs(d.find(ids[2])->placementX+10)<1e-4,"second placement applied");
    QTemporaryDir temp;check(temp.isValid(),"temporary dir");
    const auto path=temp.path()+"/articulated.cadscene";
    check(d.saveAs(path,&error),"save solved assembly");
    cad::SceneDocument opened;check(opened.load(path,&error),"reload assembly");
    check(std::abs(opened.find(ids[1])->placementY-10)<1e-4,"placement persisted");
    const double unchanged=d.find(ids[1])->placementY;
    const auto before=coordinates;
    auto wrong=joints;wrong[3].childAtZero.translation.x=60;
    check(!d.solveArticulatedAssemblyNetwork(ids[0],wrong,&coordinates,&error),"unreachable loop rejected");
    check(coordinates==before,"caller output unchanged on error");
    check(std::abs(d.find(ids[1])->placementY-unchanged)<1e-4,"scene unchanged on error");
    check(d.undo(),"single undo");
    check(std::abs(d.find(ids[1])->placementY)<1e-4,"undo returns all components");
    check(d.redo(),"redo");
    check(std::abs(d.find(ids[1])->placementY-10)<1e-4,"redo returns placements");
    // Live articulated definitions must be saved with their ORIGINAL pose
    // references, not with a delta that accumulates on every refresh.
    cad::SceneDocument live;
    check(live.addPrimitive(cad::Primitive::Box,{4,4,4},&error),"live fixed primitive");
    const QString fixed=live.parts().last().id;
    check(live.makeComponent(fixed,"Live fixed",&error),"live fixed occurrence");
    check(live.addPrimitive(cad::Primitive::Box,{3,3,3},&error),"live moving primitive");
    const QString moving=live.parts().last().id;
    check(live.makeComponent(moving,"Live moving",&error),"live moving occurrence");
    J saved;saved.parentBodyId=fixed;saved.childBodyId=moving;
    saved.kind=cad::articulated::JointKind::Fixed;
    saved.initial=0;saved.minimum=0;saved.maximum=0;saved.driven=true;
    check(live.setLiveArticulatedNetwork(fixed,{saved},&error),"save live articulated network");
    check(live.hasLiveArticulatedNetwork()&&live.liveArticulatedJointDefinitions().size()==1,
          "live articulated definitions missing");
    const auto originalToken=live.find(moving)->geometryToken;
    check(live.refreshLiveArticulatedNetwork(&error),"first no-op articulated refresh");
    check(live.find(moving)->geometryToken==originalToken,"no-op refresh changed geometry token");
    check(live.translate(fixed,7,3,0,&error),"move fixed live-articulated occurrence");
    check(live.refreshLiveArticulatedNetwork(&error),"refresh after fixed source edit");
    check(std::abs(live.find(moving)->placementX-7)<1e-4&&
          std::abs(live.find(moving)->placementY-3)<1e-4,
          "linked articulated occurrence failed to follow the fixed frame");
    const auto movedToken=live.find(moving)->geometryToken;
    check(live.refreshLiveArticulatedNetwork(&error),"repeat articulated refresh");
    check(live.find(moving)->geometryToken==movedToken,
          "repeat articulated refresh must not accumulate a transform");
    const QString liveFile=temp.path()+"/live-articulated.cadscene";
    check(live.saveAs(liveFile,&error),"save persistent articulated network");
    cad::SceneDocument loadedLive;
    check(loadedLive.load(liveFile,&error),"reopen persistent articulated network");
    check(loadedLive.hasLiveArticulatedNetwork()&&
          loadedLive.liveArticulatedFixedBodyId()==fixed,"saved articulated graph was lost");
    const auto loadedToken=loadedLive.find(moving)->geometryToken;
    check(loadedLive.refreshLiveArticulatedNetwork(&error),"reopened articulated refresh");
    check(loadedLive.find(moving)->geometryToken==loadedToken,"reopened refresh changed unchanged geometry");
    check(!loadedLive.erase(moving,&error),"deleting one articulated member should be rejected");
    check(loadedLive.removeLiveArticulatedNetwork(),"remove articulated definitions");
    check(loadedLive.undo()&&loadedLive.hasLiveArticulatedNetwork(),"undo did not restore articulated constraints");
    check(loadedLive.redo()&&!loadedLive.hasLiveArticulatedNetwork(),"redo did not remove articulated constraints");

    // Editable driven coordinates keep the original mechanism reference frames,
    // survive save/reopen, and commit atomically with geometry and undo history.
    cad::SceneDocument editable;
    check(editable.addPrimitive(cad::Primitive::Box,{4,4,4},&error),"editable fixed body");
    const QString driveBase=editable.parts().last().id;
    check(editable.makeComponent(driveBase,"Drive reference",&error),"editable fixed component");
    check(editable.addPrimitive(cad::Primitive::Box,{3,3,3},&error),"editable child body");
    const QString driveChild=editable.parts().last().id;
    check(editable.makeComponent(driveChild,"Drive child",&error),"editable moving component");
    J drive;drive.parentBodyId=driveBase;drive.childBodyId=driveChild;
    drive.kind=cad::articulated::JointKind::Revolute;
    drive.axis={0,0,1};drive.driven=true;
    drive.minimum=0;drive.maximum=0;drive.initial=0;
    check(editable.setLiveArticulatedNetwork(driveBase,{drive},&error),"create editable driven joint");
    const auto startPose=editable.find(driveChild)->geometryToken;
    const auto previousDrive=editable.liveArticulatedJointDefinitions()[0].initial;
    QString readOnlyReport="old report";
    check(editable.previewLiveArticulatedJointCoordinate(0,pi/2,&readOnlyReport,&error),
          "read-only driven revolute preview");
    check(readOnlyReport.contains("ARTICULATED POSE PREVIEW")&&
          readOnlyReport.contains("NOMINAL FIT"),"driven preview missing geometry results");
    check(editable.find(driveChild)->geometryToken==startPose&&
          editable.liveArticulatedJointDefinitions()[0].initial==previousDrive,
          "read-only articulated preview mutated document or joints");
    const auto successfulReport=readOnlyReport;
    check(!editable.previewLiveArticulatedJointCoordinate(0,10*pi,&readOnlyReport,&error),
          "out-of-range driven preview accepted");
    check(readOnlyReport==successfulReport&&editable.find(driveChild)->geometryToken==startPose,
          "rejected driven preview mutated geometry or caller report");
    check(!editable.previewLiveArticulatedJointCoordinate(0,pi/2,&readOnlyReport,&error,-1.0),
          "negative required-clearance preview accepted");
    check(readOnlyReport==successfulReport,"invalid clearance overwrote caller report");
    check(!editable.previewLiveArticulatedJointCoordinate(20,pi/2,&readOnlyReport,&error),
          "nonexistent articulated joint preview accepted");
    check(!editable.previewLiveArticulatedJointCoordinate(0,pi/2,nullptr,&error),
          "null preview report accepted");
    check(editable.updateLiveArticulatedJointCoordinate(0,pi/2,&error),"set driven angle");
    check(std::abs(editable.find(driveChild)->placementYawDegrees-90)<1e-4,
          "edited joint did not rotate moving component");
    const auto setPose=editable.find(driveChild)->geometryToken;
    check(editable.refreshLiveArticulatedNetwork(&error),"refresh edited driving coordinate");
    check(editable.find(driveChild)->geometryToken==setPose,
          "refresh reapplied same driven motion");
    check(!editable.updateLiveArticulatedJointCoordinate(0,10*pi,&error),
          "out-of-range driven angle accepted");
    check(editable.find(driveChild)->geometryToken==setPose&&
          std::abs(editable.liveArticulatedJointDefinitions()[0].initial-pi/2)<1e-8,
          "failed edit changed current mechanism");
    check(editable.undo(),"undo editable mechanism");
    check(editable.find(driveChild)->geometryToken==startPose&&
          std::abs(editable.liveArticulatedJointDefinitions()[0].initial)<1e-8,
          "undo did not restore both pose and saved driving coordinate");
    check(editable.redo(),"redo editable mechanism");
    check(editable.find(driveChild)->geometryToken==setPose&&
          std::abs(editable.liveArticulatedJointDefinitions()[0].initial-pi/2)<1e-8,
          "redo did not restore driving coordinate");
    const QString editFile=temp.path()+"/editable-driving.cadscene";
    check(editable.saveAs(editFile,&error),"save edited driven mechanism");
    cad::SceneDocument reloadedDrive;
    check(reloadedDrive.load(editFile,&error),"reload edited driven mechanism");
    check(std::abs(reloadedDrive.liveArticulatedJointDefinitions()[0].initial-pi/2)<1e-8,
          "edited drive angle missing after reload");
    check(reloadedDrive.updateLiveArticulatedJointCoordinate(0,0.0,&error),
          "edit reopened mechanism back to zero");
    check(std::abs(reloadedDrive.find(driveChild)->placementYawDegrees)<1e-4,
          "edited reopened mechanism accumulated stale rotation");

    // Preview a moving prismatic body against a fixed body, and prove the
    // nominal pairwise gap is computed from the proposed, not saved, pose.
    cad::SceneDocument slider;
    check(slider.addPrimitive(cad::Primitive::Box,{4,4,4},&error),"slider fixed box");
    const QString sliderFixed=slider.parts().last().id;
    check(slider.makeComponent(sliderFixed,"Slider fixed",&error),"slider fixed component");
    check(slider.addPrimitive(cad::Primitive::Box,{3,3,3},&error),"slider free box");
    const QString sliderChild=slider.parts().last().id;
    check(slider.makeComponent(sliderChild,"Slider child",&error),"slider child component");
    J slide;slide.parentBodyId=sliderFixed;slide.childBodyId=sliderChild;
    slide.kind=cad::articulated::JointKind::Prismatic;slide.axis={1,0,0};
    slide.driven=true;slide.initial=slide.minimum=slide.maximum=0;
    check(slider.setLiveArticulatedNetwork(sliderFixed,{slide},&error),"slider live joint");
    const QString originalSliderToken=slider.find(sliderChild)->geometryToken;
    QString slideReport;
    check(slider.previewLiveArticulatedJointCoordinate(0,20,&slideReport,&error,2.0),
          "prismatic clearance preview");
    check(slideReport.contains("0 interferences")&&slideReport.contains("Minimum separated pair gap"),
          "prismatic clearance preview missing true proposed gap");
    check(slider.find(sliderChild)->geometryToken==originalSliderToken&&
          std::abs(slider.liveArticulatedJointDefinitions()[0].initial)<1e-12,
          "prismatic preview changed saved pose or driving coordinate");
    check(slider.updateLiveArticulatedJointCoordinate(0,20,&error),"commit previewed slider coordinate");
    check(std::abs(slider.find(sliderChild)->placementX-20)<1e-4,
          "committed prismatic pose disagrees with preview");

    // A moving prismatic slider MUST be checked against its stationary root.
    // Previous workspace exports omitted the fixed member, leaving no pair
    // for a one-moving-body mechanism. No change to the saved slider is made.
    QByteArray sliderWorkspace("keep previous bytes");
    check(slider.buildLiveArticulatedEnvelopeCsv({{0,0.0}},3,2,false,
          &sliderWorkspace,&error)&&
          sliderWorkspace.contains("continuous_translation_sweep_status")&&
          sliderWorkspace.contains("SUPPORTED_TRANSLATION")&&
          sliderWorkspace.contains("swept_aabb_contact_segments")&&
          sliderWorkspace.contains(sliderFixed.toUtf8())&&
          std::abs(slider.liveArticulatedJointDefinitions()[0].initial-20.0)<1e-12,
          "prismatic fixture clearance was omitted from the motion workspace");
    // A complete slider traversal crosses the stationary fixture, but the
    // endpoint and three-intermediate-pose sampling can miss that narrow
    // encounter. The strict commit must fail before checkpoint/geometry edit.
    check(slider.updateLiveArticulatedJointCoordinate(0,-20.0,&error),
          "prepare starting pose for hidden mid-segment fixture collision");
    const auto beforeStrict=slider.find(sliderChild)->geometryToken;
    check(!slider.commitLiveArticulatedCoordinatesChecked({{0,20.0}},0.0,
          true,3,1,false,&error)&&
          slider.find(sliderChild)->geometryToken==beforeStrict&&
          std::abs(slider.liveArticulatedJointDefinitions()[0].initial+20.0)<1e-12,
          "strict commit allowed a translating slider to sweep through the fixture");
    const QByteArray sliderWorkspaceSnapshot=sliderWorkspace;
    check(!slider.buildLiveArticulatedEnvelopeCsv({{0,9999.0}},3,2,false,
          &sliderWorkspace,&error)&&sliderWorkspace==sliderWorkspaceSnapshot,
          "invalid translational sweep overwrote previously valid workspace evidence");

    // Two coupled drives are edited as one graph-wide pose. In particular,
    // no intermediate single-drive transaction may enter the undo history.
    cad::SceneDocument multi;
    QStringList chain;
    for(int i=0;i<3;++i){
        check(multi.addPrimitive(cad::Primitive::Box,{3,3,3},&error),"multi-drive body");
        chain<<multi.parts().last().id;
        check(multi.makeComponent(chain.last(),QStringLiteral("Chain %1").arg(i),&error),
              "multi-drive component");
    }
    J hinge;hinge.parentBodyId=chain[0];hinge.childBodyId=chain[1];
    hinge.kind=cad::articulated::JointKind::Revolute;
    hinge.axis={0,0,1};hinge.driven=true;
    J slide2;slide2.parentBodyId=chain[1];slide2.childBodyId=chain[2];
    slide2.kind=cad::articulated::JointKind::Prismatic;
    slide2.axis={1,0,0};slide2.driven=true;
    check(multi.setLiveArticulatedNetwork(chain[0],{hinge,slide2},&error),
          "save two-drive articulated chain");
    const QString originalSecond=multi.find(chain[2])->geometryToken;
    QString batchReport="original report";
    QMap<int,double> both{{0,pi/2},{1,20.0}};
    check(multi.previewLiveArticulatedCoordinates(both,&batchReport,&error,0.0),
          "preview two coupled drives");
    // Strict sampled-fit mode rejects unsafe geometry without changing the
    // committed network or the caller's previously successful report.
    bool unsafeNominal=false;
    QString largeClearanceReport;
    check(multi.previewLiveArticulatedCoordinates(both,&largeClearanceReport,&error,
            1000000.0,&unsafeNominal)&&unsafeNominal,
          "high-clearance preview should flag unsafe nominal pairs");
    QString strictOutput="unchanged strict output";
    check(!multi.previewLiveArticulatedTrajectory(both,4,&strictOutput,&error,
            1000000.0,true)&&strictOutput=="unchanged strict output"&&
          multi.find(chain[2])->geometryToken==originalSecond,
          "strict unsafe sampled-motion policy was not atomic");
    bool untouchedStatus=true;
    QString badStatusReport="no overwrite";
    check(!multi.previewLiveArticulatedCoordinates({{90,3}},&badStatusReport,&error,
            0.0,&untouchedStatus)&&untouchedStatus&&badStatusReport=="no overwrite",
          "rejected fit preview overwrote status or report");
    QString trajectoryReport="previous trajectory";
    check(multi.previewLiveArticulatedTrajectory(both,4,&trajectoryReport,&error),
          "preview sampled coupled-drive trajectory");
    check(trajectoryReport.contains("SAMPLED ARTICULATED MULTI-DRIVE")&&
          trajectoryReport.contains("FRAME 0/4")&&trajectoryReport.contains("FRAME 4/4")&&
          trajectoryReport.contains("Exact nominal pairs"),
          "motion trajectory did not check its sampled poses and solid fits");
    check(multi.find(chain[2])->geometryToken==originalSecond&&
          std::abs(multi.liveArticulatedJointDefinitions()[0].initial)<1e-12,
          "read-only trajectory unexpectedly committed a pose");
    QString refinedReport="preserve previous";
    check(multi.previewLiveArticulatedTrajectory(both,3,&refinedReport,&error,
            0.0,false,4)&&refinedReport.contains("Base intervals: 3; refinement factor: 4")&&
            refinedReport.contains("FRAME 12/12")&&
            multi.find(chain[2])->geometryToken==originalSecond,
          "refined 12-interval articulated motion preview mutated the committed mechanism");
    // Export the SAME sampled solver results as an audit-ready CSV without
    // committing either drive. Invalid strict paths preserve previous bytes.
    QByteArray motionCsv("previous csv buffer");
    check(multi.buildLiveArticulatedMotionCsv(both,3,4,true,0.0,false,
          &motionCsv,&error)&&motionCsv.startsWith("sample,fraction,progression,")&&
          motionCsv.contains("joint_1_radians")&&motionCsv.contains("joint_2_mm")&&
          motionCsv.contains("12,1,smoothstep,")&&
          multi.find(chain[2])->geometryToken==originalSecond,
          "sampled evidence export lacks real independently solved motion rows");
    // Sampled swept-space envelopes derive from the actual regenerated OCCT
    // solids at every pose, not merely from joint-coordinate extrema.
    QByteArray workspace("previous workspace");
    check(multi.buildLiveArticulatedEnvelopeCsv(both,3,2,false,&workspace,&error)&&
          workspace.startsWith("body_id,body_name,samples,")&&
          workspace.contains("sampled pose AABB")&&
          workspace.contains("UNSUPPORTED_ROTATION")&&
          workspace.contains(",7,")&&
          multi.find(chain[2])->geometryToken==originalSecond,
          "moving-body workspace did not derive sampled B-rep envelopes");
    const QByteArray oldWorkspace=workspace;
    check(!multi.buildLiveArticulatedEnvelopeCsv(both,1,2,false,&workspace,&error)&&
          workspace==oldWorkspace,
          "invalid envelope resolution overwrote the previous CSV");
    check(!multi.buildLiveArticulatedEnvelopeCsv({{0,10*pi}},3,2,true,&workspace,&error)&&
          workspace==oldWorkspace,
          "invalid envelope drive overwrote a valid workspace");
    const QByteArray validMotion=motionCsv;
    check(!multi.buildLiveArticulatedMotionCsv(both,4,1,false,1000000.0,true,
          &motionCsv,&error)&&motionCsv==validMotion,
          "strict failed sampled evidence export overwrote valid output");
    check(!multi.buildLiveArticulatedMotionCsv(both,1,1,false,0.0,false,
          &motionCsv,&error)&&motionCsv==validMotion,
          "invalid sample budget overwrote valid motion evidence");
    const QString refinedSnapshot=refinedReport;
    check(!multi.previewLiveArticulatedTrajectory(both,3,&refinedReport,&error,
            0.0,false,0)&&refinedReport==refinedSnapshot,
          "invalid zero sampling refinement overwrote preview output");
    check(!multi.previewLiveArticulatedTrajectory(both,3,&refinedReport,&error,
            0.0,false,5)&&refinedReport==refinedSnapshot,
          "over-budget motion sampling refinement was silently accepted");
    QString easedReport="unchanged eased report";
    check(multi.previewLiveArticulatedTrajectory(both,4,&easedReport,&error,
          0.0,false,2,true),"ease-in/out multi-drive preview");
    check(easedReport.contains("Progression: ease in/out")&&
          easedReport.contains("FRAME 8/8"),"smoothstep preview report missing geometry samples");
    check(multi.liveArticulatedJointDefinitions()[0].initial==0&&
          multi.liveArticulatedJointDefinitions()[1].initial==0,
          "smoothstep preview committed a mechanism change");
    const QString originalTrajectoryReport=trajectoryReport;
    check(!multi.previewLiveArticulatedTrajectory(both,1,&trajectoryReport,&error)&&
          trajectoryReport==originalTrajectoryReport,
          "invalid sample budget overwrote caller output");
    check(!multi.previewLiveArticulatedTrajectory({{0,10*pi}},4,
          &trajectoryReport,&error)&&trajectoryReport==originalTrajectoryReport,
          "invalid path target was accepted or overwrote the successful trajectory");
    check(!multi.previewLiveArticulatedTrajectory(both,4,&trajectoryReport,&error,-0.1)&&
          trajectoryReport==originalTrajectoryReport,
          "invalid trajectory clearance was accepted or changed output");
    check(batchReport.contains("MULTI-DRIVE PREVIEW")&&
          batchReport.contains("Exact nominal pairs")&&
          multi.find(chain[2])->geometryToken==originalSecond,
          "multi-drive geometry preview mutated the live scene");
    const QString beforeBad=batchReport;
    check(!multi.previewLiveArticulatedCoordinates({{0,10*pi},{1,20.0}},
          &batchReport,&error)&&batchReport==beforeBad,
          "invalid multi-drive preview changed output report");
    check(!multi.updateLiveArticulatedCoordinates({{0,pi/2},{22,20.0}},&error)&&
          multi.find(chain[2])->geometryToken==originalSecond,
          "invalid batch index partially committed mechanism");
    // The policy must be checked by the core commit operation itself,
    // including direct API calls that bypass the UI preview dialog.
    check(!multi.commitLiveArticulatedCoordinatesChecked(both,1000000.0,
            true,3,2,false,&error)&&
          multi.find(chain[2])->geometryToken==originalSecond&&
          std::abs(multi.liveArticulatedJointDefinitions()[0].initial)<1e-12,
          "strict core commit accepted unsafe articulated clearance");
    check(!multi.commitLiveArticulatedCoordinatesChecked(both,-1.0,
            false,3,1,false,&error)&&
          multi.find(chain[2])->geometryToken==originalSecond,
          "invalid core-commit policy changed the mechanism");
    check(multi.commitLiveArticulatedCoordinatesChecked(both,0.0,
            false,3,2,true,&error),
          "checked core API must atomically commit two articulated drives");
    check(std::abs(multi.liveArticulatedJointDefinitions()[0].initial-pi/2)<1e-9&&
          std::abs(multi.liveArticulatedJointDefinitions()[1].initial-20.0)<1e-9&&
          multi.find(chain[2])->geometryToken!=originalSecond,
          "multi-drive batch did not update both joint coordinates and pose");
    check(multi.undo()&&multi.find(chain[2])->geometryToken==originalSecond&&
          std::abs(multi.liveArticulatedJointDefinitions()[0].initial)<1e-12&&
          std::abs(multi.liveArticulatedJointDefinitions()[1].initial)<1e-12,
          "multi-drive undo was not atomic");
    check(multi.redo()&&
          std::abs(multi.liveArticulatedJointDefinitions()[1].initial-20.0)<1e-9,
          "multi-drive redo did not restore both coordinates");
    const QString multiPath=temp.path()+"/multi-drive.cadscene";
    check(multi.saveAs(multiPath,&error),"save multi-drive batch");
    cad::SceneDocument reopenedMulti;
    check(reopenedMulti.load(multiPath,&error)&&
          std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0].initial-pi/2)<1e-9&&
          std::abs(reopenedMulti.liveArticulatedJointDefinitions()[1].initial-20.0)<1e-9,
          "saved multi-drive coordinates were lost");

    // A saved connection can now change its axis, zero-reference offset and
    // meaningful travel limits in one staged solve and one undo checkpoint.
    const auto definitionBefore=reopenedMulti.liveArticulatedJointDefinitions()[0];
    auto editedDefinition=definitionBefore;
    editedDefinition.axis={0,1,0};
    editedDefinition.minimum=0.0;
    editedDefinition.maximum=pi;
    editedDefinition.initial=pi/2;
    editedDefinition.childAtZero.translation={0,0,3};
    // A saved articulation's zero-frame orientation is now user-editable.
    // Rotation is row-major Rz(30 degrees), followed by the existing drive.
    const double frameYaw=pi/6;
    editedDefinition.childAtZero.rotation={{
        std::cos(frameYaw),-std::sin(frameYaw),0,
        std::sin(frameYaw),std::cos(frameYaw),0,
        0,0,1}};
    const QString beforeDefinitionToken=reopenedMulti.find(chain[1])->geometryToken;
    QString definitionPreview="untouched report";
    check(reopenedMulti.previewLiveArticulatedJointDefinitionEdit(0,editedDefinition,
        &definitionPreview,&error),"preview edited joint axis/offset/limits");
    check(definitionPreview.contains("ARTICULATED JOINT-DEFINITION PREVIEW")&&
          reopenedMulti.find(chain[1])->geometryToken==beforeDefinitionToken,
          "definition preview changed committed solid");
    const QString successfulDefinitionReport=definitionPreview;
    auto invalidDefinition=editedDefinition;
    invalidDefinition.axis={0,0,0};
    check(!reopenedMulti.previewLiveArticulatedJointDefinitionEdit(0,invalidDefinition,
        &definitionPreview,&error)&&definitionPreview==successfulDefinitionReport,
        "zero axis preview accepted or overwrote caller report");
    invalidDefinition=editedDefinition;invalidDefinition.childBodyId=chain[0];
    check(!reopenedMulti.replaceLiveArticulatedJointDefinition(0,invalidDefinition,&error),
        "joint retarget by replacement was accepted");
    check(reopenedMulti.replaceLiveArticulatedJointDefinition(0,editedDefinition,&error),
        "commit joint axis/offset/limit edit");
    check(std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0].axis.y-1)<1e-10&&
          reopenedMulti.liveArticulatedJointDefinitions()[0].maximum==pi,
          "committed joint axis or limits missing");
    check(std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0]
          .childAtZero.rotation[0]-std::cos(frameYaw))<1e-10,
          "edited reference-frame orientation was not saved");
    const QString changedDefinitionToken=reopenedMulti.find(chain[1])->geometryToken;
    check(changedDefinitionToken!=beforeDefinitionToken,
          "definition edit did not regenerate dependent geometry");
    check(reopenedMulti.replaceLiveArticulatedJointDefinition(0,editedDefinition,&error),
          "no-op joint definition edit");
    check(reopenedMulti.find(chain[1])->geometryToken==changedDefinitionToken,
          "unchanged definition regenerated geometry");
    check(!reopenedMulti.updateLiveArticulatedCoordinates({{0,-0.1}},&error)&&
          reopenedMulti.find(chain[1])->geometryToken==changedDefinitionToken,
          "drive incorrectly bypassed explicit travel limits");
    check(reopenedMulti.updateLiveArticulatedCoordinates({{0,pi/3}},&error),
          "legal drive edit inside explicit limits rejected");
    check(std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0].minimum)<1e-10&&
          std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0].maximum-pi)<1e-10,
          "drive edit silently overwrote explicit limits");
    check(reopenedMulti.undo()&&
          std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0].initial-pi/2)<1e-9,
          "undo limited drive edit did not restore initial coordinate");
    check(reopenedMulti.undo()&&
          std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0].axis.z-1)<1e-10&&
          reopenedMulti.find(chain[1])->geometryToken==beforeDefinitionToken,
          "undo did not restore original joint definition and solid");
    check(reopenedMulti.redo()&&
          std::abs(reopenedMulti.liveArticulatedJointDefinitions()[0].axis.y-1)<1e-10,
          "redo did not restore edited joint axis");
    const QString definitionsPath=temp.path()+"/joint-definition-edited.cadscene";
    check(reopenedMulti.saveAs(definitionsPath,&error),"save edited articulated definition");
    cad::SceneDocument savedDefinition;
    check(savedDefinition.load(definitionsPath,&error)&&
          std::abs(savedDefinition.liveArticulatedJointDefinitions()[0].axis.y-1)<1e-10&&
          std::abs(savedDefinition.liveArticulatedJointDefinitions()[0].maximum-pi)<1e-10,
          "joint definition was not persisted on disk");

    // Rebinding must not silently replace the member set or rewrite a saved
    // joint's identity/kinematic parameters. A connected replacement graph is
    // staged and atomically committed; disconnected graphs fail closed.
    cad::SceneDocument rebindScene;
    QStringList rebindIds;
    for(int i=0;i<3;++i){
        check(rebindScene.addPrimitive(cad::Primitive::Box,{2,2,2},&error),
              "create rebind primitive");
        const QString id=rebindScene.parts().last().id;
        check(rebindScene.makeComponent(id,QString("Rebind member %1").arg(i),&error),
              "make rebind occurrence");
        rebindIds<<id;
    }
    J firstLink;firstLink.parentBodyId=rebindIds[0];firstLink.childBodyId=rebindIds[1];
    firstLink.kind=cad::articulated::JointKind::Fixed;
    firstLink.initial=firstLink.minimum=firstLink.maximum=0;
    J secondLink=firstLink;
    secondLink.parentBodyId=rebindIds[1];secondLink.childBodyId=rebindIds[2];
    check(rebindScene.setLiveArticulatedNetwork(rebindIds[0],{firstLink,secondLink},&error),
          "save three-member articulated chain");
    const auto previousRebindToken=rebindScene.find(rebindIds[2])->geometryToken;
    QString rebindPreview="previous preview";
    check(!rebindScene.previewLiveArticulatedJointRebind(1,rebindIds[0],rebindIds[0],
         &rebindPreview,&error)&&rebindPreview=="previous preview",
          "self-join rebind preview incorrectly accepted or mutated report");
    check(!rebindScene.rebindLiveArticulatedJoint(1,rebindIds[0],"foreign-occurrence",&error),
          "rebind accepted a nonexistent occurrence");
    check(!rebindScene.previewLiveArticulatedJointRebind(1,rebindIds[0],rebindIds[1],
          &rebindPreview,&error)&&rebindPreview=="previous preview",
          "disconnected rebind preview incorrectly succeeded");
    check(!rebindScene.rebindLiveArticulatedJoint(1,rebindIds[0],rebindIds[1],&error),
          "disconnected rebind committed");
    check(rebindScene.liveArticulatedJointDefinitions()[1].parentBodyId==rebindIds[1]&&
          rebindScene.find(rebindIds[2])->geometryToken==previousRebindToken,
          "rejected rebind altered saved link or geometry");
    check(rebindScene.previewLiveArticulatedJointRebind(1,rebindIds[0],rebindIds[2],
          &rebindPreview,&error),"preview valid rebind");
    check(rebindPreview.contains("READ-ONLY ARTICULATED JOINT REBIND PREVIEW")&&
          rebindScene.liveArticulatedJointDefinitions()[1].parentBodyId==rebindIds[1]&&
          rebindScene.find(rebindIds[2])->geometryToken==previousRebindToken,
          "valid rebind preview mutated committed scene");
    check(rebindScene.rebindLiveArticulatedJoint(1,rebindIds[0],rebindIds[2],&error),
          "commit connected joint rebind");
    check(rebindScene.liveArticulatedJointDefinitions()[1].parentBodyId==rebindIds[0]&&
          rebindScene.liveArticulatedJointDefinitions()[1].childBodyId==rebindIds[2]&&
          rebindScene.liveArticulatedJointDefinitions()[1].kind==secondLink.kind,
          "rebinding changed joint kind or did not save endpoints");
    check(rebindScene.rebindLiveArticulatedJoint(1,rebindIds[0],rebindIds[2],&error),
          "identical rebind no-op");
    check(rebindScene.undo()&&
          rebindScene.liveArticulatedJointDefinitions()[1].parentBodyId==rebindIds[1],
          "rebind undo did not restore original joint graph");
    check(rebindScene.redo()&&
          rebindScene.liveArticulatedJointDefinitions()[1].parentBodyId==rebindIds[0],
          "rebind redo did not restore new joint graph");
    const QString rebindPath=temp.path()+"/rebound-articulated.cadscene";
    check(rebindScene.saveAs(rebindPath,&error),"save rebound articulated graph");
    cad::SceneDocument reloadedRebind;
    check(reloadedRebind.load(rebindPath,&error)&&
          reloadedRebind.liveArticulatedJointDefinitions()[1].parentBodyId==rebindIds[0],
          "rebound graph did not survive save and reopen");
    check(reloadedRebind.refreshLiveArticulatedNetwork(&error),
          "reloaded rebound graph did not resolve");

    QFile original(liveFile);check(original.open(QIODevice::ReadOnly),"read saved articulated scene");
    QJsonObject archive=QJsonDocument::fromJson(original.readAll()).object();original.close();
    QJsonObject network=archive.value("live_articulated_mates").toObject();
    QJsonArray frames=network.value("reference_poses").toArray();
    QJsonObject frame=frames[0].toObject();
    QJsonObject pose=frame.value("frame").toObject();
    pose.insert("rotation",QJsonArray{1.0,0.0,0.0});
    frame.insert("frame",pose);frames.replace(0,frame);
    network.insert("reference_poses",frames);archive.insert("live_articulated_mates",network);
    const QString corruptFile=temp.path()+"/bad-articulated.cadscene";
    QFile corrupt(corruptFile);check(corrupt.open(QIODevice::WriteOnly),"create corrupted mechanism scene");
    check(corrupt.write(QJsonDocument(archive).toJson(QJsonDocument::Compact))>0,"write corrupt mechanism");
    corrupt.close();
    cad::SceneDocument intact;check(intact.addPrimitive(cad::Primitive::Sphere,{3},&error),"seed protected scene");
    const QString protectedId=intact.parts().first().id;
    check(!intact.load(corruptFile,&error),"corrupt articulated pose was accepted");
    check(intact.parts().size()==1&&intact.find(protectedId),"bad mechanism load changed current scene");

}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);
try{run();std::cout<<checks<<" scene articulated checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
