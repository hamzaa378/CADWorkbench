#include "core/SurfaceModeling.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pnt.hxx>

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace {
int assertions=0;
void require(bool condition,const char* message){
    ++assertions;
    if(!condition)throw std::runtime_error(message);
}

TopoDS_Wire squareWire(double z){
    const gp_Pnt points[]={{0,0,z},{10,0,z},{10,10,z},{0,10,z}};
    BRepBuilderAPI_MakeWire wire;
    for(int i=0;i<4;++i)wire.Add(BRepBuilderAPI_MakeEdge(points[i],points[(i+1)%4]).Edge());
    if(!wire.IsDone())throw std::runtime_error("Could not make surface-shell test wire.");
    return wire.Wire();
}
}

int main(){
    try{
        const TopoDS_Edge first=BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,0),gp_Pnt(10,0,0)).Edge();
        const TopoDS_Edge second=BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,5),gp_Pnt(10,0,5)).Edge();
        TopoDS_Face face;QString error;
        require(cad::makeRuledSurface(first,second,&face,&error),"Could not create ruled face between parallel edges.");
        require(!face.IsNull()&&BRepCheck_Analyzer(face).IsValid(),"Ruled edge surface is null or invalid.");
        GProp_GProps faceProperties;BRepGProp::SurfaceProperties(face,faceProperties);
        require(std::abs(faceProperties.Mass()-50.0)<1e-7,"Ruled face area is not the expected 50 mm².");

        TopoDS_Shell shell;
        require(cad::makeRuledSurfaceShell(squareWire(0),squareWire(5),&shell,&error),
                "Could not create ruled shell between matching closed wires.");
        require(!shell.IsNull()&&BRepCheck_Analyzer(shell).IsValid(),"Ruled surface shell is null or invalid.");
        GProp_GProps shellProperties;BRepGProp::SurfaceProperties(shell,shellProperties);
        require(std::abs(shellProperties.Mass()-200.0)<1e-6,"Ruled shell area is not the expected 200 mm².");

        TopoDS_Face extrudedSurface;
        require(cad::makeExtrudedSurface(first,gp_Vec(0,0,5),&extrudedSurface,&error),
                "Could not extrude a surface from a profile edge.");
        require(!extrudedSurface.IsNull()&&BRepCheck_Analyzer(extrudedSurface).IsValid(),
                "Extruded surface face is null or invalid.");
        GProp_GProps extrusionProperties;BRepGProp::SurfaceProperties(extrudedSurface,extrusionProperties);
        require(std::abs(extrusionProperties.Mass()-50.0)<1e-7,
                "Extruded surface area is not the expected 50 mm².");
        TopoDS_Face preservedExtrusion=extrudedSurface;
        require(!cad::makeExtrudedSurface(first,gp_Vec(0,0,0),&preservedExtrusion,&error)&&
                preservedExtrusion.IsSame(extrudedSurface),
                "Zero-length extrusion was accepted or changed the existing output face.");

        const TopoDS_Edge revolveProfile=BRepBuilderAPI_MakeEdge(gp_Pnt(2,0,0),gp_Pnt(2,0,5)).Edge();
        const gp_Ax1 revolveAxis(gp_Pnt(0,0,0),gp_Dir(0,0,1));
        TopoDS_Face revolvedSurface;
        require(cad::makeRevolvedSurface(revolveProfile,revolveAxis,std::numbers::pi/2,&revolvedSurface,&error),
                "Could not revolve a surface profile around an axis.");
        require(!revolvedSurface.IsNull()&&BRepCheck_Analyzer(revolvedSurface).IsValid(),
                "Revolved surface face is null or invalid.");
        GProp_GProps revolutionProperties;BRepGProp::SurfaceProperties(revolvedSurface,revolutionProperties);
        require(std::abs(revolutionProperties.Mass()-5.0*std::numbers::pi)<1e-6,
                "Quarter-turn revolved surface area is incorrect.");
        TopoDS_Face preservedRevolution=revolvedSurface;
        require(!cad::makeRevolvedSurface(revolveProfile,revolveAxis,0.0,&preservedRevolution,&error)&&
                preservedRevolution.IsSame(revolvedSurface),
                "Zero-angle revolution was accepted or changed the existing output face.");

        const TopoDS_Face planarFace=BRepBuilderAPI_MakeFace(squareWire(0)).Face();
        TopoDS_Shape offsetSurface;
        require(cad::makeOffsetSurface(planarFace,2.0,1e-5,&offsetSurface,&error),
                "Could not offset a planar surface face.");
        require(!offsetSurface.IsNull()&&BRepCheck_Analyzer(offsetSurface).IsValid(),
                "Offset surface is null or invalid.");
        GProp_GProps offsetProperties;BRepGProp::SurfaceProperties(offsetSurface,offsetProperties);
        require(std::abs(offsetProperties.Mass()-100.0)<1e-6,
                "Offsetting a planar surface changed its expected area.");
        TopoDS_Shape preservedOffset=offsetSurface;
        require(!cad::makeOffsetSurface(planarFace,0.0,1e-5,&preservedOffset,&error)&&
                preservedOffset.IsSame(offsetSurface),
                "Zero-distance surface offset was accepted or changed the existing output shape.");

        TopoDS_Face extendedSurface;
        require(cad::extendSurfaceFace(planarFace,1.0,true,true,true,true,&extendedSurface,&error),
                "Could not extend the planar surface face on all four UV sides.");
        require(!extendedSurface.IsNull()&&BRepCheck_Analyzer(extendedSurface).IsValid(),
                "Extended surface face is null or invalid.");
        GProp_GProps extensionProperties;BRepGProp::SurfaceProperties(extendedSurface,extensionProperties);
        require(extensionProperties.Mass()>100.0,
                "Extending all sides did not increase the planar face area.");
        TopoDS_Face preservedExtension=extendedSurface;
        require(!cad::extendSurfaceFace(planarFace,1.0,false,false,false,false,&preservedExtension,&error)&&
                preservedExtension.IsSame(extendedSurface),
                "An extension without selected sides was accepted or changed the output face.");

        TopoDS_Shell loftedShell;
        const std::vector<TopoDS_Wire> sections{squareWire(0),squareWire(5),squareWire(10)};
        require(cad::makeLoftedSurfaceShell(sections,&loftedShell,&error),
                "Could not loft a surface through three ordered section wires.");
        require(!loftedShell.IsNull()&&BRepCheck_Analyzer(loftedShell).IsValid(),
                "Lofted surface shell is null or invalid.");
        GProp_GProps loftedProperties;BRepGProp::SurfaceProperties(loftedShell,loftedProperties);
        require(std::abs(loftedProperties.Mass()-400.0)<1e-5,
                "Three-section square loft did not produce the expected lateral surface area.");
        TopoDS_Shell preservedLoft=loftedShell;
        require(!cad::makeLoftedSurfaceShell({squareWire(0)},&preservedLoft,&error)&&
                preservedLoft.IsSame(loftedShell)&&error.contains("at least two"),
                "A single loft section was accepted or changed the existing output shell.");
        require(!cad::makeLoftedSurfaceShell({squareWire(0),TopoDS_Wire{}},&preservedLoft,&error)&&
                preservedLoft.IsSame(loftedShell),
                "An invalid loft section was accepted or changed the existing output shell.");
        require(!cad::makeLoftedSurfaceShell(sections,nullptr,&error),
                "Null loft-shell output was accepted.");

        TopoDS_Face preserved=face;
        require(!cad::makeRuledSurface(TopoDS_Edge{},second,&preserved,&error)&&preserved.IsSame(face),
                "Rejected edge input changed the caller's existing output face.");
        require(!cad::makeRuledSurface(first,second,nullptr,&error),"Null ruled-face output was accepted.");

        BRepBuilderAPI_MakeWire mismatchedWire;
        mismatchedWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(0,0,10),gp_Pnt(5,0,10)).Edge());
        mismatchedWire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(5,0,10),gp_Pnt(10,0,10)).Edge());
        TopoDS_Shell preservedShell=shell;
        require(!cad::makeRuledSurfaceShell(squareWire(0),mismatchedWire.Wire(),&preservedShell,&error)&&
                preservedShell.IsSame(shell)&&error.contains("same number"),
                "Mismatched shell boundaries were accepted or changed the existing output.");
        require(!cad::makeRuledSurfaceShell(squareWire(0),squareWire(5),nullptr,&error),
                "Null ruled-shell output was accepted.");
        std::cout<<"PASS: "<<assertions<<" surface-modeling assertions\n";
        return EXIT_SUCCESS;
    }catch(const std::exception& error){
        std::cerr<<"FAIL after "<<assertions<<" assertions: "<<error.what()<<'\n';
        return EXIT_FAILURE;
    }
}
