#include "core/ShapeFeatureTree.h"
#include "core/BooleanReferenceLineage.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_BSplineSurface.hxx> // required complete type for OCCT Handle upcast
#include <GeomConvert.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <QCoreApplication>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
int curvedFace(const TopoDS_Shape& shape,GeomAbs_SurfaceType type){
    int n=0;for(TopExp_Explorer e(shape,TopAbs_FACE);e.More();e.Next()){
        ++n;if(BRepAdaptor_Surface(TopoDS::Face(e.Current())).GetType()==type)return n;
    }return -1;
}
void run(){
    QString error;
    const TopoDS_Shape cylinder=BRepPrimAPI_MakeCylinder(10,20).Shape();
    QMap<QString,double> anchor;
    require(cad::captureCurvedFaceAnchor(cylinder,curvedFace(cylinder,GeomAbs_Cylinder),&anchor,&error),"cylinder capture");
    require(anchor.size()==14,"cylinder descriptor length");
    TopoDS_Face face;
    require(cad::resolveCurvedFaceAnchor(cylinder,anchor,&face,&error),"cylinder resolution");
    gp_Trsf shift;shift.SetTranslation(gp_Vec(500,1000,-800));
    const auto moved=BRepBuilderAPI_Transform(cylinder,shift,true).Shape();
    require(cad::resolveCurvedFaceAnchor(moved,anchor,&face,&error),"translated cylinder resolution");
    const TopoDS_Shape plane=BRepBuilderAPI_MakeFace(
        gp_Pln(gp_Pnt(0,0,14),gp_Dir(0,0,1)),-30,30,-30,30).Shape();
    TopTools_ListOfShape args,tools;args.Append(cylinder);tools.Append(plane);
    BRepAlgoAPI_Splitter split;split.SetArguments(args);split.SetTools(tools);split.Build();
    require(split.IsDone()&&!split.Shape().IsNull(),"cylinder splitter geometry");
    require(cad::resolveCurvedFaceAnchor(split.Shape(),anchor,&face,&error),"split cylinder successor");
    auto invalid=anchor;invalid.remove("curved_radius");
    require(!cad::resolveCurvedFaceAnchor(cylinder,invalid,&face,&error),"incomplete cylinder anchor");
    invalid=anchor;invalid["curved_radius"]+=5;
    require(!cad::resolveCurvedFaceAnchor(cylinder,invalid,&face,&error),"wrong cylinder radius");
    const auto sphere=BRepPrimAPI_MakeSphere(12).Shape();
    QMap<QString,double> sphereAnchor;
    require(cad::captureCurvedFaceAnchor(sphere,curvedFace(sphere,GeomAbs_Sphere),&sphereAnchor,&error),"sphere capture");
    require(cad::resolveCurvedFaceAnchor(sphere,sphereAnchor,&face,&error),"sphere resolution");
    require(!cad::resolveCurvedFaceAnchor(cylinder,sphereAnchor,&face,&error),"sphere cannot match cylinder");
    const auto cone=BRepPrimAPI_MakeCone(9,4,25).Shape();
    QMap<QString,double> coneAnchor;
    require(cad::captureCurvedFaceAnchor(cone,curvedFace(cone,GeomAbs_Cone),&coneAnchor,&error),"cone capture");
    require(coneAnchor.size()==15,"cone extended support descriptor");
    require(cad::resolveCurvedFaceAnchor(cone,coneAnchor,&face,&error),"cone resolution");
    const auto movedCone=BRepBuilderAPI_Transform(cone,shift,true).Shape();
    require(cad::resolveCurvedFaceAnchor(movedCone,coneAnchor,&face,&error),"translated cone resolution");
    auto badCone=coneAnchor;badCone["curved_aux"]+=0.1;
    require(!cad::resolveCurvedFaceAnchor(cone,badCone,&face,&error),"different cone angle rejected");
    const auto torus=BRepPrimAPI_MakeTorus(16,4).Shape();
    QMap<QString,double> torusAnchor;
    require(cad::captureCurvedFaceAnchor(torus,curvedFace(torus,GeomAbs_Torus),&torusAnchor,&error),"torus capture");
    require(torusAnchor.size()==15,"torus extended support descriptor");
    require(cad::resolveCurvedFaceAnchor(torus,torusAnchor,&face,&error),"torus resolution");
    const auto movedTorus=BRepBuilderAPI_Transform(torus,shift,true).Shape();
    require(cad::resolveCurvedFaceAnchor(movedTorus,torusAnchor,&face,&error),"translated torus resolution");
    auto badTorus=torusAnchor;badTorus["curved_aux"]+=1;
    require(!cad::resolveCurvedFaceAnchor(torus,badTorus,&face,&error),"different torus minor radius rejected");
    cad::ShapeFeatureNode torusOpening;torusOpening.id="generated-torus";
    torusOpening.type="Open-face shell";
    torusOpening.parameters={{"face_index",double(curvedFace(torus,GeomAbs_Torus))},{"thickness",0.8}};
    for(auto it=torusAnchor.cbegin();it!=torusAnchor.cend();++it)
        torusOpening.parameters.insert(it.key(),it.value());
    cad::ShapeFeatureNode torusRecovered;
    const auto newFilletRadius=BRepPrimAPI_MakeTorus(16,4.4).Shape();
    require(cad::rebaseFeatureOnCurrentStage(newFilletRadius,torusOpening,&torusRecovered,&error),
        "Direct-stage toroidal fillet must follow a safe minor-radius update");
    require(std::abs(torusRecovered.parameters.value("curved_aux")-4.4)<1e-5,
        "Fillet radius is not refreshed from current toroidal geometry");
    require(!cad::rebaseFeatureOnCurrentStage(BRepPrimAPI_MakeTorus(16,6).Shape(),torusOpening,&torusRecovered,&error),
        "Direct-stage fillet must reject a radius update above 20 percent");
    TopoDS_Compound twinTori;BRep_Builder torusBuilder;torusBuilder.MakeCompound(twinTori);
    torusBuilder.Add(twinTori,newFilletRadius);
    torusBuilder.Add(twinTori,BRepPrimAPI_MakeTorus(16,4.4).Shape());
    require(!cad::rebaseFeatureOnCurrentStage(twinTori,torusOpening,&torusRecovered,&error),
        "Multiple possible generated toroidal faces are ambiguous");
    // A shell opening can use the same persisted curved witness through
    // a Boolean crop and a rigid rotation without relying on face indices.
    cad::ShapeFeatureNode opening;
    opening.id="curved-shell-opening";opening.type="Open-face shell";
    opening.parameters={{"face_index",double(curvedFace(cylinder,GeomAbs_Cylinder))},
                        {"thickness",1.0}};
    for(auto it=anchor.cbegin();it!=anchor.cend();++it)
        opening.parameters.insert(it.key(),it.value());
    require(cad::validateShapeFeatureNodes({opening},&error),"curved opening validation");
    // Similarity lineage: an analytic curved opening can track scaled radii,
    // witness points and axes without changing its independent shell thickness.
    cad::ShapeFeatureNode uniform;
    uniform.id="uniform-scale";uniform.type="Uniform scale";
    uniform.parameters={{"factor",1.5}};
    gp_Trsf enlarge;enlarge.SetScale(gp_Pnt(0,0,0),1.5);
    const TopoDS_Shape largerCylinder=BRepBuilderAPI_Transform(cylinder,enlarge,true).Shape();
    cad::ShapeFeatureNode scaledOpening;
    require(cad::rebaseFeatureThroughSimilarity(cylinder,largerCylinder,uniform,opening,
                    &scaledOpening,&error),"cylindrical opening must survive positive uniform scale");
    require(std::abs(scaledOpening.parameters.value("curved_radius")-15.0)<1e-5,
            "cylindrical radius should scale with the supporting geometry");
    require(std::abs(scaledOpening.parameters.value("thickness")-1.0)<1e-12,
            "shell wall thickness remains an independent downstream dimension");
    cad::ShapeFeatureNode torusScaled;
    const TopoDS_Shape largeTorus=BRepBuilderAPI_Transform(torus,enlarge,true).Shape();
    require(cad::rebaseFeatureThroughSimilarity(torus,largeTorus,uniform,
                    torusOpening,&torusScaled,&error),"toroidal opening follows similarity");
    require(std::abs(torusScaled.parameters.value("curved_radius")-24.0)<1e-5&&
            std::abs(torusScaled.parameters.value("curved_aux")-6.0)<1e-5,
            "both toroidal radii must scale consistently");
    // Direct-stage generated cylinder: a small coaxial radius change may
    // preserve the face's radial design location even though exact support
    // radius matching fails. More than one candidate or a large change fails.
    const TopoDS_Shape modifiedCylinder=BRepPrimAPI_MakeCylinder(11,20).Shape();
    cad::ShapeFeatureNode movedOpening;
    require(cad::rebaseFeatureOnCurrentStage(modifiedCylinder,opening,&movedOpening,&error),
            "Direct-generated coaxial cylinder did not survive a 10% radius edit");
    require(std::abs(movedOpening.parameters.value("curved_radius")-11.0)<1e-5,
            "Direct-stage cylindrical reference did not retarget the current radius");
    const TopoDS_Shape largeChange=BRepPrimAPI_MakeCylinder(16,20).Shape();
    require(!cad::rebaseFeatureOnCurrentStage(largeChange,opening,&movedOpening,&error),
            "Direct-stage cylindrical witness accepted a 60% radius change");
    TopoDS_Compound ambiguousCylinders;BRep_Builder cylinderBuilder;
    cylinderBuilder.MakeCompound(ambiguousCylinders);
    cylinderBuilder.Add(ambiguousCylinders,modifiedCylinder);
    cylinderBuilder.Add(ambiguousCylinders,BRepPrimAPI_MakeCylinder(11,20).Shape());
    require(!cad::rebaseFeatureOnCurrentStage(ambiguousCylinders,opening,&movedOpening,&error),
            "Coaxial generated face with multiple possible successors was accepted");

    BRepAlgoAPI_Cut crop(cylinder,BRepPrimAPI_MakeBox(gp_Pnt(-20,-20,14),40,40,20).Shape());
    crop.Build();require(crop.IsDone()&&!crop.Shape().IsNull(),"cylinder crop");
    cad::ShapeFeatureNode retargeted;
    require(cad::rebaseFeatureThroughBoolean(cylinder,crop.Shape(),crop,
        opening,&retargeted,&error),"curved shell opening follows kernel cut history");
    require(cad::validateShapeFeatureNodes({retargeted},&error),"rebased curved opening validation");
    // Two consecutive cuts should never move the original physical curved
    // witness to a new midpoint. This detects topological anchor drift.
    for(const char* axis:{"x","y","z"}){
        const QString key=QStringLiteral("curved_witness_")+QString::fromLatin1(axis);
        require(std::abs(retargeted.parameters.value(key)-anchor.value(key))<1e-10,
            "first curved Boolean replaced the original witness");
    }
    // Trim the upper band without consuming the original witnessed region.
    const double secondCutZ=0.5*(14.0+anchor.value("curved_witness_z"));
    BRepAlgoAPI_Cut secondCrop(crop.Shape(),
        BRepPrimAPI_MakeBox(gp_Pnt(-20,-20,secondCutZ),40,40,20).Shape());
    secondCrop.Build();
    require(secondCrop.IsDone()&&!secondCrop.Shape().IsNull(),"second curved crop geometry");
    cad::ShapeFeatureNode twiceRetargeted;
    require(cad::rebaseFeatureThroughBoolean(crop.Shape(),secondCrop.Shape(),secondCrop,
        retargeted,&twiceRetargeted,&error),"curved opening survives two Boolean crops");
    require(cad::validateShapeFeatureNodes({twiceRetargeted},&error),
        "twice rebased curved opening validates");
    for(const char* axis:{"x","y","z"}){
        const QString key=QStringLiteral("curved_witness_")+QString::fromLatin1(axis);
        require(std::abs(twiceRetargeted.parameters.value(key)-anchor.value(key))<1e-10,
            "second curved Boolean drifted the original design witness");
    }

    cad::ShapeFeatureNode rotate;rotate.id="turn";rotate.type="Rotate X";
    rotate.parameters={{"degrees",45.0}};
    gp_Trsf turn;turn.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),std::acos(-1.0)/4.0);
    const TopoDS_Shape rotated=BRepBuilderAPI_Transform(cylinder,turn,true).Shape();
    cad::ShapeFeatureNode rotatedOpening;
    require(cad::rebaseFeatureThroughRigid(cylinder,rotated,rotate,opening,
        &rotatedOpening,&error),"curved opening follows rigid rotation");
    require(cad::validateShapeFeatureNodes({rotatedOpening},&error),"rigid curved opening validation");
    const gp_Pnt oldCurved(anchor.value("curved_witness_x"),anchor.value("curved_witness_y"),
                           anchor.value("curved_witness_z"));
    const gp_Pnt onceCurved=oldCurved.Transformed(turn);
    require(std::abs(rotatedOpening.parameters.value("curved_witness_x")-onceCurved.X())<1e-6&&
            std::abs(rotatedOpening.parameters.value("curved_witness_y")-onceCurved.Y())<1e-6&&
            std::abs(rotatedOpening.parameters.value("curved_witness_z")-onceCurved.Z())<1e-6,
            "rigid curved face must retain transported original witness");
    const auto twiceGeometry=BRepBuilderAPI_Transform(rotated,turn,true).Shape();
    cad::ShapeFeatureNode twiceRotatedOpening;
    require(cad::rebaseFeatureThroughRigid(rotated,twiceGeometry,rotate,rotatedOpening,
                    &twiceRotatedOpening,&error),"second curved rigid stage must resolve");
    const gp_Pnt twiceCurved=onceCurved.Transformed(turn);
    require(std::abs(twiceRotatedOpening.parameters.value("curved_witness_x")-twiceCurved.X())<1e-6&&
            std::abs(twiceRotatedOpening.parameters.value("curved_witness_y")-twiceCurved.Y())<1e-6&&
            std::abs(twiceRotatedOpening.parameters.value("curved_witness_z")-twiceCurved.Z())<1e-6,
            "two rigid curved stages drifted the original design witness");
    auto malformedOpening=opening;malformedOpening.parameters.remove("curved_witness_x");
    require(!cad::validateShapeFeatureNodes({malformedOpening},&error),"partial curved anchor rejected");
    // A smooth freeform face has no analytic center/radius. Its persisted
    // witness/normal may follow a SMALL local control-point deformation, but
    // only if one trimmed face remains close enough and oriented consistently.
    TColgp_Array2OfPnt poles(1,4,1,4);
    for(int u=1;u<=4;++u)for(int v=1;v<=4;++v)
        poles.SetValue(u,v,gp_Pnt(4*u,4*v,0.2*(u-2)*(u-2)+0.15*(v-2)*(v-2)));
    Handle(Geom_BezierSurface) freeform=new Geom_BezierSurface(poles);
    const auto bezierFace=BRepBuilderAPI_MakeFace(freeform,1e-7).Shape();
    QMap<QString,double> freeformAnchor;
    require(cad::captureCurvedFaceAnchor(bezierFace,1,&freeformAnchor,&error),
        "Bezier curved face has an interior witnessed anchor");
    require(freeformAnchor.value("curved_type")==6.0&&freeformAnchor.size()==17,
        "Bezier anchor has bounded local signature");
    require(cad::resolveCurvedFaceAnchor(bezierFace,freeformAnchor,&face,&error),
        "Bezier face self-resolution");
    poles.SetValue(2,2,gp_Pnt(8,8,0.2*0+0.15*0+0.08));
    Handle(Geom_BezierSurface) slightChange=new Geom_BezierSurface(poles);
    const auto editedBezier=BRepBuilderAPI_MakeFace(slightChange,1e-7).Shape();
    require(cad::resolveCurvedFaceAnchor(editedBezier,freeformAnchor,&face,&error),
        "small local Bezier deformation retains witness");
    const auto splineSurface=GeomConvert::SurfaceToBSplineSurface(freeform);
    const auto splineFace=BRepBuilderAPI_MakeFace(splineSurface,1e-7).Shape();
    QMap<QString,double> splineAnchor;
    require(cad::captureCurvedFaceAnchor(splineFace,1,&splineAnchor,&error),
        "B-spline surface captured");
    require(splineAnchor.value("curved_type")==5.0&&splineAnchor.size()==17,
        "B-spline support kind is preserved");
    require(cad::resolveCurvedFaceAnchor(splineFace,splineAnchor,&face,&error),
        "B-spline self-resolution");
    // Legacy single-interior-point v1 spline anchors remain readable.
    auto legacySpline=splineAnchor;
    for(const char* axis:{"x","y","z"})legacySpline.remove(QStringLiteral("curved_witness2_")+axis);
    require(cad::resolveCurvedFaceAnchor(splineFace,legacySpline,&face,&error),
        "Existing single-point spline anchors remain readable");
    auto invalidSecond=splineAnchor;
    invalidSecond["curved_witness2_x"]+=100.0;
    require(!cad::resolveCurvedFaceAnchor(splineFace,invalidSecond,&face,&error),
        "Freeform second witness must not silently jump to another region");
    gp_Trsf largeMove;largeMove.SetTranslation(gp_Vec(0,0,3));
    const auto detached=BRepBuilderAPI_Transform(bezierFace,largeMove,true).Shape();
    require(!cad::resolveCurvedFaceAnchor(detached,freeformAnchor,&face,&error),
        "unrelated distant Bezier surface cannot steal witness");
    gp_Trsf nearMove;nearMove.SetTranslation(gp_Vec(0,0,0.02));
    BRep_Builder builder;TopoDS_Compound ambiguous;builder.MakeCompound(ambiguous);
    builder.Add(ambiguous,bezierFace);
    builder.Add(ambiguous,BRepBuilderAPI_Transform(bezierFace,nearMove,true).Shape());
    require(!cad::resolveCurvedFaceAnchor(ambiguous,freeformAnchor,&face,&error),
        "two close spline faces are ambiguous, not silently reattached");

    // Pattern lineage must track an EXPLICIT occurrence, not merely the
    // closest lookalike. The selected cylinder travels exactly with its copy.
    cad::ShapeFeatureNode linear;linear.id="pattern1";linear.type="Linear pattern X";
    linear.parameters={{"count",3},{"spacing",50}};
    TopoDS_Compound repeated;builder.MakeCompound(repeated);
    builder.Add(repeated,cylinder);
    for(int instance=1;instance<3;++instance){
        gp_Trsf position;position.SetTranslation(gp_Vec(instance*50,0,0));
        builder.Add(repeated,BRepBuilderAPI_Transform(cylinder,position,true).Shape());
    }
    cad::ShapeFeatureNode chosenOpening;
    require(cad::rebaseFeatureThroughPattern(cylinder,repeated,linear,1,
            opening,&chosenOpening,&error),"linear pattern occurrence one follows curved shell witness");
    require(cad::validateShapeFeatureNodes({chosenOpening},&error),
        "patterned shell opening anchor validates");
    require(!cad::rebaseFeatureThroughPattern(cylinder,repeated,linear,3,
            opening,&chosenOpening,&error),"deleted pattern occurrence is rejected");
    cad::ShapeFeatureNode circular;
    circular.id="pattern2";circular.type="Circular pattern Z";
    circular.parameters={{"count",3},{"sweep_degrees",180}};
    TopoDS_Compound rotatedCopies;builder.MakeCompound(rotatedCopies);
    builder.Add(rotatedCopies,cylinder);
    for(int instance=1;instance<3;++instance){
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),
                             instance*90*std::acos(-1.0)/180);
        builder.Add(rotatedCopies,BRepBuilderAPI_Transform(cylinder,rotation,true).Shape());
    }
    // The cylinder lies exactly on the pattern's rotation axis: its copies
    // overlap, and the resolver must not choose an arbitrary occurrence.
    require(!cad::rebaseFeatureThroughPattern(cylinder,rotatedCopies,circular,1,
            opening,&chosenOpening,&error),"overlapping circular instances remain ambiguous");

}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);
 try{run();std::cout<<checks<<" curved lineage checks passed\n";return 0;}
 catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
