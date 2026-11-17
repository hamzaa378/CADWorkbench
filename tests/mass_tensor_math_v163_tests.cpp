#include "core/MassTensorAnalysis.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
int checks=0;
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
void near(double got,double expected,const char* message,double relative=1e-10){
    require(std::abs(got-expected)<=relative*std::max({1.0,std::abs(got),std::abs(expected)}),message);
}
cad::tensor::Input box(double a,double b,double c,double density){
    const double volume=a*b*c;
    cad::tensor::Input input;
    input.volumeMm3=volume;
    input.densityKgPerM3=density;
    input.centerMm={a/2.0,b/2.0,c/2.0};
    input.geometricInertiaMm5={
        volume*(b*b+c*c)/12.0,0,0,
        0,volume*(a*a+c*c)/12.0,0,
        0,0,volume*(a*a+b*b)/12.0
    };
    return input;
}
void testRectangularSolids(){
    std::string error;
    for(double density:{1000.0,2700.0,7850.0,12000.0}){
        for(double scale:{0.5,1.0,2.0,5.0}){
            const auto input=box(10*scale,20*scale,30*scale,density);
            cad::tensor::Report report;
            require(cad::tensor::analyze(input,&report,&error),"Valid box was rejected");
            const double mass=input.volumeMm3*density*1e-9;
            near(report.massKg,mass,"Mass scaling incorrect");
            near(report.volumeMm3,input.volumeMm3,"Volume changed");
            for(int i=0;i<3;++i)near(report.centerMm[i],input.centerMm[i],"Center changed");
            const double ixx=mass*((20*scale)*(20*scale)+(30*scale)*(30*scale))/12.0;
            const double iyy=mass*((10*scale)*(10*scale)+(30*scale)*(30*scale))/12.0;
            const double izz=mass*((10*scale)*(10*scale)+(20*scale)*(20*scale))/12.0;
            near(report.principalMomentsKgMm2[0],izz,"Least inertia incorrect");
            near(report.principalMomentsKgMm2[1],iyy,"Middle inertia incorrect");
            near(report.principalMomentsKgMm2[2],ixx,"Largest inertia incorrect");
            for(int i=0;i<3;++i)near(report.radiiOfGyrationMm[i]*report.radiiOfGyrationMm[i],
                                       report.principalMomentsKgMm2[i]/mass,"Radius of gyration incorrect");
            require(report.principalAxesUnique,"Unequal box dimensions should have unique principal axes");
            for(int i=0;i<3;++i){
                double length=0;
                for(double value:report.principalAxes[i])length+=value*value;
                near(length,1.0,"Principal axis not normalized");
            }
        }
    }
}
void testRotationAndTranslation(){
    auto input=box(10,20,30,7850);
    cad::tensor::Report base,rotated;
    require(cad::tensor::analyze(input,&base),"Original box inertia invalid");
    // Rotate the centroidal tensor by 45 degrees about the Z axis.
    const double theta=0.78539816339744830962;
    const double c=std::cos(theta),s=std::sin(theta);
    const double x=input.geometricInertiaMm5[0],y=input.geometricInertiaMm5[4];
    input.geometricInertiaMm5[0]=c*c*x+s*s*y;
    input.geometricInertiaMm5[1]=input.geometricInertiaMm5[3]=c*s*(x-y);
    input.geometricInertiaMm5[4]=s*s*x+c*c*y;
    input.centerMm={200,-300,75};
    require(cad::tensor::analyze(input,&rotated),"Rotated tensor rejected");
    for(int i=0;i<3;++i)near(rotated.principalMomentsKgMm2[i],base.principalMomentsKgMm2[i],
                             "Principal moment changed under rigid rotation");
    for(int i=0;i<3;++i)near(rotated.centerMm[i],input.centerMm[i],"Translation changed center");
    near(rotated.inertiaKgMm2[1],rotated.inertiaKgMm2[3],"Off diagonal tensor lost symmetry");
    near(rotated.principalAxes[0][0],0.0,"Least axis should point along Z");
    near(std::abs(rotated.principalAxes[0][2]),1.0,"Least axis is not Z");
    for(int axis=0;axis<3;++axis){
        for(int other=axis+1;other<3;++other){
            double dot=0;
            for(int j=0;j<3;++j)dot+=rotated.principalAxes[axis][j]*rotated.principalAxes[other][j];
            near(dot,0.0,"Rotated principal axes are not orthogonal");
        }
    }
}
void testPhysicalAssembly(){
    auto a=box(10,10,10,1000);
    a.centerMm={-5,5,5};
    auto b=box(10,10,10,1000);
    b.centerMm={15,5,5};
    cad::tensor::Report first,second,combined;
    require(cad::tensor::analyze(a,&first),"Assembly first component invalid");
    require(cad::tensor::analyze(b,&second),"Assembly second component invalid");
    require(cad::tensor::aggregate({first,second},&combined),"Two-component assembly failed");
    const double mass=first.massKg;
    near(combined.massKg,2*mass,"Assembly total mass incorrect");
    near(combined.volumeMm3,2000,"Assembly summed volume incorrect");
    near(combined.centerMm[0],5.0,"Assembly COM X incorrect");
    near(combined.centerMm[1],5.0,"Assembly COM Y incorrect");
    near(combined.principalMomentsKgMm2[0],2*mass*100/6,"Assembly Ixx wrong");
    near(combined.principalMomentsKgMm2[1],2*(mass*100/6+mass*100),"Assembly Iyy wrong");
    near(combined.principalMomentsKgMm2[2],2*(mass*100/6+mass*100),"Assembly Izz wrong");
    require(!combined.principalAxesUnique,"Symmetric assembly axes should be flagged as ambiguous");
    b.densityKgPerM3=2000;
    require(cad::tensor::analyze(b,&second),"Second density change failed");
    require(cad::tensor::aggregate({first,second},&combined),"Mixed-density assembly failed");
    near(combined.massKg,first.massKg+second.massKg,"Mixed-density mass wrong");
    near(combined.centerMm[0],(-5*first.massKg+15*second.massKg)/combined.massKg,
         "Weighted COM wrong");
    const auto original=combined;
    std::string error;
    require(!cad::tensor::aggregate({},&combined,&error)&&!error.empty(),"Empty assembly accepted");
    near(combined.massKg,original.massKg,"Invalid assembly changed result");
    auto invalid=first;invalid.massKg=0.0;
    require(!cad::tensor::aggregate({invalid},&combined),"Invalid component mass accepted");
    require(!cad::tensor::aggregate({first},nullptr),"Null assembly result accepted");
}
void testDegenerateAndRejectedInputs(){
    std::string error;
    cad::tensor::Report result;
    auto input=box(10,10,10,7850);
    require(cad::tensor::analyze(input,&result,&error),"Cube inertia invalid");
    require(!result.principalAxesUnique,"Cube has no unique principal directions");
    const auto oldResult=result;
    auto invalid=input;
    invalid.densityKgPerM3=0;
    require(!cad::tensor::analyze(invalid,&result,&error)&&!error.empty(),"Zero density accepted");
    near(result.massKg,oldResult.massKg,"Failed measurement modified output");
    invalid=input;invalid.volumeMm3=-1;
    require(!cad::tensor::analyze(invalid,&result),"Negative volume accepted");
    invalid=input;invalid.densityKgPerM3=std::numeric_limits<double>::quiet_NaN();
    require(!cad::tensor::analyze(invalid,&result),"NaN density accepted");
    invalid=input;invalid.densityKgPerM3=1000001;
    require(!cad::tensor::analyze(invalid,&result),"Excessive density accepted");
    invalid=input;invalid.centerMm[1]=std::numeric_limits<double>::infinity();
    require(!cad::tensor::analyze(invalid,&result),"Infinite center accepted");
    invalid=input;invalid.geometricInertiaMm5[0]=std::numeric_limits<double>::infinity();
    require(!cad::tensor::analyze(invalid,&result),"Infinite moment accepted");
    invalid=input;invalid.geometricInertiaMm5[1]=1000;
    require(!cad::tensor::analyze(invalid,&result),"Asymmetric tensor accepted");
    invalid=input;invalid.geometricInertiaMm5.fill(0.0);
    require(!cad::tensor::analyze(invalid,&result),"Zero tensor accepted");
    invalid=input;invalid.geometricInertiaMm5[0]=-input.geometricInertiaMm5[0];
    require(!cad::tensor::analyze(invalid,&result),"Negative eigenvalue accepted");
    require(!cad::tensor::analyze(input,nullptr),"Null output accepted");
}
} // namespace
int main(){
    try{testRectangularSolids();testRotationAndTranslation();testPhysicalAssembly();testDegenerateAndRejectedInputs();
        std::cout<<"PASS: "<<checks<<" mass tensor checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}
