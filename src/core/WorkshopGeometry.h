#pragma once
// Read-only geometric inspection for manufacturing planning.
// Cylindrical surface records are NOT certified through-hole recognition.
#include <TopoDS_Shape.hxx>
#include <string>
#include <vector>
namespace cad::workshop {
struct CylinderSurface {
    int faceIndex=0; // 1-based TopExp unique-face index at time of inspection
    double diameterMm=0.0;
    double axialSpanMm=0.0; // axial parametric span, not drilling depth
    double lateralAreaMm2=0.0;
    double centerXmm=0.0,centerYmm=0.0,centerZmm=0.0;
    double axisX=0.0,axisY=0.0,axisZ=1.0;
    bool reversedOrientation=false; // orientation is not proof of internal bore
};
struct CylinderInventory {
    bool validTopology=false;
    int solids=0;
    int faces=0;
    std::vector<CylinderSurface> cylinders;
    std::vector<std::string> warnings;
};
bool inspectCylindricalSurfaces(const TopoDS_Shape& shape,
                                CylinderInventory* output,std::string* error=nullptr);
}
