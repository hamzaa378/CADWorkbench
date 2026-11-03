#include "SurfaceModeling.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepFill.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepOffset_Mode.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <GeomAbs_JoinType.hxx>

#include <cmath>
#include <exception>
#include <numbers>

namespace cad {
namespace {
bool fail(QString* error,const QString& message) {
    if(error)*error=message;
    return false;
}

bool validEdge(const TopoDS_Edge& edge) {
    if(edge.IsNull()||!BRepCheck_Analyzer(edge).IsValid())return false;
    GProp_GProps properties;
    BRepGProp::LinearProperties(edge,properties);
    return std::isfinite(properties.Mass())&&properties.Mass()>1e-9;
}

bool validWire(const TopoDS_Wire& wire,int* edgeCount) {
    if(wire.IsNull()||!BRepCheck_Analyzer(wire).IsValid())return false;
    int count=0;
    for(TopExp_Explorer edges(wire,TopAbs_EDGE);edges.More();edges.Next()){
        if(!validEdge(TopoDS::Edge(edges.Current())))return false;
        ++count;
    }
    if(edgeCount)*edgeCount=count;
    return count>0;
}

bool validSurface(const TopoDS_Shape& shape) {
    if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid())return false;
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(shape,properties);
    return std::isfinite(properties.Mass())&&properties.Mass()>1e-9;
}

bool validSurfaceOnly(const TopoDS_Shape& shape) {
    if(shape.IsNull())return false;
    const TopAbs_ShapeEnum type=shape.ShapeType();
    return (type==TopAbs_FACE||type==TopAbs_SHELL)&&validSurface(shape);
}
} // namespace

bool makeRuledSurface(const TopoDS_Edge& firstBoundary,const TopoDS_Edge& secondBoundary,
                      TopoDS_Face* result,QString* error) {
    if(!result)return fail(error,"Provide an output face for the ruled surface.");
    if(!validEdge(firstBoundary)||!validEdge(secondBoundary))
        return fail(error,"Both ruled-surface boundaries must be valid, non-degenerate edges.");
    try{
        const TopoDS_Face candidate=BRepFill::Face(firstBoundary,secondBoundary);
        if(candidate.IsNull()||candidate.ShapeType()!=TopAbs_FACE||!validSurface(candidate))
            return fail(error,"OCCT could not create a valid ruled surface between those edges.");
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("Ruled-surface kernel error: %1").arg(failure.GetMessageString()));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Ruled-surface construction failed: %1").arg(QString::fromUtf8(failure.what())));
    }
}

bool makeRuledSurfaceShell(const TopoDS_Wire& firstBoundary,const TopoDS_Wire& secondBoundary,
                           TopoDS_Shell* result,QString* error) {
    if(!result)return fail(error,"Provide an output shell for the ruled surface.");
    int firstEdgeCount=0,secondEdgeCount=0;
    if(!validWire(firstBoundary,&firstEdgeCount)||!validWire(secondBoundary,&secondEdgeCount))
        return fail(error,"Both ruled-surface boundaries must be valid wires with non-degenerate edges.");
    if(firstEdgeCount!=secondEdgeCount)
        return fail(error,"Ruled-surface boundary wires must have the same number of corresponding edges.");
    try{
        const TopoDS_Shell candidate=BRepFill::Shell(firstBoundary,secondBoundary);
        if(candidate.IsNull()||candidate.ShapeType()!=TopAbs_SHELL||!validSurface(candidate))
            return fail(error,"OCCT could not create a valid ruled shell between those wires.");
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("Ruled-shell kernel error: %1").arg(failure.GetMessageString()));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Ruled-shell construction failed: %1").arg(QString::fromUtf8(failure.what())));
    }
}

bool makeLoftedSurfaceShell(const std::vector<TopoDS_Wire>& sectionWires,
                            TopoDS_Shell* result,QString* error) {
    if(!result)return fail(error,"Provide an output shell for the lofted surface.");
    if(sectionWires.size()<2)
        return fail(error,"A lofted surface needs at least two section wires.");
    int expectedEdgeCount=-1;
    for(const TopoDS_Wire& section:sectionWires){
        int edgeCount=0;
        if(!validWire(section,&edgeCount))
            return fail(error,"Every loft section must be a valid wire with non-degenerate edges.");
        if(expectedEdgeCount<0)expectedEdgeCount=edgeCount;
        else if(edgeCount!=expectedEdgeCount)
            return fail(error,"Loft section wires must have the same number of corresponding edges.");
    }
    try{
        BRepOffsetAPI_ThruSections loft(false,false);
        for(const TopoDS_Wire& section:sectionWires)loft.AddWire(section);
        loft.Build();
        if(!loft.IsDone())return fail(error,"OCCT could not build a surface through those loft sections.");
        const TopoDS_Shape candidate=loft.Shape();
        if(candidate.IsNull()||candidate.ShapeType()!=TopAbs_SHELL||!validSurface(candidate))
            return fail(error,"OCCT loft result was not a valid non-zero-area surface shell.");
        *result=TopoDS::Shell(candidate);
        return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("Surface-loft kernel error: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Surface-loft construction failed: %1").arg(QString::fromUtf8(failure.what())));
    }
}

