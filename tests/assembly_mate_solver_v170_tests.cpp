#include "core/AssemblyMateSolver.h"
#include <stdexcept>
#include <cmath>
#include <iostream>
using namespace cad::mates;
static bool close(Vec3 a,Vec3 b,double tol=1e-4){
    return std::abs(a.x-b.x)<tol&&std::abs(a.y-b.y)<tol&&std::abs(a.z-b.z)<tol;
}
int main(){
    try{
    int checks=0;
    const auto require=[](bool success,const char* message){if(!success)throw std::runtime_error(message);};
    // A fully constrained triangle fixes three translations and all rotations.
    std::vector<Constraint> triangle;
    const double a=0.6;const double c=std::cos(a),s=std::sin(a);
    const auto map=[&](Vec3 p){return Vec3{c*p.x-s*p.y+15,s*p.x+c*p.y-8,p.z+4};};
    for(Vec3 p:{Vec3{0,0,0},Vec3{30,0,0},Vec3{0,20,0},Vec3{10,10,0}})
        triangle.push_back({Kind::Point,map(p),p,{},{}});
    Transform pose;std::string error;
    require(solve(triangle,&pose,&error),"mate regression check 1");++checks;
    for(const auto& mate:triangle){require(close(apply(pose,mate.childPoint),mate.parentPoint),"mate regression check 2");++checks;}
    // Plane + a nonparallel axis + a point fully constrain the child pose.
    const Vec3 axisX{1,0,0},axisY{0,1,0},axisZ{0,0,1};
    const auto direction=[&](Vec3 p){return Vec3{c*p.x-s*p.y,s*p.x+c*p.y,p.z};};
    const std::vector<Constraint> frame={
        {Kind::Point,map({2,3,4}),{2,3,4},{},{}},
        {Kind::CoaxialAxis,map({0,0,0}),{0,0,0},direction(axisX),axisX},
        {Kind::CoincidentPlane,map({0,0,0}),{0,0,0},direction(axisY),axisY}};
    require(solve(frame,&pose,&error),"mate regression check 3");++checks;
    for(const auto& mate:frame){require(close(apply(pose,mate.childPoint),mate.parentPoint),"mate regression check 4");++checks;}
    // The native editor's point-plus-two-axis mode must constrain a complete frame.
    const std::vector<Constraint> dualAxes={
        {Kind::Point,map({0,0,0}),{0,0,0},{},{}},
        {Kind::CoaxialAxis,map({0,0,0}),{0,0,0},direction(axisX),axisX},
        {Kind::CoaxialAxis,map({0,0,0}),{0,0,0},direction(axisY),axisY}};
    require(solve(dualAxes,&pose,&error),"two-axis frame mode failed");++checks;
    require(close(apply(pose,{0,0,0}),map({0,0,0})),"two-axis frame origin incorrect");++checks;
    require(close(apply(pose,{1,0,0}),map({1,0,0})),"two-axis frame rotation incorrect");++checks;
    // F05: signed plane separation works with fully constrained lock observations.
    std::vector<Constraint> offsetFrame={
        {Kind::Point,{0,6,0},{0,0,0},{},{}},
        {Kind::CoaxialAxis,{0,6,0},{0,0,0},axisX,axisX},
        {Kind::CoincidentPlane,{0,0,0},{0,0,0},axisY,axisY,6.0}};
    require(solve(offsetFrame,&pose,&error),"signed 6-mm plane mate must solve");++checks;
    require(std::abs(pose.translation.y-6.0)<1e-4,"signed plane separation wrong");++checks;
    const auto previousOffsetPose=pose;
    offsetFrame[2].offsetMm=8.0;
    require(!solve(offsetFrame,&pose,&error),"contradictory plane offset must fail");++checks;
    require(std::abs(pose.translation.y-previousOffsetPose.translation.y)<1e-12,
            "failed plane offset modified previous solved placement");++checks;
    offsetFrame[2].offsetMm=6.0;
    offsetFrame[1].offsetMm=1.0;
    require(!solve(offsetFrame,&pose,&error),"axis must reject plane offset field");++checks;
        // A point and only one axis leaves the rotation about that axis free.
    require(!solve({dualAxes[0],dualAxes[1]},&pose,&error),"single axis incorrectly fully constrained");++checks;
    // A single plane is underconstrained even if it is already coincident.
    const Transform saved=pose;
    require(!solve({{Kind::CoincidentPlane,{0,0,0},{0,0,0},axisZ,axisZ}},&pose,&error),"mate regression check 5");++checks;
    require(pose.translation.x==saved.translation.x,"mate regression check 6");++checks;
    // Inconsistent point pairs must not return a compromise.
    auto impossible=triangle;impossible[3].parentPoint.x+=5;
    require(!solve(impossible,&pose,&error),"mate regression check 7");++checks;
    require(!solve({{Kind::CoaxialAxis,{},{},{2,0,0},axisX}},&pose,&error),"mate regression check 8");++checks;
    require(!solve({},&pose,&error),"mate regression check 9");++checks;
    // Off-origin assembly: the same six-DOF pose at 100 m coordinates.
    std::vector<Constraint> far;
    for(Vec3 p:{Vec3{100000,200000,1000},Vec3{100030,200000,1000},
                Vec3{100000,200020,1000},Vec3{100004,200006,1020}}){
        const Vec3 local{p.x-100000,p.y-200000,p.z-1000};
        far.push_back({Kind::Point,{100050+c*local.x-s*local.y,
                                    200070+s*local.x+c*local.y,1025+local.z},p,{},{}});
    }
    require(solve(far,&pose,&error),"mate regression check 10");++checks;
    for(const auto& mate:far){require(close(apply(pose,mate.childPoint),mate.parentPoint),"mate regression check 11");++checks;}
    std::cout<<"assembly mate solver: "<<checks<<" regression assertions passed\n";
    return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
