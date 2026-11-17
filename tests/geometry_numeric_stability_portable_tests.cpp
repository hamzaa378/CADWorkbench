#include "core/BranchReferenceAgreement.h"
#include "core/AssemblyToleranceBudget.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
static int checks=0;
static void verify(bool yes,const char* reason){++checks;if(!yes)throw std::runtime_error(reason);}
int main(){try {
    using cad::lineage::samePhysicalWitness;
    using cad::workshop::ToleranceTerm;
    using cad::workshop::AssemblyToleranceBudget;
    using cad::workshop::assessAssemblyToleranceBudget;
    std::string error;
    // Regression: the previous implementation squared 1e-200, underflowing
    // both delta and tolerance and incorrectly approving separated witnesses.
    verify(!samePhysicalWitness({0,0,0},{2.e-200,0,0},1.e-200),"small separated witnesses must differ");
    verify(samePhysicalWitness({0,0,0},{0.5e-200,0,0},1.e-200),"small adjacent witnesses must agree");
    verify(!samePhysicalWitness({0,0,0},{9e150,0,0},8e150),"large separated witnesses must differ");
    verify(samePhysicalWitness({0,0,0},{4e150,0,0},8e150),"large adjacent witnesses must agree");
    verify(!samePhysicalWitness({0,0,0},{0,0,0},0),"zero tolerance rejected");
    verify(!samePhysicalWitness({INFINITY,0,0},{0,0,0}),"infinite witness rejected");
    verify(!samePhysicalWitness({0,0,0},{NAN,0,0}),"nan witness rejected");
    for(int i=1;i<8000;++i){
        const double epsilon=1e-6*i/8000.0;
        verify(samePhysicalWitness({0,0,0},{epsilon,0,0},1e-6),"linear witness tolerance");
        verify(!samePhysicalWitness({0,0,0},{epsilon,0,0},epsilon*0.9),"small relative witness violation");
    }
    const std::vector<ToleranceTerm> unit={{"nominal",0,0,0,+1}};
    AssemblyToleranceBudget budget;
    verify(assessAssemblyToleranceBudget(unit,-1e308,1e308,&budget,&error),"finite symmetric fit limits");
    verify(budget.targetCenterMm==0,"midpoint of wide symmetric bounds must not overflow");
    verify(budget.decision==cad::workshop::FitDecision::FeasibleWorstCase,"finite symmetric margins");
    verify(assessAssemblyToleranceBudget(unit,1e307,1e308,&budget,&error),"finite unbalanced fit bounds");
    verify(std::isfinite(budget.targetCenterMm)&&budget.targetCenterMm>1e307,"wide midpoint is finite");
    std::cout << checks << " numeric stability checks passed\n";
} catch(const std::exception& e){std::cerr << e.what() << '\n';return 1;}}
