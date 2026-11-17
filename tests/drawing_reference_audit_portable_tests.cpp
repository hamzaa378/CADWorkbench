#include "core/DrawingReferenceAudit.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::drawing_audit;
static int checks=0;
static void check(bool okay,const char* msg){++checks;if(!okay)throw std::runtime_error(msg);}
int main(){try{
 std::string err;
 const std::vector<Source> source={{"A","geom1","meta1"},{"B","geom2","meta2"}};
 const std::vector<Annotation> current={{"diameter","A","geom1","",true,false},
                                       {"bom","B","","meta2",false,true},
                                       {"inspection","A","geom1","meta1",true,true}};
 DrawingReferenceReport out;
 check(auditDrawingReferences(source,current,&out,&err),"current drawing audited");
 check(err.empty(),"error cleared");
 check(out.eligibleForReview,"all tokens current");
 check(out.checkedAnnotations==3&&out.currentAnnotations==3,"all annotations accounted");
 check(out.issues.empty(),"no orphaned dimensions");
 auto annotations=current;
 annotations[0].capturedGeometryToken="stale";
 annotations[1].capturedMetadataToken="stale";
 annotations[2].bodyId="deleted";
 check(auditDrawingReferences(source,annotations,&out,&err),"stale drawing audits without overwriting content");
 check(!out.eligibleForReview&&out.issues.size()==3,"all three errors reported");
 check(out.currentAnnotations==0,"none current");
 check(out.issues[0].kind==IssueKind::GeometryStale,"geometry drift recognized");
 check(out.issues[1].kind==IssueKind::MetadataStale,"metadata drift recognized");
 check(out.issues[2].kind==IssueKind::MissingBody,"deleted source recognized");
 auto snapshot=out;
 annotations[1].id=annotations[0].id;
 check(!auditDrawingReferences(source,annotations,&out,&err),"duplicate annotations fail closed");
 check(out.issues.size()==snapshot.issues.size(),"failed audit preserves prior report");
 annotations=current;annotations[0].capturedGeometryToken="";
 check(!auditDrawingReferences(source,annotations,&out,&err),"required geometry token missing");
 annotations=current;annotations[2].geometryDependent=false;annotations[2].metadataDependent=false;
 check(!auditDrawingReferences(source,annotations,&out,&err),"untracked annotation forbidden");
 annotations=current;annotations[2].capturedMetadataToken="";
 check(!auditDrawingReferences(source,annotations,&out,&err),"required metadata token missing");
 auto duplicates=source;duplicates[1].bodyId="A";
 check(!auditDrawingReferences(duplicates,current,&out,&err),"duplicate source id forbidden");
 duplicates=source;duplicates[1].geometryToken="";
 check(!auditDrawingReferences(duplicates,current,&out,&err),"missing source token forbidden");
 check(!auditDrawingReferences(source,{},&out,&err),"empty annotation inventory forbidden");
 check(!auditDrawingReferences({},current,&out,&err),"empty body inventory forbidden");
 check(!auditDrawingReferences(source,current,nullptr,&err),"null output forbidden");
 check(out.issues.size()==snapshot.issues.size(),"failure never overwrites report");
 std::cout<<checks<<" drawing revision audit checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
