#include "core/AssemblyNetworkStructureAudit.h"
#include "core/AssemblyResiliencePlanner.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {int n=0;void check(bool x,const char* msg){++n;if(!x)throw std::runtime_error(msg);}}
int main(){try{
  using namespace cad::mates;
  StructuralMateAudit out;std::string error;
  const std::vector<StructuralMateLink> ring={{0,1,3},{1,2,3},{2,3,3},{3,0,3}};
  check(auditMateStructure(4,0,ring,&out,&error),"ring audited");
  check(out.connected&&out.connectedComponents==1,"ring connected");
  check(out.independentCycles==1&&out.bridgeLinkIndices.empty(),"ring cycle not bridge");
  check(out.cycleLinkIndices.size()==4,"ring edges cyclic");
  check(out.independentPathsToFixed==std::vector<unsigned char>({0,2,2,2}),
        "ring provides two distinct fixed-frame link routes to every other occurrence");
  auto tail=ring;tail.push_back({3,4,3});
  check(auditMateStructure(5,0,tail,&out,&error),"tail audited");
  check(out.independentCycles==1&&out.bridgeLinkIndices.size()==1&&out.bridgeLinkIndices[0]==4,"tail bridge");
  check(out.independentPathsToFixed==std::vector<unsigned char>({0,2,2,2,1}),
        "unprotected tail correctly has only one independent fixed-frame path");
  check(out.bridgeImpacts.size()==1&&out.bridgeImpacts.front().linkIndex==4&&
        out.bridgeImpacts.front().detachedFromFixed==std::vector<std::size_t>{4},
        "bridge disconnection identifies actual detached occurrence");
  check(auditMateStructure(5,4,tail,&out,&error),"tail with new root audited");
  check(out.bridgeImpacts.size()==1&&
        out.bridgeImpacts.front().detachedFromFixed==std::vector<std::size_t>({0,1,2,3}),
        "fixed node switch reverses failure impact");
  check(auditMateStructure(5,0,tail,&out,&error),"restore root-zero tail for occurrence audit");
  check(out.criticalOccurrenceImpacts.size()==1&&
        out.criticalOccurrenceImpacts.front().occurrenceIndex==3&&
        out.criticalOccurrenceImpacts.front().detachedFromFixed==std::vector<std::size_t>{4},
        "ring-tail occurrence 3 is single point of failure for node 4");
  check(auditMateStructure(5,4,tail,&out,&error),"rooted at leaf");
  check(out.criticalOccurrenceImpacts.size()==1&&
        out.criticalOccurrenceImpacts[0].occurrenceIndex==3&&
        out.criticalOccurrenceImpacts[0].detachedFromFixed==std::vector<std::size_t>({0,1,2}),
        "leaf root loses ring when junction occurrence removed");
  check(auditMateStructure(5,0,{{0,1,1},{1,2,1},{1,3,1},{1,4,1}},&out,&error),"star audit");
  check(out.criticalOccurrenceImpacts.size()==1&&
        out.criticalOccurrenceImpacts[0].occurrenceIndex==1&&
        out.criticalOccurrenceImpacts[0].detachedFromFixed==std::vector<std::size_t>({2,3,4}),
        "star hub removal isolates three surviving occurrences");
  StructuralReinforcement reinforcement;
  check(planStructuralReinforcement(5,0,tail,&reinforcement,&error),"tail reinforcement plan");
  check(reinforcement.vulnerableFixedFrameLinksBefore==1&&
        reinforcement.vulnerableFixedFrameLinksAfter==0&&
        reinforcement.suggestedConnections.size()==1,
        "ring tail needs one independent support link");
  check(planStructuralReinforcement(5,0,{{0,1,1},{1,2,1},{1,3,1},{1,4,1}},
        &reinforcement,&error),"star reinforcement plan");
  check(reinforcement.vulnerableFixedFrameLinksBefore==4&&
        reinforcement.vulnerableFixedFrameLinksAfter==0&&
        reinforcement.suggestedConnections.size()==2,
        "star leaf pairing should eliminate all bridges with two links");
  check(planStructuralReinforcement(4,0,ring,&reinforcement,&error),"ring reinforcement plan");
  check(reinforcement.suggestedConnections.empty()&&
        reinforcement.vulnerableFixedFrameLinksBefore==0,
        "resilient ring should not request unnecessary constraints");
  check(planStructuralReinforcement(5,0,ring,&reinforcement,&error),"disconnected reinforcement plan");
  check(reinforcement.alreadyUnreachable==std::vector<std::size_t>{4}&&
        reinforcement.suggestedConnections.empty(),
        "unreachable component must not receive imaginary mates");
  const auto previousReinforcement=reinforcement;
  check(!planStructuralReinforcement(4,4,ring,&reinforcement,&error),
        "reinforcement must reject missing fixed occurrence");
  check(reinforcement.alreadyUnreachable==previousReinforcement.alreadyUnreachable&&
        reinforcement.suggestedConnections==previousReinforcement.suggestedConnections,
        "invalid reinforcement must leave old report intact");
  check(auditMateStructure(4,0,ring,&out,&error),"ring restored");
  check(out.criticalOccurrenceImpacts.empty(),"redundant ring has no critical occurrence");
  std::vector<StructuralMateLink> parallel={{0,1,3},{0,1,3}};
  check(auditMateStructure(2,0,parallel,&out,&error),"parallel allowed");
  check(out.independentCycles==1&&out.bridgeLinkIndices.empty(),"parallel not a bridge");
  check(out.criticalOccurrenceImpacts.empty(),"parallel links don't create critical occurrences");
  check(out.independentPathsToFixed==std::vector<unsigned char>({0,2}),
        "parallel saved mating connections count as two paths");
  check(auditMateStructure(2,0,{{0,1,1},{0,1,1},{0,1,1},{0,1,1}},&out,&error),"many parallel links");
  check(out.independentPathsToFixed[1]==3,"redundancy explicitly saturates at 3+");
  check(auditMateStructure(5,0,ring,&out,&error),"disconnected audited");
  check(!out.connected&&out.unreachableNodes==std::vector<std::size_t>{4},"unreachable node identified");
  check(out.connectedComponents==2&&out.independentCycles==1,"forest identity");
  check(out.independentPathsToFixed[4]==0,"disconnected occurrence has no fixed-frame paths");
  check(out.criticalOccurrenceImpacts.empty(),"already unreachable nodes don't create artificial single-point failures");
  const auto saved=out;
  check(!auditMateStructure(4,0,{{0,4,3}},&out,&error),"foreign node rejected");
  check(out.connectedComponents==saved.connectedComponents,"invalid does not overwrite result");
  check(out.criticalOccurrenceImpacts.size()==saved.criticalOccurrenceImpacts.size(),
        "invalid audit keeps previous critical-occurrence diagnostics");
  check(!auditMateStructure(4,0,{{0,1,0}},&out,&error),"empty mate rejected");
  check(!auditMateStructure(4,0,{{2,2,3}},&out,&error),"self mate rejected");
  check(!auditMateStructure(0,0,ring,&out,&error),"empty network rejected");
  check(!auditMateStructure(4,4,ring,&out,&error),"missing fixed node rejected");
  check(!auditMateStructure(257,0,ring,&out,&error),"oversized network rejected");
  check(auditMateStructure(1,0,{},&out,&error),"single fixed node valid");
  check(out.connected&&out.independentCycles==0&&out.unreachableNodes.empty(),"single node complete");
  std::cout<<n<<" assembly structure assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
