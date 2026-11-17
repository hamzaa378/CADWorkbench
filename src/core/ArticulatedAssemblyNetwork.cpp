#include "ArticulatedAssemblyNetwork.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace cad::articulated {
namespace {
using mates::Vec3;using mates::Transform;
void fail(std::string* e,const std::string& message){if(e)*e=message;}
bool finite(double x){return std::isfinite(x)&&std::abs(x)<1e6;}
double norm(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
Vec3 sum(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 scaled(Vec3 a,double k){return {a.x*k,a.y*k,a.z*k};}
Transform movement(const Joint& j,double q){
    Transform delta;
    if(j.kind==JointKind::Prismatic)delta.translation=scaled(j.axis,q);
    if(j.kind==JointKind::Revolute){
        const auto [x,y,z]=j.axis;
        const double c=std::cos(q),s=std::sin(q),v=1-c;
        delta.rotation={{c+x*x*v,x*y*v-z*s,x*z*v+y*s,
                         y*x*v+z*s,c+y*y*v,y*z*v-x*s,
                         z*x*v-y*s,z*y*v+x*s,c+z*z*v}};
    }
    return mates::multiply(delta,j.childAtZero);
}
struct Layout {
    std::vector<std::vector<std::pair<std::size_t,bool>>> adjacency;
    std::vector<std::size_t> order,freeIndices;
    std::vector<bool> tree;
};
bool buildLayout(std::size_t n,std::size_t fixed,const std::vector<Joint>& edges,Layout& l){
    l.adjacency.assign(n,{});l.tree.assign(edges.size(),false);
    for(std::size_t i=0;i<edges.size();++i){
        l.adjacency[edges[i].parent].push_back({i,true});
        l.adjacency[edges[i].child].push_back({i,false});
        if(!edges[i].driven)l.freeIndices.push_back(i);
    }
    std::vector<bool> used(n,false);used[fixed]=true;
    std::queue<std::size_t> next;next.push(fixed);
    while(!next.empty()){
        auto node=next.front();next.pop();
        for(const auto& [i,forward]:l.adjacency[node]){
            const auto target=forward?edges[i].child:edges[i].parent;
            if(used[target])continue;
            used[target]=true;l.tree[i]=true;l.order.push_back(i);
            next.push(target);
        }
    }
    return std::all_of(used.begin(),used.end(),[](bool v){return v;});
}
// Traverse the chosen forest using current joint coordinates. The fixed node
// is the only unmovable gauge, so there is no unobservable global rigid motion.
bool evaluate(std::size_t n,std::size_t fixed,const std::vector<Joint>& edges,
              const Layout& l,const std::vector<double>& qs,
              std::vector<Transform>& poses,std::vector<double>& residuals){
    poses.assign(n,{});
    std::vector<bool> placed(n,false);placed[fixed]=true;
    std::queue<std::size_t> pending;pending.push(fixed);
    while(!pending.empty()){
        auto from=pending.front();pending.pop();
        for(const auto& [index,forward]:l.adjacency[from]){
            if(!l.tree[index])continue;
            const auto to=forward?edges[index].child:edges[index].parent;
            if(placed[to])continue;
            const auto rel=movement(edges[index],qs[index]);
            poses[to]=mates::multiply(poses[from],forward?rel:mates::inverseRigid(rel));
            for(double v:poses[to].rotation)if(!finite(v))return false;
            auto p=poses[to].translation;
            if(!finite(p.x)||!finite(p.y)||!finite(p.z))return false;
            placed[to]=true;pending.push(to);
        }
    }
    residuals.clear();
    for(std::size_t i=0;i<edges.size();++i){
        if(l.tree[i])continue;
        const auto predicted=mates::multiply(poses[edges[i].parent],movement(edges[i],qs[i]));
        const auto& actual=poses[edges[i].child];
        // Compare all 9 rotation entries. This remains well conditioned near
        // 180 degrees, unlike a singular Euler-angle closure residual.
        const Vec3 dp=sum(predicted.translation,scaled(actual.translation,-1));
        residuals.push_back(dp.x);residuals.push_back(dp.y);residuals.push_back(dp.z);
        for(int k=0;k<9;++k)residuals.push_back(10.0*(predicted.rotation[k]-actual.rotation[k]));
    }
    return true;
}
double squared(const std::vector<double>& residual){
    double total=0;for(double value:residual){if(!finite(value))return std::numeric_limits<double>::infinity();total+=value*value;}
    return total;
}
// Rank-revealing pivoted Gaussian elimination; the positive damping for an LM
// step is applied only after the undamped normal matrix has been rank-tested.
bool eliminate(std::vector<std::vector<double>> matrix,std::vector<double> rhs,
               std::vector<double>& answer,double tolerance){
    const std::size_t n=rhs.size();answer.assign(n,0.0);
    double scale=0;for(const auto& row:matrix)for(double x:row)scale=std::max(scale,std::abs(x));
    if(scale==0||!std::isfinite(scale))return false;
    for(std::size_t i=0;i<n;++i){
        std::size_t pivot=i;
        for(std::size_t j=i+1;j<n;++j)if(std::abs(matrix[j][i])>std::abs(matrix[pivot][i]))pivot=j;
        if(std::abs(matrix[pivot][i])<scale*tolerance)return false;
        std::swap(matrix[i],matrix[pivot]);std::swap(rhs[i],rhs[pivot]);
        const double divisor=matrix[i][i];
        for(std::size_t j=i;j<n;++j)matrix[i][j]/=divisor;
        rhs[i]/=divisor;
        for(std::size_t j=0;j<n;++j)if(j!=i){
            const double factor=matrix[j][i];
            for(std::size_t k=i;k<n;++k)matrix[j][k]-=factor*matrix[i][k];
            rhs[j]-=factor*rhs[i];
        }
    }
    answer=std::move(rhs);return true;
}
void normalMatrix(const std::vector<double>& residual,
                  const std::vector<std::vector<double>>& columns,
                  std::vector<std::vector<double>>& h,std::vector<double>& rhs){
    const auto n=columns.size();h.assign(n,std::vector<double>(n,0));rhs.assign(n,0);
    for(std::size_t a=0;a<n;++a){
        for(std::size_t k=0;k<residual.size();++k)rhs[a]-=columns[a][k]*residual[k];
        for(std::size_t b=0;b<n;++b)
            for(std::size_t k=0;k<residual.size();++k)h[a][b]+=columns[a][k]*columns[b][k];
    }
}
} // namespace
bool solveNetwork(std::size_t count,std::size_t fixed,const std::vector<Joint>& joints,
                  Solution* output,std::string* error){
    if(!output||count<2||count>24||fixed>=count||joints.size()<count-1||joints.size()>64){
        fail(error,"Articulated network requires 2-24 nodes, a fixed node, and 1-64 joints.");return false;
    }
    std::size_t freeCount=0;
    for(const auto& joint:joints){
        if(joint.parent>=count||joint.child>=count||joint.parent==joint.child||
           (joint.kind!=JointKind::Fixed&&joint.kind!=JointKind::Revolute&&joint.kind!=JointKind::Prismatic)||
           (joint.kind!=JointKind::Fixed&&(!finite(joint.axis.x)||!finite(joint.axis.y)||
             !finite(joint.axis.z)||std::abs(norm(joint.axis)-1)>1e-6))||
           !finite(joint.initial)||!finite(joint.minimum)||!finite(joint.maximum)||
           joint.minimum>joint.maximum||joint.initial<joint.minimum-1e-12||
           joint.initial>joint.maximum+1e-12||
           (joint.kind==JointKind::Fixed&&!joint.driven)){
            fail(error,"Malformed joint index, axis, driving flag, or coordinate limits.");return false;
        }
        const auto& t=joint.childAtZero;
        if(!finite(t.translation.x)||!finite(t.translation.y)||!finite(t.translation.z)){
            fail(error,"Nonfinite joint reference-frame translation.");return false;
        }
        for(double v:t.rotation)if(!finite(v)){
            fail(error,"Nonfinite joint reference-frame rotation.");return false;
        }
        // Check orthonormality; malformed transforms must not enter the graph.
        const auto& r=t.rotation;
        const double det=r[0]*(r[4]*r[8]-r[5]*r[7])-
                         r[1]*(r[3]*r[8]-r[5]*r[6])+
                         r[2]*(r[3]*r[7]-r[4]*r[6]);
        if(!finite(det)||std::abs(det-1.0)>1e-5){
            fail(error,"Joint frame must be a proper right-handed rigid rotation, not a reflection.");return false;
        }
        const auto inv=mates::inverseRigid(t);
        const auto product=mates::multiply(t,inv);
        for(int i=0;i<9;++i)if(std::abs(product.rotation[i]-(i%4==0?1.0:0.0))>1e-5){
            fail(error,"Joint reference frame is not an orthogonal rotation.");return false;
        }
        if(!joint.driven)++freeCount;
    }
    if(freeCount>12){fail(error,"At most twelve undriven joint coordinates are supported.");return false;}
    Layout layout;
    if(!buildLayout(count,fixed,joints,layout)){
        fail(error,"Articulated mate graph is disconnected from the fixed component.");return false;
    }
    // Theoretical maximum closure rank is six per independent cycle. A tree
    // cannot determine a free joint coordinate, even if numerical iterations
    // happen to leave that coordinate at its initial value.
    if(freeCount>6*(joints.size()-(count-1))){
        fail(error,"Assembly is underconstrained: not enough independent loop equations.");return false;
    }
    std::vector<double> coords;coords.reserve(joints.size());
    for(const auto& joint:joints)coords.push_back(joint.initial);
    std::vector<Transform> poses;std::vector<double> residual;
    if(!evaluate(count,fixed,joints,layout,coords,poses,residual)){
        fail(error,"Initial joint placements exceeded numerical limits.");return false;
    }
    double current=squared(residual),lambda=1e-5;
    int iterations=0;
    std::vector<std::vector<double>> columns;
    auto computeColumns=[&]()->bool{
        columns.assign(freeCount,std::vector<double>(residual.size(),0));
        for(std::size_t j=0;j<freeCount;++j){
            const auto index=layout.freeIndices[j];
            const double h=joints[index].kind==JointKind::Revolute?1e-6:1e-4;
            const double upper=std::min(joints[index].maximum,coords[index]+h);
            const double lower=std::max(joints[index].minimum,coords[index]-h);
            if(upper-lower<1e-10)return false;
            auto a=coords,b=coords;a[index]=upper;b[index]=lower;
            std::vector<Transform> ignored;std::vector<double> r1,r2;
            if(!evaluate(count,fixed,joints,layout,a,ignored,r1)||
               !evaluate(count,fixed,joints,layout,b,ignored,r2))return false;
            for(std::size_t k=0;k<r1.size();++k)columns[j][k]=(r1[k]-r2[k])/(upper-lower);
        }
        return true;
    };
    for(;iterations<120&&current>1e-12&&freeCount>0;++iterations){
        if(!computeColumns())break;
        std::vector<std::vector<double>> h;
        std::vector<double> rhs;normalMatrix(residual,columns,h,rhs);
        bool improved=false;
        for(int attempt=0;attempt<16;++attempt){
            auto augmented=h;for(std::size_t i=0;i<freeCount;++i)augmented[i][i]+=lambda;
            std::vector<double> step;
            if(!eliminate(augmented,rhs,step,1e-14)){lambda*=10;continue;}
            auto trial=coords;
            for(std::size_t i=0;i<freeCount;++i){
                const auto index=layout.freeIndices[i];
                trial[index]=std::clamp(trial[index]+step[i],joints[index].minimum,joints[index].maximum);
            }
            std::vector<Transform> updated;std::vector<double> r;
            if(!evaluate(count,fixed,joints,layout,trial,updated,r)){lambda*=10;continue;}
            const double candidate=squared(r);
            if(candidate+1e-18<current){
                coords=std::move(trial);poses=std::move(updated);residual=std::move(r);
                current=candidate;lambda=std::max(1e-12,lambda*0.4);improved=true;break;
            }
            lambda*=10;
        }
        if(!improved)break;
    }
    if(!std::isfinite(current)||current>1e-8){
        fail(error,"Articulated assembly loop did not converge to its positional closure tolerance.");return false;
    }
    // A zero residual is not a valid solve when a free coordinate is
    // unobservable. Rank-check the undamped closure Jacobian at the result.
    if(freeCount){
        if(!computeColumns()){
            fail(error,"Articulated joint is pinned at a limit or has an undefined local derivative.");return false;
        }
        std::vector<std::vector<double>> h;std::vector<double> rhs,zero(freeCount,0);
        normalMatrix(residual,columns,h,rhs);
        if(!eliminate(h,zero,rhs,1e-8)){
            fail(error,"Articulated loop remains underconstrained or singular at this configuration.");return false;
        }
    }
    Solution next;
    next.poses=std::move(poses);next.coordinates=std::move(coords);
    next.iterations=iterations;
    for(double value:residual)next.maximumClosureErrorMm=std::max(next.maximumClosureErrorMm,std::abs(value));
    *output=std::move(next);
    return true;
}
} // namespace cad::articulated
