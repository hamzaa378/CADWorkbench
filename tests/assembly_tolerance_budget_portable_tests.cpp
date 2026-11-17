#include "core/AssemblyToleranceBudget.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
using namespace cad::workshop;
namespace {
bool near(double a,double b){return std::fabs(a-b)<1e-9;}
}
int main(){
    const std::vector<ToleranceTerm> chain={
        {"housing length",50.0,-0.15,0.10,+1},
        {"bearing width",20.0,-0.05,0.08,-1},
        {"shim",29.5,-0.02,0.03,-1}
    };
    AssemblyToleranceBudget r;std::string err;
    assert(assessAssemblyToleranceBudget(chain,0.20,0.80,&r,&err));
    assert(err.empty());
    // The chain is nominally 0.5 mm; minimum = 49.85-20.08-29.53=0.24.
    // maximum = 50.1-19.95-29.48=0.67.
    assert(near(r.minimumMarginMm,0.04));
    assert(near(r.maximumMarginMm,0.13));
    assert(near(r.totalWorstCaseWidthMm,0.43));
    assert(r.decision==FitDecision::FeasibleWorstCase);
    assert(r.rankedContributors.size()==3);
    assert(r.rankedContributors[0].label=="housing length");
    assert(near(r.rankedContributors[0].effectOnWorstCaseWidthMm,0.25));
    assert(r.rankedContributors[1].label=="bearing width");
    assert(near(r.rankedContributors[1].lowerSideEffectMm,0.08));
    assert(near(r.rankedContributors[1].upperSideEffectMm,0.05));
    assert(assessAssemblyToleranceBudget(chain,0.3,0.8,&r,&err) && r.decision==FitDecision::TooTight);
    assert(assessAssemblyToleranceBudget(chain,0.2,0.6,&r,&err) && r.decision==FitDecision::TooLoose);
    assert(assessAssemblyToleranceBudget(chain,0.3,0.6,&r,&err) && r.decision==FitDecision::BothLimitsViolated);
    const auto old=r;
    assert(!assessAssemblyToleranceBudget(chain,0.9,0.3,&r,&err));
    assert(r.decision==old.decision && r.rankedContributors.size()==old.rankedContributors.size());
    assert(!assessAssemblyToleranceBudget(chain,std::numeric_limits<double>::quiet_NaN(),1.0,&r,&err));
    assert(!assessAssemblyToleranceBudget({},0,1,&r,&err));
    assert(!assessAssemblyToleranceBudget({{"bad",1,0,0,0}},0,1,&r,&err));
    assert(!assessAssemblyToleranceBudget(chain,0,1,nullptr,&err));
    ToleranceResult exact;
    assert(calculateToleranceStack(chain,&exact,&err));
    assert(assessAssemblyToleranceBudget(chain,exact.worstMinMm,exact.worstMaxMm,&r,&err));
    assert(r.decision==FitDecision::FeasibleWorstCase);
    std::cout << "PASS: tolerance budget, asymmetric limits, risk ranking, rollback validation\n";
}
