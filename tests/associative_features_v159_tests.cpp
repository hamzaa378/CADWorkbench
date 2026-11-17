#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include <BRepGProp.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pnt.hxx>
#include <TopoDS.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <numbers>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
double volume(const TopoDS_Shape& body){GProp_GProps p;BRepGProp::VolumeProperties(body,p);return p.Mass();}
cad::ShapeFeatureNode feature(const QString& type,const QMap<QString,double>& args){
    cad::ShapeFeatureNode n;n.id=type;n.type=type;n.parameters=args;return n;
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        QString error;
        cad::SceneDocument scene;
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"First box failed");
        const QString first=scene.parts().last().id;
        check(scene.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Second box failed");
        const QString second=scene.parts().last().id;
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Fuse,first,second,&error),"Linked union failed");
        const QString linked=scene.parts().last().id;
        check(scene.linkedBooleanCurrent(linked),"New linked body is stale");
        check(std::abs(volume(scene.find(linked)->shape)-4000)<0.1,"Incorrect initial linked volume");
        const auto translate=feature("Translate",{{"x",10},{"y",0},{"z",0}});
        check(scene.setShapeFeatureTree(second,{translate},&error),"Could not edit linked source");
        check(scene.linkedBooleanCurrent(linked),"Linked output not regenerated");
        check(std::abs(volume(scene.find(linked)->shape)-6000)<0.1,"Linked union did not follow source translation");
        check(scene.undo(),"Linked update undo failed");
        check(std::abs(volume(scene.find(linked)->shape)-4000)<0.1,"Linked undo did not restore output");
        check(scene.redo(),"Linked update redo failed");
        check(std::abs(volume(scene.find(linked)->shape)-6000)<0.1,"Linked redo did not restore output");
        // A second downstream body forms a multi-level document dependency DAG.
        check(scene.addLinkedBooleanBody(cad::BooleanOp::Cut,linked,first,&error),"Linked second-level cut failed");
        const QString nested=scene.parts().last().id;
        check(std::abs(volume(scene.find(nested)->shape)-2000)<0.1,"Nested body difference volume mismatch");
        const QString beforeToken=scene.find(second)->geometryToken;
        auto invalid=feature("Cylinder cut Z",{{"x",500},{"y",500},{"z",0},{"radius",2},{"depth",20}});
        check(!scene.setShapeFeatureTree(second,{invalid},&error),"Invalid upstream feature was accepted");
        check(scene.find(second)->geometryToken==beforeToken&&scene.linkedBooleanCurrent(nested),
              "Failed edit mutated linked document");
        check(!scene.erase(first,&error),"Deleting a source left its dependent dangling");
        QTemporaryDir temp;check(temp.isValid(),"No temporary directory");
        const QString name=temp.path()+"/links.cadscene";
        check(scene.saveAs(name,&error),"Could not save linked document");
        cad::SceneDocument loaded;
        check(loaded.load(name,&error),"Could not reopen linked document");
        check(loaded.linkedBooleanCurrent(linked)&&loaded.linkedBooleanCurrent(nested),
              "Linked dependencies did not survive save/reopen");
        check(std::abs(volume(loaded.find(nested)->shape)-2000)<0.1,"Linked B-rep not preserved");
        QFile input(name);check(input.open(QIODevice::ReadOnly),"Could not open source JSON");
        auto json=QJsonDocument::fromJson(input.readAll()).object();input.close();
        auto bodies=json.value("parts").toArray();
        auto corrupt=bodies[2].toObject();auto link=corrupt.value("linked_boolean").toObject();
        link.insert("primary",QStringLiteral("missing-source"));corrupt.insert("linked_boolean",link);
        bodies.replace(2,corrupt);json.insert("parts",bodies);
        const QString bad=temp.path()+"/invalid.cadscene";
        QFile out(bad);check(out.open(QIODevice::WriteOnly),"Cannot write invalid fixture");
        out.write(QJsonDocument(json).toJson());out.close();
        check(!loaded.load(bad,&error)&&loaded.linkedBooleanCurrent(linked),
              "Invalid dependency graph replaced open scene");

        // Direct geometry edits must also update linked bodies transactionally.
        cad::SceneDocument direct;
        check(direct.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Direct source A failed");
        const QString directA=direct.parts().last().id;
        check(direct.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Direct source B failed");
        const QString directB=direct.parts().last().id;
        check(direct.addLinkedBooleanBody(cad::BooleanOp::Common,directA,directB,&error),
              "Initial linked intersection failed");
        const QString directResult=direct.parts().last().id;
        check(!direct.translate(directB,40,0,0,&error),
              "Source edit that destroys linked intersection was accepted");
        check(direct.linkedBooleanCurrent(directResult)&&
              std::abs(volume(direct.find(directResult)->shape)-4000)<0.1,
              "Failed downstream regeneration mutated source or result");
        check(direct.translate(directB,5,0,0,&error),"Valid linked transform failed");
        check(std::abs(volume(direct.find(directResult)->shape)-3000)<0.1,
              "Direct source translation failed to propagate to linked result");

        // Face-normal drilling on a non-parametric solid is valid even when
        // world -Z would be the wrong direction. Linked descendants rebuild
        // atomically after the source is changed by the direct hole.
        cad::SceneDocument axisDoc;
        check(axisDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),
              "Face-normal hole source failed");
        const QString axisSource=axisDoc.parts().last().id;
        check(axisDoc.addPrimitive(cad::Primitive::Box,{3,3,3},&error),
              "Face-normal hole secondary source failed");
        check(axisDoc.addLinkedBooleanBody(cad::BooleanOp::Fuse,axisSource,
                axisDoc.parts().last().id,&error),"Face-normal linked body failed");
        const QString axisResult=axisDoc.parts().last().id;
        const double before=volume(axisDoc.find(axisResult)->shape);
        cad::FaceReference pickedFace;
        int faceIndex=0;
        gp_Pnt entry;
        for(TopExp_Explorer faces(axisDoc.find(axisSource)->shape,TopAbs_FACE);
            faces.More();faces.Next()){
            ++faceIndex;
            const TopoDS_Face candidate=TopoDS::Face(faces.Current());
            const BRepAdaptor_Surface surface(candidate);
            if(surface.GetType()!=GeomAbs_Plane)continue;
            // Face orientation converts the analytic surface normal to the
            // physical OUTWARD normal.  Reversing on FORWARD instead would
            // select the opposite box face and contradict the inward-axis test.
            gp_Dir outward=surface.Plane().Axis().Direction();
            if(candidate.Orientation()==TopAbs_REVERSED)outward.Reverse();
            if(outward.X()>0.99){
                GProp_GProps faceMass;BRepGProp::SurfaceProperties(candidate,faceMass);
                entry=faceMass.CentreOfMass();
                pickedFace={axisSource,axisDoc.find(axisSource)->geometryToken,faceIndex};
                break;
            }
        }
        check(pickedFace.faceIndex>0,"No positive X planar face on test box");
        cad::HoleCommand axisHole;
        check(axisDoc.prepareHoleOnPlanarFace(pickedFace,entry.X(),entry.Y(),entry.Z(),
                                               4,1,true,&axisHole,&error),
              "Face-normal hole preparation failed");
        check(axisHole.inwardAxis.X()<-0.99,"Face normal did not point into the solid");
        check(!axisDoc.prepareHoleOnPlanarFace(pickedFace,entry.X()-1,entry.Y(),entry.Z(),
                                                4,1,true,&axisHole,&error),
              "Off-plane hole center was accepted");
        check(axisDoc.prepareHoleOnPlanarFace(pickedFace,entry.X(),entry.Y(),entry.Z(),
                                               4,1,true,&axisHole,&error),
              "Face-normal hole could not be re-prepared");
        const auto axialPreview=cad::SceneDocument::buildHolePreview(axisHole);
        check(!axialPreview.shape.IsNull(),"Angled hole preview was invalid");
        check(std::abs(volume(axisDoc.find(axisSource)->shape)-
                       volume(axialPreview.shape)-80*std::numbers::pi)<1e-3,
              "Face-normal through-all hole removed incorrect volume");
        check(axisDoc.commitHole(axisHole,axialPreview.shape,&error),
              "Face-normal hole commit or linked regeneration failed");
        check(axisDoc.linkedBooleanCurrent(axisResult),"Hole left linked result stale");
        check(volume(axisDoc.find(axisResult)->shape)<before-1.0,
              "Linked result did not reflect the new hole");
        check(axisDoc.undo(),"Face-normal hole undo failed");
        check(axisDoc.linkedBooleanCurrent(axisResult)&&
              std::abs(volume(axisDoc.find(axisResult)->shape)-before)<1e-4,
              "Undo did not restore linked descendant");
        check(axisDoc.redo()&&axisDoc.linkedBooleanCurrent(axisResult),
              "Redo failed to restore committed hole and dependent geometry");

        // Editable arbitrary-axis cut on a general solid: revising depth must
        // rebuild both its stable-ID feature node and downstream linked body.
        cad::SceneDocument editable;
        check(editable.addPrimitive(cad::Primitive::Box,{20,20,10},&error),
              "Editable axial source box failed");
        const QString editableId=editable.parts().last().id;
        check(editable.addPrimitive(cad::Primitive::Box,{3,3,3},&error),
              "Editable axial linked tool box failed");
        const QString editableOther=editable.parts().last().id;
        const auto angled=feature("Cylinder cut arbitrary axis",
            {{"x",20},{"y",10},{"z",5},{"axis_x",-1},{"axis_y",0},{"axis_z",0},
             {"radius",2},{"depth",8}});
        check(editable.setShapeFeatureTree(editableId,{angled},&error),
              "Arbitrary-axis cylinder feature could not build");
        const double shortVolume=volume(editable.find(editableId)->shape);
        check(std::abs(shortVolume-(4000-32*std::numbers::pi))<1e-3,
              "Arbitrary-axis 8 mm cutter removed wrong volume");
        check(editable.addLinkedBooleanBody(cad::BooleanOp::Fuse,editableId,editableOther,&error),
              "Dependent editable-axis fuse failed");
        const QString editableLinked=editable.parts().last().id;
        auto longer=angled;
        longer.parameters.insert("depth",14);
        check(editable.setShapeFeatureTree(editableId,{longer},&error),
              "Editing arbitrary-axis cut depth failed");
        check(editable.linkedBooleanCurrent(editableLinked),
              "Edited arbitrary-axis cut left derived body stale");
        check(std::abs(volume(editable.find(editableId)->shape)-(4000-56*std::numbers::pi))<1e-3,
              "Editable arbitrary-axis depth did not regenerate geometry");
        check(!editable.setShapeFeatureTree(editableId,
                 {feature("Cylinder cut arbitrary axis",
                    {{"x",20},{"y",10},{"z",5},{"axis_x",0},{"axis_y",0},{"axis_z",0},
                     {"radius",2},{"depth",14}})},&error),
              "Zero drill axis was accepted");
        check(editable.linkedBooleanCurrent(editableLinked)&&
              std::abs(volume(editable.find(editableId)->shape)-(4000-56*std::numbers::pi))<1e-3,
              "Rejected drill axis damaged existing solid or dependency");

        // Through-all is an ASSOCIATIVE feature: its cutter spans the current
        // input shape after every upstream scaling, not a saved millimetre depth.
        cad::SceneDocument throughDoc;
        check(throughDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),
              "Through-all feature source creation failed");
        const QString throughSource=throughDoc.parts().last().id;
        const auto through=feature("Cylinder through all arbitrary axis",
            {{"x",20},{"y",10},{"z",5},{"axis_x",-1},{"axis_y",0},
             {"axis_z",0},{"radius",2}});
        check(throughDoc.setShapeFeatureTree(throughSource,{through},&error),
              "Arbitrary-axis through-all node failed");
        check(std::abs(volume(throughDoc.find(throughSource)->shape)-
                       (4000-80*std::numbers::pi))<1e-3,
              "Through-all original box bore volume incorrect");
        check(throughDoc.addPrimitive(cad::Primitive::Box,{3,3,3},&error),
              "Through-all linked tool failed");
        const QString throughTool=throughDoc.parts().last().id;
        check(throughDoc.addLinkedBooleanBody(cad::BooleanOp::Fuse,
              throughSource,throughTool,&error),"Through-all linked child failed");
        const QString throughLinked=throughDoc.parts().last().id;
        const auto scale1=feature("Uniform scale",{{"factor",1.5}});
        check(throughDoc.setShapeFeatureTree(throughSource,{scale1,through},&error),
              "Through-all operation did not rebuild after upstream scale");
        check(throughDoc.linkedBooleanCurrent(throughLinked),
              "Through-all scale left Boolean descendant stale");
        check(std::abs(volume(throughDoc.find(throughSource)->shape)-
                       (13500-120*std::numbers::pi))<1e-3,
              "Through-all did not extend to the new scaled thickness");
        const auto scale2=feature("Uniform scale",{{"factor",2.0}});
        check(throughDoc.setShapeFeatureTree(throughSource,{scale2,through},&error),
              "Second upstream scale failed to regenerate through-all cutter");
        check(std::abs(volume(throughDoc.find(throughSource)->shape)-
                       (32000-160*std::numbers::pi))<1e-3,
              "Through-all bore did not follow the second body extent");
        const QString tokenBefore=throughDoc.find(throughSource)->geometryToken;
        auto missingCut=through;
        missingCut.parameters["y"]=1000;
        check(!throughDoc.setShapeFeatureTree(throughSource,{scale2,missingCut},&error),
              "Out-of-body through-all hole was accepted");
        check(throughDoc.find(throughSource)->geometryToken==tokenBefore&&
              throughDoc.linkedBooleanCurrent(throughLinked),
              "Rejected through-all edit changed source or linked child");
        auto zeroAxis=through;
        zeroAxis.parameters["axis_x"]=0;
        check(!throughDoc.setShapeFeatureTree(throughSource,{scale2,zeroAxis},&error),
              "Zero through-all axis was accepted");
        check(throughDoc.find(throughSource)->geometryToken==tokenBefore,
              "Zero-axis rejection mutated the feature output");
        const QString throughFile=temp.path()+"/through-all-feature.cadscene";
        check(throughDoc.saveAs(throughFile,&error),
              "Through-all shape feature could not be persisted");
        cad::SceneDocument throughLoaded;
        check(throughLoaded.load(throughFile,&error),
              "Through-all shape feature could not be reloaded");
        check(throughLoaded.linkedBooleanCurrent(throughLinked)&&
              std::abs(volume(throughLoaded.find(throughSource)->shape)-
                       (32000-160*std::numbers::pi))<1e-3,
              "Through-all feature was stale after save/reopen");

        // Face-attached through-all machining is regenerated from a selected
        // planar face, and the pilot and head geometry share one Boolean
        // history transaction. Both operations remain editable after saving.
        cad::SceneDocument fastenerDoc;
        check(fastenerDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),
              "Fastener stock could not be created");
        const QString stockId=fastenerDoc.parts().last().id;
        int drillFace=0,faceCounter=0;
        for(TopExp_Explorer iter(fastenerDoc.find(stockId)->shape,TopAbs_FACE);
            iter.More();iter.Next()){
            ++faceCounter;
            GProp_GProps f;BRepGProp::SurfaceProperties(iter.Current(),f);
            if(std::abs(f.CentreOfMass().Z()-10.0)<1e-6){drillFace=faceCounter;break;}
        }
        check(drillFace>0,"Planar drilling-face reference not found");
        QMap<QString,double> fastenerAnchor;
        check(cad::capturePlanarFaceAnchor(fastenerDoc.find(stockId)->shape,
              drillFace,&fastenerAnchor,&error),"Could not capture drilling face anchor");
        auto counterbore=feature("Face counterbore through all",
              {{"face_index",double(drillFace)},{"u",0},{"v",0},
               {"radius",2},{"head_radius",4},{"recess_depth",3}});
        for(auto it=fastenerAnchor.cbegin();it!=fastenerAnchor.cend();++it)
            counterbore.parameters.insert(it.key(),it.value());
        check(fastenerDoc.setShapeFeatureTree(stockId,{counterbore},&error),
              "Face counterbore feature failed");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
              (4000-76*std::numbers::pi))<0.001,
              "Counterbore did not remove pilot and head recess volumes");
        // Body translation moves the face while keeping its physical local UV
        // placement. This exercises witnessed face relinking after editing.
        const auto translated=feature("Translate",{{"x",0},{"y",0},{"z",5}});
        check(fastenerDoc.setShapeFeatureTree(stockId,{translated,counterbore},&error),
              "Face counterbore did not follow translated source face");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
              (4000-76*std::numbers::pi))<0.001,
              "Translated counterbore volume mismatch");
        auto widened=counterbore;widened.parameters["head_radius"]=5;
        check(fastenerDoc.setShapeFeatureTree(stockId,{translated,widened},&error),
              "Edited counterbore head radius was not regenerated");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
              (4000-(40+63)*std::numbers::pi))<0.001,
              "Edited counterbore head radius did not change stock volume");
        const QString fastenerToken=fastenerDoc.find(stockId)->geometryToken;
        auto missed=counterbore;missed.parameters["u"]=1000;
        check(!fastenerDoc.setShapeFeatureTree(stockId,{translated,missed},&error),
              "Off-face counterbore was accepted");
        check(fastenerDoc.find(stockId)->geometryToken==fastenerToken,
              "Failed counterbore operation changed committed stock");
        const QString fastenerScene=temp.path()+"/face-fasteners.cadscene";
        check(fastenerDoc.saveAs(fastenerScene,&error),
              "Could not persist face counterbore");
        cad::SceneDocument loadedFasteners;
        check(loadedFasteners.load(fastenerScene,&error),
              "Could not load face counterbore");
        check(std::abs(volume(loadedFasteners.find(stockId)->shape)-
              (4000-103*std::numbers::pi))<0.001,
              "Saved counterbore lost its dimensions");
        auto sink=counterbore;
        sink.id="Face countersink through all";
        sink.type="Face countersink through all";
        sink.parameters.remove("recess_depth");
        sink.parameters["included_angle"]=90;
        check(fastenerDoc.setShapeFeatureTree(stockId,{sink},&error),
              "Face countersink operation failed");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
              (4000-(40.0+32.0/3.0)*std::numbers::pi))<0.001,
              "Countersink conical head removal is incorrect");
        auto invalidAngle=sink;invalidAngle.parameters["included_angle"]=0;
        check(!fastenerDoc.setShapeFeatureTree(stockId,{invalidAngle},&error),
              "Zero countersink included angle was accepted");
        auto invalidHead=sink;invalidHead.parameters["head_radius"]=2;
        check(!fastenerDoc.setShapeFeatureTree(stockId,{invalidHead},&error),
              "Head diameter equal to pilot diameter was accepted");
        // Plain face through-all hole is also a saved, face-tracked feature.
        auto faceBore=sink;faceBore.id="Face through-all hole";
        faceBore.type="Face through-all hole";
        faceBore.parameters.remove("head_radius");
        faceBore.parameters.remove("included_angle");
        check(fastenerDoc.setShapeFeatureTree(stockId,{faceBore},&error),
              "Plain face through-all hole failed");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
              (4000-40*std::numbers::pi))<0.001,
              "Plain face through-all hole did not span the stock");

        // Manufacturing patterns are one saved, face-witnessed feature, not
        // a disconnected collection of fixed-depth cuts. Pattern stations
        // must update when dimensions change and reject invalid placement.
        auto grid=feature("Face hole grid through all",
            {{"face_index",double(drillFace)},{"u",0},{"v",0},{"radius",1},
             {"columns",2},{"rows",2},{"pitch_u",8},{"pitch_v",8},
             {"rotation_degrees",0}});
        for(auto it=fastenerAnchor.cbegin();it!=fastenerAnchor.cend();++it)
            grid.parameters.insert(it.key(),it.value());
        check(fastenerDoc.setShapeFeatureTree(stockId,{grid},&error),
              "Face hole grid failed on solid stock");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000-40*std::numbers::pi))<1e-3,
              "2x2 through grid did not remove four complete holes");
        auto expandedGrid=grid;
        expandedGrid.parameters["columns"]=3;
        expandedGrid.parameters["pitch_u"]=6;
        check(fastenerDoc.setShapeFeatureTree(stockId,{expandedGrid},&error),
              "Editing grid hole count did not rebuild");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000-60*std::numbers::pi))<1e-3,
              "3x2 grid through-all regeneration has wrong volume");
        const QString gridToken=fastenerDoc.find(stockId)->geometryToken;
        auto gridOverlap=expandedGrid;
        gridOverlap.parameters["pitch_u"]=2;
        check(!fastenerDoc.setShapeFeatureTree(stockId,{gridOverlap},&error),
              "Overlapping grid holes were accepted");
        check(fastenerDoc.find(stockId)->geometryToken==gridToken,
              "Invalid hole-grid edit changed the committed model");
        auto outOfFace=expandedGrid;
        outOfFace.parameters["u"]=10;
        check(!fastenerDoc.setShapeFeatureTree(stockId,{outOfFace},&error),
              "Grid with holes outside the face was accepted");
        check(fastenerDoc.find(stockId)->geometryToken==gridToken,
              "Off-face pattern changed the committed model");
        auto rotatedGrid=grid;
        rotatedGrid.parameters["rotation_degrees"]=45;
        check(fastenerDoc.setShapeFeatureTree(stockId,{rotatedGrid},&error),
              "Rotated four-hole grid failed");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000-40*std::numbers::pi))<1e-3,
              "Rotated face grid removed wrong volume");
        check(fastenerDoc.setShapeFeatureTree(stockId,{translated,grid},&error),
              "Grid did not follow a translated witnessed face");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000-40*std::numbers::pi))<1e-3,
              "Translated grid changed its material removal");
        auto bolt=feature("Face bolt circle through all",
            {{"face_index",double(drillFace)},{"u",0},{"v",0},{"radius",1},
             {"count",4},{"pcd",12},{"start_angle_degrees",0}});
        for(auto it=fastenerAnchor.cbegin();it!=fastenerAnchor.cend();++it)
            bolt.parameters.insert(it.key(),it.value());
        check(fastenerDoc.setShapeFeatureTree(stockId,{bolt},&error),
              "Four-hole face bolt circle failed");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000-40*std::numbers::pi))<1e-3,
              "Bolt circle removed the wrong number of holes");
        auto rotatedBolt=bolt;
        rotatedBolt.parameters["start_angle_degrees"]=45;
        check(fastenerDoc.setShapeFeatureTree(stockId,{rotatedBolt},&error),
              "Rotating bolt-circle stations failed");
        const auto boltToken=fastenerDoc.find(stockId)->geometryToken;
        auto crowdedBolt=bolt;
        crowdedBolt.parameters["pcd"]=2;
        check(!fastenerDoc.setShapeFeatureTree(stockId,{crowdedBolt},&error),
              "Overlapping bolt-circle holes were accepted");
        check(fastenerDoc.find(stockId)->geometryToken==boltToken,
              "Rejected bolt-circle edit damaged model");
        const QString patternScene=temp.path()+"/machining-patterns.cadscene";
        check(fastenerDoc.saveAs(patternScene,&error),
              "Could not save bolt-circle feature");
        cad::SceneDocument loadedPattern;
        check(loadedPattern.load(patternScene,&error),
              "Could not load bolt-circle feature");
        check(loadedPattern.find(stockId)->hasShapeFeatureTree&&
              loadedPattern.find(stockId)->shapeFeatureNodes.first().type==
                   QStringLiteral("Face bolt circle through all")&&
              std::abs(volume(loadedPattern.find(stockId)->shape)-
                       (4000-40*std::numbers::pi))<1e-3,
              "Saved bolt circle lost its geometry or parameters");

        // Obround milling/drilling on a general solid: the slot's two round
        // ends and rectangular bridge form one editable, witnessed feature.
        // The length parameter is the distance BETWEEN arc centers, not the
        // overall length. A through-all slot must follow upstream thickness.
        auto slot=feature("Face slot through all",
            {{"face_index",double(drillFace)},{"u",0},{"v",0},
             {"radius",2},{"center_spacing",8},{"rotation_degrees",0}});
        for(auto it=fastenerAnchor.cbegin();it!=fastenerAnchor.cend();++it)
            slot.parameters.insert(it.key(),it.value());
        check(fastenerDoc.setShapeFeatureTree(stockId,{slot},&error),
              "Face obround through-all slot failed");
        const double slotArea=32.0+4.0*std::numbers::pi;
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000.0-10.0*slotArea))<1e-3,
              "Obround through-all slot volume incorrect");
        check(fastenerDoc.setShapeFeatureTree(stockId,{translated,slot},&error),
              "Witnessed slot did not follow source translation");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000.0-10.0*slotArea))<1e-3,
              "Translated through-all obround slot changed its volume");
        auto longerSlot=slot;
        longerSlot.parameters["center_spacing"]=10;
        longerSlot.parameters["rotation_degrees"]=30;
        check(fastenerDoc.setShapeFeatureTree(stockId,{longerSlot},&error),
              "Editable rotated obround slot failed");
        const double longerArea=40.0+4.0*std::numbers::pi;
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000.0-10.0*longerArea))<1e-3,
              "Rotated slotted cut did not regenerate to edited dimensions");
        const QString slotToken=fastenerDoc.find(stockId)->geometryToken;
        auto invalidSlot=longerSlot;
        invalidSlot.parameters["center_spacing"]=-1;
        check(!fastenerDoc.setShapeFeatureTree(stockId,{invalidSlot},&error),
              "Negative obround center spacing was accepted");
        check(fastenerDoc.find(stockId)->geometryToken==slotToken,
              "Rejected slot edit changed committed stock");
        auto blindSlot=slot;
        blindSlot.id="Face slot pocket";
        blindSlot.type="Face slot pocket";
        blindSlot.parameters.insert("depth",4);
        check(fastenerDoc.setShapeFeatureTree(stockId,{blindSlot},&error),
              "Face obround blind pocket failed");
        check(std::abs(volume(fastenerDoc.find(stockId)->shape)-
                       (4000.0-4.0*slotArea))<1e-3,
              "Obround blind pocket did not respect its saved depth");
        const QString slotScene=temp.path()+"/face-slot.cadscene";
        check(fastenerDoc.saveAs(slotScene,&error),
              "Could not save editable obround pocket");
        cad::SceneDocument reloadedSlot;
        check(reloadedSlot.load(slotScene,&error),
              "Could not reload editable obround pocket");
        check(std::abs(volume(reloadedSlot.find(stockId)->shape)-
                       (4000.0-4.0*slotArea))<1e-3,
              "Reloaded obround pocket changed its saved dimensions");

        // A merged planar face may acquire a different center of area while
        // the original witnessed design region remains in place. A feature
        // must not drift to the new face centroid. This targets the topology
        // and identity bug, not merely the total volume of removed material.
        const TopoDS_Shape baseStock=BRepPrimAPI_MakeBox(20,20,10).Shape();
        const TopoDS_Shape extension=BRepPrimAPI_MakeBox(
            gp_Pnt(20,0,0),gp_Pnt(40,20,10)).Shape();
        BRepAlgoAPI_Fuse expanded(baseStock,extension);expanded.Build();
        check(expanded.IsDone(),"Could not fuse coplanar stock extension");
        expanded.SimplifyResult(true,true);
        TopoDS_Shape mergedOutput;
        check(cad::rebuildShapeFeatureTree(expanded.Shape(),false,{faceBore},
                                          &mergedOutput,&error),
              "Face-attached hole failed after coplanar source merge");
        const TopoDS_Shape originalHoleProbe=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(10,10,10),gp_Dir(0,0,-1)),1,10).Shape();
        BRepAlgoAPI_Common remainingAtDatum(mergedOutput,originalHoleProbe);
        remainingAtDatum.Build();
        check(remainingAtDatum.IsDone()&&
              volume(remainingAtDatum.Shape())<1e-4,
              "Coplanar face merge moved a witnessed machining datum");
        // In-plane translation is different from a coplanar merge: the
        // selected face retains its area, and both witnesses remain inside.
        // Its machining datum MUST follow the rigid body translation.
        TopoDS_Shape translatedOutput;
        const auto moveInPlane=feature("Translate",{{"x",5},{"y",0},{"z",0}});
        check(cad::rebuildShapeFeatureTree(baseStock,false,{moveInPlane,faceBore},
                                          &translatedOutput,&error),
              "In-plane translated face hole failed regeneration");
        const TopoDS_Shape translatedHoleProbe=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(15,10,10),gp_Dir(0,0,-1)),1,10).Shape();
        BRepAlgoAPI_Common translationCommon(translatedOutput,translatedHoleProbe);
        translationCommon.Build();
        check(translationCommon.IsDone()&&volume(translationCommon.Shape())<1e-4,
              "In-plane translation failed to move the hole with its source face");
        TopoDS_Shape mergedSlotOutput;
        check(cad::rebuildShapeFeatureTree(expanded.Shape(),false,{slot},
                                          &mergedSlotOutput,&error),
              "Obround slot failed after co-planar stock merge");
        const TopoDS_Shape slotLocationProbe=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(6,10,10),gp_Dir(0,0,-1)),1,10).Shape();
        BRepAlgoAPI_Common remainingInSlot(mergedSlotOutput,slotLocationProbe);
        remainingInSlot.Build();
        check(remainingInSlot.IsDone()&&volume(remainingInSlot.Shape())<1e-4,
              "Co-planar face merge displaced the slot from its design datum");

        // A direct hole that eliminates the whole intersection of a linked
        // child must be rejected with full source/descendant rollback.
        cad::SceneDocument rollback;
        check(rollback.addPrimitive(cad::Primitive::Box,{20,20,10},&error),
              "Hole-rollback source failed");
        const QString rollbackSource=rollback.parts().last().id;
        check(rollback.addPrimitive(cad::Primitive::Cylinder,{2,10},&error),
              "Hole-rollback tool failed");
        const QString rollbackTool=rollback.parts().last().id;
        check(rollback.translate(rollbackTool,10,10,0,&error),
              "Hole-rollback tool placement failed");
        check(rollback.addLinkedBooleanBody(cad::BooleanOp::Common,rollbackSource,
                                            rollbackTool,&error),
              "Hole-rollback linked intersection failed");
        const QString rollbackChild=rollback.parts().last().id;
        const auto rollbackSourceToken=rollback.find(rollbackSource)->geometryToken;
        const double oldSourceVolume=volume(rollback.find(rollbackSource)->shape);
        const double oldChildVolume=volume(rollback.find(rollbackChild)->shape);
        cad::HoleCommand annihilating;
        check(rollback.prepareHole(rollbackSource,10,10,10,6,1,true,
                                   &annihilating,&error),
              "Hole-rollback preparation failed");
        const auto destructive=cad::SceneDocument::buildHolePreview(annihilating);
        check(!destructive.shape.IsNull(),"Hole-rollback source preview failed");
        check(!rollback.commitHole(annihilating,destructive.shape,&error),
              "Hole destroyed a dependent intersection but was committed");
        check(rollback.find(rollbackSource)->geometryToken==rollbackSourceToken&&
              std::abs(volume(rollback.find(rollbackSource)->shape)-oldSourceVolume)<1e-6&&
              std::abs(volume(rollback.find(rollbackChild)->shape)-oldChildVolume)<1e-6&&
              rollback.linkedBooleanCurrent(rollbackChild),
              "Failed dependent regeneration changed an original body or its child");

        cad::SceneDocument edgeDoc;
        check(edgeDoc.addPrimitive(cad::Primitive::Box,{30,20,10},&error),"Edge fixture failed");
        QMap<QString,double> edgeAnchor;
        check(cad::captureStableEdgeAnchor(edgeDoc.parts().first().shape,1,&edgeAnchor,&error),
              "Could not capture edge anchor");
        int reidentified=0;
        check(cad::resolveStableEdgeAnchor(edgeDoc.parts().first().shape,edgeAnchor,&reidentified,&error)&&
              reidentified==1,"Edge anchor did not resolve its original edge");
        check(!cad::resolveStableEdgeAnchor(edgeDoc.parts().first().shape,{},&reidentified,&error),
              "Incomplete edge anchor was accepted");
        // Box edges are shared by adjacent faces. Each *distinct* edge must be
        // resolvable without a duplicate occurrence triggering false ambiguity.
        TopTools_IndexedMapOfShape uniqueEdges;
        TopExp::MapShapes(edgeDoc.parts().first().shape,TopAbs_EDGE,uniqueEdges);
        check(uniqueEdges.Extent()==12,"Box fixture did not have twelve distinct edges");
        for(int uniqueIndex=1;uniqueIndex<=uniqueEdges.Extent();++uniqueIndex){
            QMap<QString,double> eachAnchor;
            check(cad::captureStableEdgeAnchor(edgeDoc.parts().first().shape,
                                               uniqueIndex,&eachAnchor,&error),
                  "Cannot capture a distinct box edge");
            int matched=-1;
            check(cad::resolveStableEdgeAnchor(edgeDoc.parts().first().shape,
                                               eachAnchor,&matched,&error)&&matched==uniqueIndex,
                  "A distinct box edge did not round-trip through geometric matching");
        }
        // A new anchor is body-relative, so whole-part translations do not
        // change which edge is identified. Legacy four-value hints still work.
        QMap<QString,double> legacyEdge=edgeAnchor;
        legacyEdge.remove("edge_body_x");legacyEdge.remove("edge_body_y");legacyEdge.remove("edge_body_z");
        check(cad::resolveStableEdgeAnchor(edgeDoc.parts().first().shape,
                                           legacyEdge,&reidentified,&error),
              "Legacy four-field edge hint stopped resolving");
        TopoDS_Shape movedEdgeShape=edgeDoc.parts().first().shape;
        gp_Trsf shift;shift.SetTranslation(gp_Vec(1650,-750,420));
        movedEdgeShape=BRepBuilderAPI_Transform(movedEdgeShape,shift,true).Shape();
        check(cad::resolveStableEdgeAnchor(movedEdgeShape,edgeAnchor,&reidentified,&error),
              "Body-relative edge hint failed after large translation");
        QMap<QString,double> incompleteBody=edgeAnchor;incompleteBody.remove("edge_body_z");
        check(!cad::resolveStableEdgeAnchor(movedEdgeShape,incompleteBody,&reidentified,&error),
              "Incomplete edge reference center was accepted");
        check(!cad::captureStableEdgeAnchor(edgeDoc.parts().first().shape,
                                             uniqueEdges.Extent()+1,&edgeAnchor,&error),
              "Nonexistent unique edge index was accepted");
        QMap<QString,double> invalidEdgeAnchor={{"edge_x",0.},{"edge_y",0.},
            {"edge_z",0.},{"edge_length",-1.}};
        check(!cad::resolveStableEdgeAnchor(edgeDoc.parts().first().shape,
                                             invalidEdgeAnchor,&reidentified,&error),
              "Negative edge length was accepted");
        // Different topological edges may coincide geometrically. In that
        // case a geometric hint must fail closed, rather than guess an edge.
        TopoDS_Compound ambiguousBody;
        BRep_Builder builder;
        builder.MakeCompound(ambiguousBody);
        builder.Add(ambiguousBody,BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,0),gp_Pnt(10,0,0)).Edge());
        builder.Add(ambiguousBody,BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,0),gp_Pnt(10,0,0)).Edge());
        QMap<QString,double> ambiguousAnchor;
        check(cad::captureStableEdgeAnchor(ambiguousBody,1,&ambiguousAnchor,&error),
              "Ambiguous geometry fixture could not capture a source edge");
        check(!cad::resolveStableEdgeAnchor(ambiguousBody,ambiguousAnchor,&reidentified,&error),
              "Identical but distinct edges should not be selected arbitrarily");

        cad::SceneDocument faceDoc;
        check(faceDoc.addPrimitive(cad::Primitive::Box,{30,30,12},&error),"Face fixture failed");
        const auto original=faceDoc.parts().first();
        int witnessedFaceIndex=0,topIndex=0;
        for(TopExp_Explorer e(original.shape,TopAbs_FACE);e.More();e.Next()){
            ++witnessedFaceIndex;
            const TopoDS_Face face=TopoDS::Face(e.Current());
            BRepAdaptor_Surface s(face);if(s.GetType()!=GeomAbs_Plane)continue;
            GProp_GProps props;BRepGProp::SurfaceProperties(face,props);
            if(std::abs(props.CentreOfMass().Z()-12)<0.001){topIndex=witnessedFaceIndex;break;}
        }
        check(topIndex>0,"Could not find a top planar face");
        QMap<QString,double> anchor;
        check(cad::capturePlanarFaceAnchor(original.shape,topIndex,&anchor,&error),"Anchor capture failed");
        // Deliberately wrong face traversal index. The anchor must resolve the top.
        cad::ShapeFeatureNode pocket=feature("Face normal pocket",{{"face_index",topIndex==1?2.0:1.0},
            {"u",0},{"v",0},{"radius",2},{"depth",7}});
        for(auto it=anchor.cbegin();it!=anchor.cend();++it)pocket.parameters.insert(it.key(),it.value());
        check(faceDoc.setShapeFeatureTree(original.id,{pocket},&error),"Stable planar anchor did not resolve: ");
        check(volume(faceDoc.parts().first().shape)<10800-70,"Anchor pocket did not cut the expected body");
        anchor["face_anchor_area"]=-1;
        pocket.parameters["face_anchor_area"]=-1;
        check(!cad::validateShapeFeatureNodes({pocket},&error),"Invalid anchor passed validation");

        const QList<cad::ShapeFeatureNode> forms={
            feature("Revolve annulus Z",{{"inner_radius",3},{"outer_radius",7},{"height",15}}),
            feature("Sweep circle path",{{"radius",2},{"path_length",30},
                                       {"lateral_offset",5},{"vertical_rise",0}}),
            feature("Loft rectangles",{{"width0",10},{"depth0",10},{"width1",20},
                                        {"depth1",15},{"height",20},{"offset_x",2},{"offset_y",2}})
        };
        for(const auto& form:forms){
            cad::SceneDocument doc;
            check(doc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Form base failed");
            const QString id=doc.parts().first().id;
            check(doc.setShapeFeatureTree(id,{form},&error),"One editable form operation failed");
            check(doc.shapeFeatureTreeCurrent(id)&&BRepCheck_Analyzer(doc.find(id)->shape).IsValid(),
                  "Form operation did not create valid feature-tree B-rep");
            check(doc.undo()&&doc.redo(),"Form operation did not support undo/redo");
        }
        std::cout<<"PASS: linked multi-body history, stable face references, additive form features\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
    }
}
