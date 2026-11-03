#include "../src/core/MeshRepair.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace {
int checks=0;
void require(bool condition,const char* message){++checks;if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
void triangle(std::vector<cad::mesh::Point3>& vertices,std::vector<cad::mesh::Triangle>& faces,
              const std::array<cad::mesh::Point3,3>& points,bool reverse){
    const auto base=static_cast<std::uint32_t>(vertices.size());
    if(reverse)vertices.insert(vertices.end(),{points[0],points[2],points[1]});
    else vertices.insert(vertices.end(),points.begin(),points.end());
    faces.push_back({base,base+1,base+2});
}
}

int main(){
    using cad::mesh::Point3;using cad::mesh::Triangle;
    std::vector<Point3> vertices;std::vector<Triangle> faces;
    const Point3 p000{0,0,0},p100{1,0,0},p110{1,1,0},p010{0,1,0};
    const Point3 p001{0,0,1},p101{1,0,1},p111{1,1,1},p011{0,1,1};
    const std::array<std::array<Point3,3>,12> cube{{
        {{p000,p110,p100}},{{p000,p010,p110}},{{p001,p101,p111}},{{p001,p111,p011}},
        {{p000,p100,p101}},{{p000,p101,p001}},{{p100,p110,p111}},{{p100,p111,p101}},
        {{p110,p010,p011}},{{p110,p011,p111}},{{p010,p000,p001}},{{p010,p001,p011}}
    }};
    for(std::size_t i=0;i<cube.size();++i)triangle(vertices,faces,cube[i],i%3==0);
    triangle(vertices,faces,cube[0],false); // duplicate face
    const auto degenerateBase=static_cast<std::uint32_t>(vertices.size());
    vertices.insert(vertices.end(),{{0,0,0},{0,0,0},{0,0,0}});faces.push_back({degenerateBase,degenerateBase+1,degenerateBase+2});
    vertices.push_back({9,9,9}); // unused point should be compacted away

    cad::mesh::RepairResult result;std::string error;
    require(cad::mesh::repairTriangleMesh(vertices,faces,1e-4,&result,&error),"Closed cube repair failed");
    require(result.triangles.size()==12&&result.vertices.size()==8,"Welding and cleanup did not produce a compact cube mesh");
    require(result.stats.outputTriangles==12&&result.stats.outputVertices==8,
            "Output mesh counts were not returned in repair statistics");
    require(result.stats.weldedVertices>0&&result.stats.removedDuplicateTriangles==1&&
            result.stats.removedDegenerateTriangles==1,"Weld/cleanup statistics were incorrect");
    require(result.stats.flippedTriangles>0&&result.stats.connectedComponents==1,"Winding repair or connected-component count failed");
    require(result.stats.boundaryEdges==0&&result.stats.nonManifoldEdges==0,"Repaired cube has boundary or non-manifold edges");

    struct Use{int count=0,balance=0;};std::map<std::pair<std::uint32_t,std::uint32_t>,Use> edgeUses;
    auto edge=[&](std::uint32_t a,std::uint32_t b){auto& use=edgeUses[std::minmax(a,b)];++use.count;use.balance+=(a<b?1:-1);};
    for(const Triangle& face:result.triangles){edge(face.a,face.b);edge(face.b,face.c);edge(face.c,face.a);}
    for(const auto& entry:edgeUses)require(entry.second.count==2&&entry.second.balance==0,"Repaired cube face winding is inconsistent");

    const std::vector<Point3> openVertices{{0,0,0},{1,0,0},{0,1,0}};
    const std::vector<Triangle> openFaces{{0,1,2}};cad::mesh::RepairResult open;
    require(cad::mesh::repairTriangleMesh(openVertices,openFaces,1e-3,&open,&error)&&
            open.stats.boundaryEdges==3&&open.stats.nonManifoldEdges==0,"Open mesh boundary diagnostics failed");

    std::vector<Point3> openCubeVertices;std::vector<Triangle> openCubeFaces;
    for(std::size_t face=0;face<cube.size();++face){
        if(face==2||face==3)continue; // omit the top square cap
        triangle(openCubeVertices,openCubeFaces,cube[face],face%3==0);
    }
    cad::mesh::RepairResult openCube;
    require(cad::mesh::repairTriangleMesh(openCubeVertices,openCubeFaces,1e-4,&openCube,&error)&&
            openCube.stats.boundaryEdges==4,"Open cube fixture did not contain one square boundary loop");
    require(cad::mesh::fillPlanarBoundaryHoles(&openCube,1e-6,32,&error),
            "Planar square opening could not be filled");
    require(openCube.stats.filledHoles==1&&openCube.stats.filledHoleTriangles==2&&
            openCube.stats.outputTriangles==12&&openCube.stats.boundaryEdges==0&&openCube.stats.nonManifoldEdges==0,
            "Planar hole-fill counts or closed-surface diagnostics were incorrect");
    edgeUses.clear();
    for(const Triangle& face:openCube.triangles){edge(face.a,face.b);edge(face.b,face.c);edge(face.c,face.a);}
    for(const auto& entry:edgeUses)require(entry.second.count==2&&entry.second.balance==0,
            "Filled cap winding did not oppose its surrounding boundary edges");

    const std::vector<Point3> warpedVertices{{0,0,0},{1,0,0},{1,1,0.1},{0,1,0},{0.5,0.5,0}};
    const std::vector<Triangle> warpedFaces{{0,1,4},{1,2,4},{2,3,4},{3,0,4}};
    cad::mesh::RepairResult warped;require(cad::mesh::repairTriangleMesh(warpedVertices,warpedFaces,1e-4,&warped,&error),
            "Could not prepare a warped open boundary for safe-fill rejection");
    const auto warpedTriangles=warped.triangles;const auto warpedBoundary=warped.stats.boundaryEdges;
    require(!cad::mesh::fillPlanarBoundaryHoles(&warped,1e-4,32,&error)&&
            warped.triangles.size()==warpedTriangles.size()&&warped.stats.boundaryEdges==warpedBoundary,
            "Non-planar hole was filled or changed the caller's mesh");
    require(!cad::mesh::fillPlanarBoundaryHoles(&warped,1e-4,3,&error)&&
            warped.triangles.size()==warpedTriangles.size(),"Oversized boundary loop was filled or partially changed the mesh");
    require(!cad::mesh::fillPlanarBoundaryHoles(&open,0.0,32,&error)&&open.stats.boundaryEdges==3,
            "Invalid planarity tolerance changed an open mesh");

    const std::vector<Point3> nonManifoldVertices{{0,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1}};
    const std::vector<Triangle> nonManifoldFaces{{0,1,2},{1,0,3},{0,1,4}};cad::mesh::RepairResult nonManifold;
    require(cad::mesh::repairTriangleMesh(nonManifoldVertices,nonManifoldFaces,1e-3,&nonManifold,&error)&&
            nonManifold.stats.nonManifoldEdges==1,"Non-manifold edge should be reported without discarding the input surface");
    const std::size_t nonManifoldTriangleCount=nonManifold.triangles.size();
    require(!cad::mesh::fillPlanarBoundaryHoles(&nonManifold,1e-4,32,&error)&&
            nonManifold.triangles.size()==nonManifoldTriangleCount,
            "Non-manifold mesh was hole-filled or changed during rejection");

    const std::vector<Triangle> invalidFaces{{0,1,3}};cad::mesh::RepairResult sentinel;
    sentinel.vertices={{7,8,9}};sentinel.triangles={{0,0,0}};
    require(!cad::mesh::repairTriangleMesh(openVertices,invalidFaces,1e-3,&sentinel,&error)&&
            sentinel.vertices.size()==1&&sentinel.vertices[0].x==7&&sentinel.triangles.size()==1,
            "Invalid mesh input changed existing output");
    std::cout<<"PASS: "<<checks<<" mesh repair checks\n";return 0;
}
