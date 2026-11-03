#include "MeshRepair.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace cad::mesh {
namespace {
using Index = std::uint32_t;

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

struct Cell {
    std::int64_t x, y, z;
    bool operator==(const Cell& other) const noexcept { return x==other.x&&y==other.y&&z==other.z; }
};

struct CellHash {
    std::size_t operator()(const Cell& cell) const noexcept {
        auto mix=[](std::uint64_t value) {
            value^=value>>30;value*=0xbf58476d1ce4e5b9ULL;
            value^=value>>27;value*=0x94d049bb133111ebULL;value^=value>>31;
            return value;
        };
        const auto x=mix(static_cast<std::uint64_t>(cell.x));
        const auto y=mix(static_cast<std::uint64_t>(cell.y));
        const auto z=mix(static_cast<std::uint64_t>(cell.z));
        return static_cast<std::size_t>(x^(y+0x9e3779b97f4a7c15ULL+(x<<6)+(x>>2))^
                                        (z+0x517cc1b727220a95ULL+(y<<6)+(y>>2)));
    }
};

struct Edge {
    Index first, second;
    bool operator==(const Edge& other) const noexcept { return first==other.first&&second==other.second; }
};

struct EdgeHash {
    std::size_t operator()(const Edge& edge) const noexcept {
        std::size_t value=std::hash<Index>{}(edge.first);
        value^=std::hash<Index>{}(edge.second)+std::size_t{0x9e3779b9U}+(value<<6)+(value>>2);
        return value;
    }
};

struct FaceKey {
    std::array<Index,3> indices;
    bool operator==(const FaceKey& other) const noexcept { return indices==other.indices; }
};

struct FaceHash {
    std::size_t operator()(const FaceKey& face) const noexcept {
        std::size_t value=static_cast<std::size_t>(face.indices[0]);
        value^=static_cast<std::size_t>(face.indices[1])+0x9e3779b9U+(value<<6)+(value>>2);
        value^=static_cast<std::size_t>(face.indices[2])+0x85ebca6bU+(value<<6)+(value>>2);
        return value;
    }
};

struct EdgeUse {
    Index triangle;
    bool forward;
};

struct Neighbor {
    Index triangle;
    bool mustFlipRelative;
};

struct DirectedEdge {
    Index from;
    Index to;
};

struct Point2 {
    double x;
    double y;
};

Cell cellFor(const Point3& point, double tolerance) {
    return {static_cast<std::int64_t>(std::floor(point.x/tolerance)),
            static_cast<std::int64_t>(std::floor(point.y/tolerance)),
            static_cast<std::int64_t>(std::floor(point.z/tolerance))};
}

double distanceSquared(const Point3& a,const Point3& b) {
    const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
    return x*x+y*y+z*z;
}

bool degenerate(const Point3& a,const Point3& b,const Point3& c) {
    const double abx=b.x-a.x,aby=b.y-a.y,abz=b.z-a.z;
    const double acx=c.x-a.x,acy=c.y-a.y,acz=c.z-a.z;
    const double bcx=c.x-b.x,bcy=c.y-b.y,bcz=c.z-b.z;
    const double cx=aby*acz-abz*acy,cy=abz*acx-abx*acz,cz=abx*acy-aby*acx;
    const double crossSquared=cx*cx+cy*cy+cz*cz;
    const double edgeScaleSquared=std::max({abx*abx+aby*aby+abz*abz,
                                            acx*acx+acy*acy+acz*acz,
                                            bcx*bcx+bcy*bcy+bcz*bcz});
    return !std::isfinite(crossSquared)||edgeScaleSquared==0.0||
           crossSquared<=std::max(1e-30,edgeScaleSquared*edgeScaleSquared*1e-28);
}

double dot(const Point3& a,const Point3& b) {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}

Point3 subtract(const Point3& a,const Point3& b) {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}

Point3 cross(const Point3& a,const Point3& b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}

double length(const Point3& value) {
    return std::sqrt(dot(value,value));
}

double cross2(const Point2& a,const Point2& b,const Point2& c) {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}

bool onSegment(const Point2& a,const Point2& b,const Point2& point,double epsilon) {
    return point.x>=std::min(a.x,b.x)-epsilon&&point.x<=std::max(a.x,b.x)+epsilon&&
           point.y>=std::min(a.y,b.y)-epsilon&&point.y<=std::max(a.y,b.y)+epsilon;
}

bool segmentsIntersect(const Point2& a,const Point2& b,const Point2& c,const Point2& d,double epsilon) {
    const double abC=cross2(a,b,c),abD=cross2(a,b,d),cdA=cross2(c,d,a),cdB=cross2(c,d,b);
    if(((abC>epsilon&&abD< -epsilon)||(abC< -epsilon&&abD>epsilon))&&
       ((cdA>epsilon&&cdB< -epsilon)||(cdA< -epsilon&&cdB>epsilon)))return true;
    return (std::abs(abC)<=epsilon&&onSegment(a,b,c,epsilon))||
           (std::abs(abD)<=epsilon&&onSegment(a,b,d,epsilon))||
           (std::abs(cdA)<=epsilon&&onSegment(c,d,a,epsilon))||
           (std::abs(cdB)<=epsilon&&onSegment(c,d,b,epsilon));
}

bool projectPlanarLoop(const std::vector<Point3>& vertices,const std::vector<Index>& loop,
                       double planarityTolerance,std::vector<Point2>* projected,std::string* error) {
    Point3 normal{};Point3 centroid{};
    for(std::size_t i=0;i<loop.size();++i){
        const Point3& current=vertices[loop[i]];const Point3& next=vertices[loop[(i+1)%loop.size()]];
        normal.x+=(current.y-next.y)*(current.z+next.z);
        normal.y+=(current.z-next.z)*(current.x+next.x);
        normal.z+=(current.x-next.x)*(current.y+next.y);
        centroid.x+=current.x;centroid.y+=current.y;centroid.z+=current.z;
    }
    centroid.x/=static_cast<double>(loop.size());centroid.y/=static_cast<double>(loop.size());centroid.z/=static_cast<double>(loop.size());
    const double normalLength=length(normal);
    if(!std::isfinite(normalLength)||normalLength<=1e-12){if(error)*error="Boundary loop has no stable planar area.";return false;}
    normal.x/=normalLength;normal.y/=normalLength;normal.z/=normalLength;
    for(Index index:loop){
        if(std::abs(dot(subtract(vertices[index],centroid),normal))>planarityTolerance){
            if(error)*error="Boundary loop is not planar within the requested tolerance.";return false;
        }
    }
    const Point3 reference=std::abs(normal.x)<=std::abs(normal.y)&&std::abs(normal.x)<=std::abs(normal.z)
        ? Point3{1,0,0} : (std::abs(normal.y)<=std::abs(normal.z)?Point3{0,1,0}:Point3{0,0,1});
    Point3 u=cross(reference,normal);const double uLength=length(u);
    if(uLength<=1e-12){if(error)*error="Boundary loop plane has an unstable projection axis.";return false;}
    u.x/=uLength;u.y/=uLength;u.z/=uLength;
    const Point3 v=cross(normal,u);
    projected->clear();projected->reserve(loop.size());
    for(Index index:loop){const Point3 relative=subtract(vertices[index],centroid);projected->push_back({dot(relative,u),dot(relative,v)});}

    double twiceArea=0.0;double extent=0.0;
    for(std::size_t i=0;i<projected->size();++i){
        const Point2& a=(*projected)[i];const Point2& b=(*projected)[(i+1)%projected->size()];
        twiceArea+=a.x*b.y-b.x*a.y;
        extent=std::max(extent,std::max(std::abs(a.x),std::abs(a.y)));
    }
    const double epsilon=std::max(1e-14,extent*extent*1e-12);
    if(!std::isfinite(twiceArea)||std::abs(twiceArea)<=epsilon){if(error)*error="Boundary loop projection has negligible area.";return false;}
    const double orientation=twiceArea>0.0?1.0:-1.0;
    for(std::size_t i=0;i<projected->size();++i){
        const double turn=cross2((*projected)[i],(*projected)[(i+1)%projected->size()],(*projected)[(i+2)%projected->size()]);
        if(orientation*turn<=epsilon){if(error)*error="Only strictly convex planar boundary loops can be filled automatically.";return false;}
    }
    for(std::size_t i=0;i<projected->size();++i){
        const std::size_t iNext=(i+1)%projected->size();
        for(std::size_t j=i+1;j<projected->size();++j){
            const std::size_t jNext=(j+1)%projected->size();
            if(i==j||iNext==j||jNext==i)continue;
            if(segmentsIntersect((*projected)[i],(*projected)[iNext],(*projected)[j],(*projected)[jNext],epsilon)){
                if(error)*error="Boundary loop is self-intersecting and cannot be filled safely.";return false;
            }
        }
    }
    return true;
}

} // namespace

