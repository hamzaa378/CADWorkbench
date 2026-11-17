#include "core/SceneDocument.h"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include "core/ShapeFeatureTree.h"
#include "core/BooleanReferenceLineage.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <BRepGProp.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <gp_Pnt.hxx>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <iostream>
#include <stdexcept>
#include <cmath>
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
cad::SketchModel sketch(double width){
    cad::SketchModel s;
    s.points={{"a",0,0},{"b",width,0},{"c",width,10},{"d",0,10}};
    s.segments={{"e1","a","b"},{"e2","b","c"},{"e3","c","d"},{"e4","d","a"}};
    return s;
}
cad::SketchModel sketchAtX(double origin,double width){
    cad::SketchModel s=sketch(width);
    for(auto& point:s.points)point.x+=origin;
    return s;
}
QStringList boundary(){return {"a","b","c","d"};}
double volume(const cad::SceneDocument& d,const QString& id){
    GProp_GProps p;BRepGProp::VolumeProperties(d.find(id)->shape,p);return p.Mass();
}
cad::InterleavedHistoryStep translate(const QString& id,double x){
    cad::InterleavedHistoryStep s;s.id=id;s.kind="Shape";
    s.feature.id=id;s.feature.type="Translate";
    s.feature.parameters={{"x",x},{"y",0},{"z",0}};
    return s;
}
cad::InterleavedHistoryStep sketchStep(const QString& id,const QString& source,const QString& kind){
    cad::InterleavedHistoryStep s;s.id=id;s.kind=kind;s.sketchSourceId=source;
    s.depthMm=5;s.offsetMm=8;return s;
}
void run(){
    cad::SceneDocument d;QString error;
    check(d.addConstrainedPolygonExtrusion(sketch(10),boundary(),10,&error),"first sketch");
    const QString first=d.parts().last().id;
    check(d.addConstrainedPolygonExtrusion(sketch(5),boundary(),10,&error),"second sketch");
    const QString second=d.parts().last().id;
    check(d.addPrimitive(cad::Primitive::Box,{12,12,10},&error),"target");
    const QString target=d.parts().last().id;
    check(!d.setInterleavedFeatureHistory(target,{translate("just-translate",2)},&error),
          "shape-only history uses ordinary feature tree instead");
    check(!d.find(target)->hasInterleavedHistory,"rejected shape-only history unchanged");
    QVector<cad::InterleavedHistoryStep> steps={
        translate("move-before-boss",2),sketchStep("boss",first,"SketchFuse"),
        translate("move-between-features",-2),sketchStep("pocket",second,"SketchCut")};
    check(d.setInterleavedFeatureHistory(target,steps,&error),"interleaved apply");
    check(d.interleavedFeatureHistoryCurrent(target),"history current");
    // Detached dependency preflight must mirror the actual full scene
    // propagation and never modify the committed target or the caller's
    // previously successful report on a bad candidate.
    const QString initialHistoryToken=d.find(target)->geometryToken;
    QString fullPreview="previous report";
    check(d.previewInterleavedHistoryWithDependents(target,steps,&fullPreview,&error)&&
          fullPreview.contains("FULL PARAMETRIC DEPENDENCY PREFLIGHT")&&
          d.find(target)->geometryToken==initialHistoryToken,
          "full dependent-feature preview must validate without committing");
    const QString goodPreview=fullPreview;
    auto invalidPreviewSteps=steps;invalidPreviewSteps[1].offsetMm=10000.0;
    check(!d.previewInterleavedHistoryWithDependents(target,invalidPreviewSteps,&fullPreview,&error)&&
          fullPreview==goodPreview&&d.find(target)->geometryToken==initialHistoryToken,
          "invalid dependent-feature preview must preserve document and report");

    check(std::abs(volume(d,target)-1530)<0.01,"interleaved volume");
    QStringList affected;
    check(d.linkedBodyImpact(first,&affected,&error)&&affected.contains(target),"source in dependency graph");
    check(!d.erase(second,&error),"dependency protected from deletion");
    check(d.updateConstrainedPolygonExtrusion(first,sketch(8),boundary(),10,&error),"upstream boss source edit");
    check(std::abs(volume(d,target)-1470)<0.01,"upstream first source regenerated all steps");
    check(d.updateConstrainedPolygonExtrusion(second,sketch(4),boundary(),10,&error),"upstream pocket source edit");
    check(std::abs(volume(d,target)-1520)<0.01,"upstream second source regenerated all steps");
    const QString committed=d.find(target)->geometryToken;
    auto bad=steps;bad[1].offsetMm=100;
    check(!d.setInterleavedFeatureHistory(target,bad,&error),"detached boss rejected");
    check(d.find(target)->geometryToken==committed,"rejected edit rolled back");
    check(d.undo(),"undo source edit");
    check(std::abs(volume(d,target)-1470)<0.01,"undo restored previous sketch-linked result");
    check(d.redo(),"redo source edit");
    check(std::abs(volume(d,target)-1520)<0.01,"redo restored final sketch-linked result");
    QTemporaryDir temp;check(temp.isValid(),"temporary folder");
    const auto file=temp.path()+"/interleaved.cadscene";
    check(d.saveAs(file,&error),"save interleaved history");
    cad::SceneDocument loaded;
    check(loaded.load(file,&error),"reopen interleaved history");
    check(loaded.interleavedFeatureHistoryCurrent(target),"loaded history current");
    check(std::abs(volume(loaded,target)-1520)<0.01,"reopened final volume");
    check(loaded.updateConstrainedPolygonExtrusion(first,sketch(9),boundary(),10,&error),"edit after reopening");
    check(std::abs(volume(loaded,target)-1550)<0.01,"reopened source edit regenerated");
    QFile in(file);check(in.open(QIODevice::ReadOnly),"read saved document");
    auto document=QJsonDocument::fromJson(in.readAll());in.close();
    auto root=document.object();auto parts=root.value("parts").toArray();
    for(int i=0;i<parts.size();++i){
        auto part=parts[i].toObject();
        if(!part.contains("interleaved_history"))continue;
        auto history=part.value("interleaved_history").toObject();
        auto entries=history.value("steps").toArray();
        auto step=entries[1].toObject();step.insert("source","missing-sketch-body");
        entries[1]=step;history.insert("steps",entries);part.insert("interleaved_history",history);
        parts[i]=part;
    }
    root.insert("parts",parts);check(in.open(QIODevice::WriteOnly|QIODevice::Truncate),"write corrupt file");
    in.write(QJsonDocument(root).toJson());in.close();
    const auto unchanged=volume(loaded,target);
    check(!loaded.load(file,&error),"corrupted source references rejected");
    check(std::abs(volume(loaded,target)-unchanged)<0.01,"failed reload preserved open scene");

    // A real fork/join DAG: both transformations consume the same earlier
    // sketch result rather than the preceding step. A Boolean combines them.
    cad::SceneDocument branched;
    check(branched.addConstrainedPolygonExtrusion(sketch(10),boundary(),10,&error),"branch source");
    const QString sourceId=branched.parts().last().id;
    check(branched.addPrimitive(cad::Primitive::Box,{12,12,10},&error),"branch base");
    const QString partId=branched.parts().last().id;
    auto stage=sketchStep("boss-source",sourceId,"SketchFuse");
    stage.inputs={"@base"};
    auto left=translate("left-branch",-2);left.inputs={"boss-source"};
    auto right=translate("right-branch",2);right.inputs={"boss-source"};
    cad::InterleavedHistoryStep merge;
    merge.id="joined";merge.kind="Shape";merge.feature.id="joined";
    merge.feature.type="Fuse branches";
    merge.inputs={"left-branch","right-branch"};
    const QVector<cad::InterleavedHistoryStep> dag={stage,left,right,merge};
    check(branched.setInterleavedFeatureHistory(partId,dag,&error),"fork/join history built");
    check(branched.interleavedFeatureHistoryCurrent(partId),"fork/join current");
    const double initial=volume(branched,partId);
    check(initial>1440.0,"fork/join produced a connected expanded solid");
    check(branched.updateConstrainedPolygonExtrusion(sourceId,sketch(8),boundary(),10,&error),
          "upstream source rebuilds both branches");
    const double updated=volume(branched,partId);
    check(std::abs(initial-updated)>1.0,"source edit changes combined branch result");
    const QString goodToken=branched.find(partId)->geometryToken;
    auto forward=dag;forward[1].inputs={"joined"};
    check(!branched.setInterleavedFeatureHistory(partId,forward,&error),"forward dependency rejected");
    check(branched.find(partId)->geometryToken==goodToken,"forward edit rolled back");
    auto duplicate=dag;duplicate[3].inputs={"left-branch","left-branch"};
    check(!branched.setInterleavedFeatureHistory(partId,duplicate,&error),"duplicate parent rejected");
    check(branched.find(partId)->geometryToken==goodToken,"duplicate edit rolled back");
    auto invalidCombine=dag;invalidCombine[3].inputs={"left-branch"};
    check(!branched.setInterleavedFeatureHistory(partId,invalidCombine,&error),"one-input Boolean rejected");
    QTemporaryDir branchDir;check(branchDir.isValid(),"fork/join temp directory");
    const QString branchFile=branchDir.path()+"/branched.cadscene";
    check(branched.saveAs(branchFile,&error),"save branching graph");
    QFile branchSaved(branchFile);check(branchSaved.open(QIODevice::ReadOnly),"open saved graph");
    auto branchJson=QJsonDocument::fromJson(branchSaved.readAll());branchSaved.close();
    auto branchParts=branchJson.object().value("parts").toArray();
    bool foundDag=false;
    for(const auto& entry:branchParts){
        if(entry.toObject().contains("interleaved_history")){
            foundDag=entry.toObject().value("interleaved_history").toObject().value("type").toString()==
                QStringLiteral("dag-interleaved-history-v2");
        }
    }
    check(foundDag,"branching history saved as versioned DAG");
    cad::SceneDocument reopenedDAG;
    check(reopenedDAG.load(branchFile,&error),"reopen branching history");
    check(reopenedDAG.interleavedFeatureHistoryCurrent(partId),"reopened branching graph valid");
    check(std::abs(volume(reopenedDAG,partId)-updated)<0.001,"reopened branching geometry preserved");
    // Tamper with the persisted graph and verify atomic load rejection.
    auto malformedRoot=branchJson.object();auto modifiedParts=malformedRoot.value("parts").toArray();
    for(int i=0;i<modifiedParts.size();++i){
        auto obj=modifiedParts[i].toObject();
        if(!obj.contains("interleaved_history"))continue;
        auto hist=obj.value("interleaved_history").toObject();
        auto records=hist.value("steps").toArray();
        auto fork=records[1].toObject();fork.insert("inputs",QJsonArray{QStringLiteral("joined")});
        records[1]=fork;hist.insert("steps",records);
        obj.insert("interleaved_history",hist);modifiedParts[i]=obj;
    }
    malformedRoot.insert("parts",modifiedParts);
    check(branchSaved.open(QIODevice::WriteOnly|QIODevice::Truncate),"write malformed graph");
    branchSaved.write(QJsonDocument(malformedRoot).toJson());branchSaved.close();
    check(!reopenedDAG.load(branchFile,&error),"saved forward reference rejected");
    check(std::abs(volume(reopenedDAG,partId)-updated)<0.001,"failed graph load preserved document");

    // Downstream fillet attaches to an edge of the frozen base, *through* a
    // live sketch-driven Boolean. A sketch edit rebuilds the Boolean, remaps
    // the edge with kernel history, and regenerates the fillet transactionally.
    cad::SceneDocument tracked;QString issue;
    check(tracked.addConstrainedPolygonExtrusion(sketch(10),boundary(),10,&issue),
          "tracked sketch source created");
    const QString liveSource=tracked.parts().last().id;
    check(tracked.addPrimitive(cad::Primitive::Box,{12,12,10},&issue),
          "tracked Boolean target created");
    const QString livePart=tracked.parts().last().id;
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(tracked.find(livePart)->shape,TopAbs_EDGE,edges);
    int originalEdge=0;
    for(int i=1;i<=edges.Extent();++i){
        GProp_GProps props;BRepGProp::LinearProperties(edges.FindKey(i),props);
        const gp_Pnt center=props.CentreOfMass();
        if(std::abs(center.X()-6)<1e-5&&std::abs(center.Y())<1e-5&&
           std::abs(center.Z())<1e-5){originalEdge=i;break;}
    }
    check(originalEdge>0,"tracked lower box edge located");
    QMap<QString,double> storedAnchor;
    check(cad::captureStableEdgeAnchor(tracked.find(livePart)->shape,originalEdge,&storedAnchor,&issue),
          "tracked source edge witness captured");
    auto trackedBoss=sketchStep("live-boss",liveSource,"SketchFuse");
    cad::InterleavedHistoryStep trackedFillet;
    trackedFillet.id="downstream-fillet";trackedFillet.kind="Shape";
    trackedFillet.inputs={"live-boss"};trackedFillet.referenceSourceId="@base";
    trackedFillet.feature.id=trackedFillet.id;
    trackedFillet.feature.type="Fillet selected edge";
    trackedFillet.feature.parameters={{"edge_index",double(originalEdge)},{"radius",0.2}};
    for(auto it=storedAnchor.cbegin();it!=storedAnchor.cend();++it)
        trackedFillet.feature.parameters.insert(it.key(),it.value());
    const QVector<cad::InterleavedHistoryStep> linkedFeature={trackedBoss,trackedFillet};
    check(tracked.setInterleavedFeatureHistory(livePart,linkedFeature,&issue),
          "live Boolean with downstream selected-edge fillet");
    check(tracked.interleavedFeatureHistoryCurrent(livePart),"live topology feature current");
    const double beforeUpdate=volume(tracked,livePart);
    check(tracked.updateConstrainedPolygonExtrusion(liveSource,sketch(8),boundary(),10,&issue),
          "source edit regenerates Boolean and selected fillet");
    const double afterUpdate=volume(tracked,livePart);
    check(std::abs(beforeUpdate-afterUpdate)>1,"sketch edit changed final filleted volume");
    check(tracked.interleavedFeatureHistoryCurrent(livePart),"downstream topology feature is current");
    const QString stableToken=tracked.find(livePart)->geometryToken;
    auto wrongBranch=linkedFeature;
    wrongBranch[1].referenceSourceId="missing-branch";
    check(!tracked.setInterleavedFeatureHistory(livePart,wrongBranch,&issue),
          "unknown Boolean reference branch rejected");
    check(tracked.find(livePart)->geometryToken==stableToken,
          "failed retarget preserved last committed feature result");
    QTemporaryDir trackedDir;check(trackedDir.isValid(),"tracked history temporary directory");
    const QString trackedPath=trackedDir.path()+"/tracked.cadscene";
    check(tracked.saveAs(trackedPath,&issue),"save topology-linked history");
    QFile trackedFile(trackedPath);
    check(trackedFile.open(QIODevice::ReadOnly),"read topology-linked project");
    const QJsonDocument trackedJson=QJsonDocument::fromJson(trackedFile.readAll());
    trackedFile.close();
    bool foundV3=false;
    for(const auto& value:trackedJson.object().value("parts").toArray()){
        const auto hist=value.toObject().value("interleaved_history").toObject();
        if(hist.isEmpty())continue;
        foundV3=hist.value("type").toString()==QStringLiteral("dag-interleaved-history-v3")&&
            hist.value("steps").toArray().last().toObject().value("reference_source").toString()==
                QStringLiteral("@base");
    }
    check(foundV3,"saved topology-linked history uses strictly versioned v3 schema");
    cad::SceneDocument restored;
    check(restored.load(trackedPath,&issue),"reopen Boolean reference links");
    check(std::abs(volume(restored,livePart)-afterUpdate)<0.01,
          "reopened downstream fillet matches committed shape");
    check(restored.updateConstrainedPolygonExtrusion(liveSource,sketch(9),boundary(),10,&issue),
          "reopened topology link regenerates on another source edit");
    check(restored.interleavedFeatureHistoryCurrent(livePart),"reopened edited history current");
    auto corruptedRoot=trackedJson.object();auto corruptParts=corruptedRoot.value("parts").toArray();
    for(int i=0;i<corruptParts.size();++i){
        auto obj=corruptParts[i].toObject();auto hist=obj.value("interleaved_history").toObject();
        if(hist.isEmpty())continue;
        auto stages=hist.value("steps").toArray();auto last=stages.last().toObject();
        last.insert("reference_source",QStringLiteral("nonexistent"));
        stages[stages.size()-1]=last;hist.insert("steps",stages);
        obj.insert("interleaved_history",hist);corruptParts[i]=obj;
    }
    corruptedRoot.insert("parts",corruptParts);
    check(trackedFile.open(QIODevice::WriteOnly|QIODevice::Truncate),"write invalid topology branch");
    trackedFile.write(QJsonDocument(corruptedRoot).toJson());trackedFile.close();
    const QString preserved=restored.find(livePart)->geometryToken;
    check(!restored.load(trackedPath,&issue),"tampered branch provenance rejected on load");
    check(restored.find(livePart)->geometryToken==preserved,
          "failed topology-link load did not mutate the open document");

    // A reference from the frozen base must survive TWO consecutive sketch
    // Booleans. Capture the original geometry once and always remap through
    // the current kernel history (not stored topology indices).
    cad::SceneDocument chained;
    check(chained.addConstrainedPolygonExtrusion(sketch(6),boundary(),10,&issue),
          "chain first sketch source");
    const QString chainFirst=chained.parts().last().id;
    check(chained.addConstrainedPolygonExtrusion(sketch(3),boundary(),10,&issue),
          "chain second sketch source");
    const QString chainSecond=chained.parts().last().id;
    check(chained.addPrimitive(cad::Primitive::Box,{12,12,10},&issue),"chain target created");
    const QString chainTarget=chained.parts().last().id;
    TopTools_IndexedMapOfShape chainEdges;
    TopExp::MapShapes(chained.find(chainTarget)->shape,TopAbs_EDGE,chainEdges);
    int chainEdge=0;
    for(int i=1;i<=chainEdges.Extent();++i){
        GProp_GProps props;BRepGProp::LinearProperties(chainEdges.FindKey(i),props);
        const gp_Pnt center=props.CentreOfMass();
        if(std::abs(center.X()-6)<1e-5&&std::abs(center.Y())<1e-5&&
           std::abs(center.Z())<1e-5){chainEdge=i;break;}
    }
    check(chainEdge>0,"chain base lower edge fixture");
    QMap<QString,double> chainAnchor;
    check(cad::captureStableEdgeAnchor(chained.find(chainTarget)->shape,chainEdge,&chainAnchor,&issue),
          "chain base edge witness capture");
    auto chainBoss=sketchStep("first-boolean",chainFirst,"SketchFuse");
    chainBoss.inputs={"@base"};
    auto chainPocket=sketchStep("second-boolean",chainSecond,"SketchCut");
    chainPocket.inputs={"first-boolean"};
    auto chainFillet=trackedFillet;
    chainFillet.id="chain-fillet";chainFillet.feature.id=chainFillet.id;
    chainFillet.inputs={"second-boolean"};chainFillet.referenceSourceId="@base";
    chainFillet.feature.parameters={{"edge_index",double(chainEdge)},{"radius",0.2}};
    for(auto it=chainAnchor.cbegin();it!=chainAnchor.cend();++it)
        chainFillet.feature.parameters.insert(it.key(),it.value());
    const QVector<cad::InterleavedHistoryStep> twoBooleans={chainBoss,chainPocket,chainFillet};
    const QString previewToken=chained.find(chainTarget)->geometryToken;
    QMap<QString,TopoDS_Shape> previews;
    check(chained.previewInterleavedFeatureHistory(chainTarget,{chainBoss,chainPocket},&previews,&issue),
          "read only staged intermediate geometry preview");
    check(previews.contains("@base")&&previews.contains("first-boolean")&&
          previews.contains("second-boolean"),"intermediate solids returned by stable IDs");
    check(chained.find(chainTarget)->geometryToken==previewToken&&
          !chained.find(chainTarget)->hasInterleavedHistory,"preview made no document changes");
    auto invalidPreview=chainPocket;invalidPreview.inputs={"missing-feature"};
    check(!chained.previewInterleavedFeatureHistory(chainTarget,{chainBoss,invalidPreview},
          &previews,&issue),"invalid staged prefix rejected");
    check(previews.contains("first-boolean")&&chained.find(chainTarget)->geometryToken==previewToken,
          "failed staged prefix did not replace prior preview or scene");
    check(chained.setInterleavedFeatureHistory(chainTarget,twoBooleans,&issue),
          "two Boolean stages and downstream selected-edge fillet rebuilt");
    const double chainBefore=volume(chained,chainTarget);
    check(chained.updateConstrainedPolygonExtrusion(chainFirst,sketch(8),boundary(),10,&issue),
          "two-Boolean edge ancestry regenerated after first source edit");
    check(chained.updateConstrainedPolygonExtrusion(chainSecond,sketch(4),boundary(),10,&issue),
          "two-Boolean edge ancestry regenerated after second source edit");
    check(chained.interleavedFeatureHistoryCurrent(chainTarget),"chain remains current");
    check(std::abs(volume(chained,chainTarget)-chainBefore)>1,
          "chain source edit changes final filleted solid");
    // Source a feature from a selected *intermediate* stage instead of @base.
    auto fromIntermediate=twoBooleans;
    TopTools_IndexedMapOfShape intermediateEdges;
    TopExp::MapShapes(previews.value("first-boolean"),TopAbs_EDGE,intermediateEdges);
    int intermediateEdge=0;
    for(int i=1;i<=intermediateEdges.Extent();++i){
        GProp_GProps props;BRepGProp::LinearProperties(intermediateEdges.FindKey(i),props);
        const gp_Pnt center=props.CentreOfMass();
        if(std::abs(center.X()-6)<1e-5&&std::abs(center.Y())<1e-5&&
           std::abs(center.Z())<1e-5){intermediateEdge=i;break;}
    }
    check(intermediateEdge>0,"intermediate edge located by physical position");
    QMap<QString,double> intermediateAnchor;
    check(cad::captureStableEdgeAnchor(previews.value("first-boolean"),intermediateEdge,
          &intermediateAnchor,&issue),"intermediate witnessed edge captured");
    fromIntermediate[2].referenceSourceId="first-boolean";
    fromIntermediate[2].feature.parameters={{"edge_index",double(intermediateEdge)},{"radius",0.2}};
    for(auto it=intermediateAnchor.cbegin();it!=intermediateAnchor.cend();++it)
        fromIntermediate[2].feature.parameters.insert(it.key(),it.value());
    check(chained.setInterleavedFeatureHistory(chainTarget,fromIntermediate,&issue),
          "tracked intermediate edge through later Boolean stage");
    const QString chainCommitted=chained.find(chainTarget)->geometryToken;
    // Rigid and positive uniform-similarity stages have exact bijective
    // source shapes, so witnessed features may follow them. Mirror and
    // unsupported topology replacements remain explicit boundaries.
    auto rigidChain=twoBooleans;
    auto moved=translate("tracked-rigid-translation",0);
    moved.inputs={"first-boolean"};
    rigidChain.insert(1,moved);
    rigidChain[2].inputs={"tracked-rigid-translation"};
    check(chained.setInterleavedFeatureHistory(chainTarget,rigidChain,&issue),
          "rigid stage between Boolean operators preserves selected-edge lineage");
    const QString rigidCommitted=chained.find(chainTarget)->geometryToken;
    auto supportedSimilarity=rigidChain;
    supportedSimilarity[1].feature.type="Uniform scale";
    supportedSimilarity[1].feature.parameters={{"factor",1.0}};
    check(chained.setInterleavedFeatureHistory(chainTarget,supportedSimilarity,&issue),
          "an exact one-to-one uniform similarity stage supports witnessed ancestry");
    const QString similarityCommitted=chained.find(chainTarget)->geometryToken;
    auto unsupported=supportedSimilarity;
    unsupported[1].feature.type="Mirror XY";
    unsupported[1].feature.parameters.clear();
    check(!chained.setInterleavedFeatureHistory(chainTarget,unsupported,&issue),
          "unsupported mirror cannot advertise persistent Boolean ancestry");
    check(chained.find(chainTarget)->geometryToken==similarityCommitted,
          "unsupported lineage edit was rolled back");
    check(chained.setInterleavedFeatureHistory(chainTarget,fromIntermediate,&issue),
          "restore intermediate-edge reference for persistence testing");
    QTemporaryDir chainDir;check(chainDir.isValid(),"multi-stage temp directory");
    const QString chainPath=chainDir.path()+"/multi-stage.cadscene";
    check(chained.saveAs(chainPath,&issue),"save intermediate topology link");
    cad::SceneDocument chainedRestored;
    check(chainedRestored.load(chainPath,&issue),"load intermediate topology link");
    check(chainedRestored.updateConstrainedPolygonExtrusion(chainSecond,sketch(2),boundary(),10,&issue),
          "reopened intermediate topology link regenerated");
    check(chainedRestored.interleavedFeatureHistoryCurrent(chainTarget),
          "reopened multi-stage dependency remains current");
    // A planar face operation can likewise be rooted on the frozen base,
    // even when two separate Boolean generations intervene.
    const TopoDS_Shape chainBase=chained.find(chainTarget)->interleavedBase;
    int lowerFace=0,index=0;
    for(TopExp_Explorer walk(chainBase,TopAbs_FACE);walk.More();walk.Next()){
        ++index;
        GProp_GProps props;BRepGProp::SurfaceProperties(walk.Current(),props);
        if(std::abs(props.CentreOfMass().Z())<1e-6){lowerFace=index;break;}
    }
    check(lowerFace>0,"chain bottom planar reference located");
    QMap<QString,double> faceAnchor;
    check(cad::capturePlanarFaceAnchor(chainBase,lowerFace,&faceAnchor,&issue),
          "planar witness captured before multi-stage Boolean");
    cad::InterleavedHistoryStep faceBoss;
    faceBoss.id="chained-face-boss";faceBoss.kind="Shape";
    faceBoss.inputs={"second-boolean"};faceBoss.referenceSourceId="@base";
    faceBoss.feature.id=faceBoss.id;faceBoss.feature.type="Face normal boss";
    faceBoss.feature.parameters={{"face_index",double(lowerFace)},{"u",0},
                                 {"v",0},{"radius",0.4},{"depth",1.0}};
    for(auto it=faceAnchor.cbegin();it!=faceAnchor.cend();++it)
        faceBoss.feature.parameters.insert(it.key(),it.value());
    auto faceSteps=twoBooleans;faceSteps[2]=faceBoss;
    check(chained.setInterleavedFeatureHistory(chainTarget,faceSteps,&issue),
          "planar boss rebased through two Boolean stages");
    const QString faceCommitted=chained.find(chainTarget)->geometryToken;
    // A Boolean diamond must agree about the exact referenced successor
    // across both branches; it must never arbitrarily prefer one route.
    auto parallel=sketchStep("parallel-boss",chainSecond,"SketchFuse");
    parallel.inputs={"@base"};
    cad::InterleavedHistoryStep diamond;
    diamond.id="diamond-union";diamond.kind="Shape";
    diamond.feature.id=diamond.id;diamond.feature.type="Fuse branches";
    diamond.inputs={"first-boolean","parallel-boss"};
    QVector<cad::InterleavedHistoryStep> ambiguous={chainBoss,parallel,diamond,chainFillet};
    ambiguous[3].inputs={"diamond-union"};
    // The same physical edge may survive *both* branches of a Boolean
    // diamond. Convergent evidence is not ambiguous: the surviving reference
    // is accepted only when both independent routes agree.
    auto conflicting=ambiguous;
    conflicting[1].kind="Shape";
    conflicting[1].sketchSourceId.clear();
    conflicting[1].id="parallel-boss";
    conflicting[1].feature.id="parallel-boss";
    conflicting[1].feature.type="Translate";
    conflicting[1].feature.parameters={{"x",3.0},{"y",0.0},{"z",0.0}};
    // Here the two ancestors place the selected edge in different positions:
    // the reference planner must not guess which machining location to use.
    check(!chained.setInterleavedFeatureHistory(chainTarget,conflicting,&issue),
          "disagreeing Boolean ancestry routes must reject tracked references");
    check(chained.find(chainTarget)->geometryToken==faceCommitted,
          "ambiguous Boolean diamond preserved committed shape");
    // The rejected alternative must not poison persisted ancestry or make
    // a later save/reopen depend on a partially applied branch.
    QTemporaryDir rejectedMergeDir;
    check(rejectedMergeDir.isValid(),"conflicting-diamond temp directory");
    const QString rejectedMergePath=rejectedMergeDir.path()+"/last-good-diamond.cadscene";
    check(chained.saveAs(rejectedMergePath,&issue),"save after rejected ancestry merge");
    cad::SceneDocument recoveredMerge;
    check(recoveredMerge.load(rejectedMergePath,&issue),
          "load last committed history after rejected ancestry merge");
    check(recoveredMerge.find(chainTarget)!=nullptr&&
          recoveredMerge.interleavedFeatureHistoryCurrent(chainTarget),
          "rejected merge did not corrupt persistent feature history");
    check(std::abs(volume(recoveredMerge,chainTarget)-volume(chained,chainTarget))<1e-5,
          "rejected branch did not change the saved solid volume");

    // A patterned instance is pinned by its integer occurrence, not by a
    // geometric nearest-neighbor guess. Saving a non-seed reference creates
    // the explicit v4 mixed-history schema, and a later count reduction is
    // transactional: an invalid instance cannot change the previous body.
    cad::SceneDocument patterned;
    check(patterned.addConstrainedPolygonExtrusion(sketch(10),boundary(),10,&issue),
          "pattern external sketch created");
    const QString patternSource=patterned.parts().last().id;
    check(patterned.addPrimitive(cad::Primitive::Box,{12,12,10},&issue),
          "pattern frozen base created");
    const QString patternPart=patterned.parts().last().id;
    auto sourceBoss=sketchStep("pattern-source-boss",patternSource,"SketchFuse");
    sourceBoss.inputs={"@base"};
    QMap<QString,TopoDS_Shape> firstStage;
    check(patterned.previewInterleavedFeatureHistory(patternPart,{sourceBoss},&firstStage,&issue),
          "source solid preview before pattern");
    TopTools_IndexedMapOfShape sourceEdges;
    TopExp::MapShapes(firstStage.value("pattern-source-boss"),TopAbs_EDGE,sourceEdges);
    int baseEdge=0;
    for(int i=1;i<=sourceEdges.Extent();++i){
        GProp_GProps props;BRepGProp::LinearProperties(sourceEdges.FindKey(i),props);
        const gp_Pnt center=props.CentreOfMass();
        if(std::abs(center.X()-6)<1e-5&&std::abs(center.Y())<1e-5&&
           std::abs(center.Z())<1e-5){baseEdge=i;break;}
    }
    check(baseEdge>0,"pattern base edge located");
    QMap<QString,double> patternAnchor;
    check(cad::captureStableEdgeAnchor(firstStage.value("pattern-source-boss"),baseEdge,
        &patternAnchor,&issue),"capture edge before pattern occurrence");
    cad::InterleavedHistoryStep patternStage;
    patternStage.id="pattern-three";patternStage.kind="Shape";
    patternStage.feature.id=patternStage.id;
    patternStage.feature.type="Linear pattern X";
    patternStage.feature.parameters={{"count",3},{"spacing",30}};
    patternStage.inputs={"pattern-source-boss"};
    cad::InterleavedHistoryStep instanceFillet;
    instanceFillet.id="pattern-instance-fillet";instanceFillet.kind="Shape";
    instanceFillet.inputs={"pattern-three"};
    instanceFillet.referenceSourceId="pattern-source-boss";
    instanceFillet.referencePatternOccurrence=1;
    instanceFillet.feature.id=instanceFillet.id;
    instanceFillet.feature.type="Fillet selected edge";
    instanceFillet.feature.parameters={{"edge_index",double(baseEdge)},{"radius",0.2}};
    for(auto it=patternAnchor.cbegin();it!=patternAnchor.cend();++it)
        instanceFillet.feature.parameters.insert(it.key(),it.value());
    // Both diamond paths are kernel-confirmed and converge to precisely
    // the same face/edge. Unlike genuinely ambiguous branches, this merge
    // can now preserve the selected reference without choosing a side.
    auto leftZero=translate("same-left",0);leftZero.inputs={"pattern-source-boss"};
    auto rightZero=translate("same-right",0);rightZero.inputs={"pattern-source-boss"};
    cad::InterleavedHistoryStep identicalMerge;
    identicalMerge.id="identical-branches";identicalMerge.kind="Shape";
    identicalMerge.feature.id=identicalMerge.id;
    identicalMerge.feature.type="Fuse branches";
    identicalMerge.inputs={"same-left","same-right"};
    auto mergedFillet=instanceFillet;
    mergedFillet.id="merged-edge-fillet";mergedFillet.feature.id=mergedFillet.id;
    mergedFillet.inputs={"identical-branches"};
    mergedFillet.referencePatternOccurrence=0;
    check(patterned.setInterleavedFeatureHistory(patternPart,
          {sourceBoss,leftZero,rightZero,identicalMerge,mergedFillet},&issue),
          "convergent Boolean diamond keeps one kernel-confirmed edge identity");
    const QVector<cad::InterleavedHistoryStep> withPattern={sourceBoss,patternStage,instanceFillet};
    check(patterned.setInterleavedFeatureHistory(patternPart,withPattern,&issue),
          "fillet selects exactly one non-seed pattern occurrence");
    check(patterned.interleavedFeatureHistoryCurrent(patternPart),
          "patterned history current");
    const QString stablePattern=patterned.find(patternPart)->geometryToken;
    auto deletedOccurrence=withPattern;
    deletedOccurrence[1].feature.parameters["count"]=2;
    deletedOccurrence[2].referencePatternOccurrence=2; // valid with old 3, deleted with new 2
    check(!patterned.setInterleavedFeatureHistory(patternPart,deletedOccurrence,&issue),
          "deleted occurrence rejected after count reduction");
    check(patterned.find(patternPart)->geometryToken==stablePattern,
          "invalid pattern count rollback preserved committed geometry");
    QTemporaryDir occurrenceFolder;
    check(occurrenceFolder.isValid(),"pattern history temp directory");
    const QString occurrenceFile=occurrenceFolder.path()+"/patterned.cadscene";
    check(patterned.saveAs(occurrenceFile,&issue),"save pattern occurrence in v4");
    QFile projectFile(occurrenceFile);
    check(projectFile.open(QIODevice::ReadOnly),"read v4 project");
    auto projectJson=QJsonDocument::fromJson(projectFile.readAll());projectFile.close();
    auto projectParts=projectJson.object().value("parts").toArray();
    bool foundV4=false;
    for(const auto& value:projectParts){
        const auto record=value.toObject().value("interleaved_history").toObject();
        if(record.isEmpty())continue;
        const auto finalNode=record.value("steps").toArray().last().toObject();
        foundV4=record.value("type").toString()==QStringLiteral("dag-interleaved-history-v4")&&
            finalNode.value("reference_occurrence").toInt()==1;
    }
    check(foundV4,"v4 schema persisted selected instance");
    cad::SceneDocument reloadedPattern;
    check(reloadedPattern.load(occurrenceFile,&issue),"load v4 pattern occurrence");
    check(reloadedPattern.interleavedFeatureHistoryCurrent(patternPart),
          "loaded pattern occurrence current");
    auto invalidRoot=projectJson.object();
    for(int i=0;i<projectParts.size();++i){
        auto item=projectParts[i].toObject();
        auto record=item.value("interleaved_history").toObject();
        if(record.isEmpty())continue;
        auto stages=record.value("steps").toArray();auto finalNode=stages.last().toObject();
        finalNode.insert("reference_occurrence",99);
        stages[stages.size()-1]=finalNode;record.insert("steps",stages);
        item.insert("interleaved_history",record);projectParts[i]=item;
    }
    invalidRoot.insert("parts",projectParts);
    check(projectFile.open(QIODevice::WriteOnly|QIODevice::Truncate),
          "tamper pattern occurrence project");
    projectFile.write(QJsonDocument(invalidRoot).toJson());projectFile.close();
    const QString loadedToken=reloadedPattern.find(patternPart)->geometryToken;
    check(!reloadedPattern.load(occurrenceFile,&issue),"malformed v4 occurrence rejected");
    check(reloadedPattern.find(patternPart)->geometryToken==loadedToken,
          "invalid v4 load left last committed model untouched");

    // v5: a particular copy inside a second pattern has an independent,
    // explicitly named occurrence selection for EACH stage. Old v4 projects
    // are tested above and remain readable.
    cad::InterleavedHistoryStep secondaryPattern;
    secondaryPattern.id="pattern-nested-y";secondaryPattern.kind="Shape";
    secondaryPattern.inputs={"pattern-three"};
    secondaryPattern.feature.id=secondaryPattern.id;
    secondaryPattern.feature.type="Linear pattern Y";
    secondaryPattern.feature.parameters={{"count",2},{"spacing",45}};
    auto nestedFillet=instanceFillet;
    nestedFillet.id="nested-occurrence-fillet";nestedFillet.feature.id=nestedFillet.id;
    nestedFillet.inputs={"pattern-nested-y"};
    nestedFillet.referencePatternOccurrence=0;
    nestedFillet.referencePatternOccurrences={{"pattern-three",1},{"pattern-nested-y",1}};
    const QVector<cad::InterleavedHistoryStep> nested={sourceBoss,patternStage,secondaryPattern,nestedFillet};
    check(patterned.setInterleavedFeatureHistory(patternPart,nested,&issue),
          "nested pattern nonseed occurrence rebuilt");
    check(patterned.interleavedFeatureHistoryCurrent(patternPart),
          "nested pattern currently resolved");
    const QString nestedToken=patterned.find(patternPart)->geometryToken;
    auto missingNested=nested;
    missingNested[2].feature.parameters["count"]=2;
    missingNested[3].referencePatternOccurrences["pattern-nested-y"]=2;
    check(!patterned.setInterleavedFeatureHistory(patternPart,missingNested,&issue),
          "nested instance outside changed occurrence count fails closed");
    check(patterned.find(patternPart)->geometryToken==nestedToken,
          "failed nested instance selection preserves previous model");
    QTemporaryDir nestedFolder;check(nestedFolder.isValid(),"nested temp folder");
    const QString nestedPath=nestedFolder.path()+"/nested.cadscene";
    check(patterned.saveAs(nestedPath,&issue),"save v5 nested selections");
    QFile nestedFile(nestedPath);
    check(nestedFile.open(QIODevice::ReadOnly),"read saved v5 project");
    const auto nestedJson=QJsonDocument::fromJson(nestedFile.readAll());nestedFile.close();
    bool foundV5=false;
    for(const auto& entry:nestedJson.object().value("parts").toArray()){
        const auto history=entry.toObject().value("interleaved_history").toObject();
        if(history.isEmpty())continue;
        const auto finalStep=history.value("steps").toArray().last().toObject();
        const auto map=finalStep.value("reference_occurrences").toObject();
        foundV5=history.value("type").toString()=="dag-interleaved-history-v5"&&
            map.value("pattern-three").toInt()==1&&map.value("pattern-nested-y").toInt()==1;
    }
    check(foundV5,"v5 schema stores both stable occurrence IDs");
    cad::SceneDocument nestedReloaded;
    check(nestedReloaded.load(nestedPath,&issue),"reopen v5 nested pattern");
    check(nestedReloaded.interleavedFeatureHistoryCurrent(patternPart),
          "v5 geometry valid after reopen");
    auto maliciousRoot=nestedJson.object();auto maliciousParts=maliciousRoot.value("parts").toArray();
    for(int i=0;i<maliciousParts.size();++i){
        auto obj=maliciousParts[i].toObject();auto history=obj.value("interleaved_history").toObject();
        if(history.isEmpty())continue;
        auto entries=history.value("steps").toArray();auto item=entries.last().toObject();
        auto map=item.value("reference_occurrences").toObject();
        map.insert("unknown-pattern-id",1);item.insert("reference_occurrences",map);
        entries[entries.size()-1]=item;history.insert("steps",entries);
        obj.insert("interleaved_history",history);maliciousParts[i]=obj;
    }
    maliciousRoot.insert("parts",maliciousParts);
    check(nestedFile.open(QIODevice::WriteOnly|QIODevice::Truncate),"write malformed v5");
    nestedFile.write(QJsonDocument(maliciousRoot).toJson());nestedFile.close();
    const QString intact=nestedReloaded.find(patternPart)->geometryToken;
    check(!nestedReloaded.load(nestedPath,&issue),"unknown v5 pattern key rejected");
    check(nestedReloaded.find(patternPart)->geometryToken==intact,
          "malformed nested project never mutates open document");

    // Shape lofts and sweeps can be explicit nodes in a mixed DAG. They
    // rebuild as solids after the upstream constrained sketch is changed.
    cad::SceneDocument additive;QString additiveError;
    check(additive.addConstrainedPolygonExtrusion(sketch(8),boundary(),10,&additiveError),
          "additive sketch source");
    const QString additiveSource=additive.parts().last().id;
    check(additive.addPrimitive(cad::Primitive::Box,{12,12,10},&additiveError),
          "additive base");
    const QString additiveTarget=additive.parts().last().id;
    auto additiveBoss=sketchStep("live-form-boss",additiveSource,"SketchFuse");
    auto loft=translate("loft-placeholder",0);
    loft.id="form-loft";loft.feature.id=loft.id;loft.feature.type="Loft rectangles";
    loft.feature.parameters={{"width0",5},{"depth0",5},{"width1",6},{"depth1",6},
        {"height",8},{"offset_x",1},{"offset_y",1}};
    loft.inputs={"live-form-boss"};
    auto sweep=translate("sweep-placeholder",0);
    sweep.id="form-sweep";sweep.feature.id=sweep.id;sweep.feature.type="Sweep circle path";
    sweep.feature.parameters={{"radius",1.0},{"path_length",14.0},
        {"lateral_offset",2.0},{"vertical_rise",2.0}};
    sweep.inputs={"form-loft"};
    check(additive.setInterleavedFeatureHistory(additiveTarget,{additiveBoss,loft,sweep},&additiveError),
          "loft and sweep regenerate in linked mixed history");
    const double additiveBefore=volume(additive,additiveTarget);
    check(additive.updateConstrainedPolygonExtrusion(additiveSource,sketch(7),boundary(),10,&additiveError),
          "edit upstream sketch regenerates loft and sweep");
    check(std::abs(volume(additive,additiveTarget)-additiveBefore)>1.0,
          "upstream source changed the generated model");

    // A selective fillet must leave *other* edge witnesses attached only if
    // OpenCascade kept their exact topological identity. The rounded edge
    // itself is consumed and may not be guessed back from nearby geometry.
    cad::SceneDocument surviving;QString survivalError;
    check(surviving.addConstrainedPolygonExtrusion(sketch(8),boundary(),10,&survivalError),
          "survival test sketch source");
    const QString survivalSource=surviving.parts().last().id;
    check(surviving.addPrimitive(cad::Primitive::Box,{12,12,10},&survivalError),
          "survival test base box");
    const QString survivalPart=surviving.parts().last().id;
    QMap<QString,double> consumedAnchor,untouchedAnchor;
    check(cad::captureStableEdgeAnchor(surviving.find(survivalPart)->shape,1,
          &consumedAnchor,&survivalError),"first box-edge witness");
    check(cad::captureStableEdgeAnchor(surviving.find(survivalPart)->shape,6,
          &untouchedAnchor,&survivalError),"unrelated box-edge witness");
    cad::InterleavedHistoryStep firstFillet;
    firstFillet.id="round-first";firstFillet.kind="Shape";
    firstFillet.inputs={"@base"};firstFillet.feature.id=firstFillet.id;
    firstFillet.feature.type="Fillet selected edge";
    firstFillet.feature.parameters={{"edge_index",1},{"radius",0.15}};
    for(auto it=consumedAnchor.cbegin();it!=consumedAnchor.cend();++it)
        firstFillet.feature.parameters.insert(it.key(),it.value());
    cad::InterleavedHistoryStep otherFillet;
    otherFillet.id="round-unrelated";otherFillet.kind="Shape";
    otherFillet.inputs={"round-first"};otherFillet.referenceSourceId="@base";
    otherFillet.feature.id=otherFillet.id;
    otherFillet.feature.type="Fillet selected edge";
    otherFillet.feature.parameters={{"edge_index",6},{"radius",0.12}};
    for(auto it=untouchedAnchor.cbegin();it!=untouchedAnchor.cend();++it)
        otherFillet.feature.parameters.insert(it.key(),it.value());
    auto survivalBoss=sketchStep("survival-live-boss",survivalSource,"SketchFuse");
    survivalBoss.inputs={"round-unrelated"};
    // Isolate the first fillet and downstream sketch from ancestry retargeting:
    // a failure in the three-step chain must not hide an earlier kernel error.
    auto firstOnlyBoss=survivalBoss;
    firstOnlyBoss.inputs={"round-first"};
    const bool firstOnly=surviving.setInterleavedFeatureHistory(survivalPart,
          {firstFillet,firstOnlyBoss},&survivalError);
    if(!firstOnly)std::cerr<<"First selective fillet or linked sketch failed: "
                           <<survivalError.toStdString()<<'\n';
    check(firstOnly,"first selective fillet and linked sketch succeed");
    const bool survived=surviving.setInterleavedFeatureHistory(survivalPart,
          {firstFillet,otherFillet,survivalBoss},&survivalError);
    if(!survived){
        // Keep the actual CAD error in the CTest failure line, even with
        // non-verbose CTest output or a buffered stderr stream.
        throw std::runtime_error(
            "unaffected edge survives a prior selective fillet: " +
            survivalError.toStdString());
    }
    check(survived,"unaffected edge survives a prior selective fillet");
    const QString beforeBad=surviving.find(survivalPart)->geometryToken;
    auto consumed=otherFillet;
    consumed.feature.parameters={{"edge_index",1},{"radius",0.12}};
    for(auto it=consumedAnchor.cbegin();it!=consumedAnchor.cend();++it)
        consumed.feature.parameters.insert(it.key(),it.value());
    check(!surviving.setInterleavedFeatureHistory(survivalPart,
          {firstFillet,consumed,survivalBoss},&survivalError),
          "already consumed source edge cannot be retargeted");
    check(surviving.find(survivalPart)->geometryToken==beforeBad,
          "consumed-edge failure is transactional");

    // Atomic fan-out regeneration: one dependent branch remains valid after a
    // source translation, but the other cut loses all intersection. The whole
    // source edit MUST roll back, including the successful sibling branch.
    cad::SceneDocument forked;
    check(forked.addConstrainedPolygonExtrusion(sketch(10),boundary(),10,&issue),
          "fan-out external sketch source created");
    const QString fanSource=forked.parts().last().id;
    check(forked.addPrimitive(cad::Primitive::Box,{12,12,10},&issue),
          "fan-out add target created");
    const QString fanFuse=forked.parts().last().id;
    check(forked.addPrimitive(cad::Primitive::Box,{4,12,10},&issue),
          "fan-out cut target created");
    const QString fanCut=forked.parts().last().id;
    const auto branchFuse=sketchStep("fan-out-fuse",fanSource,"SketchFuse");
    const auto branchCut=sketchStep("fan-out-cut",fanSource,"SketchCut");
    check(forked.setInterleavedFeatureHistory(fanFuse,{branchFuse},&issue),
          "fan-out sibling one linked");
    check(forked.setInterleavedFeatureHistory(fanCut,{branchCut},&issue),
          "fan-out sibling two linked");
    const QString sourceBefore=forked.find(fanSource)->geometryToken;
    const QString fuseBefore=forked.find(fanFuse)->geometryToken;
    const QString cutBefore=forked.find(fanCut)->geometryToken;
    const double sourceVolume=volume(forked,fanSource);
    const double fuseVolume=volume(forked,fanFuse);
    const double cutVolume=volume(forked,fanCut);
    check(!forked.updateConstrainedPolygonExtrusion(fanSource,sketchAtX(8,10),
          boundary(),10,&issue),"one invalid branch rejects entire source edit");
    check(forked.find(fanSource)->geometryToken==sourceBefore&&
          forked.find(fanFuse)->geometryToken==fuseBefore&&
          forked.find(fanCut)->geometryToken==cutBefore,
          "failed fan-out rebuild preserves all three geometry tokens");
    check(std::abs(volume(forked,fanSource)-sourceVolume)<1e-6&&
          std::abs(volume(forked,fanFuse)-fuseVolume)<1e-6&&
          std::abs(volume(forked,fanCut)-cutVolume)<1e-6,
          "failed fan-out rebuild preserves every body volume");
    check(forked.interleavedFeatureHistoryCurrent(fanFuse)&&
          forked.interleavedFeatureHistoryCurrent(fanCut),
          "failed fan-out rebuild keeps both links current");
    check(forked.updateConstrainedPolygonExtrusion(fanSource,sketch(8),
          boundary(),10,&issue),"valid fan-out source edit regenerates both siblings");
    check(forked.find(fanFuse)->geometryToken!=fuseBefore&&
          forked.find(fanCut)->geometryToken!=cutBefore,
          "valid fan-out rebuild refreshes both branch tokens");
    check(forked.undo(),"fan-out undo restores both dependent branches");
    check(forked.find(fanFuse)->geometryToken==fuseBefore&&
          forked.find(fanCut)->geometryToken==cutBefore,
          "undo fan-out restores both branch identity tokens");
    check(forked.redo(),"fan-out redo reapplies both dependent branches");

    // v1.70 additional machining provenance cases (executed by Windows CTest).
    // A kernel's Modified() list may return edge fragments even though the
    // saved edge witness has been removed by a pocket. Reject, do not pick
    // the first fragment and silently move the downstream fillet.
    const TopoDS_Shape sample=BRepPrimAPI_MakeBox(12,12,10).Shape();
    QMap<QString,double> sampledEdge;
    check(cad::captureStableEdgeAnchor(sample,1,&sampledEdge,&issue),
          "source vertical edge captured before midpoint cut");
    cad::ShapeFeatureNode referencedEdge;
    referencedEdge.id="edge-after-midpoint-cut";
    referencedEdge.type="Fillet selected edge";
    referencedEdge.parameters={{"edge_index",1},{"radius",0.12}};
    for(auto it=sampledEdge.cbegin();it!=sampledEdge.cend();++it)
        referencedEdge.parameters.insert(it.key(),it.value());
    auto edgeRemoval=std::make_shared<BRepAlgoAPI_Cut>(sample,
        BRepPrimAPI_MakeBox(gp_Pnt(-1,-1,4),3,3,2).Shape());
    edgeRemoval->Build();
    check(edgeRemoval->IsDone(),"midpoint cut built");
    cad::ShapeFeatureNode rebasedEdge;
    check(!cad::rebaseFeatureThroughBoolean(sample,edgeRemoval->Shape(),*edgeRemoval,
          referencedEdge,&rebasedEdge,&issue),
          "clipped edge witness cannot be reassigned to surviving fragment");

    // The selected top face may still survive with a hole while the
    // machining center at (6,6) has been cut away. A stable face witness at
    // (1,1) alone must not permit the machine operation to drift.
    QMap<QString,double> topFaceAnchor;
    int topFaceIndex=0;
    for(int i=1;i<=6;++i){
        QMap<QString,double> probe;
        if(cad::capturePlanarFaceAnchor(sample,i,&probe,&issue)&&
           probe.value("face_anchor_nz")>0.99&&probe.value("face_anchor_z")>9.9){
            topFaceIndex=i;topFaceAnchor=probe;break;
        }
    }
    check(topFaceIndex>0,"top machining face identified");
    cad::ShapeFeatureNode faceMachining;
    faceMachining.id="top-machining";
    faceMachining.type="Face normal pocket";
    faceMachining.parameters={{"face_index",double(topFaceIndex)},
        {"u",0.0},{"v",0.0},{"radius",0.6},{"depth",2.0}};
    topFaceAnchor.insert("face_anchor_witness_x",1.0);
    topFaceAnchor.insert("face_anchor_witness_y",1.0);
    topFaceAnchor.insert("face_anchor_witness_z",10.0);
    for(auto it=topFaceAnchor.cbegin();it!=topFaceAnchor.cend();++it)
        faceMachining.parameters.insert(it.key(),it.value());
    const TopoDS_Shape centerTool=BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(6,6,8),gp_Dir(0,0,1)),1,4).Shape();
    auto faceRemoval=std::make_shared<BRepAlgoAPI_Cut>(sample,centerTool);
    faceRemoval->Build();
    check(faceRemoval->IsDone(),"machining center cut built");
    cad::ShapeFeatureNode reboundFace;
    check(!cad::rebaseFeatureThroughBoolean(sample,faceRemoval->Shape(),*faceRemoval,
          faceMachining,&reboundFace,&issue),
          "face survived but removed machining center cannot be reattached");
    const TopoDS_Shape cornerTool=BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(10,10,8),gp_Dir(0,0,1)),0.6,4).Shape();
    auto safeCut=std::make_shared<BRepAlgoAPI_Cut>(sample,cornerTool);
    safeCut->Build();
    check(safeCut->IsDone(),"nearby cut created a valid changed top face");
    check(cad::rebaseFeatureThroughBoolean(sample,safeCut->Shape(),*safeCut,
          faceMachining,&reboundFace,&issue),
          "face witness and real machining center survive an unrelated pocket");
    check(std::abs(reboundFace.parameters.value("u"))>1e-5||
          std::abs(reboundFace.parameters.value("v"))>1e-5,
          "downstream machining offsets compensate for changed face centroid");
    cad::ShapeFeatureNode safelyRebasedEdge;
    check(cad::rebaseFeatureThroughBoolean(sample,safeCut->Shape(),*safeCut,
          referencedEdge,&safelyRebasedEdge,&issue),
          "unaffected witnessed edge remains traceable across machining cut");

    // Capture *actual current-operation* provenance for routine face cuts.
    // The source is a valid face-attached shape operation, and the kernel
    // builder must report exactly the shape committed by one-node replay.
    // This direct-replay fixture intentionally exercises the legacy anchor
    // format without witnesses. Modern face capture includes TWO witness
    // groups. Dropping only the primary leaves an orphaned secondary and
    // rightly fails validateShapeFeatureNodes before any Boolean is built.
    cad::ShapeFeatureNode faceCut=faceMachining;
    faceCut.id="recorded-face-cut";
    check(faceCut.parameters.contains("face_anchor_witness2_x"),
          "captured planar face must provide a secondary witness");
    for(const char* suffix:{"x","y","z"})
        faceCut.parameters.remove(QStringLiteral("face_anchor_witness_")+
                                  QString::fromLatin1(suffix));
    check(!cad::validateShapeFeatureNodes({faceCut},&issue),
          "orphaned secondary face witness must be rejected");
    for(const char* suffix:{"x","y","z"})
        faceCut.parameters.remove(QStringLiteral("face_anchor_witness2_")+
                                  QString::fromLatin1(suffix));
    check(cad::validateShapeFeatureNodes({faceCut},&issue),
          "legacy face anchor without either witness group is valid");
    TopoDS_Shape machined;
    std::shared_ptr<BRepBuilderAPI_MakeShape> machiningHistory;
    const bool machinedOkay=cad::rebuildShapeFeatureTree(sample,false,{faceCut},
        &machined,&issue,nullptr,nullptr,nullptr,&machiningHistory);
    const QByteArray machiningDiagnostic=(
        QStringLiteral("face machining produces a valid solid: ")+issue).toUtf8();
    check(machinedOkay,machiningDiagnostic.constData());
    check(machiningHistory&&!machiningHistory->Shape().IsNull()&&
          machiningHistory->Shape().IsSame(machined),
          "face machining exposes current kernel result identity");
    check(cad::supportsRecordedModifierLineage(faceCut),
          "face pocket is registered for live provenance replay");
}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);
try{run();std::cout<<checks<<" interleaved history checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
