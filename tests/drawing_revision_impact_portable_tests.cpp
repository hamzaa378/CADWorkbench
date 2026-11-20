#include "core/DrawingRevisionImpact.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <vector>
using namespace cad::drawing_audit;
int main(){
    std::size_t count=0;
    auto test=[&](bool v,const char* why){++count;if(!v){std::cerr<<why<<"\n";std::abort();}};
    std::vector<Source> oldSources,updatedSources;
    std::vector<Annotation> oldAnnotations,updatedAnnotations;
    for(int i=1;i<=800;++i){
        const std::string id=std::to_string(i);
        oldSources.push_back({"BODY-"+id,"geom1","meta1"});
        oldAnnotations.push_back({"ANN-"+id,"BODY-"+id,"geom1","meta1",true,true});
    }
    updatedSources=oldSources;updatedAnnotations=oldAnnotations;
    DrawingRevisionImpact result;std::string error;
    test(compareDrawingRevisions(oldSources,oldAnnotations,updatedSources,updatedAnnotations,&result,&error),"compare matches");
    test(result.referenceMatches==800&&result.currentIssues.empty()&&!result.releaseApproved,"matched references still not certified");
    updatedSources[0].geometryToken="geom2";
    updatedSources[1].metadataToken="meta2";
    updatedAnnotations[2].capturedGeometryToken="stale2";
    updatedAnnotations[3].bodyId="BODY-6"; // source remains, but identity changed
    updatedAnnotations.erase(updatedAnnotations.begin()+4);
    updatedAnnotations.push_back({"ANN-new","BODY-6","geom1","meta1",true,true});
    test(compareDrawingRevisions(oldSources,oldAnnotations,updatedSources,updatedAnnotations,&result,&error),"changed package");
    test(result.changedBodyIds.size()==2&&result.currentIssues.size()==3,"source and stale annotations counted");
    test(result.added==1&&result.removed==1&&result.modified==2,"changes by immutable annotation id");
    const auto original=result;
    std::mt19937 gen(51);
    for(int repeat=0;repeat<100;++repeat){
        std::shuffle(oldSources.begin(),oldSources.end(),gen);
        std::shuffle(oldAnnotations.begin(),oldAnnotations.end(),gen);
        std::shuffle(updatedSources.begin(),updatedSources.end(),gen);
        std::shuffle(updatedAnnotations.begin(),updatedAnnotations.end(),gen);
        test(compareDrawingRevisions(oldSources,oldAnnotations,updatedSources,updatedAnnotations,&result,&error),"permutation invariant");
        test(result.added==original.added&&result.modified==original.modified&&result.removed==original.removed,"invariant counts");
        test(result.changedBodyIds==original.changedBodyIds,"invariant revision classification");
        for(std::size_t j=0;j<result.annotations.size();++j)
            test(result.annotations[j].annotationId==original.annotations[j].annotationId&&
                 result.annotations[j].kind==original.annotations[j].kind,"stable IDs");
    }
    auto malformed=updatedAnnotations;malformed[1].id=malformed[0].id;
    test(!compareDrawingRevisions(oldSources,oldAnnotations,updatedSources,malformed,&result,&error),"reject duplicate annotations");
    test(result.annotations.size()==original.annotations.size(),"failure atomic");
    test(!compareDrawingRevisions(oldSources,oldAnnotations,updatedSources,updatedAnnotations,nullptr,&error),"null report rejected");
    test(!error.empty(),"error supplied");
    std::cout<<count<<" drawing revision classification assertions passed\n";
}
