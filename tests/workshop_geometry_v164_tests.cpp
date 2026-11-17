#include "core/WorkshopGeometry.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <TopoDS_Shape.hxx>
#include <cmath>
#include <iostream>
#include <string>
int main(){
    using namespace cad::workshop;
    int checks=0,failures=0;
    const auto check=[&](bool v,const char* m){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<m<<'\n';}};
    CylinderInventory inventory;
    std::string error;
    const auto box=BRepPrimAPI_MakeBox(60,40,20).Shape();
    check(inspectCylindricalSurfaces(box,&inventory,&error),"box inspect");
    check(inventory.validTopology,"box topology");
    check(inventory.solids==1,"box one solid");
    check(inventory.faces==6,"box 6 faces");
    check(inventory.cylinders.empty(),"box no cylinders");
    const auto tool=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(20,15,-5),gp_Dir(0,0,1)),5,30).Shape();
    BRepAlgoAPI_Cut cut(box,tool);cut.Build();
    check(cut.IsDone(),"cut performed");
    check(inspectCylindricalSurfaces(cut.Shape(),&inventory,&error),"bored box inspect");
    check(inventory.validTopology,"bored box valid");
    check(inventory.cylinders.size()>=1,"cylindrical face exists");
    bool found=false;
    for(const auto& c:inventory.cylinders){
        if(std::abs(c.diameterMm-10)<1e-6){
            found=true;
            check(c.faceIndex>0&&c.faceIndex<=inventory.faces,"cylinder unique face index");
            check(c.lateralAreaMm2>0,"cylinder positive area");
            check(std::abs(std::abs(c.axisZ)-1)<1e-6,"cylinder axis");
            check(std::abs(c.centerXmm-20)<1e-6&&std::abs(c.centerYmm-15)<1e-6,"cylinder axis position");
        }
    }
    check(found,"detected 10mm cylindrical face");
    const auto before=inventory;
    check(!inspectCylindricalSurfaces(TopoDS_Shape{},&inventory,&error),"reject null shape");
    check(inventory.faces==before.faces,"failed inspection atomic");
    check(!inspectCylindricalSurfaces(box,nullptr,&error),"reject null inventory");
    std::cout<<(checks-failures)<<"/"<<checks<<" workshop OCCT checks passed\n";
    return failures?1:0;
}