bool repairTriangleMesh(const std::vector<Point3>& vertices,
                        const std::vector<Triangle>& triangles,
                        double weldTolerance,
                        RepairResult* output,
                        std::string* error) {
    if (!output) return fail(error,"Provide an output object for mesh repair.");
    if (vertices.size()<3||vertices.size()>5000000||triangles.empty()||triangles.size()>5000000)
        return fail(error,"Mesh repair accepts 3–5,000,000 vertices and 1–5,000,000 triangles.");
    if (!std::isfinite(weldTolerance)||weldTolerance<1e-6||weldTolerance>1.0)
        return fail(error,"Mesh weld tolerance must be finite and between 0.000001 and 1 mm.");
    for (const Point3& point:vertices) {
        if (!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||
            std::abs(point.x)>1e6||std::abs(point.y)>1e6||std::abs(point.z)>1e6)
            return fail(error,"Mesh coordinates must be finite and within ±1,000,000 mm.");
    }
    for (const Triangle& triangle:triangles) {
        if (triangle.a>=vertices.size()||triangle.b>=vertices.size()||triangle.c>=vertices.size())
            return fail(error,"Mesh triangle contains an out-of-range vertex index.");
    }

    RepairResult candidate;
    candidate.stats.inputVertices=vertices.size();candidate.stats.inputTriangles=triangles.size();
    candidate.vertices.reserve(vertices.size());
    std::vector<Index> remap(vertices.size());
    std::unordered_map<Cell,std::vector<Index>,CellHash> buckets;
    buckets.reserve(vertices.size());
    const double toleranceSquared=weldTolerance*weldTolerance;
    for (std::size_t i=0;i<vertices.size();++i) {
        const Point3& point=vertices[i];const Cell base=cellFor(point,weldTolerance);
        Index best=std::numeric_limits<Index>::max();double bestDistance=toleranceSquared;
        for (int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)for(int dz=-1;dz<=1;++dz) {
            const Cell key{base.x+dx,base.y+dy,base.z+dz};
            const auto found=buckets.find(key);if(found==buckets.end())continue;
            for (Index candidateIndex:found->second) {
                const double distance=distanceSquared(point,candidate.vertices[candidateIndex]);
                if(distance<=bestDistance){best=candidateIndex;bestDistance=distance;}
            }
        }
        if(best==std::numeric_limits<Index>::max()) {
            if(candidate.vertices.size()>=std::numeric_limits<Index>::max())
                return fail(error,"Mesh exceeds the supported vertex index range.");
            best=static_cast<Index>(candidate.vertices.size());candidate.vertices.push_back(point);
            buckets[base].push_back(best);
        } else {
            ++candidate.stats.weldedVertices;
        }
        remap[i]=best;
    }

    candidate.triangles.reserve(triangles.size());
    std::unordered_set<FaceKey,FaceHash> faces;faces.reserve(triangles.size());
    for(const Triangle& source:triangles) {
        Triangle triangle{remap[source.a],remap[source.b],remap[source.c]};
        if(triangle.a==triangle.b||triangle.b==triangle.c||triangle.c==triangle.a||
           degenerate(candidate.vertices[triangle.a],candidate.vertices[triangle.b],candidate.vertices[triangle.c])) {
            ++candidate.stats.removedDegenerateTriangles;continue;
        }
        FaceKey face{{triangle.a,triangle.b,triangle.c}};std::sort(face.indices.begin(),face.indices.end());
        if(!faces.insert(face).second){++candidate.stats.removedDuplicateTriangles;continue;}
        candidate.triangles.push_back(triangle);
    }
    if(candidate.triangles.empty())return fail(error,"Mesh repair removed every triangle as degenerate or duplicate.");

    std::unordered_map<Edge,std::vector<EdgeUse>,EdgeHash> edges;
    edges.reserve(candidate.triangles.size()*2);
    auto addEdge=[&](Index triangle,Index from,Index to){
        const Edge key{std::min(from,to),std::max(from,to)};
        edges[key].push_back({triangle,from<to});
    };
    for(Index i=0;i<candidate.triangles.size();++i){
        const Triangle& triangle=candidate.triangles[i];
        addEdge(i,triangle.a,triangle.b);addEdge(i,triangle.b,triangle.c);addEdge(i,triangle.c,triangle.a);
    }

    std::vector<std::vector<Neighbor>> adjacency(candidate.triangles.size());
    for(const auto& edgeEntry:edges){
        const auto& uses=edgeEntry.second;
        if(uses.size()==1){++candidate.stats.boundaryEdges;continue;}
        if(uses.size()!=2){++candidate.stats.nonManifoldEdges;continue;}
        const bool relativeFlip=uses[0].forward==uses[1].forward;
        adjacency[uses[0].triangle].push_back({uses[1].triangle,relativeFlip});
        adjacency[uses[1].triangle].push_back({uses[0].triangle,relativeFlip});
    }

    std::vector<std::int8_t> flip(candidate.triangles.size(),-1);
    for(Index start=0;start<candidate.triangles.size();++start){
        if(flip[start]>=0)continue;
        ++candidate.stats.connectedComponents;flip[start]=0;std::queue<Index> pending;pending.push(start);
        while(!pending.empty()){
            const Index current=pending.front();pending.pop();
            for(const Neighbor& neighbor:adjacency[current]){
                const std::int8_t required=static_cast<std::int8_t>(flip[current]^(neighbor.mustFlipRelative?1:0));
                if(flip[neighbor.triangle]<0){flip[neighbor.triangle]=required;pending.push(neighbor.triangle);}
                else if(flip[neighbor.triangle]!=required)
                    return fail(error,"Mesh has a non-orientable or contradictory manifold surface component.");
            }
        }
    }
    for(std::size_t i=0;i<candidate.triangles.size();++i){
        if(flip[i]){std::swap(candidate.triangles[i].b,candidate.triangles[i].c);++candidate.stats.flippedTriangles;}
    }

    // Compact vertices after welding and face cleanup so downstream CAD kernels
    // receive only referenced points and stable, dense indices.
    std::vector<Index> compact(candidate.vertices.size(),std::numeric_limits<Index>::max());
    std::vector<Point3> used;used.reserve(candidate.vertices.size());
    auto compactIndex=[&](Index old){
        if(compact[old]==std::numeric_limits<Index>::max()){
            compact[old]=static_cast<Index>(used.size());used.push_back(candidate.vertices[old]);
        }
        return compact[old];
    };
    for(Triangle& triangle:candidate.triangles){
        triangle.a=compactIndex(triangle.a);triangle.b=compactIndex(triangle.b);triangle.c=compactIndex(triangle.c);
    }
    candidate.vertices=std::move(used);
    candidate.stats.outputVertices=candidate.vertices.size();
    candidate.stats.outputTriangles=candidate.triangles.size();
    *output=std::move(candidate);
    if(error)error->clear();
    return true;
}

