#include "core/DrawingReferenceAudit.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::drawing_audit;
static int n=0;
static void check(bool v,const char* msg){++n;if(!v)throw std::runtime_error(msg);}
int main(){try{
 std::vector<Source> parts;
 std::vector<Annotation> dimensions;
 for(int i=0;i<2500;++i){
   std::string id="part"+std::to_string(i),geom="g"+std::to_string(i);
   parts.push_back({id,geom,"meta"+std::to_string(i)});
   dimensions.push_back({"dim"+std::to_string(i),id,geom,"",true,false});
 }
 DrawingReferenceReport out;std::string err;
 check(auditDrawingReferences(parts,dimensions,&out,&err),"large drawing audit valid");
 check(out.currentAnnotations==2500&&out.eligibleForReview,"large report is complete");
 for(int i=0;i<2500;i+=17)parts[i].geometryToken+=" changed";
 check(auditDrawingReferences(parts,dimensions,&out,&err),"large modified drawing audited");
 check(out.issues.size()==148,"all modified bodies detected independently");
 check(!out.eligibleForReview,"stale issue blocks review");
 for(auto& ann:dimensions)if(ann.id=="dim0")ann.bodyId="missing";
 check(auditDrawingReferences(parts,dimensions,&out,&err),"orphan audit valid");
 check(out.issues[0].kind==IssueKind::MissingBody,"orphan classification retains annotation order");
 check(out.issues.size()==148,"orphan replaced one stale issue");
 const auto prior=out;
 dimensions.push_back(dimensions.front());
 check(!auditDrawingReferences(parts,dimensions,&out,&err),"duplicate in large inventory rejected");
 check(out.issues.size()==prior.issues.size(),"large inventory rollback atomic");
 std::cout<<n<<" drawing stress checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
