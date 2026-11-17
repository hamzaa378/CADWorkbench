#pragma once
#include <array>
#include <string>
#include <vector>

// A bounded, deterministic rigid-body constraint solve in mm/radians. Coordinates
// are expressed in the CURRENT world frame; output maps child world -> desired
// child world. Exact frame/axis/point and plane mates are not a dynamics solver.
namespace cad::mates {
struct Vec3 {double x=0,y=0,z=0;};
enum class Kind { Point, CoaxialAxis, CoincidentPlane };
struct Constraint {
    Kind kind=Kind::Point;
    Vec3 parentPoint,childPoint;
    // Unit direction of the axes or plane normals; ignored for Point.
    Vec3 parentDirection,childDirection;
    // Signed offset along the PARENT plane normal in millimetres.
    // Zero retains the old coincident-plane behavior and scene compatibility.
    // Only Kind::CoincidentPlane may use a nonzero value.
    double offsetMm=0.0;
};
struct Transform {
    std::array<double,9> rotation{{1,0,0,0,1,0,0,0,1}};
    Vec3 translation;
    double maximumResidualMm=0;
};
bool solve(const std::vector<Constraint>& constraints,Transform* result,std::string* error=nullptr);
Vec3 apply(const Transform& transform,const Vec3& point);
}
