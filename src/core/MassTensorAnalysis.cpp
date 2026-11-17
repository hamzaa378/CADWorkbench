#include "MassTensorAnalysis.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace cad::tensor {
namespace {
bool fail(std::string* error,const char* message){if(error)*error=message;return false;}
bool finite(double x){return std::isfinite(x);}
double norm(const std::array<double,3>& a){return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}
std::array<double,3> cross(const std::array<double,3>& a,const std::array<double,3>& b){
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
void canonical(std::array<double,3>& axis){
    int largest=0;
    for(int j=1;j<3;++j)if(std::abs(axis[j])>std::abs(axis[largest]))largest=j;
    if(axis[largest]<0.0)for(double& item:axis)item=-item;
}
} // namespace
bool analyze(const Input& input,Report* output,std::string* error){
    if(!output)return fail(error,"Mass tensor output is null.");
    if(!finite(input.volumeMm3)||input.volumeMm3<=1e-9)
        return fail(error,"Positive, finite solid volume is required.");
    if(!finite(input.densityKgPerM3)||input.densityKgPerM3<=0.0||input.densityKgPerM3>1e6)
        return fail(error,"Density must be finite and in (0, 1,000,000] kg/m^3.");
    for(double c:input.centerMm)if(!finite(c))return fail(error,"Nonfinite center of mass.");
    for(double i:input.geometricInertiaMm5)if(!finite(i))return fail(error,"Nonfinite inertia tensor.");
    const double factor=input.densityKgPerM3*1e-9; // kg per mm^3
    const double mass=input.volumeMm3*factor;
    if(!finite(mass)||mass<=0.0)return fail(error,"Nonfinite body mass.");
    Report report;
    report.volumeMm3=input.volumeMm3;
    report.densityKgPerM3=input.densityKgPerM3;
    report.massKg=mass;
    report.centerMm=input.centerMm;
    for(int i=0;i<9;++i)report.inertiaKgMm2[i]=input.geometricInertiaMm5[i]*factor;
    double maximum=0.0;
    for(double v:report.inertiaKgMm2){
        if(!finite(v))return fail(error,"Scaled inertia exceeds numeric range.");
        maximum=std::max(maximum,std::abs(v));
    }
    if(maximum<=0.0)return fail(error,"The solid inertia tensor is zero.");
    const double symmetricTolerance=1e-9*maximum;
    for(int i=0;i<3;++i)for(int j=i+1;j<3;++j){
        const double a=report.inertiaKgMm2[i*3+j];
        const double b=report.inertiaKgMm2[j*3+i];
        if(std::abs(a-b)>symmetricTolerance)
            return fail(error,"Inertia tensor is not symmetric.");
        const double average=(a+b)*0.5;
        report.inertiaKgMm2[i*3+j]=average;
        report.inertiaKgMm2[j*3+i]=average;
    }
    // Symmetric Jacobi diagonalization. Eigenvectors are stored by columns.
    double a[3][3]{};double vectors[3][3]{};
    for(int i=0;i<3;++i){for(int j=0;j<3;++j)a[i][j]=report.inertiaKgMm2[3*i+j];vectors[i][i]=1.0;}
    bool converged=false;
    for(int step=0;step<70;++step){
        int p=0,q=1;
        for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)
            if(std::abs(a[i][j])>std::abs(a[p][q])){p=i;q=j;}
        if(std::abs(a[p][q])<=std::max(1e-13*maximum,1e-25)){converged=true;break;}
        const double tau=(a[q][q]-a[p][p])/(2.0*a[p][q]);
        const double t=(tau>=0?1.0:-1.0)/(std::abs(tau)+std::sqrt(1.0+tau*tau));
        const double c=1.0/std::sqrt(1+t*t),s=t*c;
        const double app=a[p][p],aqq=a[q][q],apq=a[p][q];
        a[p][p]=app-t*apq;a[q][q]=aqq+t*apq;a[p][q]=a[q][p]=0.0;
        for(int r=0;r<3;++r){
            if(r!=p&&r!=q){const double rp=a[r][p],rq=a[r][q];a[r][p]=a[p][r]=c*rp-s*rq;a[r][q]=a[q][r]=s*rp+c*rq;}
            const double vp=vectors[r][p],vq=vectors[r][q];
            vectors[r][p]=c*vp-s*vq;vectors[r][q]=s*vp+c*vq;
        }
    }
    if(!converged)return fail(error,"Principal inertia solver did not converge.");
    std::array<int,3> indices{0,1,2};
    std::sort(indices.begin(),indices.end(),[&](int i,int j){return a[i][i]<a[j][j];});
    for(int i=0;i<3;++i){
        const int col=indices[i];
        const double value=a[col][col];
        if(value<-1e-9*maximum)return fail(error,"Negative principal inertia is not physically valid.");
        report.principalMomentsKgMm2[i]=std::max(0.0,value);
        report.radiiOfGyrationMm[i]=std::sqrt(report.principalMomentsKgMm2[i]/mass);
        std::array<double,3> axis{vectors[0][col],vectors[1][col],vectors[2][col]};
        const double magnitude=norm(axis);
        if(!(magnitude>0.0)||!finite(magnitude))return fail(error,"Invalid principal direction.");
        for(double& x:axis)x/=magnitude;
        canonical(axis);
        report.principalAxes[i]=axis;
    }
    // Enforce a right-handed orthonormal frame, including the two-axis ambiguity for symmetric shapes.
    auto third=cross(report.principalAxes[0],report.principalAxes[1]);
    const double thirdNorm=norm(third);
    if(thirdNorm<1e-8)return fail(error,"Principal axes are not independent.");
    for(double& x:third)x/=thirdNorm;
    report.principalAxes[2]=third;
    const double gapTolerance=1e-8*maximum;
    report.principalAxesUnique=(report.principalMomentsKgMm2[1]-report.principalMomentsKgMm2[0]>gapTolerance)&&
        (report.principalMomentsKgMm2[2]-report.principalMomentsKgMm2[1]>gapTolerance);
    *output=report;
    if(error)error->clear();
    return true;
}
bool aggregate(const std::vector<Report>& components,Report* output,std::string* error){
    if(!output)return fail(error,"Assembly mass tensor output is null.");
    if(components.empty())return fail(error,"At least one measured component is required.");
    double totalMass=0.0,totalVolume=0.0;
    std::array<double,3> weightedCenter{};
    for(const auto& part:components){
        if(!finite(part.massKg)||part.massKg<=0.0||!finite(part.volumeMm3)||part.volumeMm3<=0.0)
            return fail(error,"Assembly contains a component without valid mass and volume.");
        for(double c:part.centerMm)if(!finite(c))return fail(error,"Assembly center is nonfinite.");
        for(double inertia:part.inertiaKgMm2)if(!finite(inertia))return fail(error,"Assembly inertia is nonfinite.");
        totalMass+=part.massKg;totalVolume+=part.volumeMm3;
        for(int i=0;i<3;++i)weightedCenter[i]+=part.massKg*part.centerMm[i];
    }
    if(!finite(totalMass)||totalMass<=0.0||!finite(totalVolume)||totalVolume<=0.0)
        return fail(error,"Assembly totals are invalid.");
    const std::array<double,3> center{weightedCenter[0]/totalMass,
                                       weightedCenter[1]/totalMass,weightedCenter[2]/totalMass};
    for(double c:center)if(!finite(c))return fail(error,"Assembly center exceeds numeric range.");
    std::array<double,9> combined{};
    for(const auto& part:components){
        const std::array<double,3> offset{
            part.centerMm[0]-center[0],part.centerMm[1]-center[1],part.centerMm[2]-center[2]};
        const double squared=offset[0]*offset[0]+offset[1]*offset[1]+offset[2]*offset[2];
        for(int i=0;i<3;++i)for(int j=0;j<3;++j){
            const double shift=part.massKg*((i==j?squared:0.0)-offset[i]*offset[j]);
            combined[3*i+j]+=part.inertiaKgMm2[3*i+j]+shift;
        }
    }
    const double effectiveDensity=totalMass*1e9/totalVolume;
    const double factor=effectiveDensity*1e-9;
    if(!finite(effectiveDensity)||effectiveDensity<=0.0||effectiveDensity>1e6)
        return fail(error,"Assembly effective density exceeds allowed range.");
    Input collective;
    collective.volumeMm3=totalVolume;
    collective.densityKgPerM3=effectiveDensity; // An effective density, not a new assigned material.
    collective.centerMm=center;
    for(int i=0;i<9;++i)collective.geometricInertiaMm5[i]=combined[i]/factor;
    return analyze(collective,output,error);
}

} // namespace cad::tensor
