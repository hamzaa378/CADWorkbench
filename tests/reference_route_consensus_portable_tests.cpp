#include "core/ReferenceRouteConsensus.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::lineage;
int main(){
    int checks=0;
    const auto must=[&](bool result,const char* message){++checks;if(!result)throw std::runtime_error(message);};
    ReferenceRouteEvidence a{{12.0,-5.0,3.0},{{"edge_index",4},{"edge_anchor_version",2},
        {"edge_curve_kind",1},{"edge_length",12.0},{"edge_witness_x",12.0}}};
    std::string error;
    must(agreeReferenceRoutes({a},&error),"one current stage is accepted");
    must(error.empty(),"success clears stale error");
    must(agreeReferenceRoutes({a,a},&error),"two coincident topology routes agree");
    auto b=a;b.witness[0]+=0.00009;
    must(agreeReferenceRoutes({a,b},&error),"normal transported witness tolerance");
    b.witness[0]+=0.00003;
    must(!agreeReferenceRoutes({a,b},&error),"distinct Boolean merge locations rejected");
    b=a;b.descriptor["edge_index"]=5.0;
    must(!agreeReferenceRoutes({a,b},&error),"different OCCT edge identity rejected");
    b=a;b.descriptor["edge_index"]=4.00000000001;
    must(!agreeReferenceRoutes({a,b},&error),"discrete index cannot use fuzzy comparison");
    b=a;b.descriptor.erase("edge_index");b.descriptor["face_index"]=4.0;
    must(!agreeReferenceRoutes({a,b},&error),"same-size wrong topology schema rejected");
    b=a;b.descriptor["edge_length"]*=1.01;
    must(!agreeReferenceRoutes({a,b},&error),"different trimmed edge measure rejected");
    b=a;b.descriptor["edge_length"]+=0.000001;
    must(agreeReferenceRoutes({a,b},&error),"small OCCT numeric drift is tolerated");
    b=a;b.witness[1]=std::numeric_limits<double>::infinity();
    must(!agreeReferenceRoutes({a,b},&error),"infinite design witness rejected");
    b=a;b.descriptor["edge_length"]=std::numeric_limits<double>::quiet_NaN();
    must(!agreeReferenceRoutes({a,b},&error),"NaN topology descriptor rejected");
    must(!agreeReferenceRoutes({},&error),"no evidence rejected");
    must(!agreeReferenceRoutes(std::vector<ReferenceRouteEvidence>(33,a),&error),
         "path explosion rejected");
    must(!agreeReferenceRoutes({a},&error,0),"nonpositive witness tolerance rejected");
    must(!agreeReferenceRoutes({a},&error,std::numeric_limits<double>::infinity()),
         "infinite witness tolerance rejected");
    // Three Boolean routes can each be within tolerance of route #1 while
    // the two other successors disagree. A first-route-only comparison is
    // not proof of common ownership for a split/merge.
    ReferenceRouteEvidence origin=a,plus=a,minus=a;
    origin.witness={0,0,0};plus.witness={0.00009,0,0};minus.witness={-0.00009,0,0};
    must(!agreeReferenceRoutes({origin,plus,minus},&error),
         "pairwise witness drift through a Boolean diamond must reject");
    must(error.find("pairwise")!=std::string::npos,
         "rejected multi-route diamond needs explicit pairwise diagnostic");
    plus=origin;minus=origin;
    origin.descriptor["edge_length"]=10.0;
    plus.descriptor["edge_length"]=10.000009;
    minus.descriptor["edge_length"]=9.999991;
    must(!agreeReferenceRoutes({origin,plus,minus},&error),
         "pairwise edge descriptor drift through a merge must reject");
    must(agreeReferenceRoutes({origin,origin,origin},&error),
         "exactly agreeing three-route ancestry must pass");
    a.descriptor.clear();must(!agreeReferenceRoutes({a},&error),"empty descriptor rejected");
    a.descriptor={{"edge_index",4},{"edge_anchor_version",2},
                  {"edge_curve_kind",1},{"edge_length",12.0},{"edge_witness_x",12.0}};
    std::mt19937_64 rng(0xA113B22);
    std::uniform_real_distribution<double> p(-1e3,1e3);
    for(int i=0;i<60000;++i){
        a.witness={p(rng),p(rng),p(rng)};
        a.descriptor["edge_witness_x"]=a.witness[0];
        b=a;
        must(agreeReferenceRoutes({a,b},&error),"random equal rebase must agree");
        // A witnessed split/merge cannot move by a macroscopic offset.
        b.witness[(i%3)]+=0.01;
        must(!agreeReferenceRoutes({a,b},&error),"random shifted design point rejected");
        b=a;b.descriptor["edge_index"]+=(i%5)+1;
        must(!agreeReferenceRoutes({a,b},&error),"random wrong topology successor rejected");
    }
    std::cout<<checks<<" reference-route consensus assertions passed\n";
}
