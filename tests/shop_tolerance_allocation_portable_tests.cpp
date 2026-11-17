#include "core/ShopToleranceAllocation.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::workshop;
static int checks=0;
static void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
int main(){try{
    std::string error;
    // Two identical contributors: low-cost component should be tightened first.
    const std::vector<ToleranceTerm> chain={{"cheap shim",100,-.3,.3,+1},
                                            {"expensive block",100,-.3,.3,-1}};
    const std::vector<ShopToleranceLimit> shop={{0,1},{0,100}};
    ShopToleranceAllocation plan;
    check(allocateShopTolerances(chain,shop,-.5,.5,&plan,&error),"plan must succeed");
    check(plan.shopFloorFeasible&&plan.fitsWorstCase,"shop feasible");
    check(plan.scales[0]<plan.scales[1],"cheaper shim is reduced first");
    check(plan.scales[1]==1,"expensive block left at original tolerance");
    check(std::abs(plan.proposedTerms[0].nominalMm-chain[0].nominalMm)<1e-12,"nominal fixed");
    check(plan.after.minimumMarginMm>=-1e-8&&plan.after.maximumMarginMm>=-1e-8,"worst-case fit");
    // Different shop floors and asymmetric signed deviations: tightening
    // one side of the sum must not be confused with tightening the other.
    std::vector<ToleranceTerm> asymmetric={{"shaft",50,-.25,.05,+1},
                                            {"housing",49.9,-.01,.2,-1}};
    std::vector<ShopToleranceLimit> floors={{.65,2},{.75,1}};
    check(allocateShopTolerances(asymmetric,floors,-.10,.23,&plan,&error),"infeasible still a valid study");
    check(!plan.shopFloorFeasible&&!plan.fitsWorstCase,"explicit infeasible floor");
    check(plan.scales[0]==.65&&plan.scales[1]==.75,"floor case documented");
    check(plan.edits.size()==2,"all changed shop-floor terms appear in the audit trail");
    auto old=plan;
    floors[0].minimumDeviationScale=std::numeric_limits<double>::quiet_NaN();
    check(!allocateShopTolerances(asymmetric,floors,-.10,.23,&plan,&error),"nan floor rejected");
    check(plan.scales==old.scales,"invalid input leaves previous result intact");
    floors[0].minimumDeviationScale=0;
    check(!allocateShopTolerances(asymmetric,floors,-.10,.23,nullptr,&error),"null output rejected");
    // Repeated deterministic property checks across signed dimensions and
    // industry-style asymmetric limits. A floor-feasible case must produce a
    // verified pass; an infeasible case must never report approval.
    std::mt19937 engine(41789);
    std::uniform_real_distribution<double> d(.01,.25);
    for(int iteration=0;iteration<1600;++iteration){
        std::vector<ToleranceTerm> work;
        std::vector<ShopToleranceLimit> limits;
        for(int j=0;j<4;++j){
            const double lo=d(engine),hi=d(engine);
            const int dir=(j%2==0)?1:-1;
            work.push_back({"datum_"+std::to_string(j),j<2?100.0:50.0,-lo,hi,dir});
            limits.push_back({.02*((iteration+j)%9),1.+((iteration+j)%15)});
        }
        const double low=-.16-d(engine),high=.12+d(engine);
        check(allocateShopTolerances(work,limits,low,high,&plan,&error),"random valid planning");
        check(plan.proposedTerms.size()==work.size(),"all source terms retained");
        check(plan.scales.size()==work.size(),"aligned scale vector");
        check(plan.fitsWorstCase==(plan.after.decision==FitDecision::FeasibleWorstCase),"correct fit truthfulness");
        if(plan.shopFloorFeasible)check(plan.fitsWorstCase,"floor feasible implies a feasible plan");
        for(int j=0;j<4;++j){
            check(plan.scales[j]+1e-12>=limits[j].minimumDeviationScale,"floor not exceeded");
            check(plan.scales[j]<=1.0,"never widen deviation");
            check(plan.proposedTerms[j].nominalMm==work[j].nominalMm,"nominal not modified");
            check(plan.proposedTerms[j].direction==work[j].direction,"signed direction not modified");
        }
    }
    std::cout<<checks<<" shop-tolerance allocation assertions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