bool fillPlanarBoundaryHoles(RepairResult* mesh,double planarityTolerance,
                             std::size_t maxLoopEdges,std::string* error) {
    if(!mesh)return fail(error,"Provide a mesh to fill planar boundary loops.");
    if(!std::isfinite(planarityTolerance)||planarityTolerance<=0.0||planarityTolerance>1.0)
        return fail(error,"Planarity tolerance must be finite and between 0 and 1 coordinate unit.");
    if(maxLoopEdges<3||maxLoopEdges>1000000)
        return fail(error,"Hole-fill boundary limit must be between 3 and 1,000,000 edges.");
    if(mesh->vertices.size()<3||mesh->vertices.size()>5000000||mesh->triangles.empty()||mesh->triangles.size()>5000000)
        return fail(error,"Hole filling accepts 3–5,000,000 vertices and 1–5,000,000 triangles.");

    RepairResult candidate=*mesh;
    std::unordered_map<Edge,std::vector<DirectedEdge>,EdgeHash> edgeUses;
    edgeUses.reserve(candidate.triangles.size()*2);
    const auto addEdge=[&](Index from,Index to){edgeUses[{std::min(from,to),std::max(from,to)}].push_back({from,to});};
    for(const Triangle& triangle:candidate.triangles){
        if(triangle.a>=candidate.vertices.size()||triangle.b>=candidate.vertices.size()||triangle.c>=candidate.vertices.size())
            return fail(error,"Mesh triangle contains an out-of-range vertex index.");
        if(triangle.a==triangle.b||triangle.b==triangle.c||triangle.c==triangle.a||
           degenerate(candidate.vertices[triangle.a],candidate.vertices[triangle.b],candidate.vertices[triangle.c]))
            return fail(error,"Mesh contains a degenerate triangle and cannot be hole-filled safely.");
        addEdge(triangle.a,triangle.b);addEdge(triangle.b,triangle.c);addEdge(triangle.c,triangle.a);
    }
    std::vector<DirectedEdge> boundary;
    boundary.reserve(candidate.stats.boundaryEdges);
    for(const auto& entry:edgeUses){
        if(entry.second.size()>2)return fail(error,"Mesh contains a non-manifold edge; repair it before filling holes.");
        if(entry.second.size()==1)boundary.push_back(entry.second.front());
    }
    if(boundary.empty()){
        candidate.stats.boundaryEdges=0;
        candidate.stats.nonManifoldEdges=0;
        candidate.stats.outputVertices=candidate.vertices.size();
        candidate.stats.outputTriangles=candidate.triangles.size();
        *mesh=std::move(candidate);
        if(error)error->clear();
        return true;
    }

    std::unordered_map<Index,std::size_t> outgoing,incoming;
    outgoing.reserve(boundary.size());incoming.reserve(boundary.size());
    for(std::size_t edgeIndex=0;edgeIndex<boundary.size();++edgeIndex){
        const DirectedEdge& edge=boundary[edgeIndex];
        if(!outgoing.emplace(edge.from,edgeIndex).second||!incoming.emplace(edge.to,edgeIndex).second)
            return fail(error,"Boundary edges branch or have inconsistent winding; no hole was filled.");
    }
    if(outgoing.size()!=incoming.size())return fail(error,"Boundary edges do not form closed loops; no hole was filled.");
    for(const auto& vertex:outgoing){if(!incoming.contains(vertex.first))return fail(error,"Boundary edges do not form closed loops; no hole was filled.");}

    std::vector<bool> visited(boundary.size(),false);
    std::vector<std::vector<Index>> loops;
    for(std::size_t start=0;start<boundary.size();++start){
        if(visited[start])continue;
        std::vector<Index> loop;
        std::size_t current=start;
        for(std::size_t step=0;step<=boundary.size();++step){
            if(visited[current]){
                if(current==start)break;
                return fail(error,"Boundary edges overlap before a loop closes; no hole was filled.");
            }
            visited[current]=true;
            const DirectedEdge& edge=boundary[current];
            loop.push_back(edge.from);
            const auto next=outgoing.find(edge.to);
            if(next==outgoing.end())return fail(error,"Boundary edges do not form closed loops; no hole was filled.");
            current=next->second;
            if(current==start)break;
            if(step==boundary.size())return fail(error,"Boundary loop traversal exceeded its safe limit.");
        }
        if(loop.size()<3)return fail(error,"Boundary loop has fewer than three vertices; no hole was filled.");
        if(loop.size()>maxLoopEdges)return fail(error,"Boundary loop exceeds the requested hole-fill edge limit.");
        std::vector<Point2> projected;
        std::string loopError;
        if(!projectPlanarLoop(candidate.vertices,loop,planarityTolerance,&projected,&loopError)){
            if(error)*error=loopError;return false;
        }
        for(std::size_t i=1;i+1<loop.size();++i){
            const Triangle cap{loop[0],loop[i+1],loop[i]};
            if(degenerate(candidate.vertices[cap.a],candidate.vertices[cap.b],candidate.vertices[cap.c]))
                return fail(error,"Planar hole triangulation produced a degenerate facet; no hole was filled.");
            candidate.triangles.push_back(cap);
        }
        ++candidate.stats.filledHoles;
        candidate.stats.filledHoleTriangles+=loop.size()-2;
    }

    std::unordered_map<Edge,std::size_t,EdgeHash> finalEdgeUses;
    finalEdgeUses.reserve(candidate.triangles.size()*2);
    const auto countEdge=[&](Index a,Index b){++finalEdgeUses[{std::min(a,b),std::max(a,b)}];};
    for(const Triangle& triangle:candidate.triangles){countEdge(triangle.a,triangle.b);countEdge(triangle.b,triangle.c);countEdge(triangle.c,triangle.a);}
    candidate.stats.boundaryEdges=0;candidate.stats.nonManifoldEdges=0;
    for(const auto& entry:finalEdgeUses){
        if(entry.second==1)++candidate.stats.boundaryEdges;
        else if(entry.second>2)++candidate.stats.nonManifoldEdges;
    }
    if(candidate.stats.boundaryEdges!=0||candidate.stats.nonManifoldEdges!=0)
        return fail(error,"Hole-fill result did not form a closed two-manifold surface; mesh was left unchanged.");
    candidate.stats.outputVertices=candidate.vertices.size();
    candidate.stats.outputTriangles=candidate.triangles.size();
    *mesh=std::move(candidate);
    if(error)error->clear();
    return true;
}

} // namespace cad::mesh
