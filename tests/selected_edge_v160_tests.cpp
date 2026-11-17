#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include "core/BooleanReferenceLineage.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double volume(const TopoDS_Shape& shape){GProp_GProps props;BRepGProp::VolumeProperties(shape,props);return props.Mass();}
cad::ShapeFeatureNode selectedFeature(const QString& type,double size,const QMap<QString,double>& anchor){
    cad::ShapeFeatureNode node;
    node.id=type=="Fillet selected edge"?"edge_fillet_1":"edge_chamfer_1";
    node.type=type;
    node.parameters.insert("edge_index",2.0);
    node.parameters.insert(type=="Fillet selected edge"?QStringLiteral("radius"):QStringLiteral("distance"),size);
    for(auto it=anchor.cbegin();it!=anchor.cend();++it)node.parameters.insert(it.key(),it.value());
    return node;
}
}

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        QString error;
        for(const QString& type:{QStringLiteral("Fillet selected edge"),QStringLiteral("Chamfer selected edge")}){
            cad::SceneDocument doc;
            require(doc.addPrimitive(cad::Primitive::Box,{30,20,12},&error),"Could not create anchor test body");
            const QString bodyId=doc.parts().first().id;
            QMap<QString,double> anchor;
            require(cad::captureStableEdgeAnchor(doc.parts().first().shape,1,&anchor,&error),
                    "Could not capture unique edge");
            auto node=selectedFeature(type,0.75,anchor);
            // The stored index is deliberately wrong: the anchor must win.
            require(doc.setShapeFeatureTree(bodyId,{node},&error),"Anchored selected-edge feature failed");
            const auto* result=doc.find(bodyId);
            require(doc.shapeFeatureTreeCurrent(bodyId)&&BRepCheck_Analyzer(result->shape).IsValid(),
                    "Selected-edge feature did not produce valid editable shape");
            require(volume(result->shape)>0&&volume(result->shape)<7200,
                    "Selected-edge operation did not remove any material");
            require(doc.undo()&&doc.redo(),"Selected-edge feature undo/redo failed");
            cad::ShapeFeatureNode shift;
            shift.id="large_translation";shift.type="Translate";
            shift.parameters={{"x",1400.0},{"y",-900.0},{"z",360.0}};
            require(doc.setShapeFeatureTree(bodyId,{shift,node},&error),
                    "Anchored edge feature failed after a large upstream translation");
            require(BRepCheck_Analyzer(doc.find(bodyId)->shape).IsValid(),
                    "Translated selected-edge feature produced invalid B-rep");
            QTemporaryDir tmp;require(tmp.isValid(),"No temp directory");
            const QString saveFile=tmp.path()+"/edge_feature.cadscene";
            require(doc.saveAs(saveFile,&error),"Selected-edge feature could not save");
            cad::SceneDocument reopened;
            require(reopened.load(saveFile,&error),"Selected-edge feature could not reopen");
            require(reopened.shapeFeatureTreeCurrent(bodyId),"Reopened edge feature is stale");
            require(reopened.find(bodyId)->shapeFeatureNodes.size()==2&&
                    reopened.find(bodyId)->shapeFeatureNodes.last().parameters.contains("edge_body_x"),
                    "Stable edge anchor not persisted");
            auto damaged=node;damaged.parameters["edge_length"]=-1;
            require(!reopened.setShapeFeatureTree(bodyId,{damaged},&error),
                    "Invalid stable edge length was accepted");
            require(reopened.shapeFeatureTreeCurrent(bodyId),
                    "Rejected operation corrupted the saved feature stack");
            damaged=node;damaged.parameters.remove("edge_z");
            require(!reopened.setShapeFeatureTree(bodyId,{damaged},&error),
                    "Partial edge anchor was accepted");
        }
        // NURBS witness: splitting a B-spline keeps its parameterized interior
        // point when exactly one successor contains it; crossing that point
        // becomes ambiguous and must not silently choose either half.
        TColgp_Array1OfPnt controls(1,5);
        controls.SetValue(1,gp_Pnt(0,0,0));
        controls.SetValue(2,gp_Pnt(4,3,0));
        controls.SetValue(3,gp_Pnt(8,0,0));
        controls.SetValue(4,gp_Pnt(12,-2,0));
        controls.SetValue(5,gp_Pnt(16,0,0));
        GeomAPI_PointsToBSpline interpolation(controls);
        const Handle(Geom_BSplineCurve) curve=interpolation.Curve();
        require(!curve.IsNull(),"spline curve fixture");
        const auto full=BRepBuilderAPI_MakeEdge(curve).Edge();
        BRep_Builder builder;
        TopoDS_Compound splineBody;builder.MakeCompound(splineBody);builder.Add(splineBody,full);
        QMap<QString,double> splineAnchor;
        require(cad::captureStableEdgeAnchor(splineBody,1,&splineAnchor,&error),"spline capture");
        require(splineAnchor.value("edge_anchor_version")==3.0&&
                splineAnchor.value("edge_curve_kind")==3.0,"versioned spline witness");
        int selected=0;
        require(cad::resolveStableEdgeAnchor(splineBody,splineAnchor,&selected,&error)&&selected==1,
                "spline anchored self lookup");
        const Standard_Real start=curve->FirstParameter(),stop=curve->LastParameter();
        const Standard_Real cut=start+(stop-start)*0.8;
        TopoDS_Compound trimmed;builder.MakeCompound(trimmed);
        builder.Add(trimmed,BRepBuilderAPI_MakeEdge(curve,start,cut).Edge());
        builder.Add(trimmed,BRepBuilderAPI_MakeEdge(curve,cut,stop).Edge());
        require(cad::resolveStableEdgeAnchor(trimmed,splineAnchor,&selected,&error),
                "unique trimmed spline successor");
        cad::ShapeFeatureNode splineFillet=selectedFeature("Fillet selected edge",0.3,splineAnchor);
        cad::ShapeFeatureNode rotation;rotation.id="rotate-spline";rotation.type="Rotate Z";
        rotation.parameters={{"degrees",65.0}};
        gp_Trsf rotationTr;
        rotationTr.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),65.0*std::acos(-1.0)/180.0);
        const auto rotatedSpline=BRepBuilderAPI_Transform(splineBody,rotationTr,true).Shape();
        cad::ShapeFeatureNode rotatedReference;
        require(cad::rebaseFeatureThroughRigid(splineBody,rotatedSpline,rotation,splineFillet,
                    &rotatedReference,&error),"spline edge witness follows exact rotation");
        require(cad::validateShapeFeatureNodes({rotatedReference},&error),"rotated spline reference valid");
        // F02: the spline's original interior point must survive the rigid
        // transform instead of being replaced by the rotated edge midpoint.
        const gp_Pnt oldSplineWitness(splineAnchor.value("edge_witness_x"),
            splineAnchor.value("edge_witness_y"),splineAnchor.value("edge_witness_z"));
        const gp_Pnt expectedSplineWitness=oldSplineWitness.Transformed(rotationTr);
        require(std::abs(rotatedReference.parameters.value("edge_witness_x")-
                         expectedSplineWitness.X())<1e-6&&
                std::abs(rotatedReference.parameters.value("edge_witness_y")-
                         expectedSplineWitness.Y())<1e-6,
                "rotated spline must preserve the transported physical edge witness");
        // F02: rebase twice on the same physical BSpline edge. Both its
        // interior witness AND tangent must be transformed, never resampled.
        const auto secondRotatedSpline=BRepBuilderAPI_Transform(rotatedSpline,rotationTr,true).Shape();
        cad::ShapeFeatureNode twiceSpline;
        require(cad::rebaseFeatureThroughRigid(rotatedSpline,secondRotatedSpline,rotation,
                    rotatedReference,&twiceSpline,&error),"two-stage spline edge lineage");
        const gp_Pnt expectedTwiceSpline=expectedSplineWitness.Transformed(rotationTr);
        require(std::abs(twiceSpline.parameters.value("edge_witness_x")-expectedTwiceSpline.X())<1e-6&&
                std::abs(twiceSpline.parameters.value("edge_witness_y")-expectedTwiceSpline.Y())<1e-6&&
                std::abs(twiceSpline.parameters.value("edge_witness_z")-expectedTwiceSpline.Z())<1e-6,
                "two-stage spline witness drifted to a newly sampled midpoint");
        require(std::abs(twiceSpline.parameters.value("edge_curvature")-
                         splineAnchor.value("edge_curvature"))<1e-8,
                "rigid spline transform changed local edge curvature");
        auto invalidSpline=splineAnchor;invalidSpline.insert("edge_anchor_version",2.0);
        require(!cad::resolveStableEdgeAnchor(splineBody,invalidSpline,&selected,&error),
                "v3 curve kind rejects legacy v2 version");
        auto wrongBend=splineAnchor;wrongBend.insert("edge_curvature",
            splineAnchor.value("edge_curvature")+10.0);
        require(!cad::resolveStableEdgeAnchor(splineBody,wrongBend,&selected,&error),
                "different spline curvature must not capture another edge");
        TopoDS_Compound crossing;builder.MakeCompound(crossing);
        const Standard_Real midway=(start+stop)*0.5;
        builder.Add(crossing,BRepBuilderAPI_MakeEdge(curve,start,midway).Edge());
        builder.Add(crossing,BRepBuilderAPI_MakeEdge(curve,midway,stop).Edge());
        require(!cad::resolveStableEdgeAnchor(crossing,splineAnchor,&selected,&error),
                "split through spline witness must fail rather than select a side");
        // Face-local U/V design coordinates must follow a real rotation, not
        // shift with an arbitrary replacement face centroid.
        const auto box=BRepPrimAPI_MakeBox(10,12,8).Shape();
        int topFace=0,index=0;
        for(TopExp_Explorer e(box,TopAbs_FACE);e.More();e.Next()){
            ++index;GProp_GProps prop;BRepGProp::SurfaceProperties(e.Current(),prop);
            if(std::abs(prop.CentreOfMass().Z()-8.0)<1e-6){topFace=index;break;}
        }
        require(topFace>0,"planar top face fixture");
        QMap<QString,double> boxAnchor;
        require(cad::capturePlanarFaceAnchor(box,topFace,&boxAnchor,&error),
                "rotating planar source anchor");
        cad::ShapeFeatureNode pocket;pocket.id="top-pocket";pocket.type="Face normal pocket";
        pocket.parameters={{"face_index",double(topFace)},{"u",2.0},{"v",-1.0},
                           {"radius",1.0},{"depth",2.0}};
        for(auto it=boxAnchor.cbegin();it!=boxAnchor.cend();++it)
            pocket.parameters.insert(it.key(),it.value());
        cad::ShapeFeatureNode movedPocket;
        require(cad::rebaseFeatureThroughRigid(box,
                BRepBuilderAPI_Transform(box,rotationTr,true).Shape(),rotation,
                pocket,&movedPocket,&error),"planar pocket follows rotated face frame");
        require(cad::validateShapeFeatureNodes({movedPocket},&error),
                "rigidly remapped planar pocket feature validated");
        // F01: a rigidly transported face anchor must retain the ORIGINAL
        // physical interior design point, never the new sampled face center.
        const gp_Pnt oldFaceWitness(boxAnchor.value("face_anchor_witness_x"),
            boxAnchor.value("face_anchor_witness_y"),boxAnchor.value("face_anchor_witness_z"));
        const gp_Pnt expectedFaceWitness=oldFaceWitness.Transformed(rotationTr);
        require(std::abs(movedPocket.parameters.value("face_anchor_witness_x")-
                  expectedFaceWitness.X())<1e-6&&
                std::abs(movedPocket.parameters.value("face_anchor_witness_y")-
                  expectedFaceWitness.Y())<1e-6,
                "rotated face witness must not drift to a new centroid");
        // Subsequent independent rigid moves must carry that same point.
        cad::ShapeFeatureNode driftCheck;
        require(cad::rebaseFeatureThroughRigid(
                    BRepBuilderAPI_Transform(box,rotationTr,true).Shape(),
                    BRepBuilderAPI_Transform(BRepBuilderAPI_Transform(box,rotationTr,true).Shape(),
                                             rotationTr,true).Shape(),rotation,
                    movedPocket,&driftCheck,&error),
                "second rigid stage must preserve uniquely witnessed face");
        const gp_Pnt twice=expectedFaceWitness.Transformed(rotationTr);
        require(std::abs(driftCheck.parameters.value("face_anchor_witness_x")-twice.X())<1e-5&&
                std::abs(driftCheck.parameters.value("face_anchor_witness_y")-twice.Y())<1e-5,
                "two rigid stages must preserve face-witness trajectory");
        require(std::abs(movedPocket.parameters.value("u")-2.0)>0.1||
                std::abs(movedPocket.parameters.value("v")+1.0)>0.1,
                "rotation actually remapped local design location");
        cad::ShapeFeatureNode scale;scale.id="scale";scale.type="Uniform scale";
        scale.parameters={{"factor",2.0}};
        gp_Trsf scaleTransform;
        scaleTransform.SetScale(gp_Pnt(0,0,0),2.0);
        const TopoDS_Shape scaledBox=BRepBuilderAPI_Transform(box,scaleTransform,true).Shape();
        require(cad::rebaseFeatureThroughSimilarity(box,scaledBox,scale,pocket,&movedPocket,&error),
                "uniform scale preserves witnessed planar source through similarity lineage");
        require(!cad::rebaseFeatureThroughSimilarity(box,box,scale,pocket,&movedPocket,&error),
                "the scaled witness must not attach to unchanged geometry");
        // Direct operations must not bypass downstream linked Boolean bodies.
        cad::SceneDocument linked;
        require(linked.addPrimitive(cad::Primitive::Box,{30,30,20},&error),"Source A creation failed");
        const QString a=linked.parts().last().id;
        require(linked.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Source B creation failed");
        const QString b=linked.parts().last().id;
        require(linked.translate(b,8,0,0,&error),"Source B translation failed");
        require(linked.addLinkedBooleanBody(cad::BooleanOp::Fuse,a,b,&error),"Linked union creation failed");
        const QString derived=linked.parts().last().id;
        require(linked.filletAllEdges(b,0.6,&error),"All-edge source fillet failed");
        require(linked.linkedBooleanCurrent(derived),"Direct fillet left downstream result stale");
        require(linked.chamferAllEdges(a,0.5,&error),"All-edge source chamfer failed");
        require(linked.linkedBooleanCurrent(derived),"Direct chamfer left downstream result stale");
        const QString before=linked.find(derived)->geometryToken;
        require(!linked.filletAllEdges(derived,0.5,&error),"Editing linked output should be rejected");
        require(linked.find(derived)->geometryToken==before&&linked.linkedBooleanCurrent(derived),
                "Rejected linked-body edit mutated geometry");
        require(linked.undo()&&linked.linkedBooleanCurrent(derived),
                "Undo after chamfer did not restore linked-body currency");
        require(linked.redo()&&linked.linkedBooleanCurrent(derived),
                "Redo after chamfer did not restore linked-body currency");
        // The preview/commit edge tools used to skip dependent regeneration.
        cad::SceneDocument selectedDirect;
        require(selectedDirect.addPrimitive(cad::Primitive::Box,{30,30,20},&error),
                "Direct selected-edge source A failed");
        const QString selectedA=selectedDirect.parts().last().id;
        require(selectedDirect.addPrimitive(cad::Primitive::Box,{20,20,20},&error),
                "Direct selected-edge source B failed");
        const QString selectedB=selectedDirect.parts().last().id;
        require(selectedDirect.translate(selectedB,8,0,0,&error),"Direct selected-edge setup failed");
        require(selectedDirect.addLinkedBooleanBody(cad::BooleanOp::Fuse,selectedA,selectedB,&error),
                "Direct selected-edge union failed");
        const QString selectedDerived=selectedDirect.parts().last().id;
        const cad::EdgeReference bRef{selectedB,selectedDirect.find(selectedB)->geometryToken,1};
        require(selectedDirect.filletEdge(bRef,0.45,&error),"Direct selected-edge fillet failed");
        require(selectedDirect.linkedBooleanCurrent(selectedDerived),
                "Selected-edge fillet did not rebuild dependent Boolean body");
        const cad::EdgeReference aRef{selectedA,selectedDirect.find(selectedA)->geometryToken,1};
        require(selectedDirect.chamferEdge(aRef,0.45,&error),"Direct selected-edge chamfer failed");
        require(selectedDirect.linkedBooleanCurrent(selectedDerived),
                "Selected-edge chamfer did not rebuild dependent Boolean body");
        const cad::EdgeReference forbidden{selectedDerived,
            selectedDirect.find(selectedDerived)->geometryToken,1};
        require(!selectedDirect.filletEdge(forbidden,0.2,&error),
                "A linked Boolean output accepted a direct edge fillet");
        require(!selectedDirect.chamferEdge(forbidden,0.2,&error),
                "A linked Boolean output accepted a direct edge chamfer");
        require(selectedDirect.linkedBooleanCurrent(selectedDerived),
                "Rejected derived-body edge edits modified the linked scene");
        std::cout<<"PASS: persistent selected-edge features, invalid anchors, linked direct-edit propagation\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;
    }
}