bool makeExtrudedSurface(const TopoDS_Edge& profile,const gp_Vec& translation,
                         TopoDS_Face* result,QString* error) {
    if(!result)return fail(error,"Provide an output face for the extruded surface.");
    if(!validEdge(profile))return fail(error,"The extrusion profile must be a valid, non-degenerate edge.");
    if(!std::isfinite(translation.X())||!std::isfinite(translation.Y())||!std::isfinite(translation.Z())||
       translation.SquareMagnitude()<=1e-18)
        return fail(error,"The surface extrusion vector must be finite and non-zero.");
    try{
        BRepPrimAPI_MakePrism prism(profile,translation,false,true);
        if(!prism.IsDone())return fail(error,"OCCT could not extrude the selected surface profile.");
        const TopoDS_Shape candidate=prism.Shape();
        if(candidate.ShapeType()!=TopAbs_FACE||!validSurface(candidate))
            return fail(error,"Surface extrusion did not produce a valid non-zero-area face.");
        *result=TopoDS::Face(candidate);
        return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("Surface-extrusion kernel error: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Surface extrusion failed: %1").arg(QString::fromUtf8(failure.what())));
    }
}

bool makeRevolvedSurface(const TopoDS_Edge& profile,const gp_Ax1& axis,double angleRadians,
                         TopoDS_Face* result,QString* error) {
    if(!result)return fail(error,"Provide an output face for the revolved surface.");
    if(!validEdge(profile))return fail(error,"The revolution profile must be a valid, non-degenerate edge.");
    constexpr double fullTurn=2.0*std::numbers::pi;
    if(!std::isfinite(angleRadians)||angleRadians<1e-6||angleRadians>fullTurn)
        return fail(error,"The surface revolution angle must be finite and between 0.000001 and 2π radians.");
    try{
        BRepPrimAPI_MakeRevol revol(profile,axis,angleRadians,false);
        if(!revol.IsDone())return fail(error,"OCCT could not revolve the selected surface profile.");
        const TopoDS_Shape candidate=revol.Shape();
        if(candidate.ShapeType()!=TopAbs_FACE||!validSurface(candidate))
            return fail(error,"Surface revolution did not produce a valid non-zero-area face.");
        *result=TopoDS::Face(candidate);
        return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("Surface-revolution kernel error: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Surface revolution failed: %1").arg(QString::fromUtf8(failure.what())));
    }
}

bool makeOffsetSurface(const TopoDS_Face& source,double offsetMm,double toleranceMm,
                       TopoDS_Shape* result,QString* error) {
    if(!result)return fail(error,"Provide an output shape for the offset surface.");
    if(source.IsNull()||source.ShapeType()!=TopAbs_FACE||!validSurface(source))
        return fail(error,"The offset source must be a valid, non-degenerate surface face.");
    if(!std::isfinite(offsetMm)||std::abs(offsetMm)<1e-6||std::abs(offsetMm)>1e6)
        return fail(error,"Surface offset must be finite, non-zero, and no greater than 1,000,000 mm in magnitude.");
    if(!std::isfinite(toleranceMm)||toleranceMm<1e-7||toleranceMm>1.0)
        return fail(error,"Offset tolerance must be finite and between 0.0000001 and 1 mm.");
    try{
        BRepOffsetAPI_MakeOffsetShape offsetter;
        offsetter.PerformByJoin(source,offsetMm,toleranceMm,BRepOffset_Skin,false,false,GeomAbs_Arc,false);
        if(!offsetter.IsDone())return fail(error,"OCCT could not construct the offset surface; the offset may be singular or self-intersecting.");
        const TopoDS_Shape candidate=offsetter.Shape();
        if(!validSurfaceOnly(candidate))
            return fail(error,"Offset operation did not produce a valid non-zero-area face or shell.");
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("Surface-offset kernel error: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Surface offset failed: %1").arg(QString::fromUtf8(failure.what())));
    }
}

bool extendSurfaceFace(const TopoDS_Face& source,double extensionMm,
                       bool extendUMin,bool extendUMax,bool extendVMin,bool extendVMax,
                       TopoDS_Face* result,QString* error) {
    if(!result)return fail(error,"Provide an output face for the extended surface.");
    if(source.IsNull()||source.ShapeType()!=TopAbs_FACE||!validSurface(source))
        return fail(error,"The extension source must be a valid, non-degenerate surface face.");
    if(!std::isfinite(extensionMm)||extensionMm<1e-6||extensionMm>1e6)
        return fail(error,"Surface extension must be finite and between 0.000001 and 1,000,000 mm.");
    if(!extendUMin&&!extendUMax&&!extendVMin&&!extendVMax)
        return fail(error,"Select at least one U or V side to extend.");
    try{
        TopoDS_Face candidate;
        BRepLib::ExtendFace(source,extensionMm,extendUMin,extendUMax,extendVMin,extendVMax,candidate);
        if(candidate.IsNull()||candidate.ShapeType()!=TopAbs_FACE||!validSurface(candidate))
            return fail(error,"OCCT could not extend that surface without producing invalid geometry.");
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("Surface-extension kernel error: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Surface extension failed: %1").arg(QString::fromUtf8(failure.what())));
    }
}

} // namespace cad
