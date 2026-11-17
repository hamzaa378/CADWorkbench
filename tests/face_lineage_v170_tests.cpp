#include "core/ShapeFeatureTree.h"
#include "core/BooleanReferenceLineage.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax2.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <GeomAbs_CurveType.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax2.hxx>
#include <QCoreApplication>
#include <cmath>
#include <stdexcept>
#include <iostream>
namespace {
int checks=0;
void must(bool condition,const char* why){++checks;if(!condition)throw std::runtime_error(why);}
int topIndex(const TopoDS_Shape& body){
    int index=0;for(TopExp_Explorer it(body,TopAbs_FACE);it.More();it.Next()){
        ++index;GProp_GProps props;BRepGProp::SurfaceProperties(TopoDS::Face(it.Current()),props);
        if(std::abs(props.CentreOfMass().Z()-10.0)<1e-5)return index;
    }return -1;
}
void run(){
    QString reason;
    const TopoDS_Shape box=BRepPrimAPI_MakeBox(10,10,10).Shape();
    QMap<QString,double> anchor;
    must(cad::capturePlanarFaceAnchor(box,topIndex(box),&anchor,&reason),"capture failed");
    must(anchor.size()==16,"planar face anchor captures primary/secondary interior witnesses");
    must(anchor.contains("face_anchor_witness2_x"),"secondary generated-face discriminator is recorded");
    TopoDS_Face face;
    must(cad::resolvePlanarFaceAnchor(box,anchor,&face,&reason),"original face not resolved");
    gp_Trsf trsf;trsf.SetTranslation(gp_Vec(2000,-400,150));
    const TopoDS_Shape moved=BRepBuilderAPI_Transform(box,trsf,true).Shape();
    must(cad::resolvePlanarFaceAnchor(moved,anchor,&face,&reason),"large translation lost witness");
    // The source body is duplicated three times in a compound. Its mass
    // reference shifts by 20 mm, so both the original witness and its
    // body-relative copy identify DISTINCT but otherwise identical faces.
    // Reject that ambiguous ownership instead of silently moving a hole to
    // the wrong instance. This is particularly important for split assemblies.
    BRep_Builder compoundBuilder;TopoDS_Compound duplicateSupports;
    compoundBuilder.MakeCompound(duplicateSupports);
    compoundBuilder.Add(duplicateSupports,box);
    for(const double x:{20.0,40.0}){
        gp_Trsf shiftCopy;shiftCopy.SetTranslation(gp_Vec(x,0,0));
        compoundBuilder.Add(duplicateSupports,BRepBuilderAPI_Transform(box,shiftCopy,true).Shape());
    }
    const TopoDS_Face previousFace=face;
    must(!cad::resolvePlanarFaceAnchor(duplicateSupports,anchor,&face,&reason),
         "competing shifted and original face witnesses must fail closed");
    must(reason.contains("conflicts"),"ambiguous duplicate support explains rebinding requirement");
    must(face.IsSame(previousFace),"failed face resolution must not change output face");

    // A transverse splitter divides the original top face into two faces.
    // The original witness is at x=5, unambiguously on the x<7 successor.
    const TopoDS_Shape plane=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(7,0,0),gp_Dir(1,0,0)),
                                                      -20,20,-20,20).Shape();
    TopTools_ListOfShape args,tools;args.Append(box);tools.Append(plane);
    BRepAlgoAPI_Splitter split;split.SetArguments(args);split.SetTools(tools);split.Build();
    must(split.IsDone()&&!split.Shape().IsNull(),"kernel splitter failed");
    must(cad::resolvePlanarFaceAnchor(split.Shape(),anchor,&face,&reason),"face split lost witness");
    // A second witness is only a disambiguator. A legitimate split may put it
    // on the OTHER child face, while the primary design witness still remains.
    const TopoDS_Shape nearPlane=BRepBuilderAPI_MakeFace(
        gp_Pln(gp_Pnt(5.1,0,0),gp_Dir(1,0,0)),-20,20,-20,20).Shape();
    TopTools_ListOfShape nearTools;nearTools.Append(nearPlane);
    BRepAlgoAPI_Splitter nearSplit;
    nearSplit.SetArguments(args);nearSplit.SetTools(nearTools);nearSplit.Build();
    must(nearSplit.IsDone()&&!nearSplit.Shape().IsNull(),"secondary-witness split fixture");
    must(cad::resolvePlanarFaceAnchor(nearSplit.Shape(),anchor,&face,&reason),
         "split recovery must preserve a unique primary design witness even if witness two was trimmed");
    QMap<QString,double> legacy=anchor;
    for(const QString& suffix:QStringList{"_witness_x","_witness_y","_witness_z",
                                          "_witness2_x","_witness2_y","_witness2_z"})
        legacy.remove(QStringLiteral("face_anchor")+suffix);
    must(legacy.size()==10,"legacy descriptor incorrect");
    must(cad::resolvePlanarFaceAnchor(box,legacy,&face,&reason),"legacy face resolution broke");
    QMap<QString,double> malformed=anchor;malformed.remove("face_anchor_witness_z");
    cad::ShapeFeatureNode feature;feature.id="bad";feature.type="Face normal pocket";
    feature.parameters=malformed;feature.parameters.insert("face_index",double(topIndex(box)));
    feature.parameters.insert("u",0);feature.parameters.insert("v",0);
    feature.parameters.insert("radius",1);feature.parameters.insert("depth",2);
    must(!cad::validateShapeFeatureNodes({feature},&reason),"incomplete witness was accepted");

    // Edge identity after a geometric split: follow the successor that still
    // contains the original trimmed-interior point, not its old edge index.
    TopTools_IndexedMapOfShape originalEdges;TopExp::MapShapes(box,TopAbs_EDGE,originalEdges);
    int edgeIndex=0;
    for(int i=1;i<=originalEdges.Extent();++i){
        GProp_GProps linear;BRepGProp::LinearProperties(originalEdges.FindKey(i),linear);
        const gp_Pnt mid=linear.CentreOfMass();
        if(std::abs(mid.X()-5)<1e-6&&std::abs(mid.Y())<1e-6&&
           std::abs(mid.Z()-10)<1e-6){edgeIndex=i;break;}
    }
    must(edgeIndex>0,"edge witness fixture");
    QMap<QString,double> edgeHint;
    must(cad::captureStableEdgeAnchor(box,edgeIndex,&edgeHint,&reason),"edge witness capture");
    must(edgeHint.value("edge_anchor_version")==2.0,"edge witness version captured");
    int successor=0;
    must(cad::resolveStableEdgeAnchor(split.Shape(),edgeHint,&successor,&reason),
         "line edge split should preserve unique interior witness");
    TopTools_IndexedMapOfShape splitEdges;TopExp::MapShapes(split.Shape(),TopAbs_EDGE,splitEdges);
    GProp_GProps hit;BRepGProp::LinearProperties(splitEdges.FindKey(successor),hit);
    must(hit.CentreOfMass().X()<7.0,"edge witness identified wrong split successor");
    must(cad::resolveStableEdgeAnchor(moved,edgeHint,&successor,&reason),
         "translated witnessed edge should resolve");
    // The three evenly spaced copies have a different body reference centre.
    // The old absolute witness hits copy #1, while its body-relative
    // interpretation hits copy #2. Choosing whichever trial happens first
    // would silently attach a machining feature to another occurrence.
    const int retainedSuccessor=successor;
    must(!cad::resolveStableEdgeAnchor(duplicateSupports,edgeHint,&successor,&reason),
         "absolute versus body-relative edge descendants must reject ambiguous compound copies");
    must(reason.contains("different successors")||reason.contains("ambiguous"),
         "competing edge witness placements need an actionable diagnostic");
    must(successor==retainedSuccessor,
         "failed compound edge reattachment must preserve the previous output index");
    QMap<QString,double> incompleteEdge=edgeHint;
    incompleteEdge.remove("edge_tangent_y");
    must(!cad::resolveStableEdgeAnchor(box,incompleteEdge,&successor,&reason),
         "incomplete witnessed edge reference accepted");
    // The same witness now lies *on* a topology boundary, so neither
    // neighboring successor owns it: reject rather than arbitrarily choose.
    const TopoDS_Shape boundaryPlane=BRepBuilderAPI_MakeFace(
        gp_Pln(gp_Pnt(5,0,0),gp_Dir(1,0,0)),-20,20,-20,20).Shape();
    TopTools_ListOfShape boundaryTools;boundaryTools.Append(boundaryPlane);
    BRepAlgoAPI_Splitter boundarySplit;boundarySplit.SetArguments(args);
    boundarySplit.SetTools(boundaryTools);boundarySplit.Build();
    must(boundarySplit.IsDone(),"witness boundary splitter");
    must(!cad::resolveStableEdgeAnchor(boundarySplit.Shape(),edgeHint,&successor,&reason),
         "split exactly at witness should require reselect");
    // Legacy seven-field anchors must continue resolving unchanged models.
    QMap<QString,double> oldEdge=edgeHint;
    for(const QString& name:QStringList{"edge_anchor_version","edge_curve_kind",
            "edge_witness_x","edge_witness_y","edge_witness_z",
            "edge_tangent_x","edge_tangent_y","edge_tangent_z","edge_curvature"})oldEdge.remove(name);
    must(cad::resolveStableEdgeAnchor(box,oldEdge,&successor,&reason),
         "legacy seven-field anchor resolution");
    // A circular arc can also retain its interior witness when a full circle
    // is trimmed to a smaller successor that still includes that witness.
    const gp_Circ circular(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)),10.0);
    const TopoDS_Edge fullCircle=BRepBuilderAPI_MakeEdge(circular,0,2*std::acos(-1.0)).Edge();
    QMap<QString,double> circleHint;
    must(cad::captureStableEdgeAnchor(fullCircle,1,&circleHint,&reason),"circular anchor capture");
    must(circleHint.value("edge_curve_kind")==2.0,"circle witness type captured");
    const TopoDS_Edge retainedArc=BRepBuilderAPI_MakeEdge(circular,0,1.5*std::acos(-1.0)).Edge();
    must(cad::resolveStableEdgeAnchor(retainedArc,circleHint,&successor,&reason),
         "circular successor keeps interior witness");
    const TopoDS_Edge missingArc=BRepBuilderAPI_MakeEdge(circular,1.5*std::acos(-1.0),2*std::acos(-1.0)).Edge();
    must(!cad::resolveStableEdgeAnchor(missingArc,circleHint,&successor,&reason),
         "removed circular witness must not silently reattach");

    // Kernel provenance, not an index or approximate nearest-edge heuristic,
    // controls retargeting of subsequent fillets and face-based operations.
    const TopoDS_Shape cutter=BRepPrimAPI_MakeBox(gp_Pnt(2,2,8),6,6,6).Shape();
    BRepAlgoAPI_Cut cut(box,cutter);cut.Build();
    must(cut.IsDone()&&!cut.Shape().IsNull(),"Boolean lineage fixture");
    int bottomEdge=0;
    for(int i=1;i<=originalEdges.Extent();++i){
        GProp_GProps props;BRepGProp::LinearProperties(originalEdges.FindKey(i),props);
        const gp_Pnt mid=props.CentreOfMass();
        if(std::abs(mid.X()-5)<1e-6&&std::abs(mid.Y())<1e-6&&
           std::abs(mid.Z())<1e-6){bottomEdge=i;break;}
    }
    must(bottomEdge>0,"bottom edge exists");
    QMap<QString,double> bottomHint;
    must(cad::captureStableEdgeAnchor(box,bottomEdge,&bottomHint,&reason),"capture parent edge");
    cad::ShapeFeatureNode downstream;downstream.id="fillet-after-cut";
    downstream.type="Fillet selected edge";
    downstream.parameters={{"edge_index",double(bottomEdge)},{"radius",0.2}};
    for(auto it=bottomHint.cbegin();it!=bottomHint.cend();++it)
        downstream.parameters.insert(it.key(),it.value());
    cad::ShapeFeatureNode resolved;
    must(cad::rebaseFeatureThroughBoolean(box,cut.Shape(),cut,downstream,&resolved,&reason),
         "kernel tracked selected edge after Cut");
    must(resolved.parameters.contains("edge_anchor_version"),"fresh tracked edge witness");
    // Two actual OCCT Boolean trims successively shorten the same selected
    // edge without removing its original physical design point (x=5).
    // The reference must not migrate to the trimmed edge's new midpoint.
    BRepAlgoAPI_Cut edgeTrim1(box,
        BRepPrimAPI_MakeBox(gp_Pnt(7,-1,-1),4,3,3).Shape());
    edgeTrim1.Build();must(edgeTrim1.IsDone(),"first tracked edge trim");
    cad::ShapeFeatureNode afterEdgeTrim1;
    must(cad::rebaseFeatureThroughBoolean(box,edgeTrim1.Shape(),edgeTrim1,
        downstream,&afterEdgeTrim1,&reason),"first edge trim preserves design location");
    for(const char* coord:{"x","y","z"}){
        const QString field=QStringLiteral("edge_witness_")+QString::fromLatin1(coord);
        must(std::abs(afterEdgeTrim1.parameters.value(field)-
            downstream.parameters.value(field))<1e-9,"first edge trim displaced witness");
    }
    BRepAlgoAPI_Cut edgeTrim2(edgeTrim1.Shape(),
        BRepPrimAPI_MakeBox(gp_Pnt(-1,-1,-1),4,3,3).Shape());
    edgeTrim2.Build();must(edgeTrim2.IsDone(),"second tracked edge trim");
    cad::ShapeFeatureNode afterEdgeTrim2;
    must(cad::rebaseFeatureThroughBoolean(edgeTrim1.Shape(),edgeTrim2.Shape(),edgeTrim2,
        afterEdgeTrim1,&afterEdgeTrim2,&reason),"second edge trim preserves design location");
    for(const char* coord:{"x","y","z"}){
        const QString field=QStringLiteral("edge_witness_")+QString::fromLatin1(coord);
        must(std::abs(afterEdgeTrim2.parameters.value(field)-
            downstream.parameters.value(field))<1e-9,"second edge trim displaced witness");
    }
    // The retained point must be on the actual output edge and may no
    // longer be replaced by a midpoint from a different split fragment.
    int trackedSuccessor=0;
    QMap<QString,double> trimmedHint;
    for(auto it=afterEdgeTrim2.parameters.cbegin();it!=afterEdgeTrim2.parameters.cend();++it){
        if(it.key().startsWith("edge_")&&it.key()!=QStringLiteral("edge_index"))
            trimmedHint.insert(it.key(),it.value());
    }
    must(cad::resolveStableEdgeAnchor(edgeTrim2.Shape(),trimmedHint,
        &trackedSuccessor,&reason),"saved twice-trimmed edge still resolves");
    TopoDS_Shape filleted;
    must(cad::rebuildShapeFeatureTree(cut.Shape(),false,{resolved},&filleted,&reason),
         "downstream fillet regenerated after tracked Boolean");
    cad::ShapeFeatureNode missing=downstream;
    missing.parameters.remove("edge_anchor_version");
    must(!cad::rebaseFeatureThroughBoolean(box,cut.Shape(),cut,missing,&resolved,&reason),
         "legacy edge descriptor cannot claim proven Boolean ancestry");

    const int bottomFace=[](const TopoDS_Shape& body){
        int index=0;
        for(TopExp_Explorer it(body,TopAbs_FACE);it.More();it.Next()){
            ++index;GProp_GProps props;
            BRepGProp::SurfaceProperties(TopoDS::Face(it.Current()),props);
            if(std::abs(props.CentreOfMass().Z())<1e-5)return index;
        }
        return 0;
    }(box);
    must(bottomFace>0,"bottom face exists");
    QMap<QString,double> faceHint;
    must(cad::capturePlanarFaceAnchor(box,bottomFace,&faceHint,&reason),"capture parent face");
    cad::ShapeFeatureNode opening;opening.id="shell-after-cut";opening.type="Open-face shell";
    opening.parameters={{"face_index",double(bottomFace)},{"thickness",0.5}};
    for(auto it=faceHint.cbegin();it!=faceHint.cend();++it)
        opening.parameters.insert(it.key(),it.value());
    must(cad::rebaseFeatureThroughBoolean(box,cut.Shape(),cut,opening,&resolved,&reason),
         "kernel tracked planar face after Cut");
    must(resolved.parameters.contains("face_anchor_witness_x"),"fresh tracked face witness");
    // Two dependent operations must be rebound atomically. A later bad
    // witness cannot leave the first reference silently attached to new B-rep.
    QVector<cad::ShapeFeatureNode> reboundGroup,initialGroup{downstream,opening};
    must(cad::rebaseFeaturesThroughBoolean(box,cut.Shape(),cut,initialGroup,&reboundGroup,&reason)&&
         reboundGroup.size()==2&&reboundGroup[0].id==downstream.id&&
         reboundGroup[1].id==opening.id,
         "Atomic face/edge Boolean reference batch failed");
    const QVector<cad::ShapeFeatureNode> frozenGroup=reboundGroup;
    cad::ShapeFeatureNode badGroupMember=downstream;
    badGroupMember.id="missing-witness";
    badGroupMember.parameters.remove("edge_anchor_version");
    must(!cad::rebaseFeaturesThroughBoolean(box,cut.Shape(),cut,
         {opening,badGroupMember},&reboundGroup,&reason)&&reboundGroup.size()==frozenGroup.size()&&
         reboundGroup[0].id==frozenGroup[0].id&&
         reboundGroup[0].parameters==frozenGroup[0].parameters&&
         reboundGroup[1].id==frozenGroup[1].id&&
         reboundGroup[1].parameters==frozenGroup[1].parameters,
         "Failed Boolean reference batch changed an already published result");
    must(!cad::rebaseFeaturesThroughBoolean(box,cut.Shape(),cut,
         {opening,opening},&reboundGroup,&reason)&&reboundGroup.size()==frozenGroup.size()&&
         reboundGroup[0].id==frozenGroup[0].id&&
         reboundGroup[0].parameters==frozenGroup[0].parameters&&
         reboundGroup[1].id==frozenGroup[1].id&&
         reboundGroup[1].parameters==frozenGroup[1].parameters,
         "Duplicate feature ID was accepted during Boolean reference batch recovery");
    // An off-center Boolean pocket changes a planar face centroid. Its
    // downstream hole must keep the same design origin, not drift with the
    // newly computed centroid of the trimmed face.
    BRepAlgoAPI_Cut offCenter(box,BRepPrimAPI_MakeBox(gp_Pnt(1,1,8),3,3,5).Shape());
    offCenter.Build();must(offCenter.IsDone(),"off-center face split fixture");
    QMap<QString,double> topHint;
    must(cad::capturePlanarFaceAnchor(box,topIndex(box),&topHint,&reason),
         "capture top face for location preservation");
    cad::ShapeFeatureNode hole;hole.id="stable-hole";hole.type="Face normal pocket";
    hole.parameters={{"face_index",double(topIndex(box))},{"u",0},{"v",0},
                     {"radius",0.4},{"depth",1.0}};
    for(auto it=topHint.cbegin();it!=topHint.cend();++it)
        hole.parameters.insert(it.key(),it.value());
    must(cad::rebaseFeatureThroughBoolean(box,offCenter.Shape(),offCenter,hole,&resolved,&reason),
         "face location rebased through asymmetric Boolean cut");
    must(std::abs(resolved.parameters.value("u"))+std::abs(resolved.parameters.value("v"))>0.05,
         "face offset compensates for new centroid");
    // A split/merge chain must NOT replace the physical design witness
    // with a new centroid sample. The retained witness is the persistent
    // identity, independently of the successor face area/centroid.
    for(const char* coordinate:{"x","y","z"}){
        const QString key=QStringLiteral("face_anchor_witness_")+QString::fromLatin1(coordinate);
        must(std::abs(resolved.parameters.value(key)-hole.parameters.value(key))<1e-9,
             "Boolean rebase drifted the primary physical design witness");
    }
    const cad::ShapeFeatureNode firstRebased=resolved;
    BRepAlgoAPI_Cut nextTrim(offCenter.Shape(),
        BRepPrimAPI_MakeBox(gp_Pnt(6,1,8),2,2,5).Shape());
    nextTrim.Build();must(nextTrim.IsDone(),"second Boolean trim fixture");
    must(cad::rebaseFeatureThroughBoolean(offCenter.Shape(),nextTrim.Shape(),nextTrim,
                                         firstRebased,&resolved,&reason),
         "second Boolean split retains original design location");
    for(const char* coordinate:{"x","y","z"}){
        const QString key=QStringLiteral("face_anchor_witness_")+QString::fromLatin1(coordinate);
        must(std::abs(resolved.parameters.value(key)-hole.parameters.value(key))<1e-9,
             "two-stage Boolean rebase drifted the primary witness");
    }
    // Refill the off-center pocket. A co-planar face merge is allowed only
    // when kernel provenance confirms the original physical witness.
    BRepAlgoAPI_Fuse restored(offCenter.Shape(),
        BRepPrimAPI_MakeBox(gp_Pnt(1,1,8),3,3,2).Shape());
    restored.Build();must(restored.IsDone(),"co-planar face merge fixture");
    cad::ShapeFeatureNode remerged;
    must(cad::rebaseFeatureThroughBoolean(offCenter.Shape(),restored.Shape(),restored,
                                         firstRebased,&remerged,&reason),
         "Boolean face merge retains original design location");
    must(std::abs(remerged.parameters.value("face_anchor_witness_x")-
                  hole.parameters.value("face_anchor_witness_x"))<1e-9,
         "co-planar merge moved primary design witness");
    TopoDS_Shape relocatedHole;
    must(cad::rebuildShapeFeatureTree(offCenter.Shape(),false,{resolved},&relocatedHole,&reason),
         "location-preserving downstream hole regenerated");
    // Even when the kernel reports a successor face, a removed witness
    // region must not be silently reassigned to a different place on it.
    BRepAlgoAPI_Cut witnessCut(box,BRepPrimAPI_MakeBox(gp_Pnt(4,4,8),2,2,5).Shape());
    witnessCut.Build();must(witnessCut.IsDone(),"removed-witness Boolean fixture");
    must(!cad::rebaseFeatureThroughBoolean(box,witnessCut.Shape(),witnessCut,hole,&resolved,&reason),
         "removed face witness region rejected");
    // A source edge wholly removed by a Cut cannot be silently replaced.
    BRepAlgoAPI_Cut removed(box,BRepPrimAPI_MakeBox(gp_Pnt(0,-2,-2),12,4,4).Shape());
    removed.Build();must(removed.IsDone(),"edge-removal Boolean fixture");
    must(!cad::rebaseFeatureThroughBoolean(box,removed.Shape(),removed,downstream,&resolved,&reason),
         "deleted reference rejected");

    // New kernel-provenance path: retain the actual fillet builder from the
    // same feature rebuild, so Modified() and Generated() can reattach a face.
    cad::ShapeFeatureNode rounded;
    rounded.id="round-one-edge";rounded.type="Fillet selected edge";
    rounded.parameters={{"edge_index",double(bottomEdge)},{"radius",0.2}};
    for(auto it=bottomHint.cbegin();it!=bottomHint.cend();++it)
        rounded.parameters.insert(it.key(),it.value());
    std::shared_ptr<BRepBuilderAPI_MakeShape> filletHistory;
    TopoDS_Shape roundedResult;
    must(cad::rebuildShapeFeatureTree(box,false,{rounded},&roundedResult,&reason,
                                     nullptr,nullptr,nullptr,&filletHistory),
         "recorded fillet rebuild succeeds");
    must(bool(filletHistory),"recorded fillet builder is retained");
    must(!cad::rebaseFeatureThroughBoolean(box,cut.Shape(),*filletHistory,
                                          opening,&resolved,&reason),
         "history from the wrong geometry transaction is rejected");
    cad::ShapeFeatureNode roundedDownstream;
    must(cad::rebaseFeatureThroughBoolean(box,roundedResult,*filletHistory,
                                         opening,&roundedDownstream,&reason),
         "face witness follows recorded fillet Modified descendants");
    must(roundedDownstream.parameters.contains("face_anchor_witness_x"),
         "recorded fillet returned a fresh downstream face witness");

    // OCCT may say IsDeleted() for an edge that remains EXACTLY unchanged
    // in the fillet result. Final-result membership, not IsDeleted() alone,
    // determines whether the source witness may survive. A rounded source
    // edge must still be rejected because its original TShape is absent.
    TopTools_IndexedMapOfShape preFilletEdges;
    TopExp::MapShapes(box,TopAbs_EDGE,preFilletEdges);
    must(preFilletEdges.Extent()>=6,"selective-fillet edge fixture");
    BRepFilletAPI_MakeFillet selective(box);
    selective.Add(0.15,TopoDS::Edge(preFilletEdges.FindKey(1)));
    selective.Build();
    must(selective.IsDone()&&!selective.Shape().IsNull(),
         "selective fillet kernel fixture");
    TopTools_IndexedMapOfShape postFilletEdges;
    TopExp::MapShapes(selective.Shape(),TopAbs_EDGE,postFilletEdges);
    bool exactUnaffected=false;
    for(int i=1;i<=postFilletEdges.Extent();++i)
        exactUnaffected|=postFilletEdges.FindKey(i).IsSame(preFilletEdges.FindKey(6));
    must(exactUnaffected,"unaffected edge must exist in selective fillet output");
    QMap<QString,double> unchangedAnchor,consumedAnchor;
    must(cad::captureStableEdgeAnchor(box,6,&unchangedAnchor,&reason),
         "capture untouched edge before selective fillet");
    must(cad::captureStableEdgeAnchor(box,1,&consumedAnchor,&reason),
         "capture consumed edge before selective fillet");
    cad::ShapeFeatureNode untouched;
    untouched.id="fillet-untouched";
    untouched.type="Fillet selected edge";
    untouched.parameters={{"edge_index",6.0},{"radius",0.12}};
    for(auto it=unchangedAnchor.cbegin();it!=unchangedAnchor.cend();++it)
        untouched.parameters.insert(it.key(),it.value());
    must(cad::rebaseFeatureThroughBoolean(box,selective.Shape(),selective,
                                         untouched,&resolved,&reason),
         "fillet preserves an unchanged source edge despite IsDeleted metadata");
    cad::ShapeFeatureNode consumedEdge=untouched;
    consumedEdge.parameters={{"edge_index",1.0},{"radius",0.12}};
    for(auto it=consumedAnchor.cbegin();it!=consumedAnchor.cend();++it)
        consumedEdge.parameters.insert(it.key(),it.value());
    must(!cad::rebaseFeatureThroughBoolean(box,selective.Shape(),selective,
                                          consumedEdge,&resolved,&reason),
         "fillet-consumed source edge must not regain an invented reference");

    // Manufacturing cuts also preserve current kernel lineage for their
    // unaffected or trimmed source faces.
    cad::ShapeFeatureNode drilled;
    drilled.id="top-hole";drilled.type="Cylinder cut Z";
    drilled.parameters={{"x",4},{"y",4},{"z",8},{"radius",0.8},{"depth",4}};
    TopoDS_Shape cutResult;
    std::shared_ptr<BRepBuilderAPI_MakeShape> cylinderHistory;
    must(cad::rebuildShapeFeatureTree(box,false,{drilled},&cutResult,&reason,
                                     nullptr,nullptr,nullptr,&cylinderHistory),
         "cylinder cut built with recorded current kernel history");
    must(bool(cylinderHistory),"cylinder cut builder retained");
    must(cad::rebaseFeatureThroughBoolean(box,cutResult,*cylinderHistory,
                                         hole,&resolved,&reason),
         "top face tracks through native cylinder-cut ancestry");

    // A loft output can be referenced directly, even though it has no source
    // face in the frozen box. It must be uniquely witnessed on each rebuild.
    cad::ShapeFeatureNode loft;
    loft.id="add-loft";loft.type="Loft rectangles";
    loft.parameters={{"width0",8},{"depth0",8},{"width1",7},{"depth1",7},
                     {"height",15},{"offset_x",1},{"offset_y",1}};
    TopoDS_Shape loftResult;
    std::shared_ptr<BRepBuilderAPI_MakeShape> loftHistory;
    must(cad::rebuildShapeFeatureTree(box,false,{loft},&loftResult,&reason,
                                     nullptr,nullptr,nullptr,&loftHistory),
         "recorded additive loft rebuild succeeds");
    must(bool(loftHistory),"additive loft fusion builder is retained");
    int generatedTop=0,faceNo=0;
    for(TopExp_Explorer e(loftResult,TopAbs_FACE);e.More();e.Next()){
        ++faceNo;GProp_GProps props;
        BRepGProp::SurfaceProperties(TopoDS::Face(e.Current()),props);
        if(std::abs(props.CentreOfMass().Z()-15)<1e-4){generatedTop=faceNo;break;}
    }
    must(generatedTop>0,"find generated loft top face");
    QMap<QString,double> loftTop;
    must(cad::capturePlanarFaceAnchor(loftResult,generatedTop,&loftTop,&reason),
         "capture intermediate loft-generated face");
    cad::ShapeFeatureNode loftHole;
    loftHole.id="attached-to-loft";loftHole.type="Face normal pocket";
    loftHole.parameters={{"face_index",double(generatedTop)},
                         {"u",0},{"v",0},{"radius",0.3},{"depth",1.5}};
    for(auto it=loftTop.cbegin();it!=loftTop.cend();++it)
        loftHole.parameters.insert(it.key(),it.value());
    must(cad::rebaseFeatureOnCurrentStage(loftResult,loftHole,&resolved,&reason),
         "intermediate generated face re-resolves on current loft result");
    TopoDS_Shape pocketOnLoft;
    must(cad::rebuildShapeFeatureTree(loftResult,false,{resolved},&pocketOnLoft,&reason),
         "a machining operation can consume the intermediate loft face");
    must(!cad::rebaseFeatureOnCurrentStage(witnessCut.Shape(),loftHole,&resolved,&reason),
         "generated loft witness cannot attach to unrelated edited geometry");
    // New: a generated planar cap follows a uniquely verified change of
    // its support height while its planar footprint remains unchanged.
    const TopoDS_Shape raised=BRepPrimAPI_MakeBox(10,10,14).Shape();
    QMap<QString,double> capHint;
    must(cad::capturePlanarFaceAnchor(box,topIndex(box),&capHint,&reason),
         "capture generated-face height-move reference");
    cad::ShapeFeatureNode capHole;
    capHole.id="height-move-pocket";capHole.type="Face normal pocket";
    capHole.parameters={{"face_index",double(topIndex(box))},{"u",0},{"v",0},
                        {"radius",0.5},{"depth",2}};
    for(auto it=capHint.cbegin();it!=capHint.cend();++it)
        capHole.parameters.insert(it.key(),it.value());
    must(cad::rebaseFeatureOnCurrentStage(raised,capHole,&resolved,&reason),
         "generated cap follows bounded normal-only motion");
    must(std::abs(resolved.parameters.value("face_anchor_z")-14)<1e-5,
         "moving generated-face anchor was recaptured at the new height");
    TopoDS_Shape capPocket;
    must(cad::rebuildShapeFeatureTree(raised,false,{resolved},&capPocket,&reason),
         "downstream pocket remains manufacturable after the cap height edit");
    const TopoDS_Shape shifted=BRepPrimAPI_MakeBox(gp_Pnt(4,0,0),10,10,14).Shape();
    must(!cad::rebaseFeatureOnCurrentStage(shifted,capHole,&resolved,&reason),
         "unrelated lateral motion cannot be mistaken for generated-face height edit");
    // A positive uniform scale transports physical design witnesses and
    // dimensional support geometry without changing the downstream tool size.
    cad::ShapeFeatureNode scaleUp;
    scaleUp.id="scale-up";scaleUp.type="Uniform scale";
    scaleUp.parameters={{"factor",2.0}};
    must(cad::supportsSimilarityReferenceLineage(scaleUp),
         "positive uniform scaling is registered in exact ancestry");
    must(!cad::supportsRigidReferenceLineage(scaleUp),
         "a uniform scale must not be misreported as a rigid motion");
    gp_Trsf twice;twice.SetScale(gp_Pnt(0,0,0),2.0);
    const TopoDS_Shape doubled=BRepBuilderAPI_Transform(box,twice,true).Shape();
    gp_Pnt designWitness(5,5,10);
    must(cad::transportDesignPointThroughSimilarity(scaleUp,&designWitness,&reason),
         "scale transports the design witness");
    must(designWitness.Distance(gp_Pnt(10,10,20))<1e-8,
         "scaled design witness must follow exact geometry");
    cad::ShapeFeatureNode scaleResult;
    must(cad::rebaseFeatureThroughSimilarity(box,doubled,scaleUp,capHole,&scaleResult,&reason),
         "a witnessed planar machining feature follows uniform stock scaling");
    must(std::abs(scaleResult.parameters.value("face_anchor_area")-
         capHole.parameters.value("face_anchor_area")*4.0)<1e-5,
         "scaled planar face support area is updated exactly");
    TopoDS_Shape scaledPocket;
    must(cad::rebuildShapeFeatureTree(doubled,false,{scaleResult},&scaledPocket,&reason),
         "downstream pocket rebuilds on scaled stock");
    must(std::abs(scaleResult.parameters.value("radius")-
                  capHole.parameters.value("radius"))<1e-12,
         "downstream tool diameter is NOT implicitly scaled");
    must(cad::rebaseFeatureThroughSimilarity(box,doubled,scaleUp,downstream,&scaleResult,&reason),
         "witnessed selected edge follows uniform scale");
    must(std::abs(scaleResult.parameters.value("edge_length")-
                  downstream.parameters.value("edge_length")*2.0)<1e-5,
         "edge reference length follows scaling");
    must(std::abs(scaleResult.parameters.value("radius")-
                  downstream.parameters.value("radius"))<1e-12,
         "fillet radius is an independent feature dimension");
    // A mirror of a complete solid also has a one-to-one subshape mapping.
    // Transport a physical witness, then resolve the destination in the
    // mirrored solid rather than trusting pre-mirror topology indices.
    cad::ShapeFeatureNode mirror;
    mirror.id="mirror-plane";mirror.type="Mirror XY";
    gp_Trsf reflect;reflect.SetMirror(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)));
    const TopoDS_Shape mirrored=BRepBuilderAPI_Transform(box,reflect,true).Shape();
    must(cad::supportsSimilarityReferenceLineage(mirror),
         "mirror XY is tracked as a one-to-one transformed source");
    cad::ShapeFeatureNode mirroredCap;
    must(cad::rebaseFeatureThroughSimilarity(box,mirrored,mirror,capHole,
                                            &mirroredCap,&reason),
         "mirrored planar cap retains its exact design witness");
    must(std::abs(mirroredCap.parameters.value("face_anchor_witness_z")+10)<1e-6,
         "mirrored design witness lies on reflected plane");
    cad::ShapeFeatureNode mirroredEdge;
    must(cad::rebaseFeatureThroughSimilarity(box,mirrored,mirror,downstream,
                                            &mirroredEdge,&reason),
         "selected edge follows physical mirror, not stale index");
    cad::ShapeFeatureNode unmirroredEdge;
    must(cad::rebaseFeatureThroughSimilarity(mirrored,box,mirror,mirroredEdge,
                                            &unmirroredEdge,&reason),
         "two mirrors recover original edge witness");
    must(std::abs(unmirroredEdge.parameters.value("edge_length")-
                  downstream.parameters.value("edge_length"))<1e-6,
         "mirror must not alter feature support length");
    // Circular curvature is inverse radius and must be transported by 1/s.
    gp_Trsf doubleCircle;doubleCircle.SetScale(gp_Pnt(0,0,0),2.0);
    const TopoDS_Shape biggerCircle=BRepBuilderAPI_Transform(fullCircle,doubleCircle,true).Shape();
    cad::ShapeFeatureNode circularCut;
    circularCut.id="circle-edge";circularCut.type="Chamfer selected edge";
    circularCut.parameters={{"edge_index",1.0},{"distance",0.25}};
    for(auto it=circleHint.cbegin();it!=circleHint.cend();++it)
        circularCut.parameters.insert(it.key(),it.value());
    cad::ShapeFeatureNode largerCircle;
    must(cad::rebaseFeatureThroughSimilarity(fullCircle,biggerCircle,scaleUp,
                                             circularCut,&largerCircle,&reason),
         "circular edge witness follows scaled support radius");
    must(std::abs(largerCircle.parameters.value("edge_curvature")-
                  circleHint.value("edge_curvature")/2.0)<1e-8,
         "circle curvature scales inversely to the reference dimensions");
    must(std::abs(largerCircle.parameters.value("distance")-0.25)<1e-12,
         "dependent chamfer distance remains an independent dimension");
    const auto previousScaleResult=scaleResult;
    cad::ShapeFeatureNode invalidScale=scaleUp;
    invalidScale.parameters.insert("factor",0.0);
    must(!cad::transportDesignPointThroughSimilarity(invalidScale,&designWitness,&reason),
         "zero scale must be rejected before changing output");
    must(designWitness.Distance(gp_Pnt(10,10,20))<1e-8,
         "invalid scale must preserve the last design witness");
    must(!cad::rebaseFeatureThroughSimilarity(box,doubled,invalidScale,downstream,
                                              &scaleResult,&reason),
         "invalid scale must fail closed");
    must(scaleResult.parameters==previousScaleResult.parameters,
         "failed lineage rebase must not overwrite last valid node");
    cad::ShapeFeatureNode downScale=scaleUp;
    downScale.parameters.insert("factor",0.5);
    must(cad::rebaseFeatureThroughSimilarity(doubled,box,downScale,scaleResult,
                                             &resolved,&reason),
         "scaled edge lineage supports a second inverse scale");
    must(std::abs(resolved.parameters.value("edge_length")-
                  downstream.parameters.value("edge_length"))<1e-5,
         "two scale stages retain the original physical edge dimension");
    // Removing a single coordinate must reject the optional two-point anchor.
    auto brokenCap=capHole;brokenCap.parameters.remove("face_anchor_witness2_y");
    must(!cad::validateShapeFeatureNodes({brokenCap},&reason),
         "incomplete second face witness is rejected during loading/rebuild");
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{run();std::cout<<"face lineage: "<<checks<<" checks passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
