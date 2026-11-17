#include "core/AtomicRebuildGuard.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::transaction;
int main(){
    int checks=0;
    const auto must=[&](bool x,const char* message){++checks;if(!x)throw std::runtime_error(message);};
    std::vector<BodyRevisionEvidence> before{
        {"wheel","wheel-0",true},{"frame","frame-0",true},
        {"bolt","bolt-0",true},{"drawing","drawing-0",true}};
    auto staged=before;
    std::vector<std::string> roots{"wheel","frame"};
    std::vector<std::string> changed{"SENTINEL"};std::string error;
    must(validateRebuildCommit(before,staged,roots,&changed,&error),"zero-edit transaction allowed");
    must(changed.empty()&&error.empty(),"success resets stale report and error");
    staged[0].geometryRevision="wheel-1";staged[0].sameCommittedTopology=false;
    staged[1].geometryRevision="frame-1";staged[1].sameCommittedTopology=false;
    staged[2].geometryRevision="bolt-1";staged[2].sameCommittedTopology=false;
    must(validateRebuildCommit(before,staged,roots,&changed,&error),"full detached commit valid");
    must(changed==std::vector<std::string>({"bolt","frame","wheel"}),"changed IDs deterministic");
    const auto old=changed;
    staged[2].geometryRevision="bolt-0";
    must(!validateRebuildCommit(before,staged,roots,&changed,&error),
         "linked descendant topology changed without geometry revision");
    must(changed==old,"failed commit must not alter published change list");
    staged[2].geometryRevision="bolt-1";
    staged[1].id="wheel";
    must(!validateRebuildCommit(before,staged,roots,&changed,&error),"duplicate staged identity");
    staged[1].id="frame";
    staged[3].id="missing";
    must(!validateRebuildCommit(before,staged,roots,&changed,&error),"fabricated body rejected");
    staged[3].id="drawing";
    staged[3].geometryRevision="";
    must(!validateRebuildCommit(before,staged,roots,&changed,&error),"missing geometry token rejected");
    staged[3]=before[3];
    auto truncated=staged;truncated.pop_back();
    must(!validateRebuildCommit(before,truncated,roots,&changed,&error),"deleted body rejected");
    must(!validateRebuildCommit(before,staged,{"wheel","unknown"},&changed,&error),
         "unknown target feature root rejected");
    must(!validateRebuildCommit(before,staged,{"frame","frame"},&changed,&error),
         "duplicate root rejected");
    must(!validateRebuildCommit(before,staged,{"wheel"},&changed,&error),
         "single root sent to multi-root engine rejected");
    must(!validateRebuildCommit(before,staged,roots,nullptr,&error),
         "null changed-IDs destination rejected");
    std::mt19937_64 rng(0xB0172026);
    std::vector<BodyRevisionEvidence> base;
    for(int i=0;i<40;++i)base.push_back({"part-"+std::to_string(i),"rev-0",true});
    for(int run=0;run<25000;++run){
        auto proposed=base;
        const int changedBody=int(rng()%proposed.size());
        proposed[changedBody].geometryRevision="rev-"+std::to_string(run+1);
        proposed[changedBody].sameCommittedTopology=false;
        must(validateRebuildCommit(base,proposed,{"part-0","part-1"},&changed,&error),
             "random complete stage accepted");
        must(changed.size()==1&&changed.front()==proposed[changedBody].id,
             "random change report accurate");
        // Rollback trigger: a topology edit with unchanged revision.
        proposed[changedBody].geometryRevision="rev-0";
        must(!validateRebuildCommit(base,proposed,{"part-0","part-1"},&changed,&error),
             "random unversioned B-rep mutation rejected");
    }
    std::cout<<checks<<" atomic-rebuild commit guard assertions passed\n";
}
