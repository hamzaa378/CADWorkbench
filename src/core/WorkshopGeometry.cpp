#include "WorkshopGeometry.h"
#include <BRepAdaptor_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Face.hxx>
#include <utility>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <cmath>
#include <exception>
namespace cad::workshop {
bool inspectCylindricalSurfaces(const TopoDS_Shape& shape,
                                CylinderInventory* output,std::string* error){
    if(!output){if(error)*error="Inventory output is required.";return false;}
    if(shape.IsNull()){if(error)*error="Cannot inspect an empty shape.";return false;}
    CylinderInventory draft;
    try{
        BRepCheck_Analyzer analyzer(shape);
        draft.validTopology=analyzer.IsValid();
        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(shape,TopAbs_FACE,faceMap);
        draft.faces=faceMap.Extent();
        TopTools_IndexedMapOfShape solidMap;
        TopExp::MapShapes(shape,TopAbs_SOLID,solidMap);
        draft.solids=solidMap.Extent();
        for(int i=1;i<=faceMap.Extent();++i){
            const TopoDS_Face face=TopoDS::Face(faceMap.FindKey(i));
            const BRepAdaptor_Surface adaptor(face);
            if(adaptor.GetType()!=GeomAbs_Cylinder)continue;
            const auto cylinder=adaptor.Cylinder();
            const gp_Ax1 axis=cylinder.Axis();
            const gp_Pnt origin=axis.Location();
            CylinderSurface item;
            item.faceIndex=i;
            item.diameterMm=2*cylinder.Radius();
            item.axisX=axis.Direction().X();item.axisY=axis.Direction().Y();item.axisZ=axis.Direction().Z();
            item.reversedOrientation=face.Orientation()==TopAbs_REVERSED;
            double ax=origin.X(),ay=origin.Y(),az=origin.Z();
            const double u0=adaptor.FirstUParameter(),u1=adaptor.LastUParameter();
            const double v0=adaptor.FirstVParameter(),v1=adaptor.LastVParameter();
            if(std::isfinite(u0)&&std::isfinite(u1)&&std::isfinite(v0)&&std::isfinite(v1)&&
               std::abs(u0)<1e9&&std::abs(u1)<1e9&&std::abs(v0)<1e9&&std::abs(v1)<1e9){
                const gp_Pnt sample=adaptor.Value((u0+u1)/2,(v0+v1)/2);
                const double projection=gp_Vec(origin,sample).Dot(gp_Vec(axis.Direction()));
                ax+=item.axisX*projection;ay+=item.axisY*projection;az+=item.axisZ*projection;
                item.axialSpanMm=std::abs(v1-v0);
            }else draft.warnings.push_back("One cylindrical surface has unbounded UV parameters; axial span may be unavailable.");
            item.centerXmm=ax;item.centerYmm=ay;item.centerZmm=az;
            GProp_GProps area;
            BRepGProp::SurfaceProperties(face,area);
            item.lateralAreaMm2=area.Mass();
            if(!std::isfinite(item.diameterMm)||item.diameterMm<=0||
               !std::isfinite(item.lateralAreaMm2)||item.lateralAreaMm2<0||
               !std::isfinite(item.centerXmm)||!std::isfinite(item.centerYmm)||!std::isfinite(item.centerZmm)){
                if(error)*error="Cylinder surface has invalid geometry values.";
                return false;
            }
            draft.cylinders.push_back(item);
        }
    }catch(const Standard_Failure& failure){
        if(error)*error=std::string("OpenCascade inspection failed: ")+failure.what();return false;
    }catch(const std::exception& failure){
        if(error)*error=std::string("Inspection failed: ")+failure.what();return false;
    }
    if(!draft.validTopology)draft.warnings.push_back("B-rep validity check failed; do not rely on surface measurements for manufacturing.");
    draft.warnings.push_back("Cylindrical faces include shafts and bosses as well as bores. Verify actual hole type, axis, depth and accessibility manually.");
    *output=std::move(draft);if(error)error->clear();return true;
}
}
